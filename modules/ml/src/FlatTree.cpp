/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    FlatTree.cpp

    CART decision tree implementation

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "FlatTree.h"
#include "ml_serialization.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <unordered_map>

// --- Impurity computation ---

double FlatTree::computeImpurity(const Eigen::VectorXd& y,
        const std::vector<int>& indices, const std::string& criterion,
        const std::vector<double>& classes) {
    int n = (int)indices.size();
    if (n == 0) {
        return 0.0;
    }

    if (criterion == "mse") {
        // Mean Squared Error: variance of target values
        double sum = 0, sum_sq = 0;
        for (int i : indices) {
            double v = y(i);
            sum += v;
            sum_sq += v * v;
        }
        double mean = sum / n;
        return sum_sq / n - mean * mean;
    }

    // Classification: count per class
    std::unordered_map<int, int> counts;
    for (int i : indices) {
        counts[(int)y(i)]++;
    }

    if (criterion == "gini") {
        double gini = 1.0;
        for (const auto& p : counts) {
            double prob = (double)p.second / n;
            gini -= prob * prob;
        }
        return gini;
    }

    if (criterion == "entropy") {
        double ent = 0.0;
        for (const auto& p : counts) {
            double prob = (double)p.second / n;
            if (prob > 0) {
                ent -= prob * std::log2(prob);
            }
        }
        return ent;
    }

    return 0.0;
}

// --- Find best split ---

FlatTree::SplitResult FlatTree::findBestSplit(const Eigen::MatrixXd& X,
        const Eigen::VectorXd& y, const std::vector<int>& indices,
        const std::string& criterion, const std::string& task,
        const std::vector<int>& features) const {
    SplitResult best;
    best.gain = 0.0;

    int n = (int)indices.size();
    double parent_impurity = computeImpurity(y, indices, criterion, classes);

    for (int feat : features) {
        // Sort indices by feature value
        std::vector<int> sorted_idx = indices;
        std::sort(sorted_idx.begin(), sorted_idx.end(),
            [&X, feat](int a, int b) { return X(a, feat) < X(b, feat); });

        // Try all midpoint thresholds
        for (int i = 0; i < n - 1; ++i) {
            double val_i = X(sorted_idx[i], feat);
            double val_next = X(sorted_idx[i + 1], feat);
            if (val_i == val_next) {
                continue;  // same value, no useful split
            }

            double threshold = (val_i + val_next) / 2.0;

            // Split
            std::vector<int> left_idx(sorted_idx.begin(), sorted_idx.begin() + i + 1);
            std::vector<int> right_idx(sorted_idx.begin() + i + 1, sorted_idx.end());

            double left_imp = computeImpurity(y, left_idx, criterion, classes);
            double right_imp = computeImpurity(y, right_idx, criterion, classes);

            // Weighted impurity reduction
            double n_left = (double)left_idx.size();
            double n_right = (double)right_idx.size();
            double gain = parent_impurity
                - (n_left / n) * left_imp
                - (n_right / n) * right_imp;

            if (gain > best.gain) {
                best.gain = gain;
                best.feature = feat;
                best.threshold = threshold;
                best.left_indices = std::move(left_idx);
                best.right_indices = std::move(right_idx);
            }
        }
    }

    return best;
}

// --- Recursive tree builder ---

