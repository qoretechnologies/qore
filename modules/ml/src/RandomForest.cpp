/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    RandomForest.cpp

    RandomForest implementation

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "QC_RandomForest.h"
#include "ml_serialization.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <set>
#include <unordered_map>

extern const TypedHashDecl* hashdeclRandomForestClassificationResult;
extern const TypedHashDecl* hashdeclRandomForestRegressionResult;
extern const TypedHashDecl* hashdeclFeatureImportanceInfo;

QoreRandomForest::QoreRandomForest(int n_trees, int max_depth,
        int min_samples_split, int min_samples_leaf,
        const std::string& max_features, bool bootstrap,
        const std::string& criterion, const std::string& task, int64_t seed)
    : n_trees(n_trees), max_depth(max_depth),
      min_samples_split(min_samples_split), min_samples_leaf(min_samples_leaf),
      max_features(max_features), bootstrap(bootstrap),
      criterion(criterion), task(task),
      rng(seed == 0 ? std::random_device{}() : (unsigned)seed) {
}

int QoreRandomForest::computeMaxFeatures() const {
    if (max_features == "sqrt") {
        return std::max(1, (int)std::sqrt(n_features));
    }
    if (max_features == "log2") {
        return std::max(1, (int)(std::log2(n_features)));
    }
    // Try to parse as integer
    try {
        int val = std::stoi(max_features);
        return std::max(1, std::min(val, n_features));
    } catch (...) {
        return n_features;  // default: all features
    }
}

void QoreRandomForest::fit(const MatrixXd& X, const VectorXd& y,
        ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lk(mtx);

    if (X.rows() == 0 || X.cols() == 0) {
        xsink->raiseException("ML-RANDOM-FOREST-ERROR",
            "cannot fit on empty data");
        return;
    }
    if (X.rows() != y.size()) {
        xsink->raiseException("ML-RANDOM-FOREST-ERROR",
            "X has " QLLD " rows but y has " QLLD " elements",
            (int64_t)X.rows(), (int64_t)y.size());
        return;
    }

    n_features = (int)X.cols();
    int n_samples = (int)X.rows();
    int max_feat = computeMaxFeatures();

    // Determine classes for classification
    if (task == "classification") {
        std::set<double> unique_classes;
        for (int64_t i = 0; i < y.size(); ++i) {
            unique_classes.insert(y(i));
        }
        classes.assign(unique_classes.begin(), unique_classes.end());
        n_classes = (int)classes.size();
    }

    trees.clear();
    trees.resize(n_trees);

    for (int t = 0; t < n_trees; ++t) {
        if ((t % 10) == 0 && t > 0) {
            if (qore_check_cancel(xsink, "training random forest")) {
                trees.clear();
                return;
            }
        }

        // Bootstrap sample
        std::vector<int> sample_indices;
        if (bootstrap) {
            sample_indices.resize(n_samples);
            std::uniform_int_distribution<int> dist(0, n_samples - 1);
            for (int i = 0; i < n_samples; ++i) {
                sample_indices[i] = dist(rng);
            }
        }

        // Random feature subset
        std::vector<int> all_features(n_features);
        std::iota(all_features.begin(), all_features.end(), 0);
        std::shuffle(all_features.begin(), all_features.end(), rng);
        std::vector<int> feature_subset(all_features.begin(),
            all_features.begin() + max_feat);

        // Fit tree
        trees[t].fit(X, y, max_depth, min_samples_split, min_samples_leaf,
            criterion, task, rng,
            &feature_subset,
            bootstrap ? &sample_indices : nullptr);
    }

    fitted = true;
}

QoreHashNode* QoreRandomForest::predictInternal(const RowVectorXd& point,
        ExceptionSink* xsink) const {
    if (task == "classification") {
        // Aggregate votes
        std::unordered_map<int, int> votes;
        std::vector<double> prob_accum(n_classes, 0.0);

        for (const auto& tree : trees) {
            auto probs = tree.predictProba(point);
            for (int c = 0; c < n_classes && c < (int)probs.size(); ++c) {
                prob_accum[c] += probs[c];
            }
            int predicted = (int)tree.predict(point);
            votes[predicted]++;
        }

        // Find majority class
        int best_class = 0;
        int best_votes = 0;
        for (const auto& v : votes) {
            if (v.second > best_votes) {
                best_votes = v.second;
                best_class = v.first;
            }
        }

        // Normalize probabilities
        double max_prob = 0;
        for (int c = 0; c < n_classes; ++c) {
            prob_accum[c] /= n_trees;
            if (prob_accum[c] > max_prob) {
                max_prob = prob_accum[c];
            }
        }

        ReferenceHolder<QoreHashNode> rv(
            new QoreHashNode(hashdeclRandomForestClassificationResult, xsink), xsink);
        rv->setKeyValue("predicted_class", (int64_t)best_class, xsink);
        rv->setKeyValue("max_probability", max_prob, xsink);

        ReferenceHolder<QoreListNode> prob_list(new QoreListNode(autoTypeInfo), xsink);
        for (double p : prob_accum) {
            prob_list->push(p, xsink);
        }
        rv->setKeyValue("probabilities", prob_list.release(), xsink);

        ReferenceHolder<QoreListNode> cls_list(new QoreListNode(autoTypeInfo), xsink);
        for (double c : classes) {
            cls_list->push(c, xsink);
        }
        rv->setKeyValue("classes", cls_list.release(), xsink);

        return rv.release();
    }

    // Regression: average predictions
    double sum = 0;
    for (const auto& tree : trees) {
        sum += tree.predict(point);
    }
    double prediction = sum / n_trees;

    ReferenceHolder<QoreHashNode> rv(
        new QoreHashNode(hashdeclRandomForestRegressionResult, xsink), xsink);
    rv->setKeyValue("prediction", prediction, xsink);
    return rv.release();
}

