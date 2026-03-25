/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_VarianceThreshold.h

    VarianceThreshold feature selector declaration

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_MODULE_ML_QC_VARIANCETHRESHOLD_H
#define _QORE_MODULE_ML_QC_VARIANCETHRESHOLD_H

#include "ml_common.h"

#include <mutex>

DLLEXPORT extern qore_classid_t CID_VARIANCETHRESHOLD;
DLLLOCAL extern QoreClass* QC_VARIANCETHRESHOLD;

DLLLOCAL void preinitVarianceThresholdClass();
DLLLOCAL QoreClass* initVarianceThresholdClass(QoreNamespace& ns);

//! Removes features with variance below a threshold
/** fit() computes per-feature variance. transform() keeps only features
    with variance > threshold. Default threshold 0.0 removes constant features.
*/
class QoreVarianceThreshold : public AbstractPrivateData {
public:
    DLLLOCAL QoreVarianceThreshold(double threshold);

    DLLLOCAL void fit(const MatrixXd& X, ExceptionSink* xsink);
    DLLLOCAL MatrixXd transform(const MatrixXd& X, ExceptionSink* xsink) const;
    DLLLOCAL MatrixXd fitTransform(const MatrixXd& X, ExceptionSink* xsink);

    DLLLOCAL bool isFitted() const { return fitted; }
    DLLLOCAL void setFieldNames(const std::vector<std::string>& names) {
        field_names = names;
    }
    DLLLOCAL const std::vector<std::string>& getFieldNames() const {
        return field_names;
    }

    //! Get per-feature variances (from fit)
    DLLLOCAL QoreListNode* getVariances(ExceptionSink* xsink) const;

    //! Get the mask of selected features (true = kept)
    DLLLOCAL QoreListNode* getSupport(ExceptionSink* xsink) const;

    DLLLOCAL std::vector<uint8_t> serializeState() const;
    DLLLOCAL static QoreVarianceThreshold* deserializeState(const uint8_t* data,
        size_t len, ExceptionSink* xsink);

private:
    double threshold;
    bool fitted = false;
    int n_features = 0;
    VectorXd variances;
    std::vector<bool> support;  //!< true = feature kept
    int n_selected = 0;
    std::vector<std::string> field_names;
    mutable std::mutex mtx;
};

#endif