int FlatTree::buildNode(const Eigen::MatrixXd& X, const Eigen::VectorXd& y,
        const std::vector<int>& indices, int depth, int max_depth,
        int min_samples_split, int min_samples_leaf,
        const std::string& criterion, const std::string& task,
        std::mt19937& rng, const std::vector<int>* feature_subset) {
    int node_idx = (int)nodes.size();
    nodes.emplace_back();
    CARTNode& node = nodes[node_idx];
    node.n_samples = (int)indices.size();
    node.impurity = computeImpurity(y, indices, criterion, classes);

    // Check stopping conditions
    bool make_leaf = false;
    if (max_depth > 0 && depth >= max_depth) {
        make_leaf = true;
    }
    if ((int)indices.size() < min_samples_split) {
        make_leaf = true;
    }
    // Check if all targets are the same
    {
        bool all_same = true;
        double first = y(indices[0]);
        for (size_t i = 1; i < indices.size(); ++i) {
            if (y(indices[i]) != first) {
                all_same = false;
                break;
            }
        }
        if (all_same) {
            make_leaf = true;
        }
    }

    if (!make_leaf) {
        // Determine features to consider
        std::vector<int> features;
        if (feature_subset) {
            features = *feature_subset;
        } else {
            features.resize(n_features);
            std::iota(features.begin(), features.end(), 0);
        }

        SplitResult split = findBestSplit(X, y, indices, criterion, task, features);

        if (split.feature >= 0 && !split.left_indices.empty()
                && !split.right_indices.empty()
                && (int)split.left_indices.size() >= min_samples_leaf
                && (int)split.right_indices.size() >= min_samples_leaf) {
            // Internal node
            node.feature_index = split.feature;
            node.threshold = split.threshold;
            node.weighted_impurity_reduction = split.gain * node.n_samples / total_samples;

            // Build children (note: nodes vector may reallocate, so don't hold references)
            int left_idx = buildNode(X, y, split.left_indices, depth + 1,
                max_depth, min_samples_split, min_samples_leaf,
                criterion, task, rng, feature_subset);
            int right_idx = buildNode(X, y, split.right_indices, depth + 1,
                max_depth, min_samples_split, min_samples_leaf,
                criterion, task, rng, feature_subset);

            nodes[node_idx].left = left_idx;
            nodes[node_idx].right = right_idx;
            return node_idx;
        }
    }

    // Leaf node
    if (task == "classification") {
        // Majority class and class probabilities
        std::unordered_map<int, int> counts;
        for (int i : indices) {
            counts[(int)y(i)]++;
        }
        int best_class = 0;
        int best_count = 0;
        for (const auto& p : counts) {
            if (p.second > best_count) {
                best_count = p.second;
                best_class = p.first;
            }
        }
        nodes[node_idx].value = (double)best_class;

        // Class probabilities
        nodes[node_idx].class_probs.resize(n_classes, 0.0);
        for (int c = 0; c < n_classes; ++c) {
            auto it = counts.find((int)classes[c]);
            if (it != counts.end()) {
                nodes[node_idx].class_probs[c] = (double)it->second / indices.size();
            }
        }
    } else {
        // Regression: mean of target values
        double sum = 0;
        for (int i : indices) {
            sum += y(i);
        }
        nodes[node_idx].value = sum / indices.size();
    }

    return node_idx;
}

// --- Public fit ---

void FlatTree::fit(const Eigen::MatrixXd& X, const Eigen::VectorXd& y,
        int max_depth, int min_samples_split, int min_samples_leaf,
        const std::string& criterion, const std::string& task,
        std::mt19937& rng,
        const std::vector<int>* feature_subset,
        const std::vector<int>* sample_indices) {
    nodes.clear();
    n_features = (int)X.cols();

    // Determine row indices
    std::vector<int> indices;
    if (sample_indices) {
        indices = *sample_indices;
    } else {
        indices.resize(X.rows());
        std::iota(indices.begin(), indices.end(), 0);
    }
    total_samples = (int)indices.size();

    // For classification: determine unique classes
    if (task == "classification") {
        std::set<double> unique_classes;
        for (int i : indices) {
            unique_classes.insert(y(i));
        }
        classes.assign(unique_classes.begin(), unique_classes.end());
        n_classes = (int)classes.size();
    } else {
        classes.clear();
        n_classes = 0;
    }

    // Build tree
    nodes.reserve(2 * indices.size());  // rough upper bound
    buildNode(X, y, indices, 0, max_depth, min_samples_split, min_samples_leaf,
        criterion, task, rng, feature_subset);
}

// --- Predict ---

double FlatTree::predict(const Eigen::RowVectorXd& x) const {
    if (nodes.empty()) {
        return 0.0;
    }

    int idx = 0;
    while (nodes[idx].feature_index >= 0) {
        if (x(nodes[idx].feature_index) <= nodes[idx].threshold) {
            idx = nodes[idx].left;
        } else {
            idx = nodes[idx].right;
        }
    }
    return nodes[idx].value;
}

