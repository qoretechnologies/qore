/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    LogisticRegression.cpp

    Qore ml module - LogisticRegression implementation

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

#include "QC_LogisticRegression.h"
#include "ml_serialization.h"

#include <LBFGS.h>

#include <algorithm>
#include <set>

// Extern declaration for hashdecl (defined in ml-module.cpp)
extern const TypedHashDecl* hashdeclLogisticRegressionResult;

QoreLogisticRegression::QoreLogisticRegression(double learning_rate, int max_iterations,
    double tolerance, double regularization, const std::string& penalty, bool fit_intercept)
    : learning_rate(learning_rate), max_iterations(max_iterations), tolerance(tolerance),
      regularization(regularization), penalty(penalty), fit_intercept(fit_intercept) {
}

double QoreLogisticRegression::sigmoid(double z) const {
    // Clamp to avoid overflow in exp()
    if (z > 500.0) {
        z = 500.0;
    } else if (z < -500.0) {
        z = -500.0;
    }
    return 1.0 / (1.0 + std::exp(-z));
}

VectorXd QoreLogisticRegression::softmax(const VectorXd& z) const {
    // Subtract max for numerical stability
    double max_z = z.maxCoeff();
    VectorXd exp_z = (z.array() - max_z).exp();
    double sum = exp_z.sum();
    if (sum <= 0.0) {
        // Fallback: uniform probabilities
        return VectorXd::Constant(z.size(), 1.0 / z.size());
    }
    return exp_z / sum;
}

void QoreLogisticRegression::fit(const MatrixXd& X, const VectorXd& y, ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lk(mtx);

    if (X.rows() == 0 || X.cols() == 0) {
        xsink->raiseException("ML-LOGISTIC-REGRESSION-ERROR",
            "cannot fit on empty data: provide at least one sample with one feature");
        return;
    }
    if (X.rows() != y.size()) {
        xsink->raiseException("ML-LOGISTIC-REGRESSION-ERROR",
            "X has %d rows but y has %d elements; they must match",
            static_cast<int>(X.rows()), static_cast<int>(y.size()));
        return;
    }

    int n_samples = static_cast<int>(X.rows());
    n_features = static_cast<int>(X.cols());

    // Extract unique sorted classes from y
    std::set<double> class_set;
    for (int i = 0; i < n_samples; ++i) {
        class_set.insert(y(i));
    }
    classes.assign(class_set.begin(), class_set.end());
    int n_classes = static_cast<int>(classes.size());

    if (n_classes < 2) {
        xsink->raiseException("ML-LOGISTIC-REGRESSION-ERROR",
            "need at least 2 distinct classes for classification; found %d", n_classes);
        return;
    }

    is_binary = (n_classes == 2);

    // Number of weight vectors: 1 for binary, n_classes for OvR multiclass
    int n_models = is_binary ? 1 : n_classes;
    weights = MatrixXd::Zero(n_models, n_features);
    intercepts = VectorXd::Zero(n_models);

    // For binary classification: map classes[0] -> 0, classes[1] -> 1
    // For multiclass OvR: for each class c, create binary target (1 if class == c, 0 otherwise)

    for (int m = 0; m < n_models; ++m) {
        // Create binary target vector for this model
        VectorXd y_binary(n_samples);
        if (is_binary) {
            for (int i = 0; i < n_samples; ++i) {
                y_binary(i) = (y(i) == classes[1]) ? 1.0 : 0.0;
            }
        } else {
            for (int i = 0; i < n_samples; ++i) {
                y_binary(i) = (y(i) == classes[m]) ? 1.0 : 0.0;
            }
        }

        // L-BFGS quasi-Newton optimizer for each binary classifier
        int n_params = fit_intercept ? n_features + 1 : n_features;
        VectorXd params = VectorXd::Zero(n_params);

        auto objective = [&](const VectorXd& x, VectorXd& grad) -> double {
            RowVectorXd w_local = x.head(n_features).transpose();
            double b_local = fit_intercept ? x(n_features) : 0.0;

            // Forward pass: sigmoid(X * w^T + b)
            VectorXd z = (X * w_local.transpose()).array() + b_local;
            VectorXd predictions(n_samples);
            for (int i = 0; i < n_samples; ++i) {
                predictions(i) = sigmoid(z(i));
            }

            // Log-loss: -(1/n) * sum(y*log(p) + (1-y)*log(1-p))
            double loss = 0.0;
            for (int i = 0; i < n_samples; ++i) {
                double p = std::clamp(predictions(i), 1e-15, 1.0 - 1e-15);
                loss -= y_binary(i) * std::log(p) + (1.0 - y_binary(i)) * std::log(1.0 - p);
            }
            loss /= n_samples;

            // L2 regularization
            if (penalty == "l2") {
                loss += 0.5 * regularization * w_local.squaredNorm();
            }

            // Gradient
            VectorXd error = predictions - y_binary;
            VectorXd grad_w = X.transpose() * error / n_samples;
            if (penalty == "l2") {
                grad_w += regularization * w_local.transpose();
            }
            grad.head(n_features) = grad_w;
            if (fit_intercept) {
                grad(n_features) = error.mean();
            }

            return loss;
        };

        LBFGSpp::LBFGSParam<double> lbfgs_params;
        lbfgs_params.max_iterations = max_iterations;
        lbfgs_params.epsilon = tolerance;
        lbfgs_params.m = 10;

        LBFGSpp::LBFGSSolver<double> lbfgs_solver(lbfgs_params);
        double final_loss;
        try {
            lbfgs_solver.minimize(objective, params, final_loss);
        } catch (const std::exception&) {
            // L-BFGS may throw on convergence issues; use current params
        }

        weights.row(m) = params.head(n_features).transpose();
        intercepts(m) = fit_intercept ? params(n_features) : 0.0;
    }

    fitted = true;
}

