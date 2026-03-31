/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    SelectKBest.cpp

    SelectKBest implementation

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "QC_SelectKBest.h"
#include "ml_serialization.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>

QoreSelectKBest::QoreSelectKBest(int k, const std::string& score_func)
    : k(k), score_func(score_func) {
}

void QoreSelectKBest::computeFClassif(const MatrixXd& X, const VectorXd& y) {
    // ANOVA F-statistic: compare group means for each feature
    // Groups are defined by unique values in y (class labels)
    int n = (int)X.rows();

    // Collect unique classes and group indices
    std::map<double, std::vector<int>> groups;
    for (int i = 0; i < n; ++i) {
        groups[y(i)].push_back(i);
    }
    int n_groups = (int)groups.size();

    scores.resize(n_features);

    for (int f = 0; f < n_features; ++f) {
        // Overall mean for this feature
        double grand_mean = 0;
        for (int i = 0; i < n; ++i) {
            grand_mean += X(i, f);
        }
        grand_mean /= n;

        // Between-group sum of squares and within-group sum of squares
        double ss_between = 0;
        double ss_within = 0;

        for (const auto& [label, indices] : groups) {
            int ng = (int)indices.size();
            double group_mean = 0;
            for (int idx : indices) {
                group_mean += X(idx, f);
            }
            group_mean /= ng;

            ss_between += ng * (group_mean - grand_mean) * (group_mean - grand_mean);
            for (int idx : indices) {
                double d = X(idx, f) - group_mean;
                ss_within += d * d;
            }
        }

        // F = (SS_between / (k-1)) / (SS_within / (n-k))
        double df_between = n_groups - 1;
        double df_within = n - n_groups;

        if (df_within <= 0 || ss_within == 0) {
            // Degenerate case: all values same within groups or insufficient data
            scores(f) = (ss_between > 0) ? std::numeric_limits<double>::infinity() : 0.0;
        } else {
            double ms_between = ss_between / df_between;
            double ms_within = ss_within / df_within;
            scores(f) = ms_between / ms_within;
        }
    }
}

void QoreSelectKBest::computeMutualInfo(const MatrixXd& X, const VectorXd& y) {
    // Discretized mutual information estimation
    // Uses adaptive binning based on feature range (Sturges' rule for bin count)
    int n = (int)X.rows();
    int n_bins = std::max(2, (int)(1 + std::log2(n)));  // Sturges' rule

    // Collect class labels
    std::map<double, int> class_map;
    int next_label = 0;
    std::vector<int> y_labels(n);
    for (int i = 0; i < n; ++i) {
        auto it = class_map.find(y(i));
        if (it == class_map.end()) {
            class_map[y(i)] = next_label;
            y_labels[i] = next_label++;
        } else {
            y_labels[i] = it->second;
        }
    }
    int n_classes = next_label;

    // Class probabilities
    std::vector<double> p_y(n_classes, 0.0);
    for (int i = 0; i < n; ++i) {
        p_y[y_labels[i]] += 1.0;
    }
    for (int c = 0; c < n_classes; ++c) {
        p_y[c] /= n;
    }

    scores.resize(n_features);

    for (int f = 0; f < n_features; ++f) {
        // Find feature range
        double fmin = X(0, f), fmax = X(0, f);
        for (int i = 1; i < n; ++i) {
            if (X(i, f) < fmin) { fmin = X(i, f); }
            if (X(i, f) > fmax) { fmax = X(i, f); }
        }

        // Bin the feature values
        double bin_width = (fmax - fmin) / n_bins;
        if (bin_width == 0) {
            bin_width = 1.0;  // constant feature
        }

        // Joint probability table: p(bin, class)
        std::vector<std::vector<double>> joint(n_bins, std::vector<double>(n_classes, 0.0));
        std::vector<double> p_x(n_bins, 0.0);

        for (int i = 0; i < n; ++i) {
            int bin = std::min((int)((X(i, f) - fmin) / bin_width), n_bins - 1);
            joint[bin][y_labels[i]] += 1.0;
            p_x[bin] += 1.0;
        }

        // Normalize
        for (int b = 0; b < n_bins; ++b) {
            p_x[b] /= n;
            for (int c = 0; c < n_classes; ++c) {
                joint[b][c] /= n;
            }
        }

        // MI = sum over bins and classes of p(x,y) * log(p(x,y) / (p(x) * p(y)))
        double mi = 0;
        for (int b = 0; b < n_bins; ++b) {
            for (int c = 0; c < n_classes; ++c) {
                if (joint[b][c] > 0 && p_x[b] > 0 && p_y[c] > 0) {
                    mi += joint[b][c] * std::log(joint[b][c] / (p_x[b] * p_y[c]));
                }
            }
        }
        scores(f) = std::max(0.0, mi);  // MI is non-negative
    }
}

