/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    LinearRegression.cpp

    Qore ml module - LinearRegression implementation

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

#include "QC_LinearRegression.h"

// Extern declarations for hashdecls (defined in ml-module.cpp)
extern const TypedHashDecl* hashdeclLinearRegressionResult;
extern const TypedHashDecl* hashdeclLinearRegressionModelInfo;

QoreLinearRegression::QoreLinearRegression(bool fit_intercept, bool normalize)
    : fit_intercept(fit_intercept), do_normalize(normalize) {
}

void QoreLinearRegression::fit(const MatrixXd& X, const VectorXd& y, ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lk(mtx);

    if (X.rows() == 0 || X.cols() == 0) {
        xsink->raiseException("ML-LINEAR-REGRESSION-ERROR",
            "cannot fit on empty data: provide at least one sample with one feature");
        return;
    }
    if (X.rows() != y.size()) {
        xsink->raiseException("ML-LINEAR-REGRESSION-ERROR",
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

    // Solve using normal equations with QR decomposition for numerical stability
    if (fit_intercept) {
        // Augment X with a column of ones for the intercept
        MatrixXd X_aug(X_proc.rows(), X_proc.cols() + 1);
        X_aug << X_proc, MatrixXd::Ones(X_proc.rows(), 1);

        VectorXd beta = X_aug.colPivHouseholderQr().solve(y);

        coefficients = beta.head(n_features);
        intercept = beta(n_features);
    } else {
        coefficients = X_proc.colPivHouseholderQr().solve(y);
        intercept = 0.0;
    }

    // If normalized, transform coefficients back to original scale
    if (do_normalize) {
        for (int j = 0; j < n_features; ++j) {
            coefficients(j) /= feature_stds(j);
        }
        if (fit_intercept) {
            // Adjust intercept for the mean shift
            intercept -= coefficients.dot(feature_means);
        }
    }

    // Compute R-squared
    VectorXd y_pred = X * coefficients;
    if (fit_intercept) {
        y_pred.array() += intercept;
    }
    double ss_res = (y - y_pred).squaredNorm();
    double y_mean = y.mean();
    double ss_tot = (y.array() - y_mean).square().sum();
    if (ss_tot > 1e-10) {
        r_squared = 1.0 - ss_res / ss_tot;
    } else {
        r_squared = (ss_res < 1e-10) ? 1.0 : 0.0;
    }

    fitted = true;
}

void QoreLinearRegression::update(const MatrixXd& X, const VectorXd& y,
    double lr, ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-LINEAR-REGRESSION-ERROR",
            "model has not been fitted: call fit() or fitMatrix() before update()");
        return;
    }
    if (X.cols() != n_features) {
        xsink->raiseException("ML-LINEAR-REGRESSION-ERROR",
            "input has %d features but model was trained with %d features",
            static_cast<int>(X.cols()), n_features);
        return;
    }
    if (X.rows() != y.size()) {
        xsink->raiseException("ML-LINEAR-REGRESSION-ERROR",
            "X has %d rows but y has %d elements; they must match",
            static_cast<int>(X.rows()), static_cast<int>(y.size()));
        return;
    }
    if (X.rows() == 0) {
        xsink->raiseException("ML-LINEAR-REGRESSION-ERROR",
            "cannot update on empty data");
        return;
    }

    int n = static_cast<int>(X.rows());

    // SGD update: for each row adjust coefficients and intercept
    for (int i = 0; i < n; ++i) {
        if (i % 100 == 0 && qore_check_cancel(xsink, "LinearRegression update")) {
            return;
        }
        double predicted = X.row(i).dot(coefficients) + intercept;
        double error = predicted - y(i);
        coefficients -= lr * error * X.row(i).transpose();
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
    double y_mean = y.mean();
    double ss_tot = (y.array() - y_mean).square().sum();
    if (ss_tot > 1e-10) {
        r_squared = 1.0 - ss_res / ss_tot;
    } else {
        r_squared = (ss_res < 1e-10) ? 1.0 : 0.0;
    }
}

QoreHashNode* QoreLinearRegression::predictInternal(const RowVectorXd& point, ExceptionSink* xsink) const {
    if (point.size() != n_features) {
        xsink->raiseException("ML-LINEAR-REGRESSION-ERROR",
            "input has %d features but model was trained with %d features",
            static_cast<int>(point.size()), n_features);
        return nullptr;
    }

    double prediction = point.dot(coefficients) + intercept;

    ReferenceHolder<QoreListNode> coef_list(new QoreListNode(autoTypeInfo), xsink);
    for (int j = 0; j < n_features; ++j) {
        coef_list->push(coefficients(j), xsink);
    }

    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(hashdeclLinearRegressionResult, xsink), xsink);
    rv->setKeyValue("prediction", prediction, xsink);
    rv->setKeyValue("coefficients", coef_list.release(), xsink);
    rv->setKeyValue("intercept", intercept, xsink);
    rv->setKeyValue("r_squared", r_squared, xsink);

    return rv.release();
}