void QoreLogisticRegression::fitSparse(const SparseMatrixXd& X, const VectorXd& y, ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lk(mtx);

    if (X.rows() == 0 || X.cols() == 0) {
        xsink->raiseException("ML-LOGISTIC-REGRESSION-ERROR",
            "cannot fit on empty data: provide at least one sample with one feature");
        return;
    }
    if (X.rows() != y.size()) {
        xsink->raiseException("ML-LOGISTIC-REGRESSION-ERROR",
            "X has %d rows but y has %d elements; they must match",
            static_cast<int>(X.rows()), static_cast<int>(y.size()));
        return;
    }

    int n_samples = static_cast<int>(X.rows());
    n_features = static_cast<int>(X.cols());

    // Extract unique sorted classes from y
    std::set<double> class_set;
    for (int i = 0; i < n_samples; ++i) {
        class_set.insert(y(i));
    }
    classes.assign(class_set.begin(), class_set.end());
    int n_classes = static_cast<int>(classes.size());

    if (n_classes < 2) {
        xsink->raiseException("ML-LOGISTIC-REGRESSION-ERROR",
            "need at least 2 distinct classes for classification; found %d", n_classes);
        return;
    }

    is_binary = (n_classes == 2);

    // Number of weight vectors: 1 for binary, n_classes for OvR multiclass
    int n_models = is_binary ? 1 : n_classes;
    weights = MatrixXd::Zero(n_models, n_features);
    intercepts = VectorXd::Zero(n_models);

    // Note: normalization with sparse data is skipped (centering destroys sparsity)

    for (int m = 0; m < n_models; ++m) {
        // Create binary target vector for this model
        VectorXd y_binary(n_samples);
        if (is_binary) {
            for (int i = 0; i < n_samples; ++i) {
                y_binary(i) = (y(i) == classes[1]) ? 1.0 : 0.0;
            }
        } else {
            for (int i = 0; i < n_samples; ++i) {
                y_binary(i) = (y(i) == classes[m]) ? 1.0 : 0.0;
            }
        }

        // Gradient descent for this binary classifier
        RowVectorXd w = RowVectorXd::Zero(n_features);
        double b = 0.0;

        for (int iter = 0; iter < max_iterations; ++iter) {
            if (qore_check_cancel(xsink, "LogisticRegression fitSparse")) {
                return;
            }

            // Compute predictions: sigmoid(X * w^T + b)
            // Sparse * dense = dense (efficient via Eigen overloads)
            VectorXd z = (X * w.transpose()).array() + b;
            VectorXd predictions(n_samples);
            for (int i = 0; i < n_samples; ++i) {
                predictions(i) = sigmoid(z(i));
            }

            // Compute error
            VectorXd error = predictions - y_binary;

            // Compute gradients
            // gradient_w = (1/n) * X^T * error + regularization * w (for L2)
            // Sparse^T * dense = dense (efficient via Eigen overloads)
            RowVectorXd gradient_w = (X.transpose() * error).transpose() / n_samples;
            if (penalty == "l2") {
                gradient_w += regularization * w;
            }

            double gradient_b = 0.0;
            if (fit_intercept) {
                gradient_b = error.mean();
            }

            // Update weights
            RowVectorXd old_w = w;
            w -= learning_rate * gradient_w;
            if (fit_intercept) {
                b -= learning_rate * gradient_b;
            }

            // Check convergence: max absolute weight change
            double max_change = (w - old_w).array().abs().maxCoeff();
            if (max_change < tolerance) {
                break;
            }
        }

        weights.row(m) = w;
        intercepts(m) = b;
    }

    fitted = true;
}

