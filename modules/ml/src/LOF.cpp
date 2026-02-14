/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    LOF.cpp

    Qore ml module - Local Outlier Factor implementation

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

#include "QC_LOF.h"

#include <algorithm>
#include <numeric>

// Extern declaration for hashdecl (defined in ml-module.cpp)
extern const TypedHashDecl* hashdeclLOFResult;

QoreLOF::QoreLOF(int k, double threshold)
    : k(k), threshold(threshold) {
}

VectorXd QoreLOF::computeDistances(const RowVectorXd& point) const {
    int n = static_cast<int>(ref_data.rows());
    VectorXd distances(n);
    for (int i = 0; i < n; ++i) {
        distances(i) = (point - ref_data.row(i)).norm();
    }
    return distances;
}

std::vector<int> QoreLOF::findKNN(const VectorXd& distances) const {
    int n = static_cast<int>(distances.size());
    std::vector<int> indices(n);
    std::iota(indices.begin(), indices.end(), 0);

    // Partial sort to find k nearest
    int actual_k = std::min(k, n);
    std::partial_sort(indices.begin(), indices.begin() + actual_k, indices.end(),
        [&distances](int a, int b) { return distances(a) < distances(b); });

    indices.resize(actual_k);
    return indices;
}

double QoreLOF::reachabilityDistance(double actual_dist, int ref_idx) const {
    return std::max(actual_dist, ref_k_distances[ref_idx]);
}

double QoreLOF::computeLRD(const RowVectorXd& point, const VectorXd& distances,
    const std::vector<int>& knn) const {
    double sum_reach_dist = 0.0;
    for (int idx : knn) {
        sum_reach_dist += reachabilityDistance(distances(idx), idx);
    }
    if (sum_reach_dist < 1e-10) {
        return std::numeric_limits<double>::max();
    }
    return static_cast<double>(knn.size()) / sum_reach_dist;
}

void QoreLOF::fit(const MatrixXd& data, ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lk(mtx);

    if (data.rows() == 0 || data.cols() == 0) {
        xsink->raiseException("ML-LOF-ERROR",
            "cannot fit on empty data: provide at least one sample with one feature");
        return;
    }

    int n = static_cast<int>(data.rows());
    if (n < k) {
        xsink->raiseException("ML-LOF-ERROR",
            "k (%d) is larger than the number of samples (%d); reduce k or provide more data",
            k, n);
        return;
    }

    n_features = static_cast<int>(data.cols());
    ref_data = data;

    // Compute pairwise distances and k-NN for all reference points
    ref_knn_indices.resize(n);
    ref_k_distances.resize(n);

    for (int i = 0; i < n; ++i) {
        // Compute distances from point i to all other points
        VectorXd distances(n);
        for (int j = 0; j < n; ++j) {
            if (i == j) {
                distances(j) = std::numeric_limits<double>::max();
            } else {
                distances(j) = (data.row(i) - data.row(j)).norm();
            }
        }

        // Find k nearest neighbors
        std::vector<int> indices(n);
        std::iota(indices.begin(), indices.end(), 0);
        std::partial_sort(indices.begin(), indices.begin() + k, indices.end(),
            [&distances](int a, int b) { return distances(a) < distances(b); });

        ref_knn_indices[i].assign(indices.begin(), indices.begin() + k);
        ref_k_distances[i] = distances(ref_knn_indices[i].back());
    }

    // Compute LRD for all reference points
    ref_lrds.resize(n);
    for (int i = 0; i < n; ++i) {
        double sum_reach_dist = 0.0;
        for (int j_idx : ref_knn_indices[i]) {
            double actual_dist = (data.row(i) - data.row(j_idx)).norm();
            sum_reach_dist += std::max(actual_dist, ref_k_distances[j_idx]);
        }
        if (sum_reach_dist < 1e-10) {
            ref_lrds[i] = std::numeric_limits<double>::max();
        } else {
            ref_lrds[i] = static_cast<double>(k) / sum_reach_dist;
        }
    }

    fitted = true;
}

QoreHashNode* QoreLOF::scoreInternal(const RowVectorXd& point, ExceptionSink* xsink) const {
    if (point.size() != n_features) {
        xsink->raiseException("ML-LOF-ERROR",
            "input has %d features but model was trained with %d features",
            static_cast<int>(point.size()), n_features);
        return nullptr;
    }

    // Compute distances to all reference points
    VectorXd distances = computeDistances(point);

    // Find k nearest neighbors among reference data
    std::vector<int> knn = findKNN(distances);

    // Compute k-distance (distance to k-th neighbor)
    double k_distance = distances(knn.back());

    // Compute LRD of the query point
    double lrd_point = computeLRD(point, distances, knn);

    // Compute LOF as average ratio of neighbor LRDs to point LRD
    double lof_sum = 0.0;
    for (int idx : knn) {
        if (lrd_point < 1e-10) {
            // Point is at same location as neighbors; treat as non-anomalous
            lof_sum += 1.0;
        } else {
            lof_sum += ref_lrds[idx] / lrd_point;
        }
    }
    double lof_score = lof_sum / static_cast<double>(knn.size());

    bool is_anomaly = lof_score >= threshold;

    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(hashdeclLOFResult, xsink), xsink);
    rv->setKeyValue("lof_score", lof_score, xsink);
    rv->setKeyValue("is_anomaly", is_anomaly, xsink);
    rv->setKeyValue("k_distance", k_distance, xsink);
    rv->setKeyValue("k", static_cast<int64>(k), xsink);

    return rv.release();
}

QoreHashNode* QoreLOF::score(const RowVectorXd& point, ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-LOF-ERROR",
            "model has not been fitted: call fit() or fitMatrix() before scoring");
        return nullptr;
    }
    return scoreInternal(point, xsink);
}

QoreListNode* QoreLOF::scoreMatrix(const MatrixXd& data, ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-LOF-ERROR",
            "model has not been fitted: call fit() or fitMatrix() before scoring");
        return nullptr;
    }

    if (data.cols() != n_features) {
        xsink->raiseException("ML-LOF-ERROR",
            "input has %d features but model was trained with %d features",
            static_cast<int>(data.cols()), n_features);
        return nullptr;
    }

    ReferenceHolder<QoreListNode> rv(new QoreListNode(hashdeclLOFResult->getTypeInfo()), xsink);
    for (Eigen::Index i = 0; i < data.rows(); ++i) {
        RowVectorXd row = data.row(i);
        QoreHashNode* result = scoreInternal(row, xsink);
        if (*xsink) {
            return nullptr;
        }
        rv->push(result, xsink);
    }
    return rv.release();
}
