/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    DateTimeFeatures.cpp

    DateTimeFeatures transformer implementation

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "QC_DateTimeFeatures.h"
#include "ml_serialization.h"

#include <cmath>
#include <unordered_set>

// All supported feature names
static const std::unordered_set<std::string> VALID_FEATURES = {
    "year", "month", "day", "hour", "minute", "second",
    "day_of_week", "day_of_year", "quarter", "is_weekend", "week_of_year"
};

QoreDateTimeFeatures::QoreDateTimeFeatures(const std::vector<std::string>& features)
    : features(features) {
}

bool QoreDateTimeFeatures::validateFeatures(const std::vector<std::string>& features,
        ExceptionSink* xsink) {
    for (const auto& f : features) {
        if (VALID_FEATURES.find(f) == VALID_FEATURES.end()) {
            QoreStringNode* desc = new QoreStringNode;
            desc->sprintf("unknown feature name '%s'; supported features: year, month, day, "
                "hour, minute, second, day_of_week, day_of_year, quarter, is_weekend, "
                "week_of_year", f.c_str());
            xsink->raiseException("ML-DATETIMEFEATURES-ERROR", desc);
            return false;
        }
    }
    if (features.empty()) {
        xsink->raiseException("ML-DATETIMEFEATURES-ERROR",
            "at least one feature must be specified");
        return false;
    }
    return true;
}

double QoreDateTimeFeatures::extractFeature(const DateTimeNode* dt,
        const std::string& feature) const {
    if (feature == "year") {
        return (double)dt->getYear();
    } else if (feature == "month") {
        return (double)dt->getMonth();
    } else if (feature == "day") {
        return (double)dt->getDay();
    } else if (feature == "hour") {
        return (double)dt->getHour();
    } else if (feature == "minute") {
        return (double)dt->getMinute();
    } else if (feature == "second") {
        return (double)dt->getSecond();
    } else if (feature == "day_of_week") {
        return (double)dt->getDayOfWeek();
    } else if (feature == "day_of_year") {
        return (double)dt->getDayNumber();
    } else if (feature == "quarter") {
        return (double)((dt->getMonth() - 1) / 3 + 1);
    } else if (feature == "is_weekend") {
        int dow = dt->getDayOfWeek();
        return (dow == 0 || dow == 6) ? 1.0 : 0.0;
    } else if (feature == "week_of_year") {
        return (double)(dt->getDayNumber() / 7 + 1);
    }
    // Should not reach here if validateFeatures was called
    return 0.0;
}

QoreListNode* QoreDateTimeFeatures::transform(const QoreListNode* dates,
        ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lk(mtx);

    if (!dates || dates->empty()) {
        xsink->raiseException("ML-DATETIMEFEATURES-ERROR",
            "input date list is empty");
        return nullptr;
    }

    size_t n = dates->size();
    size_t nf = features.size();

    ReferenceHolder<QoreListNode> result(new QoreListNode(autoTypeInfo), xsink);
    for (size_t i = 0; i < n; ++i) {
        QoreValue v = dates->retrieveEntry(i);
        ReferenceHolder<QoreListNode> row(new QoreListNode(autoTypeInfo), xsink);

        if (v.getType() == NT_DATE) {
            const DateTimeNode* dt = v.get<const DateTimeNode>();
            for (size_t j = 0; j < nf; ++j) {
                row->push(extractFeature(dt, features[j]), xsink);
            }
        } else {
            // Null/NOTHING dates: output 0.0 for all features
            for (size_t j = 0; j < nf; ++j) {
                row->push(0.0, xsink);
            }
        }
        result->push(row.release(), xsink);
    }

    return result.release();
}

QoreListNode* QoreDateTimeFeatures::getOutputFeatureNames(ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    ReferenceHolder<QoreListNode> result(new QoreListNode(stringTypeInfo), xsink);
    for (const auto& f : features) {
        result->push(new QoreStringNode(f), xsink);
    }
    return result.release();
}

// --- Serialization ---

std::vector<uint8_t> QoreDateTimeFeatures::serializeState() const {
    std::vector<uint8_t> buf;
    MLSerialization::writeStringVector(buf, features);
    return buf;
}

QoreDateTimeFeatures* QoreDateTimeFeatures::deserializeState(const uint8_t* data,
        size_t len, ExceptionSink* xsink) {
    const uint8_t* ptr = data;
    size_t remaining = len;

    auto feats = MLSerialization::readStringVector(ptr, remaining, xsink);
    if (*xsink) {
        return nullptr;
    }

    if (!validateFeatures(feats, xsink)) {
        return nullptr;
    }

    return new QoreDateTimeFeatures(feats);
}
