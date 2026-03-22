/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    MinMaxScaler.cpp

    Qore ml module - MinMaxScaler implementation

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

#include "QC_MinMaxScaler.h"
#include "ml_serialization.h"

extern const TypedHashDecl* hashdeclMinMaxScalerInfo;

void QoreMinMaxScaler::computeScaleFactors() {
    double range = feature_max - feature_min;
    scale.resize(n_features);
    min_adj.resize(n_features);
    for (int j = 0; j < n_features; ++j) {
        double data_range = data_max(j) - data_min(j);
        if (data_range > 1e-10) {
            scale(j) = range / data_range;
        } else {
            // Constant feature: map to feature_min
            scale(j) = 0.0;
        }
        min_adj(j) = feature_min - data_min(j) * scale(j);
    }
}

void QoreMinMaxScaler::fit(const MatrixXd& data, ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lk(mtx);

    if (data.rows() == 0 || data.cols() == 0) {
        xsink->raiseException("ML-MIN-MAX-SCALER-ERROR",
            "cannot fit on empty data: provide at least one sample with one feature");
        return;
    }

    n_features = static_cast<int>(data.cols());
    data_min = data.colwise().minCoeff();
    data_max = data.colwise().maxCoeff();
    computeScaleFactors();
    fitted = true;
}

RowVectorXd QoreMinMaxScaler::transformRow(const RowVectorXd& row, ExceptionSink* xsink) const {
    if (row.size() != n_features) {
        xsink->raiseException("ML-MIN-MAX-SCALER-ERROR",
            "input has %d features but scaler was fitted with %d features",
            static_cast<int>(row.size()), n_features);
        return RowVectorXd();
    }
    return row.array() * scale.transpose().array() + min_adj.transpose().array();
}

RowVectorXd QoreMinMaxScaler::inverseTransformRow(const RowVectorXd& row, ExceptionSink* xsink) const {
    if (row.size() != n_features) {
        xsink->raiseException("ML-MIN-MAX-SCALER-ERROR",
            "input has %d features but scaler was fitted with %d features",
            static_cast<int>(row.size()), n_features);
        return RowVectorXd();
    }

    RowVectorXd result(n_features);
    for (int j = 0; j < n_features; ++j) {
        if (std::abs(scale(j)) > 1e-10) {
            result(j) = (row(j) - min_adj(j)) / scale(j);
        } else {
            // Constant feature: return data_min
            result(j) = data_min(j);
        }
    }
    return result;
}

MatrixXd QoreMinMaxScaler::transform(const MatrixXd& data, ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);

    if (!fitted) {
        xsink->raiseException("ML-MIN-MAX-SCALER-ERROR",
            "scaler has not been fitted: call fit() or fitMatrix() first");
        return MatrixXd();
    }
    if (data.cols() != n_features) {
        xsink->raiseException("ML-MIN-MAX-SCALER-ERROR",
            "input has %d features but scaler was fitted with %d features",
            static_cast<int>(data.cols()), n_features);
        return MatrixXd();
    }

    return (data.array().rowwise() * scale.transpose().array()).rowwise() + min_adj.transpose().array();
}

MatrixXd QoreMinMaxScaler::inverseTransform(const MatrixXd& data, ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);

    if (!fitted) {
        xsink->raiseException("ML-MIN-MAX-SCALER-ERROR",
            "scaler has not been fitted: call fit() or fitMatrix() first");
        return MatrixXd();
    }
    if (data.cols() != n_features) {
        xsink->raiseException("ML-MIN-MAX-SCALER-ERROR",
            "input has %d features but scaler was fitted with %d features",
            static_cast<int>(data.cols()), n_features);
        return MatrixXd();
    }

    MatrixXd result(data.rows(), n_features);
    for (Eigen::Index i = 0; i < data.rows(); ++i) {
        for (int j = 0; j < n_features; ++j) {
            if (std::abs(scale(j)) > 1e-10) {
                result(i, j) = (data(i, j) - min_adj(j)) / scale(j);
            } else {
                result(i, j) = data_min(j);
            }
        }
    }
    return result;
}

