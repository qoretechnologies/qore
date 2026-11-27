/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    KNN.cpp

    Qore ml module - K-Nearest Neighbors implementation

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

#include "QC_KNN.h"
#include "ml_serialization.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>

// Extern declarations for hashdecls (defined in ml-module.cpp)
extern const TypedHashDecl* hashdeclKNNClassificationResult;
extern const TypedHashDecl* hashdeclKNNRegressionResult;

QoreKNN::QoreKNN(int k, const std::string& metric, const std::string& task,
    const std::string& weight_func)
    : k(k), metric(metric), task(task), weight_func(weight_func) {
}

double QoreKNN::computeDistance(const RowVectorXd& a, const RowVectorXd& b) const {
    if (metric == "euclidean") {
        return (a - b).norm();
    }
    if (metric == "manhattan") {
        return (a - b).cwiseAbs().sum();
    }
    // cosine distance: 1 - cos(theta)
    double norm_a = a.norm();
    double norm_b = b.norm();
    if (norm_a < 1e-10 || norm_b < 1e-10) {
        return 1.0;
    }
    return 1.0 - a.dot(b) / (norm_a * norm_b);
}

void QoreKNN::fit(const MatrixXd& X, const VectorXd& y, ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lk(mtx);

    if (X.rows() == 0 || X.cols() == 0) {
        xsink->raiseException("ML-KNN-ERROR",
            "cannot fit on empty data: provide at least one sample with one feature");
        return;
    }
    if (X.rows() != y.size()) {
        xsink->raiseException("ML-KNN-ERROR",
            "X has %d rows but y has %d elements; they must match",
            static_cast<int>(X.rows()), static_cast<int>(y.size()));
        return;
    }

    int n = static_cast<int>(X.rows());
    if (k > n) {
        xsink->raiseException("ML-KNN-ERROR",
            "k (%d) must be <= the number of samples (%d); reduce k or provide more data",
            k, n);
        return;
    }

    n_features = static_cast<int>(X.cols());
    ref_data = X;
    ref_labels = y;
    fitted = true;
}