std::vector<double> FlatTree::predictProba(const Eigen::RowVectorXd& x) const {
    if (nodes.empty()) {
        return {};
    }

    int idx = 0;
    while (nodes[idx].feature_index >= 0) {
        if (x(nodes[idx].feature_index) <= nodes[idx].threshold) {
            idx = nodes[idx].left;
        } else {
            idx = nodes[idx].right;
        }
    }
    return nodes[idx].class_probs;
}

// --- Feature importances ---

Eigen::VectorXd FlatTree::featureImportances() const {
    Eigen::VectorXd imp = Eigen::VectorXd::Zero(n_features);

    for (const auto& node : nodes) {
        if (node.feature_index >= 0) {
            imp(node.feature_index) += node.weighted_impurity_reduction;
        }
    }

    // Normalize to sum to 1.0
    double total = imp.sum();
    if (total > 0) {
        imp /= total;
    }

    return imp;
}

// --- Serialization ---

void FlatTree::serialize(std::vector<uint8_t>& buf) const {
    MLSerialization::writeInt32(buf, n_features);
    MLSerialization::writeInt32(buf, n_classes);
    MLSerialization::writeInt32(buf, total_samples);
    MLSerialization::writeInt32(buf, (int32_t)classes.size());
    for (double c : classes) {
        MLSerialization::writeScalar(buf, c);
    }
    MLSerialization::writeInt32(buf, (int32_t)nodes.size());
    for (const auto& node : nodes) {
        MLSerialization::writeInt32(buf, node.feature_index);
        MLSerialization::writeScalar(buf, node.threshold);
        MLSerialization::writeScalar(buf, node.value);
        MLSerialization::writeInt32(buf, (int32_t)node.class_probs.size());
        for (double p : node.class_probs) {
            MLSerialization::writeScalar(buf, p);
        }
        MLSerialization::writeInt32(buf, node.n_samples);
        MLSerialization::writeScalar(buf, node.impurity);
        MLSerialization::writeScalar(buf, node.weighted_impurity_reduction);
        MLSerialization::writeInt32(buf, node.left);
        MLSerialization::writeInt32(buf, node.right);
    }
}

FlatTree FlatTree::deserialize(const uint8_t*& ptr, size_t& remaining,
        ExceptionSink* xsink) {
    FlatTree tree;
    tree.n_features = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return tree; }
    tree.n_classes = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return tree; }
    tree.total_samples = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return tree; }
    int n_classes_list = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return tree; }
    tree.classes.resize(n_classes_list);
    for (int i = 0; i < n_classes_list; ++i) {
        tree.classes[i] = MLSerialization::readScalar(ptr, remaining, xsink);
        if (*xsink) { return tree; }
    }
    int n_nodes = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return tree; }
    tree.nodes.resize(n_nodes);
    for (int i = 0; i < n_nodes; ++i) {
        CARTNode& node = tree.nodes[i];
        node.feature_index = MLSerialization::readInt32(ptr, remaining, xsink);
        if (*xsink) { return tree; }
        node.threshold = MLSerialization::readScalar(ptr, remaining, xsink);
        if (*xsink) { return tree; }
        node.value = MLSerialization::readScalar(ptr, remaining, xsink);
        if (*xsink) { return tree; }
        int n_probs = MLSerialization::readInt32(ptr, remaining, xsink);
        if (*xsink) { return tree; }
        node.class_probs.resize(n_probs);
        for (int j = 0; j < n_probs; ++j) {
            node.class_probs[j] = MLSerialization::readScalar(ptr, remaining, xsink);
            if (*xsink) { return tree; }
        }
        node.n_samples = MLSerialization::readInt32(ptr, remaining, xsink);
        if (*xsink) { return tree; }
        node.impurity = MLSerialization::readScalar(ptr, remaining, xsink);
        if (*xsink) { return tree; }
        node.weighted_impurity_reduction = MLSerialization::readScalar(ptr, remaining, xsink);
        if (*xsink) { return tree; }
        node.left = MLSerialization::readInt32(ptr, remaining, xsink);
        if (*xsink) { return tree; }
        node.right = MLSerialization::readInt32(ptr, remaining, xsink);
        if (*xsink) { return tree; }
    }
    return tree;
}
