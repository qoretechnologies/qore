/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_StandardScaler.h

    Qore ml module - StandardScaler class

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

#ifndef _QORE_MODULE_ML_QC_STANDARDSCALER_H
#define _QORE_MODULE_ML_QC_STANDARDSCALER_H

#include "ml_common.h"

#include <mutex>

DLLEXPORT extern qore_classid_t CID_STANDARDSCALER;
DLLLOCAL extern QoreClass* QC_STANDARDSCALER;

DLLLOCAL void preinitStandardScalerClass();
DLLLOCAL QoreClass* initStandardScalerClass(QoreNamespace& ns);

//! StandardScaler implementation — transforms features to zero mean and unit variance
class QoreStandardScaler : public AbstractPrivateData {
public:
    DLLLOCAL QoreStandardScaler(bool with_mean, bool with_std)
        : with_mean(with_mean), with_std(with_std) {
    }

    //! Compute mean and std from training data
    DLLLOCAL void fit(const MatrixXd& data, ExceptionSink* xsink);

    //! Transform data using fitted parameters
    DLLLOCAL MatrixXd transform(const MatrixXd& data, ExceptionSink* xsink) const;

    //! Reverse the transformation
    DLLLOCAL MatrixXd inverseTransform(const MatrixXd& data, ExceptionSink* xsink) const;

    //! Fit and transform in one step
    DLLLOCAL MatrixXd fitTransform(const MatrixXd& data, ExceptionSink* xsink);

    //! Whether the scaler has been fitted
    DLLLOCAL bool isFitted() const { return fitted; }

    //! Get model info as a Qore hash
    DLLLOCAL QoreHashNode* getInfo(ExceptionSink* xsink) const;

    //! Store field names for hash-based input
    DLLLOCAL void setFieldNames(const std::vector<std::string>& names) { field_names = names; }
    DLLLOCAL const std::vector<std::string>& getFieldNames() const { return field_names; }

    //! Transform a single row (hash API helper, no lock, no fitted check)
    DLLLOCAL RowVectorXd transformRow(const RowVectorXd& row, ExceptionSink* xsink) const;

    //! Inverse-transform a single row (no lock, no fitted check)
    DLLLOCAL RowVectorXd inverseTransformRow(const RowVectorXd& row, ExceptionSink* xsink) const;

    //! Get number of features
    DLLLOCAL int getNumFeatures() const { return n_features; }

    //! Get the mean vector (for serialization)
    DLLLOCAL const VectorXd& getMean() const { return mean_vec; }

    //! Get the std vector (for serialization)
    DLLLOCAL const VectorXd& getStd() const { return std_vec; }

    //! Get the mean vector as a Qore list (thread-safe, checks fitted)
    DLLLOCAL QoreListNode* getMeanAsList(ExceptionSink* xsink) const;

    //! Get the std vector as a Qore list (thread-safe, checks fitted)
    DLLLOCAL QoreListNode* getStdAsList(ExceptionSink* xsink) const;

    //! Serialize model state to binary
    DLLLOCAL std::vector<uint8_t> serializeState() const;

    //! Deserialize model state from binary
    DLLLOCAL static QoreStandardScaler* deserializeState(const uint8_t* data, size_t len,
        ExceptionSink* xsink);

private:
    bool with_mean;
    bool with_std;
    bool fitted = false;
    int n_features = 0;

    VectorXd mean_vec;
    VectorXd std_vec;

    std::vector<std::string> field_names;

    mutable std::mutex mtx;
};

#endif // _QORE_MODULE_ML_QC_STANDARDSCALER_H
