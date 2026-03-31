/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_OneHotEncoder.h

    OneHotEncoder class declaration

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_MODULE_ML_QC_ONEHOTENCODER_H
#define _QORE_MODULE_ML_QC_ONEHOTENCODER_H

#include "ml_common.h"

#include <mutex>
#include <unordered_map>

DLLEXPORT extern qore_classid_t CID_ONEHOTENCODER;
DLLLOCAL extern QoreClass* QC_ONEHOTENCODER;

DLLLOCAL void preinitOneHotEncoderClass();
DLLLOCAL QoreClass* initOneHotEncoderClass(QoreNamespace& ns);

//! One-hot encoder: converts categorical integer features to binary columns
/** fit() learns the unique categories per feature.
    transform() outputs a matrix where each category gets its own column.
    Follows the StandardScaler pattern for MLPipeline compatibility.
*/
class QoreOneHotEncoder : public AbstractPrivateData {
public:
    DLLLOCAL QoreOneHotEncoder(bool drop_first);

    //! Learn categories from integer-encoded feature matrix
    DLLLOCAL void fit(const MatrixXd& X, ExceptionSink* xsink);

    //! Transform integer-encoded features to one-hot binary matrix
    DLLLOCAL MatrixXd transform(const MatrixXd& X, ExceptionSink* xsink) const;

    //! Fit and transform in one step
    DLLLOCAL MatrixXd fitTransform(const MatrixXd& X, ExceptionSink* xsink);

    DLLLOCAL bool isFitted() const { return fitted; }
    DLLLOCAL void setFieldNames(const std::vector<std::string>& names) {
        field_names = names;
    }
    DLLLOCAL const std::vector<std::string>& getFieldNames() const {
        return field_names;
    }

    //! Get the output feature names (one per category per input feature)
    DLLLOCAL QoreListNode* getOutputFeatureNames(ExceptionSink* xsink) const;

    DLLLOCAL std::vector<uint8_t> serializeState() const;
    DLLLOCAL static QoreOneHotEncoder* deserializeState(const uint8_t* data,
        size_t len, ExceptionSink* xsink);

private:
    bool drop_first;  //!< drop first category to avoid multicollinearity
    bool fitted = false;
    int n_features = 0;

    //! Per-feature: sorted list of unique category values
    std::vector<std::vector<int>> categories;

    //! Total number of output columns
    int n_output_cols = 0;

    std::vector<std::string> field_names;
    mutable std::mutex mtx;
};

#endif
