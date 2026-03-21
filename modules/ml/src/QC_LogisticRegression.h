/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_LogisticRegression.h

    Qore ml module - LogisticRegression class

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

#ifndef _QORE_MODULE_ML_QC_LOGISTICREGRESSION_H
#define _QORE_MODULE_ML_QC_LOGISTICREGRESSION_H

#include "ml_common.h"

#include <mutex>

DLLEXPORT extern qore_classid_t CID_LOGISTICREGRESSION;
DLLLOCAL extern QoreClass* QC_LOGISTICREGRESSION;

DLLLOCAL void preinitLogisticRegressionClass();
DLLLOCAL QoreClass* initLogisticRegressionClass(QoreNamespace& ns);

//! Logistic regression implementation class using gradient descent
class QoreLogisticRegression : public AbstractPrivateData {
public:
    //! Constructor with hyperparameters
    DLLLOCAL QoreLogisticRegression(double learning_rate, int max_iterations, double tolerance,
        double regularization, const std::string& penalty, bool fit_intercept);

    //! Fit on feature matrix X and target vector y (thread-safe)
    DLLLOCAL void fit(const MatrixXd& X, const VectorXd& y, ExceptionSink* xsink);

    //! SGD update with new data (thread-safe)
    DLLLOCAL void update(const MatrixXd& X, const VectorXd& y, double learning_rate, ExceptionSink* xsink);

    //! Predict for a single row vector
    DLLLOCAL QoreHashNode* predict(const RowVectorXd& point, ExceptionSink* xsink) const;

    //! Predict for multiple rows
    DLLLOCAL QoreListNode* predictMatrix(const MatrixXd& X, ExceptionSink* xsink) const;

    //! Whether the model has been fitted
    DLLLOCAL bool isFitted() const { return fitted; }

    //! Store field names for hash-based input
    DLLLOCAL void setFieldNames(const std::vector<std::string>& names) { field_names = names; }
    DLLLOCAL const std::vector<std::string>& getFieldNames() const { return field_names; }

    //! Store the target field name
    DLLLOCAL void setTargetField(const std::string& name) { target_field = name; }
    DLLLOCAL const std::string& getTargetField() const { return target_field; }

private:
    //! Hyperparameters
    double learning_rate;
    int max_iterations;
    double tolerance;
    double regularization;
    std::string penalty;    // "l2" or "none"
    bool fit_intercept;

    //! Model state
    MatrixXd weights;               // n_classes x n_features (or 1 x n_features for binary)
    VectorXd intercepts;            // n_classes (or 1 for binary)
    std::vector<double> classes;    // unique class labels, sorted
    bool is_binary = false;
    int n_features = 0;
    bool fitted = false;

    //! Field names for hash-based API
    std::vector<std::string> field_names;
    std::string target_field;

    //! Mutex protecting fit/predict
    mutable std::mutex mtx;

    //! Predict for a single point (must be called with mtx held)
    DLLLOCAL QoreHashNode* predictInternal(const RowVectorXd& point, ExceptionSink* xsink) const;

    //! Sigmoid activation function with clamping
    DLLLOCAL double sigmoid(double z) const;

    //! Softmax activation for multiclass
    DLLLOCAL VectorXd softmax(const VectorXd& z) const;
};

#endif // _QORE_MODULE_ML_QC_LOGISTICREGRESSION_H
