/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_RFE.h

    RecursiveFeatureElimination class declaration

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_MODULE_ML_QC_RFE_H
#define _QORE_MODULE_ML_QC_RFE_H

#include "ml_common.h"
#include "FlatTree.h"

#include <mutex>
#include <vector>
#include <string>

DLLEXPORT extern qore_classid_t CID_RFE;
DLLLOCAL extern QoreClass* QC_RFE;

DLLLOCAL void preinitRFEClass();
DLLLOCAL QoreClass* initRFEClass(QoreNamespace& ns);

class QoreRFE : public AbstractPrivateData {
public:
    DLLLOCAL QoreRFE(int n_features_to_select, int step);

    //! Fit: iteratively eliminate features using DecisionTree importances
    DLLLOCAL void fit(const MatrixXd& X, const VectorXd& y, ExceptionSink* xsink);

    //! Select surviving features
    DLLLOCAL MatrixXd transform(const MatrixXd& X, ExceptionSink* xsink) const;

    //! Fit and transform in one step
    DLLLOCAL MatrixXd fitTransform(const MatrixXd& X, const VectorXd& y,
        ExceptionSink* xsink);

    DLLLOCAL bool isFitted() const { return fitted; }

    DLLLOCAL void setFieldNames(const std::vector<std::string>& names) {
        field_names = names;
    }
    DLLLOCAL const std::vector<std::string>& getFieldNames() const {
        return field_names;
    }

    //! Get boolean support mask (True = feature selected)
    DLLLOCAL QoreListNode* getSupport(ExceptionSink* xsink) const;

    //! Get feature ranking (1 = selected, 2+ = elimination round)
    DLLLOCAL QoreListNode* getRanking(ExceptionSink* xsink) const;

    DLLLOCAL std::vector<uint8_t> serializeState() const;
    DLLLOCAL static QoreRFE* deserializeState(const uint8_t* data,
        size_t len, ExceptionSink* xsink);

private:
    int n_features_to_select;
    int step;
    bool fitted = false;
    int n_features = 0;
    int n_selected = 0;
    std::vector<bool> support;
    std::vector<int> ranking;  // 1 = selected, 2+ = elimination round
    std::vector<std::string> field_names;
    mutable std::mutex mtx;
};

#endif
