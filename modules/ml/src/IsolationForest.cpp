/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    IsolationForest.cpp

    Qore ml module - IsolationForest implementation

    Copyright (C) 2026 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#include "QC_IsolationForest.h"
#include "ml_serialization.h"

#include <numeric>

// Extern declaration for hashdecl (defined in ml-module.cpp)
extern const TypedHashDecl* hashdeclIsolationForestResult;

// Euler-Mascheroni constant
static constexpr double EULER_MASCHERONI = 0.5772156649015329;

QoreIsolationForest::QoreIsolationForest(int n_trees, int sample_size, int max_depth,
    double threshold, int64_t seed)
    : n_trees(n_trees), sample_size(sample_size), max_depth(max_depth), threshold(threshold) {
    if (seed == 0) {
        std::random_device rd;
        rng.seed(rd());
    } else {
        rng.seed(static_cast<unsigned>(seed));
    }
}

double QoreIsolationForest::averagePathLength(int n) {
    if (n <= 1) {
        return 0.0;
    }
    if (n == 2) {
        return 1.0;
    }
    // c(n) = 2 * H(n-1) - 2*(n-1)/n
    // H(n) ~ ln(n) + Euler-Mascheroni constant
    double hn = std::log(static_cast<double>(n - 1)) + EULER_MASCHERONI;
    return 2.0 * hn - 2.0 * (static_cast<double>(n - 1)) / static_cast<double>(n);
}

void QoreIsolationForest::fit(const MatrixXd& data, ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lk(mtx);
    if (data.rows() == 0 || data.cols() == 0) {
        xsink->raiseException("ML-ISOLATION-FOREST-ERROR",
            "cannot fit on empty data: provide at least one sample with one feature");
        return;
    }

    n_features = static_cast<int>(data.cols());
    actual_sample_size = std::min(sample_size, static_cast<int>(data.rows()));

    // Compute max depth if not specified
    int max_d = max_depth;
    if (max_d <= 0) {
        max_d = static_cast<int>(std::ceil(std::log2(static_cast<double>(actual_sample_size))));
        if (max_d < 1) {
            max_d = 1;
        }
    }

    trees.clear();
    trees.reserve(n_trees);

    // Create index vector for sampling
    std::vector<int> all_indices(data.rows());
    std::iota(all_indices.begin(), all_indices.end(), 0);

    for (int t = 0; t < n_trees; ++t) {
        if (qore_check_cancel(xsink, "IsolationForest fit")) {
            return;
        }

        // Random subsample without replacement
        std::vector<int> sample_indices(all_indices);
        for (int i = static_cast<int>(sample_indices.size()) - 1; i > 0; --i) {
            std::uniform_int_distribution<int> dist(0, i);
            int j = dist(rng);
            std::swap(sample_indices[i], sample_indices[j]);
        }
        sample_indices.resize(actual_sample_size);

        trees.push_back(buildTree(data, sample_indices));
    }

    fitted = true;
}

IsolationTree QoreIsolationForest::buildTree(const MatrixXd& data,
    const std::vector<int>& indices) {
    IsolationTree tree;
    tree.nodes.reserve(2 * actual_sample_size);  // rough upper bound

    int max_d = max_depth;
    if (max_d <= 0) {
        max_d = static_cast<int>(std::ceil(std::log2(static_cast<double>(actual_sample_size))));
        if (max_d < 1) {
            max_d = 1;
        }
    }

    buildNode(tree, data, indices, 0, max_d);
    return tree;
}

