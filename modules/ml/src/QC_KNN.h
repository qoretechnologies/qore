/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_KNN.h

    Qore ml module - K-Nearest Neighbors class

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

#ifndef _QORE_MODULE_ML_QC_KNN_H
#define _QORE_MODULE_ML_QC_KNN_H

#include "ml_common.h"

#include <mutex>
#include <vector>

DLLEXPORT extern qore_classid_t CID_KNN;
DLLLOCAL extern QoreClass* QC_KNN;

DLLLOCAL void preinitKNNClass();
DLLLOCAL QoreClass* initKNNClass(QoreNamespace& ns);

//! K-Nearest Neighbors implementation class
class QoreKNN : public AbstractPrivateData {
public:
    //! Constructor with options
    DLLLOCAL QoreKNN(int k, const std::string& metric, const std::string& task,
        const std::string& weight_func);

    //! Fit on feature matrix X and label vector y (thread-safe)
    DLLLOCAL void fit(const MatrixXd& X, const VectorXd& y, ExceptionSink* xsink);

    //! Classify a single row (task must be "classification")
    DLLLOCAL QoreHashNode* predictClassification(const RowVectorXd& point, ExceptionSink* xsink) const;

    //! Predict regression for a single row (task must be "regression")
    DLLLOCAL QoreHashNode* predictRegression(const RowVectorXd& point, ExceptionSink* xsink) const;

    //! Classify multiple rows
    DLLLOCAL QoreListNode* predictClassificationMatrix(const MatrixXd& X, ExceptionSink* xsink) const;

    //! Predict regression for multiple rows
    DLLLOCAL QoreListNode* predictRegressionMatrix(const MatrixXd& X, ExceptionSink* xsink) const;

    //! Get the task type
    DLLLOCAL const std::string& getTask() const { return task; }

    //! Whether the model has been fitted
    DLLLOCAL bool isFitted() const { return fitted; }

    //! Get k parameter
    DLLLOCAL int getK() const { return k; }

    //! Store field names for hash-based input
    DLLLOCAL void setFieldNames(const std::vector<std::string>& names) { field_names = names; }
    DLLLOCAL const std::vector<std::string>& getFieldNames() const { return field_names; }

    //! Store the target field name
    DLLLOCAL void setTargetField(const std::string& name) { target_field = name; }
    DLLLOCAL const std::string& getTargetField() const { return target_field; }

    //! Serialize model state to binary
    DLLLOCAL std::vector<uint8_t> serializeState() const;

    //! Deserialize model state from binary
    DLLLOCAL static QoreKNN* deserializeState(const uint8_t* data, size_t len,
        ExceptionSink* xsink);

private:
    int k;
    std::string metric;       // "euclidean", "manhattan", "cosine"
    std::string task;          // "classification", "regression"
    std::string weight_func;   // "uniform", "distance"

    MatrixXd ref_data;         // stored training data
    VectorXd ref_labels;       // stored training labels
    int n_features = 0;
    bool fitted = false;
    std::vector<std::string> field_names;
    std::string target_field;

    mutable std::mutex mtx;

    //! Predict for a single point (must be called with mtx held)
    DLLLOCAL QoreHashNode* predictInternal(const RowVectorXd& point, ExceptionSink* xsink) const;

    //! Compute distance between two row vectors using the configured metric
    DLLLOCAL double computeDistance(const RowVectorXd& a, const RowVectorXd& b) const;
};

#endif // _QORE_MODULE_ML_QC_KNN_H