void QoreLogisticRegression::update(const MatrixXd& X, const VectorXd& y,
    double lr, ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-LOGISTIC-REGRESSION-ERROR",
            "model has not been fitted: call fit() or fitMatrix() before update()");
        return;
    }
    if (X.cols() != n_features) {
        xsink->raiseException("ML-LOGISTIC-REGRESSION-ERROR",
            "input has %d features but model was trained with %d features",
            static_cast<int>(X.cols()), n_features);
        return;
    }
    if (X.rows() != y.size()) {
        xsink->raiseException("ML-LOGISTIC-REGRESSION-ERROR",
            "X has %d rows but y has %d elements; they must match",
            static_cast<int>(X.rows()), static_cast<int>(y.size()));
        return;
    }
    if (X.rows() == 0) {
        xsink->raiseException("ML-LOGISTIC-REGRESSION-ERROR",
            "cannot update on empty data");
        return;
    }

    // Verify y contains only known classes
    for (Eigen::Index i = 0; i < y.size(); ++i) {
        bool found = false;
        for (size_t c = 0; c < classes.size(); ++c) {
            if (y(i) == classes[c]) {
                found = true;
                break;
            }
        }
        if (!found) {
            xsink->raiseException("ML-LOGISTIC-REGRESSION-ERROR",
                "target value %f at index %d is not a known class from training",
                y(i), static_cast<int>(i));
            return;
        }
    }

    if (qore_check_cancel(xsink, "LogisticRegression update")) {
        return;
    }

    int n_samples = static_cast<int>(X.rows());
    int n_models = is_binary ? 1 : static_cast<int>(classes.size());

    for (int m = 0; m < n_models; ++m) {
        // Create binary target vector for this model
        VectorXd y_binary(n_samples);
        if (is_binary) {
            for (int i = 0; i < n_samples; ++i) {
                y_binary(i) = (y(i) == classes[1]) ? 1.0 : 0.0;
            }
        } else {
            for (int i = 0; i < n_samples; ++i) {
                y_binary(i) = (y(i) == classes[m]) ? 1.0 : 0.0;
            }
        }

        // Compute predictions: sigmoid(X * w^T + b)
        RowVectorXd w = weights.row(m);
        double b = intercepts(m);

        VectorXd z = (X * w.transpose()).array() + b;
        VectorXd predictions(n_samples);
        for (int i = 0; i < n_samples; ++i) {
            predictions(i) = sigmoid(z(i));
        }

        // Compute error and gradient
        VectorXd error = predictions - y_binary;
        RowVectorXd gradient_w = (X.transpose() * error).transpose() / n_samples;
        if (penalty == "l2") {
            gradient_w += regularization * w;
        }

        double gradient_b = 0.0;
        if (fit_intercept) {
            gradient_b = error.mean();
        }

        // Update weights
        weights.row(m) -= lr * gradient_w;
        if (fit_intercept) {
            intercepts(m) -= lr * gradient_b;
        }
    }
}