QoreHashNode* QoreKNN::predictInternal(const RowVectorXd& point, ExceptionSink* xsink) const {
    if (point.size() != n_features) {
        xsink->raiseException("ML-KNN-ERROR",
            "input has %d features but model was trained with %d features",
            static_cast<int>(point.size()), n_features);
        return nullptr;
    }

    int n = static_cast<int>(ref_data.rows());

    // Compute distances to all reference points
    std::vector<double> distances(n);
    for (int i = 0; i < n; ++i) {
        distances[i] = computeDistance(point, ref_data.row(i));
    }
    int n_nan = 0;
    for (int i = 0; i < n; ++i) {
        n_nan += (distances[i] != distances[i]);
    }
    if (n_nan) {
        // a NaN distance (from a NaN or infinite feature) is infinite, the farthest, so that the distances are ordered
        for (double& d : distances) {
            if (std::isnan(d)) {
                d = std::numeric_limits<double>::infinity();
            }
        }
    }

    // Find k nearest neighbors using partial sort on indices
    std::vector<int> indices(n);
    std::iota(indices.begin(), indices.end(), 0);
    int actual_k = std::min(k, n);
    std::partial_sort(indices.begin(), indices.begin() + actual_k, indices.end(),
        [&distances](int a, int b) { return distances[a] < distances[b]; });
    // std::partial_sort is not stable: neighbors at the same distance are taken and ordered in the order of the
    // training data
    if (actual_k > 0) {
        double dk = distances[indices[actual_k - 1]];
        // the neighbors at the distance of the k-th neighbor are the last of the k, as they are sorted by distance
        int first_dk = actual_k;
        while (first_dk > 0 && distances[indices[first_dk - 1]] == dk) {
            --first_dk;
        }
        if (std::count(distances.begin(), distances.end(), dk) > actual_k - first_dk) {
            // not every point at that distance is among the k: take the first of them
            int slot = first_dk;
            for (int i = 0; i < n && slot < actual_k; ++i) {
                if (distances[i] == dk) {
                    indices[slot++] = i;
                }
            }
        }
        // order the neighbors at the same distance by their index: an insertion sort of the k sorted distances,
        // which moves neighbors only within a run of equal distances, or a sort for a large k
        if (actual_k <= 64) {
            for (int i = 1; i < actual_k; ++i) {
                int idx = indices[i];
                int j = i;
                while (j > 0 && distances[indices[j - 1]] == distances[idx] && indices[j - 1] > idx) {
                    indices[j] = indices[j - 1];
                    --j;
                }
                indices[j] = idx;
            }
        } else {
            std::sort(indices.begin(), indices.begin() + actual_k, [&distances](int a, int b) {
                return distances[a] < distances[b] || (distances[a] == distances[b] && a < b);
            });
        }
    }

    // Build neighbors list
    ReferenceHolder<QoreListNode> neighbors(new QoreListNode(autoTypeInfo), xsink);
    for (int i = 0; i < actual_k; ++i) {
        int idx = indices[i];
        ReferenceHolder<QoreHashNode> neighbor(new QoreHashNode(autoTypeInfo), xsink);
        neighbor->setKeyValue("index", static_cast<int64>(idx), xsink);
        neighbor->setKeyValue("distance", distances[idx], xsink);
        neighbor->setKeyValue("label", ref_labels(idx), xsink);
        neighbors->push(neighbor.release(), xsink);
    }

    if (task == "classification") {
        // Weighted vote for classification; an ordered map: a tie goes to the smallest class label, whatever the order
        // of the neighbors
        std::map<int64, double> votes;
        for (int i = 0; i < actual_k; ++i) {
            int idx = indices[i];
            int64 label = static_cast<int64>(std::round(ref_labels(idx)));
            double w;
            if (weight_func == "distance") {
                double d = distances[idx];
                w = (d < 1e-10) ? 1e10 : 1.0 / d;
            } else {
                w = 1.0;
            }
            votes[label] += w;
        }

        // Find class with highest vote weight
        int64 predicted_class = 0;
        double max_weight = -1.0;
        double total_weight = 0.0;
        for (const auto& p : votes) {
            total_weight += p.second;
            if (p.second > max_weight) {
                max_weight = p.second;
                predicted_class = p.first;
            }
        }

        double confidence = (total_weight > 1e-10) ? max_weight / total_weight : 0.0;

        ReferenceHolder<QoreHashNode> rv(new QoreHashNode(hashdeclKNNClassificationResult, xsink), xsink);
        rv->setKeyValue("predicted_class", predicted_class, xsink);
        rv->setKeyValue("confidence", confidence, xsink);
        rv->setKeyValue("neighbors", neighbors.release(), xsink);
        return rv.release();
    }

    // Regression: weighted average
    double weighted_sum = 0.0;
    double total_weight = 0.0;
    for (int i = 0; i < actual_k; ++i) {
        int idx = indices[i];
        double w;
        if (weight_func == "distance") {
            double d = distances[idx];
            w = (d < 1e-10) ? 1e10 : 1.0 / d;
        } else {
            w = 1.0;
        }
        weighted_sum += w * ref_labels(idx);
        total_weight += w;
    }

    double prediction = (total_weight > 1e-10) ? weighted_sum / total_weight : 0.0;

    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(hashdeclKNNRegressionResult, xsink), xsink);
    rv->setKeyValue("prediction", prediction, xsink);
    rv->setKeyValue("neighbors", neighbors.release(), xsink);
    return rv.release();
}

QoreHashNode* QoreKNN::predictClassification(const RowVectorXd& point,
        ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);

    if (!fitted) {
        xsink->raiseException("ML-KNN-ERROR", "model has not been fitted");
        return nullptr;
    }
    if (task != "classification") {
        xsink->raiseException("ML-KNN-ERROR",
            "model was configured for '%s', not classification", task.c_str());
        return nullptr;
    }
    if (point.size() != n_features) {
        xsink->raiseException("ML-KNN-ERROR",
            "input has %d features, expected %d", (int)point.size(), n_features);
        return nullptr;
    }
    return predictInternal(point, xsink);
}

QoreHashNode* QoreKNN::predictRegression(const RowVectorXd& point,
        ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);

    if (!fitted) {
        xsink->raiseException("ML-KNN-ERROR", "model has not been fitted");
        return nullptr;
    }
    if (task != "regression") {
        xsink->raiseException("ML-KNN-ERROR",
            "model was configured for '%s', not regression", task.c_str());
        return nullptr;
    }
    if (point.size() != n_features) {
        xsink->raiseException("ML-KNN-ERROR",
            "input has %d features, expected %d", (int)point.size(), n_features);
        return nullptr;
    }
    return predictInternal(point, xsink);
}

