/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    Lasso.cpp

    Qore ml module - Lasso regression implementation

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

#include "QC_Lasso.h"
#include "ml_serialization.h"

// Extern declarations for hashdecls (defined in ml-module.cpp)
extern const TypedHashDecl* hashdeclLassoResult;
extern const TypedHashDecl* hashdeclLassoModelInfo;

QoreLasso::QoreLasso(double alpha, bool fit_intercept, bool normalize,
    int max_iter, double tol)
    : alpha(alpha), fit_intercept(fit_intercept), do_normalize(normalize),
      max_iter(max_iter), tol(tol) {
}

double QoreLasso::softThreshold(double x, double lambda) {
    if (x > lambda) {
        return x - lambda;
    }
    if (x < -lambda) {
        return x + lambda;
    }
    return 0.0;
}

void QoreLasso::fit(const MatrixXd& X, const VectorXd& y, ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lk(mtx);

    if (X.rows() == 0 || X.cols() == 0) {
        xsink->raiseException("ML-LASSO-ERROR",
            "cannot fit on empty data: provide at least one sample with one feature");
        return;
    }
    if (X.rows() != y.size()) {
        xsink->raiseException("ML-LASSO-ERROR",
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

    // Center data if fit_intercept
    VectorXd y_c = y;
    RowVectorXd X_mean = X_proc.colwise().mean();
    double y_mean = 0.0;
    MatrixXd X_c = X_proc;

    if (fit_intercept) {
        y_mean = y_c.mean();
        X_c = X_proc.rowwise() - X_mean;
        y_c = y_c.array() - y_mean;
    }

    // Precompute squared norms of each column
    VectorXd X_col_sq(n_features);
    for (int j = 0; j < n_features; ++j) {
        X_col_sq(j) = X_c.col(j).squaredNorm();
    }

    // Initialize coefficients to zero
    coefficients = VectorXd::Zero(n_features);

    // L1 penalty scaled by number of samples
    double lambda = alpha * X.rows();

    // Maintain residual vector: r = y_c - X_c * coefficients
    // Updated incrementally when each coefficient changes: O(n) per feature instead of O(n*p)
    VectorXd residual = y_c;

    // Coordinate descent loop
    int iter = 0;
    for (iter = 0; iter < max_iter; ++iter) {
        // Cancellation check every 10 iterations
        if (iter % 10 == 0 && qore_check_cancel(xsink, "Lasso fit")) {
            return;
        }

        VectorXd coef_old = coefficients;

        for (int j = 0; j < n_features; ++j) {
            double old_coef_j = coefficients(j);

            // rho_j = X_j^T * (residual + X_j * old_coef_j)
            double rho_j = X_c.col(j).dot(residual) + X_col_sq(j) * old_coef_j;

            // Apply soft-thresholding
            double x_sq = X_col_sq(j);
            if (x_sq < 1e-10) {
                coefficients(j) = 0.0;
            } else {
                coefficients(j) = softThreshold(rho_j, lambda) / x_sq;
            }

            // Update residual incrementally: r -= X_j * (new_coef_j - old_coef_j)
            double delta = coefficients(j) - old_coef_j;
            if (delta != 0.0) {
                residual -= X_c.col(j) * delta;
            }
        }

        // Check convergence
        if ((coefficients - coef_old).lpNorm<Eigen::Infinity>() < tol) {
            ++iter;
            break;
        }
    }
    iterations_run = iter;

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

QoreHashNode* QoreLasso::predictInternal(const RowVectorXd& point, ExceptionSink* xsink) const {
    if (point.size() != n_features) {
        xsink->raiseException("ML-LASSO-ERROR",
            "input has %d features but model was trained with %d features",
            static_cast<int>(point.size()), n_features);
        return nullptr;
    }

    double prediction = point.dot(coefficients) + intercept;

    ReferenceHolder<QoreListNode> coef_list(new QoreListNode(autoTypeInfo), xsink);
    for (int j = 0; j < n_features; ++j) {
        coef_list->push(coefficients(j), xsink);
    }

    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(hashdeclLassoResult, xsink), xsink);
    rv->setKeyValue("prediction", prediction, xsink);
    rv->setKeyValue("coefficients", coef_list.release(), xsink);
    rv->setKeyValue("intercept", intercept, xsink);
    rv->setKeyValue("r_squared", r_squared, xsink);

    return rv.release();
}

QoreHashNode* QoreLasso::predict(const RowVectorXd& point, ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-LASSO-ERROR",
            "model has not been fitted: call fit() or fitMatrix() before predict()");
        return nullptr;
    }
    return predictInternal(point, xsink);
}

