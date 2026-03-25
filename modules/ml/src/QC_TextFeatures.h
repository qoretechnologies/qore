/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_TextFeatures.h

    TextFeatures class declaration

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_MODULE_ML_QC_TEXTFEATURES_H
#define _QORE_MODULE_ML_QC_TEXTFEATURES_H

#include "ml_common.h"

#include <map>
#include <mutex>
#include <string>
#include <vector>

DLLEXPORT extern qore_classid_t CID_TEXTFEATURES;
DLLLOCAL extern QoreClass* QC_TEXTFEATURES;

DLLLOCAL void preinitTextFeaturesClass();
DLLLOCAL QoreClass* initTextFeaturesClass(QoreNamespace& ns);

//! Converts text documents into numeric feature vectors using bag-of-words or TF-IDF weighting
/** fit() learns a vocabulary from training documents.
    transform() converts documents into numeric vectors using the learned vocabulary.
    Supports two modes: "tfidf" (TF-IDF weighting) and "count" (raw term counts).
*/
class QoreTextFeatures : public AbstractPrivateData {
public:
    DLLLOCAL QoreTextFeatures(const std::string& mode, int max_features, int min_df);

    //! Learn vocabulary from a list of text documents
    DLLLOCAL void fit(const QoreListNode* documents, ExceptionSink* xsink);

    //! Transform text documents into numeric feature vectors
    DLLLOCAL QoreListNode* transform(const QoreListNode* documents, ExceptionSink* xsink) const;

    //! Fit and transform in one step
    DLLLOCAL QoreListNode* fitTransform(const QoreListNode* documents, ExceptionSink* xsink);

    //! Get the vocabulary terms in index order
    DLLLOCAL QoreListNode* getVocabulary(ExceptionSink* xsink) const;

    DLLLOCAL bool isFitted() const { return fitted; }

    DLLLOCAL const std::vector<std::string>& getFieldNames() const {
        return field_names;
    }

    DLLLOCAL std::vector<uint8_t> serializeState() const;
    DLLLOCAL static QoreTextFeatures* deserializeState(const uint8_t* data,
        size_t len, ExceptionSink* xsink);

private:
    //! Tokenize a text string into lowercase alphanumeric tokens
    DLLLOCAL static std::vector<std::string> tokenize(const std::string& text);

    std::string mode;       //!< "tfidf" or "count"
    int max_features;       //!< maximum vocabulary size (0 = unlimited)
    int min_df;             //!< minimum document frequency
    std::map<std::string, int> vocabulary;  //!< word -> index (sorted for determinism)
    std::vector<double> idf_values;         //!< IDF values for tfidf mode
    int n_docs = 0;         //!< number of documents used for fitting
    int n_features = 0;     //!< vocabulary size after fitting
    bool fitted = false;
    std::vector<std::string> field_names;   //!< vocabulary terms in index order
    mutable std::mutex mtx;
};

#endif
