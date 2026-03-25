/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_NaiveBayes.h

    Naive Bayes classifier declaration

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_MODULE_ML_QC_NAIVEBAYES_H
#define _QORE_MODULE_ML_QC_NAIVEBAYES_H

#include "ml_common.h"

#include <mutex>

DLLEXPORT extern qore_classid_t CID_NAIVEBAYES;
DLLLOCAL extern QoreClass* QC_NAIVEBAYES;

DLLLOCAL void preinitNaiveBayesClass();
DLLLOCAL QoreClass* initNaiveBayesClass(QoreNamespace& ns);

//! Gaussian Naive Bayes classifier
/** Assumes features are independent and follow a Gaussian distribution within
    each class. Very fast to train and predict. Supports online updates.
*/
class QoreNaiveBayes : public AbstractPrivateData {
public:
    DLLLOCAL QoreNaiveBayes(double var_smoothing);

    DLLLOCAL void fit(const MatrixXd& X, const VectorXd& y, ExceptionSink* xsink);
    DLLLOCAL void update(const MatrixXd& X, const VectorXd& y, ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* predict(const RowVectorXd& point, ExceptionSink* xsink) const;
    DLLLOCAL QoreListNode* predictMatrix(const MatrixXd& X, ExceptionSink* xsink) const;

    DLLLOCAL bool isFitted() const { return fitted; }
    DLLLOCAL void setFieldNames(const std::vector<std::string>& names) {
        field_names = names;
    }
    DLLLOCAL const std::vector<std::string>& getFieldNames() const {
        return field_names;
    }
    DLLLOCAL void setTargetField(const std::string& name) { target_field = name; }
    DLLLOCAL const std::string& getTargetField() const { return target_field; }

    DLLLOCAL std::vector<uint8_t> serializeState() const;
    DLLLOCAL static QoreNaiveBayes* deserializeState(const uint8_t* data, size_t len,
        ExceptionSink* xsink);

private:
    double var_smoothing;  //!< variance smoothing (added to variances to prevent zero)

    //! Per-class statistics
    struct ClassStats {
        double prior = 0.0;       //!< log prior probability
        VectorXd mean;            //!< mean per feature
        VectorXd var;             //!< variance per feature
        int count = 0;            //!< number of training samples
    };

    std::vector<ClassStats> class_stats;
    std::vector<double> classes;
    int n_features = 0;
    int n_classes = 0;
    int total_samples = 0;
    bool fitted = false;
    std::vector<std::string> field_names;
    std::string target_field;
    mutable std::mutex mtx;

    DLLLOCAL QoreHashNode* predictInternal(const RowVectorXd& point,
        ExceptionSink* xsink) const;
    DLLLOCAL double logPdf(double x, double mean, double var) const;
};

#endif
