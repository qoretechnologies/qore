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
    DLLLOCAL QoreHashNode* predict(const RowVectorXd& point, ExceptionSink* xsink) const;
    DLLLOCAL QoreListNode* predictMatrix(const MatrixXd& X, ExceptionSink* xsink) const;
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