int QoreIsolationForest::buildNode(IsolationTree& tree, const MatrixXd& data,
    const std::vector<int>& indices, int depth, int max_d) {

    int node_idx = static_cast<int>(tree.nodes.size());
    tree.nodes.emplace_back();

    // Base case: external node (leaf)
    if (depth >= max_d || indices.size() <= 1) {
        tree.nodes[node_idx].feature = -1;
        tree.nodes[node_idx].size = static_cast<int>(indices.size());
        return node_idx;
    }

    // Choose a random feature
    std::uniform_int_distribution<int> feat_dist(0, n_features - 1);
    int feat = feat_dist(rng);

    // Find min and max for this feature among the subsampled indices
    double min_val = data(indices[0], feat);
    double max_val = min_val;
    for (size_t i = 1; i < indices.size(); ++i) {
        double v = data(indices[i], feat);
        if (v < min_val) {
            min_val = v;
        }
        if (v > max_val) {
            max_val = v;
        }
    }

    // If all values are the same, make this an external node
    if (min_val >= max_val) {
        tree.nodes[node_idx].feature = -1;
        tree.nodes[node_idx].size = static_cast<int>(indices.size());
        return node_idx;
    }

    // Choose a random split point between min and max
    std::uniform_real_distribution<double> split_dist(min_val, max_val);
    double split = split_dist(rng);

    tree.nodes[node_idx].feature = feat;
    tree.nodes[node_idx].split_value = split;

    // Partition indices
    std::vector<int> left_indices, right_indices;
    left_indices.reserve(indices.size());
    right_indices.reserve(indices.size());
    for (int idx : indices) {
        if (data(idx, feat) < split) {
            left_indices.push_back(idx);
        } else {
            right_indices.push_back(idx);
        }
    }

    // Build children - must use node_idx to access nodes after recursive calls
    // because buildNode() may grow tree.nodes, invalidating any references
    int left_child = buildNode(tree, data, left_indices, depth + 1, max_d);
    tree.nodes[node_idx].left = left_child;
    tree.nodes[node_idx].right = buildNode(tree, data, right_indices, depth + 1, max_d);

    return node_idx;
}

double QoreIsolationForest::pathLength(const IsolationTree& tree, const RowVectorXd& point) const {
    int node_idx = 0;
    int depth = 0;

    while (true) {
        const ITreeNode& node = tree.nodes[node_idx];
        // External node
        if (node.feature == -1) {
            return static_cast<double>(depth) + averagePathLength(node.size);
        }

        if (point(node.feature) < node.split_value) {
            node_idx = node.left;
        } else {
            node_idx = node.right;
        }
        ++depth;
    }
}

QoreHashNode* QoreIsolationForest::scoreInternal(const RowVectorXd& point, ExceptionSink* xsink) const {
    if (point.size() != n_features) {
        xsink->raiseException("ML-ISOLATION-FOREST-ERROR",
            "input has %d features but model was trained with %d features",
            static_cast<int>(point.size()), n_features);
        return nullptr;
    }

    // Compute average path length across all trees
    double total_path = 0.0;
    for (const auto& tree : trees) {
        total_path += pathLength(tree, point);
    }
    double avg_path = total_path / static_cast<double>(n_trees);

    // Expected path length for the training sample size
    double expected = averagePathLength(actual_sample_size);

    // Anomaly score: s(x, n) = 2^(-E(h(x)) / c(n))
    double anomaly_score;
    if (expected <= 0.0) {
        anomaly_score = 0.5;  // degenerate case
    } else {
        anomaly_score = std::pow(2.0, -(avg_path / expected));
    }

    bool is_anomaly = anomaly_score >= threshold;

    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(hashdeclIsolationForestResult, xsink), xsink);
    rv->setKeyValue("anomaly_score", anomaly_score, xsink);
    rv->setKeyValue("is_anomaly", is_anomaly, xsink);
    rv->setKeyValue("average_path_length", avg_path, xsink);
    rv->setKeyValue("expected_path_length", expected, xsink);

    return rv.release();
}

QoreHashNode* QoreIsolationForest::score(const RowVectorXd& point, ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-ISOLATION-FOREST-ERROR",
            "model has not been fitted: call fit() or fitMatrix() before scoring");
        return nullptr;
    }
    return scoreInternal(point, xsink);
}

QoreListNode* QoreIsolationForest::scoreMatrix(const MatrixXd& data, ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-ISOLATION-FOREST-ERROR",
            "model has not been fitted: call fit() or fitMatrix() before scoring");
        return nullptr;
    }

    if (data.cols() != n_features) {
        xsink->raiseException("ML-ISOLATION-FOREST-ERROR",
            "input has %d features but model was trained with %d features",
            static_cast<int>(data.cols()), n_features);
        return nullptr;
    }

    ReferenceHolder<QoreListNode> rv(new QoreListNode(hashdeclIsolationForestResult->getTypeInfo()), xsink);
    for (Eigen::Index i = 0; i < data.rows(); ++i) {
        if (i % 100 == 0 && qore_check_cancel(xsink, "IsolationForest scoreMatrix")) {
            return nullptr;
        }
        RowVectorXd row = data.row(i);
        QoreHashNode* result = scoreInternal(row, xsink);
        if (*xsink) {
            return nullptr;
        }
        rv->push(result, xsink);
    }
    return rv.release();
}