QoreListNode* QoreLasso::predictMatrix(const MatrixXd& X, ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-LASSO-ERROR",
            "model has not been fitted: call fit() or fitMatrix() before predictMatrix()");
        return nullptr;
    }

    if (X.cols() != n_features) {
        xsink->raiseException("ML-LASSO-ERROR",
            "input has %d features but model was trained with %d features",
            static_cast<int>(X.cols()), n_features);
        return nullptr;
    }

    ReferenceHolder<QoreListNode> rv(new QoreListNode(hashdeclLassoResult->getTypeInfo()), xsink);
    for (Eigen::Index i = 0; i < X.rows(); ++i) {
        if (i % 100 == 0 && qore_check_cancel(xsink, "Lasso predictMatrix")) {
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

QoreHashNode* QoreLasso::getModelInfo(ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-LASSO-ERROR",
            "model has not been fitted: call fit() or fitMatrix() first");
        return nullptr;
    }

    ReferenceHolder<QoreListNode> coef_list(new QoreListNode(autoTypeInfo), xsink);
    for (int j = 0; j < n_features; ++j) {
        coef_list->push(coefficients(j), xsink);
    }

    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(hashdeclLassoModelInfo, xsink), xsink);
    rv->setKeyValue("coefficients", coef_list.release(), xsink);
    rv->setKeyValue("intercept", intercept, xsink);
    rv->setKeyValue("r_squared", r_squared, xsink);
    rv->setKeyValue("n_features", static_cast<int64>(n_features), xsink);
    rv->setKeyValue("alpha", alpha, xsink);
    rv->setKeyValue("iterations_run", static_cast<int64>(iterations_run), xsink);

    return rv.release();
}

QoreListNode* QoreLasso::getCoefficients(ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-LASSO-ERROR",
            "model has not been fitted: call fit() or fitMatrix() first");
        return nullptr;
    }
    ReferenceHolder<QoreListNode> rv(new QoreListNode(autoTypeInfo), xsink);
    for (int j = 0; j < n_features; ++j) {
        rv->push(coefficients(j), xsink);
    }
    return rv.release();
}

double QoreLasso::getIntercept(ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-LASSO-ERROR",
            "model has not been fitted: call fit() or fitMatrix() first");
        return 0.0;
    }
    return intercept;
}

double QoreLasso::getRSquared(ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-LASSO-ERROR",
            "model has not been fitted: call fit() or fitMatrix() first");
        return 0.0;
    }
    return r_squared;
}

QoreListNode* QoreLasso::getFeatureMeans(ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-LASSO-ERROR",
            "model has not been fitted: call fit() or fitMatrix() first");
        return nullptr;
    }
    if (!do_normalize) {
        xsink->raiseException("ML-LASSO-ERROR",
            "model was not fitted with normalization; feature means are not available");
        return nullptr;
    }
    ReferenceHolder<QoreListNode> rv(new QoreListNode(autoTypeInfo), xsink);
    for (int j = 0; j < feature_means.size(); ++j) {
        rv->push(feature_means(j), xsink);
    }
    return rv.release();
}

QoreListNode* QoreLasso::getFeatureStds(ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-LASSO-ERROR",
            "model has not been fitted: call fit() or fitMatrix() first");
        return nullptr;
    }
    if (!do_normalize) {
        xsink->raiseException("ML-LASSO-ERROR",
            "model was not fitted with normalization; feature standard deviations are not available");
        return nullptr;
    }
    ReferenceHolder<QoreListNode> rv(new QoreListNode(autoTypeInfo), xsink);
    for (int j = 0; j < feature_stds.size(); ++j) {
        rv->push(feature_stds(j), xsink);
    }
    return rv.release();
}

std::vector<uint8_t> QoreLasso::serializeState() const {
    std::vector<uint8_t> buf;
    // Hyperparameters
    MLSerialization::writeScalar(buf, alpha);
    MLSerialization::writeBool(buf, fit_intercept);
    MLSerialization::writeBool(buf, do_normalize);
    MLSerialization::writeInt32(buf, max_iter);
    MLSerialization::writeScalar(buf, tol);
    // Model state
    MLSerialization::writeInt32(buf, n_features);
    MLSerialization::writeInt32(buf, iterations_run);
    MLSerialization::writeVector(buf, coefficients);
    MLSerialization::writeScalar(buf, intercept);
    MLSerialization::writeScalar(buf, r_squared);
    MLSerialization::writeVector(buf, feature_means);
    MLSerialization::writeVector(buf, feature_stds);
    MLSerialization::writeStringVector(buf, field_names);
    MLSerialization::writeString(buf, target_field);
    return buf;
}

QoreLasso* QoreLasso::deserializeState(const uint8_t* data, size_t len,
    ExceptionSink* xsink) {
    const uint8_t* ptr = data;
    size_t remaining = len;

    double alpha = MLSerialization::readScalar(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    bool fit_intercept = MLSerialization::readBool(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    bool do_normalize = MLSerialization::readBool(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int32_t max_iter = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    double tol = MLSerialization::readScalar(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int32_t n_features = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int32_t iterations_run = MLSerialization::readInt32(ptr, remaining, xsink);
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

    std::unique_ptr<QoreLasso> obj(new QoreLasso(alpha, fit_intercept, do_normalize,
        max_iter, tol));
    obj->n_features = n_features;
    obj->iterations_run = iterations_run;
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