QoreHashNode* QoreLinearRegression::predict(const RowVectorXd& point, ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-LINEAR-REGRESSION-ERROR",
            "model has not been fitted: call fit() or fitMatrix() before predict()");
        return nullptr;
    }
    return predictInternal(point, xsink);
}

QoreListNode* QoreLinearRegression::predictMatrix(const MatrixXd& X, ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-LINEAR-REGRESSION-ERROR",
            "model has not been fitted: call fit() or fitMatrix() before predictMatrix()");
        return nullptr;
    }

    if (X.cols() != n_features) {
        xsink->raiseException("ML-LINEAR-REGRESSION-ERROR",
            "input has %d features but model was trained with %d features",
            static_cast<int>(X.cols()), n_features);
        return nullptr;
    }

    ReferenceHolder<QoreListNode> rv(new QoreListNode(hashdeclLinearRegressionResult->getTypeInfo()), xsink);
    for (Eigen::Index i = 0; i < X.rows(); ++i) {
        if (i % 100 == 0 && qore_check_cancel(xsink, "LinearRegression predictMatrix")) {
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

QoreHashNode* QoreLinearRegression::getModelInfo(ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-LINEAR-REGRESSION-ERROR",
            "model has not been fitted: call fit() or fitMatrix() first");
        return nullptr;
    }

    ReferenceHolder<QoreListNode> coef_list(new QoreListNode(autoTypeInfo), xsink);
    for (int j = 0; j < n_features; ++j) {
        coef_list->push(coefficients(j), xsink);
    }

    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(hashdeclLinearRegressionModelInfo, xsink), xsink);
    rv->setKeyValue("coefficients", coef_list.release(), xsink);
    rv->setKeyValue("intercept", intercept, xsink);
    rv->setKeyValue("r_squared", r_squared, xsink);
    rv->setKeyValue("n_features", static_cast<int64>(n_features), xsink);

    return rv.release();
}

QoreListNode* QoreLinearRegression::getCoefficients(ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-LINEAR-REGRESSION-ERROR",
            "model has not been fitted: call fit() or fitMatrix() first");
        return nullptr;
    }
    ReferenceHolder<QoreListNode> rv(new QoreListNode(autoTypeInfo), xsink);
    for (int j = 0; j < n_features; ++j) {
        rv->push(coefficients(j), xsink);
    }
    return rv.release();
}

double QoreLinearRegression::getIntercept(ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-LINEAR-REGRESSION-ERROR",
            "model has not been fitted: call fit() or fitMatrix() first");
        return 0.0;
    }
    return intercept;
}

double QoreLinearRegression::getRSquared(ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-LINEAR-REGRESSION-ERROR",
            "model has not been fitted: call fit() or fitMatrix() first");
        return 0.0;
    }
    return r_squared;
}
