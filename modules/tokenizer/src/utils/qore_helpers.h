/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    qore_helpers.h

    Safe helper functions for Qore C++ API value access

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_TOKENIZER_QORE_HELPERS_H
#define _QORE_TOKENIZER_QORE_HELPERS_H

#include "qore/Qore.h"

#include <string>

namespace QoreTokenizer {

//! Safely gets a hash value from a QoreValue (returns nullptr if not a hash or null)
inline const QoreHashNode* safeGetHash(QoreValue v) {
    if (v.isNullOrNothing() || v.getType() != NT_HASH) {
        return nullptr;
    }
    return v.get<const QoreHashNode>();
}

//! Safely gets a list value from a QoreValue
inline const QoreListNode* safeGetList(QoreValue v) {
    if (v.isNullOrNothing() || v.getType() != NT_LIST) {
        return nullptr;
    }
    return v.get<const QoreListNode>();
}

//! Safely gets a string value from a QoreValue
inline const QoreStringNode* safeGetString(QoreValue v) {
    if (v.isNullOrNothing() || v.getType() != NT_STRING) {
        return nullptr;
    }
    return v.get<const QoreStringNode>();
}

//! Safely gets a string from a hash key (returns empty string if not found or not a string)
inline std::string safeGetStringKey(const QoreHashNode* h, const char* key) {
    const QoreStringNode* s = safeGetString(h->getKeyValue(key));
    return s ? s->c_str() : "";
}

//! Safely gets a bool from a hash key (returns default_val if not found)
inline bool safeGetBoolKey(const QoreHashNode* h, const char* key, bool default_val = false) {
    QoreValue v = h->getKeyValue(key);
    if (v.isNullOrNothing()) {
        return default_val;
    }
    return v.getAsBool();
}

//! Safely gets an int from a hash key (returns default_val if not found)
inline int64 safeGetIntKey(const QoreHashNode* h, const char* key, int64 default_val = 0) {
    QoreValue v = h->getKeyValue(key);
    if (v.isNullOrNothing()) {
        return default_val;
    }
    return v.getAsBigInt();
}

//! Safely gets a hash from a hash key
inline const QoreHashNode* safeGetHashKey(const QoreHashNode* h, const char* key) {
    return safeGetHash(h->getKeyValue(key));
}

//! Safely gets a list from a hash key
inline const QoreListNode* safeGetListKey(const QoreHashNode* h, const char* key) {
    return safeGetList(h->getKeyValue(key));
}

} // namespace QoreTokenizer

#endif // _QORE_TOKENIZER_QORE_HELPERS_H
