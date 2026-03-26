/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    Ridge.cpp

    Qore ml module - Ridge regression implementation

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

#include "QC_Ridge.h"
#include "ml_serialization.h"

// Extern declarations for hashdecls (defined in ml-module.cpp)
extern const TypedHashDecl* hashdeclRidgeResult;
extern const TypedHashDecl* hashdeclRidgeModelInfo;

QoreRidge::QoreRidge(double alpha, bool fit_intercept, bool normalize)
    : alpha(alpha), fit_intercept(fit_intercept), do_normalize(normalize) {
}

void QoreRidge::fit(const MatrixXd& X, const VectorXd& y, ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lk(mtx);

    if (X.rows() == 0 || X.cols() == 0) {
        xsink->raiseException("ML-RIDGE-ERROR",
            "cannot fit on empty data: provide at least one sample with one feature");
        return;
    }
    if (X.rows() != y.size()) {
        xsink->raiseException("ML-RIDGE-ERROR",
            "X has %d rows but y has %d elements; they must match",
            static_cast<int>(X.rows()), static_cast<int>(y.size()));
        return;
    }

    n_features = static_cast<int>(X.cols());

    // Optionally normalize features
    MatrixXd X_proc = X;
    if (do_normalize) {
        feature_means = X.colwise().mean();
        feature_stds.resize(n_features);
        for (int j = 0; j < n_features; ++j) {
            double std_val = std::sqrt((X.col(j).array() - feature_means(j)).square().mean());
            feature_stds(j) = (std_val > 1e-10) ? std_val : 1.0;
        }
        X_proc = (X.rowwise() - feature_means.transpose()).array().rowwise() / feature_stds.transpose().array();
    }

    // Center data for Ridge regression
    VectorXd y_proc = y;
    RowVectorXd X_mean = X_proc.colwise().mean();
    double y_mean = 0.0;

    if (fit_intercept) {
        y_mean = y_proc.mean();
        X_proc = X_proc.rowwise() - X_mean;
        y_proc = y_proc.array() - y_mean;
    }

    // Solve (X^T X + alpha * I) beta = X^T y via LDLT decomposition
    MatrixXd XtX = X_proc.transpose() * X_proc;
    XtX.diagonal().array() += alpha;
    coefficients = XtX.ldlt().solve(X_proc.transpose() * y_proc);

    // Compute intercept
    if (fit_intercept) {
        intercept = y_mean - X_mean * coefficients;
    } else {
        intercept = 0.0;
    }

    // If normalized, transform coefficients back to original scale
    if (do_normalize) {
        for (int j = 0; j < n_features; ++j) {
            coefficients(j) /= feature_stds(j);
        }
        if (fit_intercept) {
            intercept -= coefficients.dot(feature_means);
        }
    }

    // Compute R-squared on original data
    VectorXd y_pred = X * coefficients;
    if (fit_intercept) {
        y_pred.array() += intercept;
    }
    double ss_res = (y - y_pred).squaredNorm();
    double ss_tot = (y.array() - y.mean()).square().sum();
    if (ss_tot > 1e-10) {
        r_squared = 1.0 - ss_res / ss_tot;
    } else {
        r_squared = (ss_res < 1e-10) ? 1.0 : 0.0;
    }

    fitted = true;
}

