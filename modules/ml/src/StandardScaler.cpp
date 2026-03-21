/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    StandardScaler.cpp

    Qore ml module - StandardScaler implementation

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

#include "QC_StandardScaler.h"

extern const TypedHashDecl* hashdeclStandardScalerInfo;

void QoreStandardScaler::fit(const MatrixXd& data, ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lk(mtx);

    if (data.rows() == 0 || data.cols() == 0) {
        xsink->raiseException("ML-STANDARD-SCALER-ERROR",
            "cannot fit on empty data: provide at least one sample with one feature");
        return;
    }

    n_features = static_cast<int>(data.cols());

    // Compute mean per feature
    mean_vec = data.colwise().mean();

    // Compute std per feature
    std_vec.resize(n_features);
    for (int j = 0; j < n_features; ++j) {
        double variance = (data.col(j).array() - mean_vec(j)).square().mean();
        double std_val = std::sqrt(variance);
        // Avoid division by zero: if std is near zero, set to 1.0
        std_vec(j) = (std_val > 1e-10) ? std_val : 1.0;
    }

    fitted = true;
}

RowVectorXd QoreStandardScaler::transformRow(const RowVectorXd& row, ExceptionSink* xsink) const {
    if (row.size() != n_features) {
        xsink->raiseException("ML-STANDARD-SCALER-ERROR",
            "input has %d features but scaler was fitted with %d features",
            static_cast<int>(row.size()), n_features);
        return RowVectorXd();
    }

    RowVectorXd result = row;
    if (with_mean) {
        result = result.array() - mean_vec.transpose().array();
    }
    if (with_std) {
        result = result.array() / std_vec.transpose().array();
    }
    return result;
}

RowVectorXd QoreStandardScaler::inverseTransformRow(const RowVectorXd& row, ExceptionSink* xsink) const {
    if (row.size() != n_features) {
        xsink->raiseException("ML-STANDARD-SCALER-ERROR",
            "input has %d features but scaler was fitted with %d features",
            static_cast<int>(row.size()), n_features);
        return RowVectorXd();
    }

    RowVectorXd result = row;
    if (with_std) {
        result = result.array() * std_vec.transpose().array();
    }
    if (with_mean) {
        result = result.array() + mean_vec.transpose().array();
    }
    return result;
}

MatrixXd QoreStandardScaler::transform(const MatrixXd& data, ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);

    if (!fitted) {
        xsink->raiseException("ML-STANDARD-SCALER-ERROR",
            "scaler has not been fitted: call fit() or fitMatrix() first");
        return MatrixXd();
    }
    if (data.cols() != n_features) {
        xsink->raiseException("ML-STANDARD-SCALER-ERROR",
            "input has %d features but scaler was fitted with %d features",
            static_cast<int>(data.cols()), n_features);
        return MatrixXd();
    }

    MatrixXd result = data;
    if (with_mean) {
        result = result.rowwise() - mean_vec.transpose();
    }
    if (with_std) {
        result = result.array().rowwise() / std_vec.transpose().array();
    }
    return result;
}

MatrixXd QoreStandardScaler::inverseTransform(const MatrixXd& data, ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);

    if (!fitted) {
        xsink->raiseException("ML-STANDARD-SCALER-ERROR",
            "scaler has not been fitted: call fit() or fitMatrix() first");
        return MatrixXd();
    }
    if (data.cols() != n_features) {
        xsink->raiseException("ML-STANDARD-SCALER-ERROR",
            "input has %d features but scaler was fitted with %d features",
            static_cast<int>(data.cols()), n_features);
        return MatrixXd();
    }

    MatrixXd result = data;
    if (with_std) {
        result = result.array().rowwise() * std_vec.transpose().array();
    }
    if (with_mean) {
        result = result.rowwise() + mean_vec.transpose();
    }
    return result;
}

MatrixXd QoreStandardScaler::fitTransform(const MatrixXd& data, ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lk(mtx);

    if (data.rows() == 0 || data.cols() == 0) {
        xsink->raiseException("ML-STANDARD-SCALER-ERROR",
            "cannot fit on empty data: provide at least one sample with one feature");
        return MatrixXd();
    }

    n_features = static_cast<int>(data.cols());

    mean_vec = data.colwise().mean();

    std_vec.resize(n_features);
    for (int j = 0; j < n_features; ++j) {
        double variance = (data.col(j).array() - mean_vec(j)).square().mean();
        double std_val = std::sqrt(variance);
        std_vec(j) = (std_val > 1e-10) ? std_val : 1.0;
    }

    fitted = true;

    // Transform in place
    MatrixXd result = data;
    if (with_mean) {
        result = result.rowwise() - mean_vec.transpose();
    }
    if (with_std) {
        result = result.array().rowwise() / std_vec.transpose().array();
    }
    return result;
}

QoreHashNode* QoreStandardScaler::getInfo(ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);

    if (!fitted) {
        xsink->raiseException("ML-STANDARD-SCALER-ERROR",
            "scaler has not been fitted: call fit() or fitMatrix() first");
        return nullptr;
    }

    ReferenceHolder<QoreListNode> mean_list(new QoreListNode(floatTypeInfo), xsink);
    ReferenceHolder<QoreListNode> std_list(new QoreListNode(floatTypeInfo), xsink);
    for (int j = 0; j < n_features; ++j) {
        mean_list->push(mean_vec(j), xsink);
        std_list->push(std_vec(j), xsink);
    }

    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(hashdeclStandardScalerInfo, xsink), xsink);
    rv->setKeyValue("mean", mean_list.release(), xsink);
    rv->setKeyValue("std", std_list.release(), xsink);
    rv->setKeyValue("n_features", static_cast<int64>(n_features), xsink);
    rv->setKeyValue("with_mean", with_mean, xsink);
    rv->setKeyValue("with_std", with_std, xsink);

    return rv.release();
}
