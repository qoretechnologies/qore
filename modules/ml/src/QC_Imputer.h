/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_Imputer.h

    Qore ml module - Imputer class

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

#ifndef _QORE_MODULE_ML_QC_IMPUTER_H
#define _QORE_MODULE_ML_QC_IMPUTER_H

#include "ml_common.h"

#include <mutex>
#include <string>

DLLEXPORT extern qore_classid_t CID_IMPUTER;
DLLLOCAL extern QoreClass* QC_IMPUTER;

DLLLOCAL void preinitImputerClass();
DLLLOCAL QoreClass* initImputerClass(QoreNamespace& ns);

//! Imputer — fills missing values (NaN) using a configurable strategy
class QoreImputer : public AbstractPrivateData {
public:
    DLLLOCAL QoreImputer(const std::string& strategy, double constant_value)
        : strategy(strategy), constant_value(constant_value) {
    }

    //! Compute fill values from training data
    DLLLOCAL void fit(const MatrixXd& data, ExceptionSink* xsink);

    //! Replace NaN values with fitted fill values
    DLLLOCAL MatrixXd transform(const MatrixXd& data, ExceptionSink* xsink) const;

    //! Fit and transform in one step
    DLLLOCAL MatrixXd fitTransform(const MatrixXd& data, ExceptionSink* xsink);

    //! Whether the imputer has been fitted
    DLLLOCAL bool isFitted() const { return fitted; }

    //! Get imputer info
    DLLLOCAL QoreHashNode* getInfo(ExceptionSink* xsink) const;

    //! Store field names
    DLLLOCAL void setFieldNames(const std::vector<std::string>& names) { field_names = names; }
    DLLLOCAL const std::vector<std::string>& getFieldNames() const { return field_names; }

    //! Transform a single row (no lock, no fitted check)
    DLLLOCAL RowVectorXd transformRow(const RowVectorXd& row, ExceptionSink* xsink) const;

    //! Getters
    DLLLOCAL int getNumFeatures() const { return n_features; }
    DLLLOCAL const std::string& getStrategy() const { return strategy; }

    //! Serialize model state to binary
    DLLLOCAL std::vector<uint8_t> serializeState() const;

    //! Deserialize model state from binary
    DLLLOCAL static QoreImputer* deserializeState(const uint8_t* data, size_t len,
        ExceptionSink* xsink);

private:
    std::string strategy;       // "mean", "median", "most_frequent", "constant"
    double constant_value;
    bool fitted = false;
    int n_features = 0;

    VectorXd fill_values;       // one per feature

    std::vector<std::string> field_names;

    mutable std::mutex mtx;

    //! Compute the median of non-NaN values in a column
    DLLLOCAL static double computeMedian(const MatrixXd& data, Eigen::Index col);

    //! Compute the most frequent value of non-NaN values in a column
    DLLLOCAL static double computeMostFrequent(const MatrixXd& data, Eigen::Index col);
};

#endif // _QORE_MODULE_ML_QC_IMPUTER_H
