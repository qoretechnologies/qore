/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_LinearRegression.h

    Qore ml module - LinearRegression class

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

#ifndef _QORE_MODULE_ML_QC_LINEARREGRESSION_H
#define _QORE_MODULE_ML_QC_LINEARREGRESSION_H

#include "ml_common.h"

#include <mutex>

DLLEXPORT extern qore_classid_t CID_LINEARREGRESSION;
DLLLOCAL extern QoreClass* QC_LINEARREGRESSION;

DLLLOCAL void preinitLinearRegressionClass();
DLLLOCAL QoreClass* initLinearRegressionClass(QoreNamespace& ns);

//! Linear regression implementation class using normal equations
class QoreLinearRegression : public AbstractPrivateData {
public:
    //! Constructor with options
    DLLLOCAL QoreLinearRegression(bool fit_intercept, bool normalize);

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

    //! Get coefficients as a Qore list
    DLLLOCAL QoreListNode* getCoefficients(ExceptionSink* xsink) const;

    //! Get intercept value
    DLLLOCAL double getIntercept(ExceptionSink* xsink) const;

    //! Get R-squared value
    DLLLOCAL double getRSquared(ExceptionSink* xsink) const;

    //! Get model info (coefficients, intercept, r_squared, n_features)
    DLLLOCAL QoreHashNode* getModelInfo(ExceptionSink* xsink) const;

    //! Store field names for hash-based input
    DLLLOCAL void setFieldNames(const std::vector<std::string>& names) { field_names = names; }
    DLLLOCAL const std::vector<std::string>& getFieldNames() const { return field_names; }

    //! Store the target field name
    DLLLOCAL void setTargetField(const std::string& name) { target_field = name; }
    DLLLOCAL const std::string& getTargetField() const { return target_field; }

private:
    bool fit_intercept;
    bool do_normalize;
    bool fitted = false;
    int n_features = 0;

    //! Model parameters
    VectorXd coefficients;
    double intercept = 0.0;
    double r_squared = 0.0;

    //! Normalization parameters (mean and std for each feature)
    VectorXd feature_means;
    VectorXd feature_stds;

    //! Field names for hash-based API
    std::vector<std::string> field_names;
    std::string target_field;

    //! Mutex protecting fit/predict
    mutable std::mutex mtx;

    //! Predict for a single point (must be called with mtx held)
    DLLLOCAL QoreHashNode* predictInternal(const RowVectorXd& point, ExceptionSink* xsink) const;
};

#endif // _QORE_MODULE_ML_QC_LINEARREGRESSION_H
