/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_RandomForest.h

    RandomForest class declaration

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_MODULE_ML_QC_RANDOMFOREST_H
#define _QORE_MODULE_ML_QC_RANDOMFOREST_H

#include "FlatTree.h"

#include <mutex>
#include <random>

// Forward declarations for QPP binding
DLLEXPORT extern qore_classid_t CID_RANDOMFOREST;
DLLLOCAL extern QoreClass* QC_RANDOMFOREST;

DLLLOCAL void preinitRandomForestClass();
DLLLOCAL QoreClass* initRandomForestClass(QoreNamespace& ns);

//! Random Forest ensemble of CART decision trees
class QoreRandomForest : public AbstractPrivateData {
public:
    DLLLOCAL QoreRandomForest(int n_trees, int max_depth, int min_samples_split,
        int min_samples_leaf, const std::string& max_features,
        bool bootstrap, const std::string& criterion,
        const std::string& task, int64_t seed);

    DLLLOCAL void fit(const MatrixXd& X, const VectorXd& y, ExceptionSink* xsink);

    //! Classify a single row (task must be "classification")
    DLLLOCAL QoreHashNode* predictClassification(const RowVectorXd& point, ExceptionSink* xsink) const;

    //! Predict regression for a single row (task must be "regression")
    DLLLOCAL QoreHashNode* predictRegression(const RowVectorXd& point, ExceptionSink* xsink) const;

    //! Classify multiple rows
    DLLLOCAL QoreListNode* predictClassificationMatrix(const MatrixXd& X, ExceptionSink* xsink) const;

    //! Predict regression for multiple rows
    DLLLOCAL QoreListNode* predictRegressionMatrix(const MatrixXd& X, ExceptionSink* xsink) const;

    //! Get the task type
    DLLLOCAL const std::string& getTask() const { return task; }

    DLLLOCAL QoreHashNode* getFeatureImportances(ExceptionSink* xsink) const;

    DLLLOCAL bool isFitted() const { return fitted; }
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

    //! Get all tree data for ONNX export
    DLLLOCAL QoreListNode* getTreesData(ExceptionSink* xsink) const;

    //! Get number of trees
    DLLLOCAL int getNumTrees() const { return n_trees; }

    //! Get number of features
    DLLLOCAL int getNumFeatures() const { return n_features; }

    //! Get number of classes (0 for regression)
    DLLLOCAL int getNumClasses() const { return n_classes; }

    //! Get class labels
    DLLLOCAL QoreListNode* getClasses(ExceptionSink* xsink) const;

    DLLLOCAL std::vector<uint8_t> serializeState() const;
    DLLLOCAL static QoreRandomForest* deserializeState(const uint8_t* data,
        size_t len, ExceptionSink* xsink);

private:
    int n_trees;
    int max_depth;
    int min_samples_split;
    int min_samples_leaf;
    std::string max_features;  // "sqrt", "log2", or integer string
    bool bootstrap;
    std::string criterion;
    std::string task;
    std::mt19937 rng;

    std::vector<FlatTree> trees;
    bool fitted = false;
    int n_features = 0;
    int n_classes = 0;
    std::vector<double> classes;
    std::vector<std::string> field_names;
    std::string target_field;
    mutable std::mutex mtx;

    DLLLOCAL int computeMaxFeatures() const;
    DLLLOCAL QoreHashNode* predictInternal(const RowVectorXd& point,
        ExceptionSink* xsink) const;
};

#endif // _QORE_MODULE_ML_QC_RANDOMFOREST_H
