/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_PolynomialFeatures.h

    PolynomialFeatures class declaration

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_MODULE_ML_QC_POLYNOMIALFEATURES_H
#define _QORE_MODULE_ML_QC_POLYNOMIALFEATURES_H

#include "ml_common.h"

#include <mutex>
#include <vector>
#include <string>

DLLEXPORT extern qore_classid_t CID_POLYNOMIALFEATURES;
DLLLOCAL extern QoreClass* QC_POLYNOMIALFEATURES;

DLLLOCAL void preinitPolynomialFeaturesClass();
DLLLOCAL QoreClass* initPolynomialFeaturesClass(QoreNamespace& ns);

class QorePolynomialFeatures : public AbstractPrivateData {
public:
    DLLLOCAL QorePolynomialFeatures(int degree, bool include_bias, bool interaction_only);

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

    DLLLOCAL QoreListNode* getOutputFeatureNames(ExceptionSink* xsink) const;
    DLLLOCAL int getNumOutputFeatures() const;

    DLLLOCAL std::vector<uint8_t> serializeState() const;
    DLLLOCAL static QorePolynomialFeatures* deserializeState(const uint8_t* data,
        size_t len, ExceptionSink* xsink);

private:
    //! Generate the list of (feature_index, ...) tuples for all polynomial terms
    struct Term {
        std::vector<int> indices;  // sorted feature indices (with repetition for powers)
    };

    DLLLOCAL void generateTerms();
    DLLLOCAL double evaluateTerm(const MatrixXd& X, int64_t row, const Term& term) const;
    DLLLOCAL std::string termName(const Term& term) const;

    int degree;
    bool include_bias;
    bool interaction_only;
    bool fitted = false;
    int n_features = 0;
    int n_output_cols = 0;
    std::vector<Term> terms;
    std::vector<std::string> field_names;
    mutable std::mutex mtx;
};

#endif
