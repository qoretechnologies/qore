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

//! Returns a copy of all bytes of a Qore string, including any NUL characters; an empty string for nullptr
/** A NUL character is valid text, so a Qore string is always copied with its length, never as a C string, which
    would cut it at the first NUL character.
*/
inline std::string qoreStdString(const QoreString* s) {
    return s ? std::string(s->c_str(), s->size()) : std::string();
}

//! Gets the text of a Qore string in UTF-8, the encoding that the tokenizer works in, converting it if needed
/** The text is copied with its length, including any NUL characters; an empty string for nullptr

    @return 0 for OK, -1 if a Qore exception was raised (the string cannot be converted to UTF-8)
*/
inline int qoreUtf8String(const QoreString* s, std::string& rv, ExceptionSink* xsink) {
    if (!s) {
        rv.clear();
        return 0;
    }
    TempEncodingHelper utf8(s, QCS_UTF8, xsink);
    if (!utf8) {
        return -1;
    }
    rv.assign(utf8->c_str(), utf8->size());
    return 0;
}

//! Gets the text of a string value in UTF-8 (see qoreUtf8String()); an empty string for a value that is not a string
/** @return 0 for OK, -1 if a Qore exception was raised (the string cannot be converted to UTF-8)
*/
inline int qoreUtf8String(QoreValue v, std::string& rv, ExceptionSink* xsink) {
    if (v.getType() != NT_STRING) {
        rv.clear();
        return 0;
    }
    return qoreUtf8String(v.get<const QoreStringNode>(), rv, xsink);
}

//! Safely gets a string value from a QoreValue, including any NUL characters.
inline std::string safeGetStdString(QoreValue v) {
    if (v.isNullOrNothing() || v.getType() != NT_STRING) {
        return {};
    }
    QoreStringValueHelper s(v);
    return qoreStdString(*s);
}

//! Safely gets a string from a hash key (returns empty string if not found or not a string)
inline std::string safeGetStringKey(const QoreHashNode* h, const char* key) {
    return safeGetStdString(h->getKeyValue(key));
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
