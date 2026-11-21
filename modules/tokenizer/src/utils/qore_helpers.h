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

#include <cstring>
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

//! Gives the text of a Qore string as UTF-8, checked where it enters the tokenizer (qore#6604)
/** The tokenizer works on UTF-8 text: its vocabularies, regular expressions (PCRE2 in UTF mode) and Unicode
    normalizers all expect UTF-8.  The text is first checked in its own encoding with q_check_text_encoding(), and
    text in another encoding is then converted to UTF-8, so that the same text gives the same tokens in any
    encoding and bytes that are not valid text never reach the pipeline (PCRE2 refuses a subject that is not valid
    UTF-8, which skipped the Split step).

    @param s the string; nullptr gives an empty string
    @param out the UTF-8 text, including any NUL characters
    @param where the Qore method that takes the text, for the exception (ex: \c "HFTokenizer::encode()")
    @param what what the text is, for the exception (ex: \c "text")
    @param xsink for an \c INVALID-ENCODING exception or the exception of the conversion

    @return 0 for OK, -1 if an exception was raised
*/
inline int qoreUtf8StdString(const QoreString* s, std::string& out, const char* where, const char* what,
        ExceptionSink* xsink) {
    if (!s) {
        out.clear();
        return 0;
    }
    if (q_check_text_encoding(s->getEncoding(), s->c_str(), s->size(), where, what, xsink)) {
        return -1;
    }
    TempEncodingHelper utf8(s, QCS_UTF8, xsink);
    if (!utf8) {
        return -1;
    }
    out.assign(utf8->c_str(), utf8->size());
    return 0;
}

//! Gives a string value as checked UTF-8 text as qoreUtf8StdString() does; a value that is not a string gives an
//! empty string, as with safeGetStdString()
inline int safeGetUtf8StdString(QoreValue v, std::string& out, const char* where, const char* what,
        ExceptionSink* xsink) {
    if (v.isNullOrNothing() || v.getType() != NT_STRING) {
        out.clear();
        return 0;
    }
    // a value of a hash or list can be an inline short string
    QoreStringValueHelper str(v);
    return qoreUtf8StdString(*str, out, where, what, xsink);
}

//! The method named in the exception for configuration text that is not valid: the configuration of the model and
//! of the components is given to the constructor
constexpr const char* ConfigWhere = "HFTokenizer::constructor()";

//! Gets a string value of the configuration as checked UTF-8 text, as safeGetUtf8StdString() does (qore#6604)
/** The strings of the configuration (vocabulary tokens, merges, patterns, replacement strings, special tokens) are
    matched against or inserted into the UTF-8 text that is tokenized, so a string in another encoding is converted to
    UTF-8 (ex: from a configuration parsed from ISO-8859-1 JSON text, or built by hand), and one that is not valid in
    its encoding raises \c INVALID-ENCODING.

    @param v the value; a value that is not a string gives an empty string
    @param what what the text is, for the exception (ex: \c "merge")
    @param xsink for an \c INVALID-ENCODING exception or the exception of the conversion

    @return the UTF-8 text; an empty string if an exception was raised (check \a xsink)
*/
inline std::string configString(QoreValue v, const char* what, ExceptionSink* xsink) {
    std::string out;
    if (safeGetUtf8StdString(v, out, ConfigWhere, what, xsink)) {
        return {};
    }
    return out;
}

//! Gets the string value of a configuration key as checked UTF-8 text, as configString() does
/** @param h the configuration hash
    @param key the key, named in the exception (<tt>"configuration value '<key>'"</tt>)
    @param xsink for an \c INVALID-ENCODING exception or the exception of the conversion

    @return the UTF-8 text; an empty string if the value is not a string or if an exception was raised (check
    \a xsink)
*/
inline std::string configStringKey(const QoreHashNode* h, const char* key, ExceptionSink* xsink) {
    QoreValue v = h->getKeyValue(key);
    if (v.getType() != NT_STRING) {
        return {};
    }
    QoreStringMaker what("configuration value '%s'", key);
    return configString(v, what.c_str(), xsink);
}

//! Gets a key of a configuration hash as checked UTF-8 text (qore#6604)
/** A hash key is in the default encoding (parse_json() converts the keys of the JSON text to it); a key that is a
    token (a vocabulary token, a special token) is converted to UTF-8, the text that is tokenized.  With a UTF-8
    default encoding the key is checked and taken as it is.

    @param key the key, in the default encoding
    @param what what the key is, for the exception (ex: \c "vocabulary token")
    @param out the UTF-8 text
    @param xsink for an \c INVALID-ENCODING exception or the exception of the conversion

    @return 0 for OK, -1 if an exception was raised
*/
inline int configKey(const char* key, const char* what, std::string& out, ExceptionSink* xsink) {
    size_t len = strlen(key);
    if (q_check_text_encoding(QCS_DEFAULT, key, len, ConfigWhere, what, xsink)) {
        return -1;
    }
    if (QCS_DEFAULT == QCS_UTF8) {
        out.assign(key, len);
        return 0;
    }
    QoreString str(key, len, QCS_DEFAULT);
    TempEncodingHelper utf8(str, QCS_UTF8, xsink);
    if (!utf8) {
        return -1;
    }
    out.assign(utf8->c_str(), utf8->size());
    return 0;
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