void QoreRidge::update(const MatrixXd& X, const VectorXd& y,
    double lr, ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-RIDGE-ERROR",
            "model has not been fitted: call fit() or fitMatrix() before update()");
        return;
    }
    if (X.cols() != n_features) {
        xsink->raiseException("ML-RIDGE-ERROR",
            "input has %d features but model was trained with %d features",
            static_cast<int>(X.cols()), n_features);
        return;
    }
    if (X.rows() != y.size()) {
        xsink->raiseException("ML-RIDGE-ERROR",
            "X has %d rows but y has %d elements; they must match",
            static_cast<int>(X.rows()), static_cast<int>(y.size()));
        return;
    }
    if (X.rows() == 0) {
        xsink->raiseException("ML-RIDGE-ERROR",
            "cannot update on empty data");
        return;
    }

    int n = static_cast<int>(X.rows());

    // SGD update with L2 penalty: gradient = error * x + alpha * coefficients
    for (int i = 0; i < n; ++i) {
        if (i % 100 == 0 && qore_check_cancel(xsink, "Ridge update")) {
            return;
        }
        double predicted = X.row(i).dot(coefficients) + intercept;
        double error = predicted - y(i);
        coefficients -= lr * (error * X.row(i).transpose() + alpha * coefficients);
        if (fit_intercept) {
            intercept -= lr * error;
        }
    }

    // Recompute R-squared on the new batch
    VectorXd y_pred = X * coefficients;
    if (fit_intercept) {
        y_pred.array() += intercept;
    }
    double ss_res = (y - y_pred).squaredNorm();
    double y_mean_val = y.mean();
    double ss_tot = (y.array() - y_mean_val).square().sum();
    if (ss_tot > 1e-10) {
        r_squared = 1.0 - ss_res / ss_tot;
    } else {
        r_squared = (ss_res < 1e-10) ? 1.0 : 0.0;
    }
}

QoreHashNode* QoreRidge::predictInternal(const RowVectorXd& point, ExceptionSink* xsink) const {
    if (point.size() != n_features) {
        xsink->raiseException("ML-RIDGE-ERROR",
            "input has %d features but model was trained with %d features",
            static_cast<int>(point.size()), n_features);
        return nullptr;
    }

    double prediction = point.dot(coefficients) + intercept;

    ReferenceHolder<QoreListNode> coef_list(new QoreListNode(autoTypeInfo), xsink);
    for (int j = 0; j < n_features; ++j) {
        coef_list->push(coefficients(j), xsink);
    }

    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(hashdeclRidgeResult, xsink), xsink);
    rv->setKeyValue("prediction", prediction, xsink);
    rv->setKeyValue("coefficients", coef_list.release(), xsink);
    rv->setKeyValue("intercept", intercept, xsink);
    rv->setKeyValue("r_squared", r_squared, xsink);

    return rv.release();
}

QoreHashNode* QoreRidge::predict(const RowVectorXd& point, ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-RIDGE-ERROR",
            "model has not been fitted: call fit() or fitMatrix() before predict()");
        return nullptr;
    }
    return predictInternal(point, xsink);
}

QoreListNode* QoreRidge::predictMatrix(const MatrixXd& X, ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-RIDGE-ERROR",
            "model has not been fitted: call fit() or fitMatrix() before predictMatrix()");
        return nullptr;
    }

    if (X.cols() != n_features) {
        xsink->raiseException("ML-RIDGE-ERROR",
            "input has %d features but model was trained with %d features",
            static_cast<int>(X.cols()), n_features);
        return nullptr;
    }

    ReferenceHolder<QoreListNode> rv(new QoreListNode(hashdeclRidgeResult->getTypeInfo()), xsink);
    for (Eigen::Index i = 0; i < X.rows(); ++i) {
        if (i % 100 == 0 && qore_check_cancel(xsink, "Ridge predictMatrix")) {
            return nullptr;
        }
        RowVectorXd row = X.row(i);
        QoreHashNode* result = predictInternal(row, xsink);
        if (*xsink) {
            return nullptr;
        }
        rv->push(result, xsink);
    }
    return rv.release();
}

QoreHashNode* QoreRidge::getModelInfo(ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-RIDGE-ERROR",
            "model has not been fitted: call fit() or fitMatrix() first");
        return nullptr;
    }

    ReferenceHolder<QoreListNode> coef_list(new QoreListNode(autoTypeInfo), xsink);
    for (int j = 0; j < n_features; ++j) {
        coef_list->push(coefficients(j), xsink);
    }

    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(hashdeclRidgeModelInfo, xsink), xsink);
    rv->setKeyValue("coefficients", coef_list.release(), xsink);
    rv->setKeyValue("intercept", intercept, xsink);
    rv->setKeyValue("r_squared", r_squared, xsink);
    rv->setKeyValue("n_features", static_cast<int64>(n_features), xsink);
    rv->setKeyValue("alpha", alpha, xsink);

    return rv.release();
}

