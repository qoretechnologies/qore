/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_DateTimeFeatures.h

    DateTimeFeatures transformer class declaration

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_MODULE_ML_QC_DATETIMEFEATURES_H
#define _QORE_MODULE_ML_QC_DATETIMEFEATURES_H

#include "ml_common.h"

#include <mutex>
#include <string>
#include <vector>

DLLEXPORT extern qore_classid_t CID_DATETIMEFEATURES;
DLLLOCAL extern QoreClass* QC_DATETIMEFEATURES;

DLLLOCAL void preinitDateTimeFeaturesClass();
DLLLOCAL QoreClass* initDateTimeFeaturesClass(QoreNamespace& ns);

//! Extracts numeric features from date/datetime values
/** Stateless transformer that takes a list of date values and produces a list
    of numeric feature vectors (one per date). Configurable feature selection
    allows extracting specific date components.

    Supported features: "year", "month", "day", "hour", "minute", "second",
    "day_of_week" (0=Sunday..6=Saturday), "day_of_year", "quarter",
    "is_weekend" (0/1), "week_of_year".
*/
class QoreDateTimeFeatures : public AbstractPrivateData {
public:
    DLLLOCAL QoreDateTimeFeatures(const std::vector<std::string>& features);

    //! Extracts features from a list of dates; returns list of list<float>
    DLLLOCAL QoreListNode* transform(const QoreListNode* dates, ExceptionSink* xsink);

    //! Returns the list of output feature names
    DLLLOCAL QoreListNode* getOutputFeatureNames(ExceptionSink* xsink) const;

    //! Returns the number of output features
    DLLLOCAL int getNumOutputFeatures() const { return (int)features.size(); }

    //! Always true (stateless transformer)
    DLLLOCAL bool isFitted() const { return true; }

    //! Returns the feature names as a vector (for serialization)
    DLLLOCAL const std::vector<std::string>& getFeatures() const { return features; }

    DLLLOCAL std::vector<uint8_t> serializeState() const;
    DLLLOCAL static QoreDateTimeFeatures* deserializeState(const uint8_t* data,
        size_t len, ExceptionSink* xsink);

    //! Validate that all feature names are recognized
    DLLLOCAL static bool validateFeatures(const std::vector<std::string>& features,
        ExceptionSink* xsink);

private:
    //! Extract a single feature value from a DateTimeNode
    DLLLOCAL double extractFeature(const DateTimeNode* dt, const std::string& feature) const;

    std::vector<std::string> features;
    mutable std::mutex mtx;
};

#endif