QoreHashNode* QoreLogisticRegression::predictInternal(const RowVectorXd& point,
    ExceptionSink* xsink) const {
    if (point.size() != n_features) {
        xsink->raiseException("ML-LOGISTIC-REGRESSION-ERROR",
            "input has %d features but model was trained with %d features",
            static_cast<int>(point.size()), n_features);
        return nullptr;
    }

    int n_classes = static_cast<int>(classes.size());
    VectorXd probabilities(n_classes);

    if (is_binary) {
        double z = point.dot(weights.row(0).transpose()) + intercepts(0);
        double p1 = sigmoid(z);
        probabilities(0) = 1.0 - p1;
        probabilities(1) = p1;
    } else {
        // Compute raw scores for each class
        VectorXd scores(n_classes);
        for (int c = 0; c < n_classes; ++c) {
            scores(c) = point.dot(weights.row(c).transpose()) + intercepts(c);
        }
        // Apply softmax to get probabilities
        probabilities = softmax(scores);
    }

    // Find class with highest probability
    int best_class = 0;
    double max_prob = probabilities(0);
    for (int c = 1; c < n_classes; ++c) {
        if (probabilities(c) > max_prob) {
            max_prob = probabilities(c);
            best_class = c;
        }
    }

    // Build result hash
    ReferenceHolder<QoreListNode> prob_list(new QoreListNode(floatTypeInfo), xsink);
    for (int c = 0; c < n_classes; ++c) {
        prob_list->push(probabilities(c), xsink);
    }

    ReferenceHolder<QoreListNode> class_list(new QoreListNode(floatTypeInfo), xsink);
    for (int c = 0; c < n_classes; ++c) {
        class_list->push(classes[c], xsink);
    }

    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(hashdeclLogisticRegressionResult, xsink), xsink);
    rv->setKeyValue("predicted_class", classes[best_class], xsink);
    rv->setKeyValue("max_probability", max_prob, xsink);
    rv->setKeyValue("probabilities", prob_list.release(), xsink);
    rv->setKeyValue("classes", class_list.release(), xsink);

    return rv.release();
}

QoreHashNode* QoreLogisticRegression::predict(const RowVectorXd& point,
    ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-LOGISTIC-REGRESSION-ERROR",
            "model has not been fitted: call fit() or fitMatrix() before predict()");
        return nullptr;
    }
    return predictInternal(point, xsink);
}

QoreListNode* QoreLogisticRegression::predictMatrix(const MatrixXd& X,
    ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-LOGISTIC-REGRESSION-ERROR",
            "model has not been fitted: call fit() or fitMatrix() before predictMatrix()");
        return nullptr;
    }

    if (X.cols() != n_features) {
        xsink->raiseException("ML-LOGISTIC-REGRESSION-ERROR",
            "input has %d features but model was trained with %d features",
            static_cast<int>(X.cols()), n_features);
        return nullptr;
    }

    ReferenceHolder<QoreListNode> rv(new QoreListNode(hashdeclLogisticRegressionResult->getTypeInfo()), xsink);
    for (Eigen::Index i = 0; i < X.rows(); ++i) {
        if (i % 100 == 0 && qore_check_cancel(xsink, "LogisticRegression predictMatrix")) {
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

QoreListNode* QoreLogisticRegression::getWeights(ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-LOGISTIC-REGRESSION-ERROR",
            "model has not been fitted: call fit() or fitMatrix() first");
        return nullptr;
    }
    ReferenceHolder<QoreListNode> rv(new QoreListNode(autoTypeInfo), xsink);
    for (Eigen::Index i = 0; i < weights.rows(); ++i) {
        ReferenceHolder<QoreListNode> row(new QoreListNode(autoTypeInfo), xsink);
        for (Eigen::Index j = 0; j < weights.cols(); ++j) {
            row->push(weights(i, j), xsink);
        }
        rv->push(row.release(), xsink);
    }
    return rv.release();
}