QoreListNode* QoreRidge::getCoefficients(ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-RIDGE-ERROR",
            "model has not been fitted: call fit() or fitMatrix() first");
        return nullptr;
    }
    ReferenceHolder<QoreListNode> rv(new QoreListNode(autoTypeInfo), xsink);
    for (int j = 0; j < n_features; ++j) {
        rv->push(coefficients(j), xsink);
    }
    return rv.release();
}

double QoreRidge::getIntercept(ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-RIDGE-ERROR",
            "model has not been fitted: call fit() or fitMatrix() first");
        return 0.0;
    }
    return intercept;
}

double QoreRidge::getRSquared(ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-RIDGE-ERROR",
            "model has not been fitted: call fit() or fitMatrix() first");
        return 0.0;
    }
    return r_squared;
}

QoreListNode* QoreRidge::getFeatureMeans(ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-RIDGE-ERROR",
            "model has not been fitted: call fit() or fitMatrix() first");
        return nullptr;
    }
    if (!do_normalize) {
        xsink->raiseException("ML-RIDGE-ERROR",
            "model was not fitted with normalization; feature means are not available");
        return nullptr;
    }
    ReferenceHolder<QoreListNode> rv(new QoreListNode(autoTypeInfo), xsink);
    for (int j = 0; j < feature_means.size(); ++j) {
        rv->push(feature_means(j), xsink);
    }
    return rv.release();
}

QoreListNode* QoreRidge::getFeatureStds(ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-RIDGE-ERROR",
            "model has not been fitted: call fit() or fitMatrix() first");
        return nullptr;
    }
    if (!do_normalize) {
        xsink->raiseException("ML-RIDGE-ERROR",
            "model was not fitted with normalization; feature standard deviations are not available");
        return nullptr;
    }
    ReferenceHolder<QoreListNode> rv(new QoreListNode(autoTypeInfo), xsink);
    for (int j = 0; j < feature_stds.size(); ++j) {
        rv->push(feature_stds(j), xsink);
    }
    return rv.release();
}

std::vector<uint8_t> QoreRidge::serializeState() const {
    std::vector<uint8_t> buf;
    // Hyperparameters
    MLSerialization::writeScalar(buf, alpha);
    MLSerialization::writeBool(buf, fit_intercept);
    MLSerialization::writeBool(buf, do_normalize);
    // Model state
    MLSerialization::writeInt32(buf, n_features);
    MLSerialization::writeVector(buf, coefficients);
    MLSerialization::writeScalar(buf, intercept);
    MLSerialization::writeScalar(buf, r_squared);
    MLSerialization::writeVector(buf, feature_means);
    MLSerialization::writeVector(buf, feature_stds);
    MLSerialization::writeStringVector(buf, field_names);
    MLSerialization::writeString(buf, target_field);
    return buf;
}

QoreRidge* QoreRidge::deserializeState(const uint8_t* data, size_t len,
    ExceptionSink* xsink) {
    const uint8_t* ptr = data;
    size_t remaining = len;

    double alpha = MLSerialization::readScalar(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    bool fit_intercept = MLSerialization::readBool(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    bool do_normalize = MLSerialization::readBool(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int32_t n_features = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    VectorXd coefficients = MLSerialization::readVector(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    double intercept = MLSerialization::readScalar(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    double r_squared = MLSerialization::readScalar(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    VectorXd feature_means = MLSerialization::readVector(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    VectorXd feature_stds = MLSerialization::readVector(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    std::vector<std::string> field_names = MLSerialization::readStringVector(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    std::string target_field = MLSerialization::readString(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }

    std::unique_ptr<QoreRidge> obj(new QoreRidge(alpha, fit_intercept, do_normalize));
    obj->n_features = n_features;
    obj->coefficients = std::move(coefficients);
    obj->intercept = intercept;
    obj->r_squared = r_squared;
    obj->feature_means = std::move(feature_means);
    obj->feature_stds = std::move(feature_stds);
    obj->field_names = std::move(field_names);
    obj->target_field = std::move(target_field);
    obj->fitted = true;
    return obj.release();
}
