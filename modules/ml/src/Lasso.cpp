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
    int max_iter, double tol, bool warm_start)
    : alpha(alpha), fit_intercept(fit_intercept), do_normalize(normalize),
      max_iter(max_iter), tol(tol), warm_start(warm_start) {
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

    int n_samples = static_cast<int>(X.rows());

    // Per-column L2 normalization for scale-invariant regularization
    col_norms.resize(n_features);
    for (int j = 0; j < n_features; ++j) {
        col_norms(j) = X_c.col(j).norm();
        if (col_norms(j) < 1e-10) {
            col_norms(j) = 1.0;
        } else {
            X_c.col(j) /= col_norms(j);
        }
    }

    // Precompute squared norms of each normalized column
    VectorXd X_col_sq(n_features);
    for (int j = 0; j < n_features; ++j) {
        X_col_sq(j) = X_c.col(j).squaredNorm();
    }

    // Initialize coefficients (warm start or zero)
    if (!warm_start || !fitted || coefficients.size() != n_features) {
        coefficients = VectorXd::Zero(n_features);
    } else {
        // Scale existing coefficients into normalized space
        for (int j = 0; j < n_features; ++j) {
            coefficients(j) *= col_norms(j);
        }
    }

    // L1 penalty base (per-feature adjustment applied in loop)
    double lambda_base = alpha * n_samples;

    // Maintain residual vector: r = y_c - X_c * coefficients
    VectorXd residual = y_c - X_c * coefficients;

    // Coordinate descent loop with duality gap convergence
    int iter = 0;
    for (iter = 0; iter < max_iter; ++iter) {
        // Cancellation check every 10 iterations
        if (iter % 10 == 0 && qore_check_cancel(xsink, "Lasso fit")) {
            return;
        }

        for (int j = 0; j < n_features; ++j) {
            double old_coef_j = coefficients(j);

            // rho_j = X_j^T * (residual + X_j * old_coef_j)
            double rho_j = X_c.col(j).dot(residual) + X_col_sq(j) * old_coef_j;

            // Apply soft-thresholding with per-feature penalty scaling
            double x_sq = X_col_sq(j);
            if (x_sq < 1e-10) {
                coefficients(j) = 0.0;
            } else {
                // Divide lambda by col_norm to penalize original-scale coefficients
                coefficients(j) = softThreshold(rho_j, lambda_base / col_norms(j)) / x_sq;
            }

            // Update residual incrementally: r -= X_j * (new_coef_j - old_coef_j)
            double delta = coefficients(j) - old_coef_j;
            if (delta != 0.0) {
                residual -= X_c.col(j) * delta;
            }
        }

        // Duality gap convergence check (using original-scale coefficients)
        double l1_norm = 0.0;
        for (int j = 0; j < n_features; ++j) {
            l1_norm += std::abs(coefficients(j) / col_norms(j));
        }
        double primal = 0.5 * residual.squaredNorm() / n_samples + alpha * l1_norm;
        VectorXd Xtr = X_c.transpose() * residual;
        double dual_scale = std::max(1.0, Xtr.lpNorm<Eigen::Infinity>() / (alpha * n_samples));
        VectorXd theta = residual / (n_samples * dual_scale);
        double dual = 0.5 * y_c.squaredNorm() / n_samples
            - 0.5 * n_samples * (theta - y_c / n_samples).squaredNorm();
        double gap = primal - dual;
        if (gap < tol) {
            ++iter;
            break;
        }
    }
    iterations_run = iter;

    // Scale coefficients back from normalized space
    for (int j = 0; j < n_features; ++j) {
        coefficients(j) /= col_norms(j);
    }

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

void QoreLasso::fitSparse(const SparseMatrixXd& X, const VectorXd& y, ExceptionSink* xsink) {
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
    int n_samples = static_cast<int>(X.rows());

    // Convert to column-major for efficient column access in coordinate descent
    SparseMatrixXdCol X_cm = X;

    // Note: normalization skipped for sparse data (centering destroys sparsity)

    // Center y if fit_intercept
    double y_mean = 0.0;
    VectorXd y_c = y;
    if (fit_intercept) {
        y_mean = y.mean();
        y_c = y.array() - y_mean;
    }

    // Compute per-column L2 norms for scale-invariant regularization
    col_norms.resize(n_features);
    VectorXd X_col_sq(n_features);
    for (int j = 0; j < n_features; ++j) {
        col_norms(j) = X_cm.col(j).norm();
        if (col_norms(j) < 1e-10) {
            col_norms(j) = 1.0;
        }
        X_col_sq(j) = X_cm.col(j).squaredNorm();
    }

    // Initialize coefficients (warm start or zero)
    if (!warm_start || !fitted || coefficients.size() != n_features) {
        coefficients = VectorXd::Zero(n_features);
    }

    // L1 penalty scaled by number of samples
    double lambda = alpha * n_samples;

    // Maintain residual vector (sparse col ops are O(nnz_j))
    VectorXd residual = y_c - X_cm * coefficients;

    // Coordinate descent with duality gap convergence
    int iter = 0;
    for (iter = 0; iter < max_iter; ++iter) {
        if (iter % 10 == 0 && qore_check_cancel(xsink, "Lasso fitSparse")) {
            return;
        }

        for (int j = 0; j < n_features; ++j) {
            double old_coef_j = coefficients(j);
            double rho_j = X_cm.col(j).dot(residual) + X_col_sq(j) * old_coef_j;

            double x_sq = X_col_sq(j);
            if (x_sq < 1e-10) {
                coefficients(j) = 0.0;
            } else {
                // Scale lambda by column norm for scale-invariant regularization
                coefficients(j) = softThreshold(rho_j, lambda * col_norms(j)) / x_sq;
            }

            double delta = coefficients(j) - old_coef_j;
            if (delta != 0.0) {
                // Sparse column * scalar -> update residual efficiently
                residual -= X_cm.col(j) * delta;
            }
        }

        // Duality gap convergence check
        double primal = 0.5 * residual.squaredNorm() / n_samples
            + alpha * coefficients.lpNorm<1>();
        VectorXd Xtr = X_cm.transpose() * residual;
        double dual_scale = std::max(1.0, Xtr.lpNorm<Eigen::Infinity>() / (alpha * n_samples));
        VectorXd theta = residual / (n_samples * dual_scale);
        double dual = 0.5 * y_c.squaredNorm() / n_samples
            - 0.5 * n_samples * (theta - y_c / n_samples).squaredNorm();
        double gap = primal - dual;
        if (gap < tol) {
            ++iter;
            break;
        }
    }
    iterations_run = iter;

    // Compute intercept
    if (fit_intercept) {
        VectorXd col_sums = X_cm.transpose() * VectorXd::Ones(n_samples);
        VectorXd X_mean = col_sums / n_samples;
        intercept = y_mean - X_mean.dot(coefficients);
    } else {
        intercept = 0.0;
    }

    // R-squared
    VectorXd y_pred = (X_cm * coefficients).array() + intercept;
    double ss_res = (y - y_pred).squaredNorm();
    double ss_tot = (y.array() - y.mean()).square().sum();
    if (ss_tot > 1e-10) {
        r_squared = 1.0 - ss_res / ss_tot;
    } else {
        r_squared = (ss_res < 1e-10) ? 1.0 : 0.0;
    }

    fitted = true;
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
    MLSerialization::writeBool(buf, warm_start);
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

    // warm_start added after initial serialization format; default to false for backward compat
    bool warm_start = false;
    if (remaining > 0) {
        warm_start = MLSerialization::readBool(ptr, remaining, xsink);
        if (*xsink) { return nullptr; }
    }

    std::unique_ptr<QoreLasso> obj(new QoreLasso(alpha, fit_intercept, do_normalize,
        max_iter, tol, warm_start));
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