QoreListNode* QoreLogisticRegression::getIntercepts(ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-LOGISTIC-REGRESSION-ERROR",
            "model has not been fitted: call fit() or fitMatrix() first");
        return nullptr;
    }
    ReferenceHolder<QoreListNode> rv(new QoreListNode(autoTypeInfo), xsink);
    for (Eigen::Index j = 0; j < intercepts.size(); ++j) {
        rv->push(intercepts(j), xsink);
    }
    return rv.release();
}

QoreListNode* QoreLogisticRegression::getClasses(ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-LOGISTIC-REGRESSION-ERROR",
            "model has not been fitted: call fit() or fitMatrix() first");
        return nullptr;
    }
    ReferenceHolder<QoreListNode> rv(new QoreListNode(autoTypeInfo), xsink);
    for (size_t i = 0; i < classes.size(); ++i) {
        rv->push(classes[i], xsink);
    }
    return rv.release();
}

std::vector<uint8_t> QoreLogisticRegression::serializeState() const {
    std::vector<uint8_t> buf;
    // Hyperparameters
    MLSerialization::writeScalar(buf, learning_rate);
    MLSerialization::writeInt32(buf, max_iterations);
    MLSerialization::writeScalar(buf, tolerance);
    MLSerialization::writeScalar(buf, regularization);
    MLSerialization::writeString(buf, penalty);
    MLSerialization::writeBool(buf, fit_intercept);
    // Model state
    MLSerialization::writeMatrix(buf, weights);
    MLSerialization::writeVector(buf, intercepts);
    MLSerialization::writeDoubleVector(buf, classes);
    MLSerialization::writeBool(buf, is_binary);
    MLSerialization::writeInt32(buf, n_features);
    MLSerialization::writeStringVector(buf, field_names);
    MLSerialization::writeString(buf, target_field);
    return buf;
}

QoreLogisticRegression* QoreLogisticRegression::deserializeState(const uint8_t* data,
    size_t len, ExceptionSink* xsink) {
    const uint8_t* ptr = data;
    size_t remaining = len;

    double learning_rate = MLSerialization::readScalar(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int32_t max_iterations = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    double tolerance = MLSerialization::readScalar(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    double regularization = MLSerialization::readScalar(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    std::string penalty = MLSerialization::readString(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    bool fit_intercept = MLSerialization::readBool(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }

    MatrixXd weights = MLSerialization::readMatrix(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    VectorXd intercepts = MLSerialization::readVector(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    std::vector<double> classes = MLSerialization::readDoubleVector(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    bool is_binary = MLSerialization::readBool(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int32_t n_features = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    std::vector<std::string> field_names = MLSerialization::readStringVector(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    std::string target_field = MLSerialization::readString(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }

    std::unique_ptr<QoreLogisticRegression> obj(new QoreLogisticRegression(
        learning_rate, max_iterations, tolerance, regularization, penalty, fit_intercept));
    obj->weights = std::move(weights);
    obj->intercepts = std::move(intercepts);
    obj->classes = std::move(classes);
    obj->is_binary = is_binary;
    obj->n_features = n_features;
    obj->field_names = std::move(field_names);
    obj->target_field = std::move(target_field);
    obj->fitted = true;
    return obj.release();
}
