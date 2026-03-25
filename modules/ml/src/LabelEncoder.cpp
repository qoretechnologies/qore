/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    LabelEncoder.cpp

    LabelEncoder implementation

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "QC_LabelEncoder.h"
#include "ml_serialization.h"

#include <algorithm>
#include <set>

QoreLabelEncoder::QoreLabelEncoder() {
}

void QoreLabelEncoder::fit(const QoreListNode* values, ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lk(mtx);

    if (!values || values->empty()) {
        xsink->raiseException("ML-LABEL-ENCODER-ERROR", "cannot fit on empty data");
        return;
    }

    // Collect unique values as strings
    std::set<std::string> unique;
    for (size_t i = 0; i < values->size(); ++i) {
        QoreValue v = values->retrieveEntry(i);
        if (v.isNullOrNothing()) {
            continue;
        }
        QoreStringValueHelper str(v);
        unique.insert(str->c_str());
    }

    classes.assign(unique.begin(), unique.end());
    class_to_label.clear();
    for (int i = 0; i < (int)classes.size(); ++i) {
        class_to_label[classes[i]] = i;
    }

    fitted = true;
}

QoreListNode* QoreLabelEncoder::transform(const QoreListNode* values,
        ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);

    if (!fitted) {
        xsink->raiseException("ML-LABEL-ENCODER-ERROR", "encoder has not been fitted");
        return nullptr;
    }

    ReferenceHolder<QoreListNode> result(new QoreListNode(bigIntTypeInfo), xsink);
    for (size_t i = 0; i < values->size(); ++i) {
        QoreValue v = values->retrieveEntry(i);
        if (v.isNullOrNothing()) {
            result->push(-1, xsink);  // -1 for unknown/null
            continue;
        }
        QoreStringValueHelper str(v);
        auto it = class_to_label.find(str->c_str());
        if (it != class_to_label.end()) {
            result->push(it->second, xsink);
        } else {
            result->push(-1, xsink);  // unseen category
        }
    }
    return result.release();
}

QoreListNode* QoreLabelEncoder::inverseTransform(const QoreListNode* labels,
        ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);

    if (!fitted) {
        xsink->raiseException("ML-LABEL-ENCODER-ERROR", "encoder has not been fitted");
        return nullptr;
    }

    ReferenceHolder<QoreListNode> result(new QoreListNode(autoTypeInfo), xsink);
    for (size_t i = 0; i < labels->size(); ++i) {
        int label = (int)labels->retrieveEntry(i).getAsBigInt();
        if (label >= 0 && label < (int)classes.size()) {
            result->push(new QoreStringNode(classes[label]), xsink);
        } else {
            result->push(QoreValue(), xsink);  // NOTHING for invalid labels
        }
    }
    return result.release();
}

QoreListNode* QoreLabelEncoder::fitTransform(const QoreListNode* values,
        ExceptionSink* xsink) {
    fit(values, xsink);
    if (*xsink) {
        return nullptr;
    }
    return transform(values, xsink);
}

QoreListNode* QoreLabelEncoder::getClasses(ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);

    if (!fitted) {
        xsink->raiseException("ML-LABEL-ENCODER-ERROR", "encoder has not been fitted");
        return nullptr;
    }

    ReferenceHolder<QoreListNode> result(new QoreListNode(stringTypeInfo), xsink);
    for (const auto& c : classes) {
        result->push(new QoreStringNode(c), xsink);
    }
    return result.release();
}

// --- Serialization ---

std::vector<uint8_t> QoreLabelEncoder::serializeState() const {
    std::vector<uint8_t> buf;
    MLSerialization::writeStringVector(buf, classes);
    MLSerialization::writeStringVector(buf, field_names);
    return buf;
}

QoreLabelEncoder* QoreLabelEncoder::deserializeState(const uint8_t* data,
        size_t len, ExceptionSink* xsink) {
    const uint8_t* ptr = data;
    size_t remaining = len;

    auto cls = MLSerialization::readStringVector(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    auto fnames = MLSerialization::readStringVector(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }

    auto* enc = new QoreLabelEncoder();
    enc->classes = std::move(cls);
    for (int i = 0; i < (int)enc->classes.size(); ++i) {
        enc->class_to_label[enc->classes[i]] = i;
    }
    enc->field_names = std::move(fnames);
    enc->fitted = true;
    return enc;
}