QoreListNode* QoreIsolationForest::getTrees(ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-ISOLATION-FOREST-ERROR",
            "model has not been fitted: call fit() or fitMatrix() first");
        return nullptr;
    }
    ReferenceHolder<QoreListNode> rv(new QoreListNode(autoTypeInfo), xsink);
    for (const auto& tree : trees) {
        ReferenceHolder<QoreListNode> node_list(new QoreListNode(autoTypeInfo), xsink);
        for (const auto& node : tree.nodes) {
            ReferenceHolder<QoreHashNode> nh(new QoreHashNode(autoTypeInfo), xsink);
            nh->setKeyValue("feature", static_cast<int64>(node.feature), xsink);
            nh->setKeyValue("split_value", node.split_value, xsink);
            nh->setKeyValue("left", static_cast<int64>(node.left), xsink);
            nh->setKeyValue("right", static_cast<int64>(node.right), xsink);
            nh->setKeyValue("size", static_cast<int64>(node.size), xsink);
            node_list->push(nh.release(), xsink);
        }
        rv->push(node_list.release(), xsink);
    }
    return rv.release();
}

std::vector<uint8_t> QoreIsolationForest::serializeState() const {
    std::vector<uint8_t> buf;
    // Hyperparameters
    MLSerialization::writeInt32(buf, n_trees);
    MLSerialization::writeInt32(buf, sample_size);
    MLSerialization::writeInt32(buf, max_depth);
    MLSerialization::writeScalar(buf, threshold);
    // Model state
    MLSerialization::writeInt32(buf, n_features);
    MLSerialization::writeInt32(buf, actual_sample_size);
    // Trees: count + per-tree flat nodes
    MLSerialization::writeInt32(buf, static_cast<int32_t>(trees.size()));
    for (const auto& tree : trees) {
        MLSerialization::writeInt32(buf, static_cast<int32_t>(tree.nodes.size()));
        for (const auto& node : tree.nodes) {
            MLSerialization::writeInt32(buf, node.feature);
            MLSerialization::writeScalar(buf, node.split_value);
            MLSerialization::writeInt32(buf, node.left);
            MLSerialization::writeInt32(buf, node.right);
            MLSerialization::writeInt32(buf, node.size);
        }
    }
    MLSerialization::writeStringVector(buf, field_names);
    return buf;
}

QoreIsolationForest* QoreIsolationForest::deserializeState(const uint8_t* data, size_t len,
    ExceptionSink* xsink) {
    const uint8_t* ptr = data;
    size_t remaining = len;

    int32_t n_trees = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int32_t sample_size = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int32_t max_depth = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    double threshold = MLSerialization::readScalar(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int32_t n_features = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int32_t actual_sample_size = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }

    int32_t tree_count = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }

    std::vector<IsolationTree> trees;
    trees.reserve(tree_count);
    for (int32_t t = 0; t < tree_count; ++t) {
        int32_t node_count = MLSerialization::readInt32(ptr, remaining, xsink);
        if (*xsink) { return nullptr; }
        IsolationTree tree;
        tree.nodes.resize(node_count);
        for (int32_t n = 0; n < node_count; ++n) {
            tree.nodes[n].feature = MLSerialization::readInt32(ptr, remaining, xsink);
            if (*xsink) { return nullptr; }
            tree.nodes[n].split_value = MLSerialization::readScalar(ptr, remaining, xsink);
            if (*xsink) { return nullptr; }
            tree.nodes[n].left = MLSerialization::readInt32(ptr, remaining, xsink);
            if (*xsink) { return nullptr; }
            tree.nodes[n].right = MLSerialization::readInt32(ptr, remaining, xsink);
            if (*xsink) { return nullptr; }
            tree.nodes[n].size = MLSerialization::readInt32(ptr, remaining, xsink);
            if (*xsink) { return nullptr; }
        }
        trees.push_back(std::move(tree));
    }

    std::vector<std::string> field_names = MLSerialization::readStringVector(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }

    std::unique_ptr<QoreIsolationForest> obj(new QoreIsolationForest(
        n_trees, sample_size, max_depth, threshold, 1));
    obj->n_features = n_features;
    obj->actual_sample_size = actual_sample_size;
    obj->trees = std::move(trees);
    obj->field_names = std::move(field_names);
    obj->fitted = true;
    return obj.release();
}