MatrixXd QoreMinMaxScaler::fitTransform(const MatrixXd& data, ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lk(mtx);

    if (data.rows() == 0 || data.cols() == 0) {
        xsink->raiseException("ML-MIN-MAX-SCALER-ERROR",
            "cannot fit on empty data: provide at least one sample with one feature");
        return MatrixXd();
    }

    n_features = static_cast<int>(data.cols());
    data_min = data.colwise().minCoeff();
    data_max = data.colwise().maxCoeff();
    computeScaleFactors();
    fitted = true;

    return (data.array().rowwise() * scale.transpose().array()).rowwise() + min_adj.transpose().array();
}

QoreHashNode* QoreMinMaxScaler::getInfo(ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);

    if (!fitted) {
        xsink->raiseException("ML-MIN-MAX-SCALER-ERROR",
            "scaler has not been fitted: call fit() or fitMatrix() first");
        return nullptr;
    }

    ReferenceHolder<QoreListNode> dmin_list(new QoreListNode(floatTypeInfo), xsink);
    ReferenceHolder<QoreListNode> dmax_list(new QoreListNode(floatTypeInfo), xsink);
    for (int j = 0; j < n_features; ++j) {
        dmin_list->push(data_min(j), xsink);
        dmax_list->push(data_max(j), xsink);
    }

    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(hashdeclMinMaxScalerInfo, xsink), xsink);
    rv->setKeyValue("data_min", dmin_list.release(), xsink);
    rv->setKeyValue("data_max", dmax_list.release(), xsink);
    rv->setKeyValue("feature_min", feature_min, xsink);
    rv->setKeyValue("feature_max", feature_max, xsink);
    rv->setKeyValue("n_features", static_cast<int64>(n_features), xsink);

    return rv.release();
}

QoreListNode* QoreMinMaxScaler::getDataMinList(ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-MIN-MAX-SCALER-ERROR",
            "scaler has not been fitted: call fit() or fitMatrix() first");
        return nullptr;
    }
    ReferenceHolder<QoreListNode> rv(new QoreListNode(autoTypeInfo), xsink);
    for (int j = 0; j < n_features; ++j) {
        rv->push(data_min(j), xsink);
    }
    return rv.release();
}

QoreListNode* QoreMinMaxScaler::getDataMaxList(ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-MIN-MAX-SCALER-ERROR",
            "scaler has not been fitted: call fit() or fitMatrix() first");
        return nullptr;
    }
    ReferenceHolder<QoreListNode> rv(new QoreListNode(autoTypeInfo), xsink);
    for (int j = 0; j < n_features; ++j) {
        rv->push(data_max(j), xsink);
    }
    return rv.release();
}

std::vector<uint8_t> QoreMinMaxScaler::serializeState() const {
    std::vector<uint8_t> buf;
    // Hyperparameters
    MLSerialization::writeScalar(buf, feature_min);
    MLSerialization::writeScalar(buf, feature_max);
    // Model state
    MLSerialization::writeInt32(buf, n_features);
    MLSerialization::writeVector(buf, data_min);
    MLSerialization::writeVector(buf, data_max);
    MLSerialization::writeVector(buf, scale);
    MLSerialization::writeVector(buf, min_adj);
    MLSerialization::writeStringVector(buf, field_names);
    return buf;
}

QoreMinMaxScaler* QoreMinMaxScaler::deserializeState(const uint8_t* data, size_t len,
    ExceptionSink* xsink) {
    const uint8_t* ptr = data;
    size_t remaining = len;

    double feature_min = MLSerialization::readScalar(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    double feature_max = MLSerialization::readScalar(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int32_t n_features = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    VectorXd data_min_v = MLSerialization::readVector(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    VectorXd data_max_v = MLSerialization::readVector(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    VectorXd scale_v = MLSerialization::readVector(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    VectorXd min_adj_v = MLSerialization::readVector(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    std::vector<std::string> field_names = MLSerialization::readStringVector(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }

    std::unique_ptr<QoreMinMaxScaler> obj(new QoreMinMaxScaler(feature_min, feature_max));
    obj->n_features = n_features;
    obj->data_min = std::move(data_min_v);
    obj->data_max = std::move(data_max_v);
    obj->scale = std::move(scale_v);
    obj->min_adj = std::move(min_adj_v);
    obj->field_names = std::move(field_names);
    obj->fitted = true;
    return obj.release();
}
