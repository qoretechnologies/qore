/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    RFE.cpp

    RecursiveFeatureElimination implementation

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "QC_RFE.h"
#include "ml_serialization.h"

#include <algorithm>
#include <numeric>

QoreRFE::QoreRFE(int n_features_to_select, int step)
    : n_features_to_select(n_features_to_select), step(step) {
}

void QoreRFE::fit(const MatrixXd& X, const VectorXd& y, ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lk(mtx);

    if (X.rows() == 0 || X.cols() == 0) {
        xsink->raiseException("ML-RFE-ERROR", "cannot fit on empty data");
        return;
    }
    if (X.rows() != y.size()) {
        xsink->raiseException("ML-RFE-ERROR",
            "X has %d rows but y has %d elements", (int)X.rows(), (int)y.size());
        return;
    }

    n_features = (int)X.cols();
    int target_k = std::min(n_features_to_select, n_features);
    if (target_k < 1) {
        target_k = 1;
    }

    // Initialize: all features active
    std::vector<bool> active(n_features, true);
    ranking.assign(n_features, 1);  // All start at rank 1
    int n_active = n_features;
    int round = 0;

    // Determine task type from target values
    std::string task = "classification";
    bool all_integer = true;
    for (int64_t i = 0; i < y.size(); ++i) {
        if (y(i) != std::floor(y(i))) {
            all_integer = false;
            break;
        }
    }
    if (!all_integer) {
        task = "regression";
    }

    // Iteratively eliminate features
    while (n_active > target_k) {
        // Build reduced matrix with only active features
        std::vector<int> active_indices;
        active_indices.reserve(n_active);
        for (int f = 0; f < n_features; ++f) {
            if (active[f]) {
                active_indices.push_back(f);
            }
        }

        MatrixXd X_reduced(X.rows(), n_active);
        for (int c = 0; c < n_active; ++c) {
            X_reduced.col(c) = X.col(active_indices[c]);
        }

        // Train a decision tree on the reduced features
        FlatTree tree;
        tree.n_features = n_active;

        // Detect classes for classification
        if (task == "classification") {
            std::set<double> unique_classes;
            for (int64_t i = 0; i < y.size(); ++i) {
                unique_classes.insert(y(i));
            }
            tree.n_classes = (int)unique_classes.size();
            tree.classes.assign(unique_classes.begin(), unique_classes.end());
        } else {
            tree.n_classes = 0;
        }

        // Create sample indices (all samples)
        std::vector<int> sample_indices(X.rows());
        std::iota(sample_indices.begin(), sample_indices.end(), 0);

        // Fit with moderate depth to get meaningful importances
        std::mt19937 rng(42 + round);
        tree.fit(X_reduced, y, 5, 2, 1,
            task == "classification" ? "gini" : "mse",
            task, rng, nullptr, &sample_indices);

        // Get feature importances
        VectorXd importances = tree.featureImportances();

        // Determine how many to remove this round
        int n_to_remove = std::min(step, n_active - target_k);

        // Find the n_to_remove features with lowest importance
        std::vector<int> sorted_by_importance(n_active);
        std::iota(sorted_by_importance.begin(), sorted_by_importance.end(), 0);
        std::sort(sorted_by_importance.begin(), sorted_by_importance.end(),
            [&importances](int a, int b) {
                return importances(a) < importances(b);
            });

        ++round;
        for (int r = 0; r < n_to_remove; ++r) {
            int reduced_idx = sorted_by_importance[r];
            int original_idx = active_indices[reduced_idx];
            active[original_idx] = false;
            ranking[original_idx] = round + 1;  // Higher rank = eliminated earlier
        }
        n_active -= n_to_remove;
    }

    // Surviving features get rank 1
    support.assign(n_features, false);
    n_selected = 0;
    for (int f = 0; f < n_features; ++f) {
        if (active[f]) {
            support[f] = true;
            ranking[f] = 1;
            ++n_selected;
        }
    }

    fitted = true;
}

MatrixXd QoreRFE::transform(const MatrixXd& X, ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);

    if (!fitted) {
        xsink->raiseException("ML-RFE-ERROR", "selector has not been fitted");
        return MatrixXd();
    }
    if ((int)X.cols() != n_features) {
        xsink->raiseException("ML-RFE-ERROR",
            "input has %d features, expected %d", (int)X.cols(), n_features);
        return MatrixXd();
    }

    MatrixXd result(X.rows(), n_selected);
    int col = 0;
    for (int f = 0; f < n_features; ++f) {
        if (support[f]) {
            result.col(col) = X.col(f);
            ++col;
        }
    }
    return result;
}

MatrixXd QoreRFE::fitTransform(const MatrixXd& X, const VectorXd& y,
    ExceptionSink* xsink) {
    fit(X, y, xsink);
    if (*xsink) {
        return MatrixXd();
    }
    return transform(X, xsink);
}

QoreListNode* QoreRFE::getSupport(ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-RFE-ERROR", "not fitted");
        return nullptr;
    }
    ReferenceHolder<QoreListNode> result(new QoreListNode(autoTypeInfo), xsink);
    for (int f = 0; f < n_features; ++f) {
        result->push((bool)support[f], xsink);
    }
    return result.release();
}

QoreListNode* QoreRFE::getRanking(ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-RFE-ERROR", "not fitted");
        return nullptr;
    }
    ReferenceHolder<QoreListNode> result(new QoreListNode(bigIntTypeInfo), xsink);
    for (int f = 0; f < n_features; ++f) {
        result->push((int64_t)ranking[f], xsink);
    }
    return result.release();
}

// --- Serialization ---

std::vector<uint8_t> QoreRFE::serializeState() const {
    std::vector<uint8_t> buf;
    MLSerialization::writeInt32(buf, n_features_to_select);
    MLSerialization::writeInt32(buf, step);
    MLSerialization::writeInt32(buf, n_features);
    MLSerialization::writeInt32(buf, n_selected);
    MLSerialization::writeInt32(buf, (int32_t)support.size());
    for (bool s : support) {
        MLSerialization::writeInt32(buf, s ? 1 : 0);
    }
    MLSerialization::writeInt32(buf, (int32_t)ranking.size());
    for (int r : ranking) {
        MLSerialization::writeInt32(buf, r);
    }
    MLSerialization::writeStringVector(buf, field_names);
    return buf;
}

QoreRFE* QoreRFE::deserializeState(const uint8_t* data, size_t len,
    ExceptionSink* xsink) {
    const uint8_t* ptr = data;
    size_t remaining = len;

    int nfts = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int st = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int nf = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int ns = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int nsup = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    std::vector<bool> sup(nsup);
    for (int i = 0; i < nsup; ++i) {
        sup[i] = MLSerialization::readInt32(ptr, remaining, xsink) != 0;
        if (*xsink) { return nullptr; }
    }
    int nrank = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    std::vector<int> rnk(nrank);
    for (int i = 0; i < nrank; ++i) {
        rnk[i] = MLSerialization::readInt32(ptr, remaining, xsink);
        if (*xsink) { return nullptr; }
    }
    auto fnames = MLSerialization::readStringVector(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }

    auto* rfe = new QoreRFE(nfts, st);
    rfe->n_features = nf;
    rfe->n_selected = ns;
    rfe->support = std::move(sup);
    rfe->ranking = std::move(rnk);
    rfe->field_names = std::move(fnames);
    rfe->fitted = true;
    return rfe;
}
