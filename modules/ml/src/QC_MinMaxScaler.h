/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_MinMaxScaler.h

    Qore ml module - MinMaxScaler class

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

#ifndef _QORE_MODULE_ML_QC_MINMAXSCALER_H
#define _QORE_MODULE_ML_QC_MINMAXSCALER_H

#include "ml_common.h"

#include <mutex>

DLLEXPORT extern qore_classid_t CID_MINMAXSCALER;
DLLLOCAL extern QoreClass* QC_MINMAXSCALER;

DLLLOCAL void preinitMinMaxScalerClass();
DLLLOCAL QoreClass* initMinMaxScalerClass(QoreNamespace& ns);

//! MinMaxScaler — scales features to a configurable range [feature_min, feature_max]
class QoreMinMaxScaler : public AbstractPrivateData {
public:
    DLLLOCAL QoreMinMaxScaler(double feature_min, double feature_max)
        : feature_min(feature_min), feature_max(feature_max) {
    }

    //! Compute per-feature min/max from training data
    DLLLOCAL void fit(const MatrixXd& data, ExceptionSink* xsink);

    //! Scale data to [feature_min, feature_max]
    DLLLOCAL MatrixXd transform(const MatrixXd& data, ExceptionSink* xsink) const;

    //! Reverse the scaling
    DLLLOCAL MatrixXd inverseTransform(const MatrixXd& data, ExceptionSink* xsink) const;

    //! Fit and transform in one step
    DLLLOCAL MatrixXd fitTransform(const MatrixXd& data, ExceptionSink* xsink);

    //! Whether the scaler has been fitted
    DLLLOCAL bool isFitted() const { return fitted; }

    //! Get scaler info
    DLLLOCAL QoreHashNode* getInfo(ExceptionSink* xsink) const;

    //! Store field names for hash-based input
    DLLLOCAL void setFieldNames(const std::vector<std::string>& names) { field_names = names; }
    DLLLOCAL const std::vector<std::string>& getFieldNames() const { return field_names; }

    //! Transform a single row (no lock, no fitted check)
    DLLLOCAL RowVectorXd transformRow(const RowVectorXd& row, ExceptionSink* xsink) const;

    //! Inverse-transform a single row (no lock, no fitted check)
    DLLLOCAL RowVectorXd inverseTransformRow(const RowVectorXd& row, ExceptionSink* xsink) const;

    //! Getters
    DLLLOCAL int getNumFeatures() const { return n_features; }
    DLLLOCAL double getFeatureMin() const { return feature_min; }
    DLLLOCAL double getFeatureMax() const { return feature_max; }

private:
    double feature_min;
    double feature_max;
    bool fitted = false;
    int n_features = 0;

    VectorXd data_min;
    VectorXd data_max;
    VectorXd scale;     // (feature_max - feature_min) / (data_max - data_min)
    VectorXd min_adj;   // feature_min - data_min * scale

    std::vector<std::string> field_names;

    mutable std::mutex mtx;

    //! Compute scale and min_adj from data_min/data_max
    DLLLOCAL void computeScaleFactors();
};

#endif // _QORE_MODULE_ML_QC_MINMAXSCALER_H
