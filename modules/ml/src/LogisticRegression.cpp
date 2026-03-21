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

        // Gradient descent for this binary classifier
        RowVectorXd w = RowVectorXd::Zero(n_features);
        double b = 0.0;

        for (int iter = 0; iter < max_iterations; ++iter) {
            if (qore_check_cancel(xsink, "LogisticRegression fit")) {
                return;
            }

            // Compute predictions: sigmoid(X * w^T + b)
            VectorXd z = (X * w.transpose()).array() + b;
            VectorXd predictions(n_samples);
            for (int i = 0; i < n_samples; ++i) {
                predictions(i) = sigmoid(z(i));
            }

            // Compute error
            VectorXd error = predictions - y_binary;

            // Compute gradients
            // gradient_w = (1/n) * X^T * error + regularization * w (for L2)
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
