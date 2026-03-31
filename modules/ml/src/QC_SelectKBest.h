/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_SelectKBest.h

    SelectKBest class declaration

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_MODULE_ML_QC_SELECTKBEST_H
#define _QORE_MODULE_ML_QC_SELECTKBEST_H

#include "ml_common.h"

#include <mutex>
#include <vector>
#include <string>

DLLEXPORT extern qore_classid_t CID_SELECTKBEST;
DLLLOCAL extern QoreClass* QC_SELECTKBEST;

DLLLOCAL void preinitSelectKBestClass();
DLLLOCAL QoreClass* initSelectKBestClass(QoreNamespace& ns);

class QoreSelectKBest : public AbstractPrivateData {
public:
    //! score_func: "f_classif" (ANOVA F-test) or "mutual_info" (mutual information)
    DLLLOCAL QoreSelectKBest(int k, const std::string& score_func);

    //! Fit on features X and target y — computes scores per feature
    DLLLOCAL void fit(const MatrixXd& X, const VectorXd& y, ExceptionSink* xsink);

    //! Select top-k features
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

    //! Get per-feature scores
    DLLLOCAL QoreListNode* getScores(ExceptionSink* xsink) const;

    //! Get boolean support mask (True = feature selected)
    DLLLOCAL QoreListNode* getSupport(ExceptionSink* xsink) const;

    //! Get indices of selected features
    DLLLOCAL QoreListNode* getSelectedIndices(ExceptionSink* xsink) const;

    DLLLOCAL std::vector<uint8_t> serializeState() const;
    DLLLOCAL static QoreSelectKBest* deserializeState(const uint8_t* data,
        size_t len, ExceptionSink* xsink);

private:
    //! ANOVA F-statistic for each feature vs target (classification)
    DLLLOCAL void computeFClassif(const MatrixXd& X, const VectorXd& y);

    //! Mutual information between each feature and target
    DLLLOCAL void computeMutualInfo(const MatrixXd& X, const VectorXd& y);

    //! Select top-k based on scores
    DLLLOCAL void selectTopK();

    int k;
    std::string score_func;
    bool fitted = false;
    int n_features = 0;
    VectorXd scores;
    std::vector<bool> support;
    int n_selected = 0;
    std::vector<std::string> field_names;
    mutable std::mutex mtx;
};

#endif
