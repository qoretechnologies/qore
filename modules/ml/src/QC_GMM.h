/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_GMM.h

    Qore ml module - Gaussian Mixture Model class

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

#ifndef _QORE_MODULE_ML_QC_GMM_H
#define _QORE_MODULE_ML_QC_GMM_H

#include "ml_common.h"

#include <mutex>
#include <random>
#include <string>

DLLEXPORT extern qore_classid_t CID_GMM;
DLLLOCAL extern QoreClass* QC_GMM;

DLLLOCAL void preinitGMMClass();
DLLLOCAL QoreClass* initGMMClass(QoreNamespace& ns);

//! Gaussian Mixture Model implementation class using EM algorithm
class QoreGMM : public AbstractPrivateData {
public:
    //! Constructor with options
    DLLLOCAL QoreGMM(int n_components, int max_iterations, double tolerance,
        int64_t seed, const std::string& covariance_type);

    //! Fit on an Eigen matrix
    DLLLOCAL void fit(const MatrixXd& data, ExceptionSink* xsink);

    //! Online EM update with new data (thread-safe)
    DLLLOCAL void update(const MatrixXd& data, ExceptionSink* xsink);

    //! Predict cluster membership for a single point
    DLLLOCAL QoreHashNode* predict(const RowVectorXd& point, ExceptionSink* xsink) const;

    //! Predict cluster membership for multiple points
    DLLLOCAL QoreListNode* predictMatrix(const MatrixXd& data, ExceptionSink* xsink) const;

    //! Whether the model has been fitted
    DLLLOCAL bool isFitted() const { return fitted; }

    //! Get number of components
    DLLLOCAL int getNumComponents() const { return n_components; }

    //! Get means as a Qore list
    DLLLOCAL QoreListNode* getMeans(ExceptionSink* xsink) const;

    //! Get log-likelihood
    DLLLOCAL double getLogLikelihood(ExceptionSink* xsink) const;

    //! Store field names for hash-based input
    DLLLOCAL void setFieldNames(const std::vector<std::string>& names) { field_names = names; }
    DLLLOCAL const std::vector<std::string>& getFieldNames() const { return field_names; }

private:
    int n_components;
    int max_iterations;
    double tolerance;
    std::string covariance_type;  // "full" or "diagonal"
    std::mt19937 rng;
    bool fitted = false;
    int n_features = 0;
    int batch_count = 0;

    //! Model parameters
    std::vector<VectorXd> means;         // n_components means
    std::vector<MatrixXd> covariances;   // n_components covariance matrices
    VectorXd weights;                     // mixing weights
    double log_likelihood = 0.0;

    //! Field names for hash-based API
    std::vector<std::string> field_names;

    //! Mutex protecting fit/predict
    mutable std::mutex mtx;

    //! Initialize parameters using k-means++
    DLLLOCAL void initializeKMeansPP(const MatrixXd& data);

    //! E-step: compute responsibilities
    DLLLOCAL MatrixXd eStep(const MatrixXd& data) const;

    //! M-step: update parameters from responsibilities
    DLLLOCAL void mStep(const MatrixXd& data, const MatrixXd& responsibilities);

    //! Compute log probability density for a point given a component
    DLLLOCAL double logProbDensity(const RowVectorXd& point, int comp) const;

    //! Compute log-likelihood of the data
    DLLLOCAL double computeLogLikelihood(const MatrixXd& data) const;

    //! Predict for a single point (must be called with mtx held)
    DLLLOCAL QoreHashNode* predictInternal(const RowVectorXd& point, ExceptionSink* xsink) const;
};

#endif // _QORE_MODULE_ML_QC_GMM_H