QoreListNode* QoreKNN::predictClassificationMatrix(const MatrixXd& X,
        ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);

    if (!fitted) {
        xsink->raiseException("ML-KNN-ERROR", "model has not been fitted");
        return nullptr;
    }
    if (task != "classification") {
        xsink->raiseException("ML-KNN-ERROR",
            "model was configured for '%s', not classification", task.c_str());
        return nullptr;
    }
    if ((int)X.cols() != n_features) {
        xsink->raiseException("ML-KNN-ERROR",
            "input has %d features, expected %d", (int)X.cols(), n_features);
        return nullptr;
    }

    ReferenceHolder<QoreListNode> rv(
        new QoreListNode(hashdeclKNNClassificationResult->getTypeInfo()), xsink);
    for (int64_t i = 0; i < X.rows(); ++i) {
        if ((i % 100) == 0 && i > 0) {
            if (qore_check_cancel(xsink, "KNN predictClassificationMatrix")) {
                return nullptr;
            }
        }
        QoreHashNode* result = predictInternal(X.row(i), xsink);
        if (*xsink) { return nullptr; }
        rv->push(result, xsink);
    }
    return rv.release();
}

QoreListNode* QoreKNN::predictRegressionMatrix(const MatrixXd& X,
        ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);

    if (!fitted) {
        xsink->raiseException("ML-KNN-ERROR", "model has not been fitted");
        return nullptr;
    }
    if (task != "regression") {
        xsink->raiseException("ML-KNN-ERROR",
            "model was configured for '%s', not regression", task.c_str());
        return nullptr;
    }
    if ((int)X.cols() != n_features) {
        xsink->raiseException("ML-KNN-ERROR",
            "input has %d features, expected %d", (int)X.cols(), n_features);
        return nullptr;
    }

    ReferenceHolder<QoreListNode> rv(
        new QoreListNode(hashdeclKNNRegressionResult->getTypeInfo()), xsink);
    for (int64_t i = 0; i < X.rows(); ++i) {
        if ((i % 100) == 0 && i > 0) {
            if (qore_check_cancel(xsink, "KNN predictRegressionMatrix")) {
                return nullptr;
            }
        }
        QoreHashNode* result = predictInternal(X.row(i), xsink);
        if (*xsink) { return nullptr; }
        rv->push(result, xsink);
    }
    return rv.release();
}

std::vector<uint8_t> QoreKNN::serializeState() const {
    std::vector<uint8_t> buf;
    // Hyperparameters
    MLSerialization::writeInt32(buf, k);
    MLSerialization::writeString(buf, metric);
    MLSerialization::writeString(buf, task);
    MLSerialization::writeString(buf, weight_func);
    // Model state
    MLSerialization::writeInt32(buf, n_features);
    MLSerialization::writeMatrix(buf, ref_data);
    MLSerialization::writeVector(buf, ref_labels);
    MLSerialization::writeStringVector(buf, field_names);
    MLSerialization::writeString(buf, target_field);
    return buf;
}

QoreKNN* QoreKNN::deserializeState(const uint8_t* data, size_t len, ExceptionSink* xsink) {
    const uint8_t* ptr = data;
    size_t remaining = len;

    int32_t k = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    std::string metric = MLSerialization::readString(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    std::string task = MLSerialization::readString(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    std::string weight_func = MLSerialization::readString(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int32_t n_features = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    MatrixXd ref_data = MLSerialization::readMatrix(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    VectorXd ref_labels = MLSerialization::readVector(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    std::vector<std::string> field_names = MLSerialization::readStringVector(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    std::string target_field = MLSerialization::readString(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }

    std::unique_ptr<QoreKNN> obj(new QoreKNN(k, metric, task, weight_func));
    obj->n_features = n_features;
    obj->ref_data = std::move(ref_data);
    obj->ref_labels = std::move(ref_labels);
    obj->field_names = std::move(field_names);
    obj->target_field = std::move(target_field);
    obj->fitted = true;
    return obj.release();
}