QoreHashNode* QoreRandomForest::predict(const RowVectorXd& point,
        ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-RANDOM-FOREST-ERROR",
            "model has not been fitted");
        return nullptr;
    }
    if (point.size() != n_features) {
        xsink->raiseException("ML-RANDOM-FOREST-ERROR",
            "input has %d features, expected %d",
            (int)point.size(), n_features);
        return nullptr;
    }
    return predictInternal(point, xsink);
}

QoreListNode* QoreRandomForest::predictMatrix(const MatrixXd& X,
        ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-RANDOM-FOREST-ERROR",
            "model has not been fitted");
        return nullptr;
    }
    if ((int)X.cols() != n_features) {
        xsink->raiseException("ML-RANDOM-FOREST-ERROR",
            "input has %d features, expected %d",
            (int)X.cols(), n_features);
        return nullptr;
    }

    ReferenceHolder<QoreListNode> results(new QoreListNode(autoTypeInfo), xsink);
    for (int64_t i = 0; i < X.rows(); ++i) {
        if ((i % 100) == 0 && i > 0) {
            if (qore_check_cancel(xsink, "predicting with random forest")) {
                return nullptr;
            }
        }
        QoreHashNode* result = predictInternal(X.row(i), xsink);
        if (*xsink) {
            return nullptr;
        }
        results->push(result, xsink);
    }
    return results.release();
}

QoreHashNode* QoreRandomForest::getFeatureImportances(ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-RANDOM-FOREST-ERROR",
            "model has not been fitted");
        return nullptr;
    }

    // Average importances across all trees
    VectorXd imp = VectorXd::Zero(n_features);
    for (const auto& tree : trees) {
        imp += tree.featureImportances();
    }
    imp /= n_trees;

    // Renormalize
    double total = imp.sum();
    if (total > 0) {
        imp /= total;
    }

    ReferenceHolder<QoreHashNode> rv(
        new QoreHashNode(hashdeclFeatureImportanceInfo, xsink), xsink);
    ReferenceHolder<QoreListNode> names(new QoreListNode(stringTypeInfo), xsink);
    ReferenceHolder<QoreListNode> values(new QoreListNode(autoTypeInfo), xsink);
    for (int i = 0; i < n_features; ++i) {
        if (i < (int)field_names.size()) {
            names->push(new QoreStringNode(field_names[i]), xsink);
        } else {
            names->push(new QoreStringNode("feature_" + std::to_string(i)), xsink);
        }
        values->push(imp(i), xsink);
    }
    rv->setKeyValue("feature_names", names.release(), xsink);
    rv->setKeyValue("importances", values.release(), xsink);
    return rv.release();
}

// --- Serialization ---

std::vector<uint8_t> QoreRandomForest::serializeState() const {
    std::vector<uint8_t> buf;
    MLSerialization::writeInt32(buf, n_trees);
    MLSerialization::writeInt32(buf, max_depth);
    MLSerialization::writeInt32(buf, min_samples_split);
    MLSerialization::writeInt32(buf, min_samples_leaf);
    MLSerialization::writeString(buf, max_features);
    MLSerialization::writeInt32(buf, bootstrap ? 1 : 0);
    MLSerialization::writeString(buf, criterion);
    MLSerialization::writeString(buf, task);
    MLSerialization::writeInt32(buf, n_features);
    MLSerialization::writeInt32(buf, n_classes);
    MLSerialization::writeInt32(buf, (int32_t)classes.size());
    for (double c : classes) {
        MLSerialization::writeScalar(buf, c);
    }
    MLSerialization::writeStringVector(buf, field_names);
    MLSerialization::writeString(buf, target_field);
    MLSerialization::writeInt32(buf, (int32_t)trees.size());
    for (const auto& tree : trees) {
        tree.serialize(buf);
    }
    return buf;
}

QoreRandomForest* QoreRandomForest::deserializeState(const uint8_t* data,
        size_t len, ExceptionSink* xsink) {
    const uint8_t* ptr = data;
    size_t remaining = len;

    int n_trees = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int max_depth = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int min_samples_split = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int min_samples_leaf = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    std::string max_features = MLSerialization::readString(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    bool bootstrap = MLSerialization::readInt32(ptr, remaining, xsink) != 0;
    if (*xsink) { return nullptr; }
    std::string criterion = MLSerialization::readString(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    std::string task = MLSerialization::readString(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int n_features = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int n_classes = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int n_classes_list = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    std::vector<double> classes(n_classes_list);
    for (int i = 0; i < n_classes_list; ++i) {
        classes[i] = MLSerialization::readScalar(ptr, remaining, xsink);
        if (*xsink) { return nullptr; }
    }
    std::vector<std::string> field_names = MLSerialization::readStringVector(
        ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    std::string target_field = MLSerialization::readString(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int actual_n_trees = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }

    std::vector<FlatTree> trees(actual_n_trees);
    for (int i = 0; i < actual_n_trees; ++i) {
        trees[i] = FlatTree::deserialize(ptr, remaining, xsink);
        if (*xsink) { return nullptr; }
    }

    auto* rf = new QoreRandomForest(n_trees, max_depth, min_samples_split,
        min_samples_leaf, max_features, bootstrap, criterion, task, 42);
    rf->n_features = n_features;
    rf->n_classes = n_classes;
    rf->classes = std::move(classes);
    rf->field_names = std::move(field_names);
    rf->target_field = std::move(target_field);
    rf->trees = std::move(trees);
    rf->fitted = true;
    return rf;
}
