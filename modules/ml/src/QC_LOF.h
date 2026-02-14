/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_LOF.h

    Qore ml module - Local Outlier Factor class

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

#ifndef _QORE_MODULE_ML_QC_LOF_H
#define _QORE_MODULE_ML_QC_LOF_H

#include "ml_common.h"

#include <mutex>
#include <vector>

DLLEXPORT extern qore_classid_t CID_LOF;
DLLLOCAL extern QoreClass* QC_LOF;

DLLLOCAL void preinitLOFClass();
DLLLOCAL QoreClass* initLOFClass(QoreNamespace& ns);

//! Local Outlier Factor implementation class
class QoreLOF : public AbstractPrivateData {
public:
    //! Constructor with options
    DLLLOCAL QoreLOF(int k, double threshold);

    //! Fit on an Eigen matrix (stores reference data, precomputes k-NN and LRD)
    DLLLOCAL void fit(const MatrixXd& data, ExceptionSink* xsink);

    //! Score a single row vector
    DLLLOCAL QoreHashNode* score(const RowVectorXd& point, ExceptionSink* xsink) const;

    //! Score multiple rows
    DLLLOCAL QoreListNode* scoreMatrix(const MatrixXd& data, ExceptionSink* xsink) const;

    //! Whether the model has been fitted
    DLLLOCAL bool isFitted() const { return fitted; }

    //! Get k parameter
    DLLLOCAL int getK() const { return k; }

    //! Store field names for hash-based input
    DLLLOCAL void setFieldNames(const std::vector<std::string>& names) { field_names = names; }
    DLLLOCAL const std::vector<std::string>& getFieldNames() const { return field_names; }

private:
    int k;
    double threshold;
    bool fitted = false;
    int n_features = 0;

    //! Reference data stored from fit()
    MatrixXd ref_data;

    //! Precomputed k-distances for each reference point
    std::vector<double> ref_k_distances;

    //! Precomputed local reachability densities for each reference point
    std::vector<double> ref_lrds;

    //! Precomputed k-nearest neighbor indices for each reference point
    std::vector<std::vector<int>> ref_knn_indices;

    //! Field names for hash-based API
    std::vector<std::string> field_names;

    //! Mutex protecting fit/score
    mutable std::mutex mtx;

    //! Compute distances from a point to all reference points
    DLLLOCAL VectorXd computeDistances(const RowVectorXd& point) const;

    //! Find k nearest neighbors (returns indices sorted by distance)
    DLLLOCAL std::vector<int> findKNN(const VectorXd& distances) const;

    //! Compute reachability distance between a point and a reference point
    DLLLOCAL double reachabilityDistance(double actual_dist, int ref_idx) const;

    //! Compute local reachability density for a new point
    DLLLOCAL double computeLRD(const RowVectorXd& point, const VectorXd& distances,
        const std::vector<int>& knn) const;

    //! Score a single point (must be called with mtx held)
    DLLLOCAL QoreHashNode* scoreInternal(const RowVectorXd& point, ExceptionSink* xsink) const;
};

#endif // _QORE_MODULE_ML_QC_LOF_H
