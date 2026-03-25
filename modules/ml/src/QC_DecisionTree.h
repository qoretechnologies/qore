/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_DecisionTree.h

    DecisionTree class declaration

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_MODULE_ML_QC_DECISIONTREE_H
#define _QORE_MODULE_ML_QC_DECISIONTREE_H

#include "FlatTree.h"

#include <mutex>
#include <random>

// Forward declarations for QPP binding
DLLEXPORT extern qore_classid_t CID_DECISIONTREE;
DLLLOCAL extern QoreClass* QC_DECISIONTREE;

DLLLOCAL void preinitDecisionTreeClass();
DLLLOCAL QoreClass* initDecisionTreeClass(QoreNamespace& ns);

//! CART Decision Tree for classification and regression
class QoreDecisionTree : public AbstractPrivateData {
public:
    //! Constructor with hyperparameters
    DLLLOCAL QoreDecisionTree(int max_depth, int min_samples_split,
        int min_samples_leaf, const std::string& criterion,
        const std::string& task, int64_t seed);

    //! Fit on feature matrix X and target vector y
    DLLLOCAL void fit(const MatrixXd& X, const VectorXd& y, ExceptionSink* xsink);

    //! Predict for a single row
    DLLLOCAL QoreHashNode* predict(const RowVectorXd& point, ExceptionSink* xsink) const;

    //! Predict for multiple rows
    DLLLOCAL QoreListNode* predictMatrix(const MatrixXd& X, ExceptionSink* xsink) const;

    //! Get feature importances
    DLLLOCAL QoreHashNode* getFeatureImportances(ExceptionSink* xsink) const;

    //! Whether the model has been fitted
    DLLLOCAL bool isFitted() const { return fitted; }

    //! Field names for hash-based API
    DLLLOCAL void setFieldNames(const std::vector<std::string>& names) {
        field_names = names;
    }
    DLLLOCAL const std::vector<std::string>& getFieldNames() const {
        return field_names;
    }
    DLLLOCAL void setTargetField(const std::string& name) {
        target_field = name;
    }
    DLLLOCAL const std::string& getTargetField() const {
        return target_field;
    }

    //! Serialization
    DLLLOCAL std::vector<uint8_t> serializeState() const;
    DLLLOCAL static QoreDecisionTree* deserializeState(const uint8_t* data,
        size_t len, ExceptionSink* xsink);

    //! Access to internal tree (for RandomForest/GBT)
    DLLLOCAL const FlatTree& getTree() const { return tree; }

private:
    int max_depth;
    int min_samples_split;
    int min_samples_leaf;
    std::string criterion;
    std::string task;
    std::mt19937 rng;

    FlatTree tree;
    bool fitted = false;
    int n_features = 0;
    std::vector<std::string> field_names;
    std::string target_field;
    mutable std::mutex mtx;

    //! Internal predict (caller must hold lock)
    DLLLOCAL QoreHashNode* predictInternal(const RowVectorXd& point,
        ExceptionSink* xsink) const;
};

#endif // _QORE_MODULE_ML_QC_DECISIONTREE_H
