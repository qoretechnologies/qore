/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_LabelEncoder.h

    LabelEncoder class declaration

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_MODULE_ML_QC_LABELENCODER_H
#define _QORE_MODULE_ML_QC_LABELENCODER_H

#include "ml_common.h"

#include <mutex>
#include <unordered_map>

DLLEXPORT extern qore_classid_t CID_LABELENCODER;
DLLLOCAL extern QoreClass* QC_LABELENCODER;

DLLLOCAL void preinitLabelEncoderClass();
DLLLOCAL QoreClass* initLabelEncoderClass(QoreNamespace& ns);

//! Label encoder: maps string/integer categories to consecutive integers 0..n-1
/** fit() learns the mapping from unique values.
    transform() applies the mapping.
    inverseTransform() converts back to original values.
*/
class QoreLabelEncoder : public AbstractPrivateData {
public:
    DLLLOCAL QoreLabelEncoder();

    //! Learn categories from a Qore list of values
    DLLLOCAL void fit(const QoreListNode* values, ExceptionSink* xsink);

    //! Transform values to integer labels
    DLLLOCAL QoreListNode* transform(const QoreListNode* values, ExceptionSink* xsink) const;

    //! Inverse transform: integer labels back to original values
    DLLLOCAL QoreListNode* inverseTransform(const QoreListNode* labels,
        ExceptionSink* xsink) const;

    //! Fit and transform in one step
    DLLLOCAL QoreListNode* fitTransform(const QoreListNode* values, ExceptionSink* xsink);

    DLLLOCAL bool isFitted() const { return fitted; }
    DLLLOCAL const std::vector<std::string>& getFieldNames() const {
        return field_names;
    }
    DLLLOCAL void setFieldNames(const std::vector<std::string>& names) {
        field_names = names;
    }

    //! Get the learned classes (in order of their integer encoding)
    DLLLOCAL QoreListNode* getClasses(ExceptionSink* xsink) const;

    DLLLOCAL std::vector<uint8_t> serializeState() const;
    DLLLOCAL static QoreLabelEncoder* deserializeState(const uint8_t* data,
        size_t len, ExceptionSink* xsink);

private:
    bool fitted = false;
    std::vector<std::string> classes;  //!< sorted unique values (index = label)
    std::unordered_map<std::string, int> class_to_label;
    std::vector<std::string> field_names;
    mutable std::mutex mtx;
};

#endif
