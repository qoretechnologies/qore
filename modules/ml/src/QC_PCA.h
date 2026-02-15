/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_PCA.h

    Qore ml module - PCA class

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

#ifndef _QORE_MODULE_ML_QC_PCA_H
#define _QORE_MODULE_ML_QC_PCA_H

#include "ml_common.h"

#include <mutex>

DLLEXPORT extern qore_classid_t CID_PCA;
DLLLOCAL extern QoreClass* QC_PCA;

DLLLOCAL void preinitPCAClass();
DLLLOCAL QoreClass* initPCAClass(QoreNamespace& ns);

//! PCA implementation class
class QorePCA : public AbstractPrivateData {
public:
    DLLLOCAL QorePCA(int n_components, double variance_threshold, bool center, bool scale)
        : n_components(n_components), variance_threshold(variance_threshold),
          center(center), scale(scale) {
    }

    DLLLOCAL int getNumComponents() const { return actual_n_components; }
    DLLLOCAL bool isFitted() const { return fitted; }

    //! Fit the model on data
    DLLLOCAL void fitInternal(const MatrixXd& data, ExceptionSink* xsink);

    //! Transform a single point
    DLLLOCAL QoreHashNode* transformInternal(const RowVectorXd& point, ExceptionSink* xsink);

    //! Transform multiple points
    DLLLOCAL QoreListNode* transformMatrix(const MatrixXd& data, ExceptionSink* xsink);

    //! Fit and transform atomically (single lock acquisition)
    DLLLOCAL QoreListNode* fitTransformInternal(const MatrixXd& data, ExceptionSink* xsink);

    //! Get principal component vectors
    DLLLOCAL QoreListNode* getComponents(ExceptionSink* xsink);

    //! Get explained variance ratios
    DLLLOCAL QoreListNode* getExplainedVarianceRatio(ExceptionSink* xsink);

    //! Get mean vector
    DLLLOCAL QoreListNode* getMean(ExceptionSink* xsink);

    //! Store field names
    DLLLOCAL void setFieldNames(const std::vector<std::string>& names) { field_names = names; }
    DLLLOCAL const std::vector<std::string>& getFieldNames() const { return field_names; }

private:
    //! Fit implementation (assumes lock is held)
    DLLLOCAL void doFit(const MatrixXd& data, ExceptionSink* xsink);

    //! Transform matrix implementation (assumes lock is held)
    DLLLOCAL QoreListNode* doTransformMatrix(const MatrixXd& data, ExceptionSink* xsink);
    int n_components;       // requested (0 = all)
    int actual_n_components = 0; // after fitting
    double variance_threshold;
    bool center;
    bool scale;

    // Fitted state
    MatrixXd components;            // actual_n_components x n_features
    VectorXd mean_vec;
    VectorXd stddev_vec;
    VectorXd explained_variance_ratio_vec;
    int n_features = 0;
    bool fitted = false;
    std::vector<std::string> field_names;

    mutable std::mutex mtx;
};

#endif // _QORE_MODULE_ML_QC_PCA_H
