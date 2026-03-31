/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_GradientBoostedTrees.h

    GradientBoostedTrees class declaration

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_MODULE_ML_QC_GRADIENTBOOSTEDTREES_H
#define _QORE_MODULE_ML_QC_GRADIENTBOOSTEDTREES_H

#include "FlatTree.h"

#include <mutex>
#include <random>

DLLEXPORT extern qore_classid_t CID_GRADIENTBOOSTEDTREES;
DLLLOCAL extern QoreClass* QC_GRADIENTBOOSTEDTREES;

DLLLOCAL void preinitGradientBoostedTreesClass();
DLLLOCAL QoreClass* initGradientBoostedTreesClass(QoreNamespace& ns);

//! Gradient Boosted Trees for classification and regression
class QoreGradientBoostedTrees : public AbstractPrivateData {
public:
    DLLLOCAL QoreGradientBoostedTrees(int n_estimators, double learning_rate,
        int max_depth, int min_samples_split, int min_samples_leaf,
        double subsample, const std::string& task,
        double validation_fraction, int n_iter_no_change, int64_t seed);

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
    DLLLOCAL int getActualEstimators() const { return actual_n_estimators; }
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

    //! Get initial prediction (regression / binary classification)
    DLLLOCAL double getInitialPrediction() const { return initial_prediction; }

    //! Get initial predictions (multiclass)
    DLLLOCAL QoreListNode* getInitialPredictions(ExceptionSink* xsink) const;

    //! Get learning rate
    DLLLOCAL double getLearningRate() const { return learning_rate; }

    //! Get number of trees
    DLLLOCAL int getNumTrees() const { return static_cast<int>(trees.size()); }

    //! Get number of features
    DLLLOCAL int getNumFeatures() const { return n_features; }

    //! Get number of classes (0 for regression)
    DLLLOCAL int getNumClasses() const { return n_classes; }

    //! Get class labels
    DLLLOCAL QoreListNode* getClasses(ExceptionSink* xsink) const;

    DLLLOCAL std::vector<uint8_t> serializeState() const;
    DLLLOCAL static QoreGradientBoostedTrees* deserializeState(const uint8_t* data,
        size_t len, ExceptionSink* xsink);

private:
    int n_estimators;
    double learning_rate;
    int max_depth;
    int min_samples_split;
    int min_samples_leaf;
    double subsample;
    std::string task;
    double validation_fraction;
    int n_iter_no_change;
    std::mt19937 rng;

    double initial_prediction = 0.0;
    std::vector<double> initial_predictions;  // per-class for multiclass softmax
    std::vector<FlatTree> trees;
    int actual_n_estimators = 0;
    int n_features = 0;
    int n_classes = 0;
    std::vector<double> classes;
    std::vector<std::string> field_names;
    std::string target_field;
    bool fitted = false;
    mutable std::mutex mtx;

    DLLLOCAL QoreHashNode* predictInternal(const RowVectorXd& point,
        ExceptionSink* xsink) const;
    DLLLOCAL double predictRaw(const RowVectorXd& point) const;

    static double sigmoid(double x) {
        return 1.0 / (1.0 + std::exp(-x));
    }
};

#endif