void QoreSelectKBest::selectTopK() {
    // Select top-k features by score
    std::vector<int> indices(n_features);
    std::iota(indices.begin(), indices.end(), 0);

    // Sort by score descending
    std::sort(indices.begin(), indices.end(), [this](int a, int b) {
        return scores(a) > scores(b);
    });

    int actual_k = std::min(k, n_features);
    support.assign(n_features, false);
    for (int i = 0; i < actual_k; ++i) {
        support[indices[i]] = true;
    }
    n_selected = actual_k;
}

void QoreSelectKBest::fit(const MatrixXd& X, const VectorXd& y,
    ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lk(mtx);

    if (X.rows() == 0 || X.cols() == 0) {
        xsink->raiseException("ML-SELECT-K-BEST-ERROR", "cannot fit on empty data");
        return;
    }
    if (X.rows() != y.size()) {
        xsink->raiseException("ML-SELECT-K-BEST-ERROR",
            "X has %d rows but y has %d elements", (int)X.rows(), (int)y.size());
        return;
    }

    n_features = (int)X.cols();

    if (score_func == "f_classif") {
        computeFClassif(X, y);
    } else if (score_func == "mutual_info") {
        computeMutualInfo(X, y);
    } else {
        xsink->raiseException("ML-SELECT-K-BEST-ERROR",
            "unknown score function '%s'; use 'f_classif' or 'mutual_info'",
            score_func.c_str());
        return;
    }

    selectTopK();
    fitted = true;
}

MatrixXd QoreSelectKBest::transform(const MatrixXd& X, ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);

    if (!fitted) {
        xsink->raiseException("ML-SELECT-K-BEST-ERROR",
            "selector has not been fitted");
        return MatrixXd();
    }
    if ((int)X.cols() != n_features) {
        xsink->raiseException("ML-SELECT-K-BEST-ERROR",
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

MatrixXd QoreSelectKBest::fitTransform(const MatrixXd& X, const VectorXd& y,
    ExceptionSink* xsink) {
    fit(X, y, xsink);
    if (*xsink) {
        return MatrixXd();
    }
    return transform(X, xsink);
}

QoreListNode* QoreSelectKBest::getScores(ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-SELECT-K-BEST-ERROR", "not fitted");
        return nullptr;
    }
    ReferenceHolder<QoreListNode> result(new QoreListNode(autoTypeInfo), xsink);
    for (int f = 0; f < n_features; ++f) {
        result->push(scores(f), xsink);
    }
    return result.release();
}

QoreListNode* QoreSelectKBest::getSupport(ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-SELECT-K-BEST-ERROR", "not fitted");
        return nullptr;
    }
    ReferenceHolder<QoreListNode> result(new QoreListNode(autoTypeInfo), xsink);
    for (int f = 0; f < n_features; ++f) {
        result->push((bool)support[f], xsink);
    }
    return result.release();
}

QoreListNode* QoreSelectKBest::getSelectedIndices(ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-SELECT-K-BEST-ERROR", "not fitted");
        return nullptr;
    }
    ReferenceHolder<QoreListNode> result(new QoreListNode(bigIntTypeInfo), xsink);
    for (int f = 0; f < n_features; ++f) {
        if (support[f]) {
            result->push((int64_t)f, xsink);
        }
    }
    return result.release();
}

// --- Serialization ---

std::vector<uint8_t> QoreSelectKBest::serializeState() const {
    std::vector<uint8_t> buf;
    MLSerialization::writeInt32(buf, k);
    MLSerialization::writeString(buf, score_func);
    MLSerialization::writeInt32(buf, n_features);
    MLSerialization::writeInt32(buf, n_selected);
    MLSerialization::writeVector(buf, scores);
    MLSerialization::writeInt32(buf, (int32_t)support.size());
    for (bool s : support) {
        MLSerialization::writeInt32(buf, s ? 1 : 0);
    }
    MLSerialization::writeStringVector(buf, field_names);
    return buf;
}

QoreSelectKBest* QoreSelectKBest::deserializeState(const uint8_t* data,
    size_t len, ExceptionSink* xsink) {
    const uint8_t* ptr = data;
    size_t remaining = len;

    int kval = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    std::string sf = MLSerialization::readString(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int nf = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int ns = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    VectorXd sc = MLSerialization::readVector(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int nsup = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    std::vector<bool> sup(nsup);
    for (int i = 0; i < nsup; ++i) {
        sup[i] = MLSerialization::readInt32(ptr, remaining, xsink) != 0;
        if (*xsink) { return nullptr; }
    }
    auto fnames = MLSerialization::readStringVector(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }

    auto* skb = new QoreSelectKBest(kval, sf);
    skb->n_features = nf;
    skb->n_selected = ns;
    skb->scores = std::move(sc);
    skb->support = std::move(sup);
    skb->field_names = std::move(fnames);
    skb->fitted = true;
    return skb;
}
