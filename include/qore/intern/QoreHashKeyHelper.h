/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreHashKeyHelper.h

    Qore Programming Language

    Copyright (C) 2026 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.

    Note that the Qore library is released under a choice of three open-source
    licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
    information.
*/

#ifndef _QORE_INTERN_QOREHASHKEYHELPER_H

#define _QORE_INTERN_QOREHASHKEYHELPER_H

#include <qore/Qore.h>

#include <cassert>
#include <cstddef>
#include <memory>
#include <string>

//! Provides the hash key or object member name for a runtime value, in the default character encoding
/** Hash keys and object member names are stored in the default character encoding (QCS_DEFAULT); every execution
    engine (AST, IR, JIT, tiered, AOT) turns a runtime key value into a key through this class, so that a key given
    as a string in another encoding names the same key as the equivalent string in the default encoding.

    A string already in the default encoding, an inline short string, and a string in an ASCII-compatible encoding
    whose bytes are all ASCII are used as they are, without conversion or allocation; any other string is converted.
    Other values use their string representation.

    Stack only; it may not be dynamically allocated. If an encoding conversion fails or the ASCII scan is
    cancelled, a Qore-language exception is raised in the ExceptionSink given, and the key is empty.

    @code
    QoreHashKeyHelper key(key_val, xsink);
    if (*xsink) {
        return QoreValue();
    }
    return h->getKeyValue(key.c_str(), xsink);
    @endcode
*/
class QoreHashKeyHelper {
public:
    //! sets up the key for the given value
    /** @param n the key value
        @param xsink receives encoding-conversion errors and cancellation during ASCII scanning
    */
    DLLLOCAL QoreHashKeyHelper(const QoreValue& n, ExceptionSink* xsink) {
        assert(xsink);
        if (n.isShortString()) {
            // short strings are always stored in UTF-8
            if (QCS_DEFAULT == QCS_UTF8) {
                n.getShortString(buf);
                key = buf;
                len = n.shortStringLen();
                return;
            }
        } else if (n.isPointer()) {
            const AbstractQoreNode* node = n.getInternalNode();
            if (node && node->getType() == NT_STRING) {
                const QoreStringNode* str = static_cast<const QoreStringNode*>(node);
                if (usableAsIs(*str, xsink)) {
                    key = str->c_str();
                    len = str->size();
                    return;
                }
            }
        }
        if (*xsink) {
            valid = false;
            return;
        }
        setupSlow(n, xsink);
    }

    //! sets up the key for the given string
    /** @param str the key string
        @param xsink receives encoding-conversion errors and cancellation during ASCII scanning
    */
    DLLLOCAL QoreHashKeyHelper(const QoreString& str, ExceptionSink* xsink) {
        assert(xsink);
        if (usableAsIs(str, xsink)) {
            key = str.c_str();
            len = str.size();
            return;
        }
        if (*xsink) {
            valid = false;
            return;
        }
        setupSlow(str, xsink);
    }

    //! returns true if the key is valid, false if conversion or cancellation raised an exception
    DLLLOCAL explicit operator bool() const {
        return valid;
    }

    //! returns the key in the default character encoding; empty if an exception was raised
    DLLLOCAL const char* c_str() const {
        return key;
    }

    //! returns the byte length of the key
    DLLLOCAL size_t size() const {
        return len;
    }

    //! returns true if a key string in the given encoding can be used as a key without conversion
    /** @param enc the encoding of the key string
        @param p the key string
        @param size the byte length of the key string
        @param xsink receives cancellation during the ASCII scan; nullptr for a non-throwing invariant check
    */
    DLLLOCAL static bool usableAsIs(const QoreEncoding* enc, const char* p, size_t size,
            ExceptionSink* xsink = nullptr) {
        if (enc == QCS_DEFAULT) {
            return true;
        }
        if (!enc->isAsciiCompat() || !QCS_DEFAULT->isAsciiCompat()) {
            return false;
        }
        for (size_t i = 0; i < size;) {
            if (xsink && qore_check_cancel(xsink, "scanning hash key encoding")) {
                return false;
            }
            // Bound each scan between cancellation points without overflowing size_t.
            size_t count = size - i < 100 ? size - i : 100;
            for (size_t j = 0; j < count; ++j, ++i) {
                if (static_cast<unsigned char>(p[i]) & 0x80) {
                    return false;
                }
            }
        }
        return true;
    }

    //! returns true if the given string can be used as a key without conversion
    DLLLOCAL static bool usableAsIs(const QoreString& str, ExceptionSink* xsink = nullptr) {
        return usableAsIs(str.getEncoding(), str.c_str(), str.size(), xsink);
    }

    //! gets the key for a constant key value when code is compiled
    /** @param n the constant key value
        @param name set to the key in the default encoding

        @return 0 for OK, -1 if the value cannot be converted to the default encoding; then the caller must evaluate
        the key at runtime, where the conversion error is raised
    */
    DLLLOCAL static int getConstKey(const QoreValue& n, std::string& name);

private:
    //! the key in the default encoding
    const char* key = "";
    //! the byte length of the key
    size_t len = 0;
    //! a converted or generated key string owned by this object
    std::unique_ptr<QoreString> tmp;
    //! false if conversion or cancellation prevented producing a key
    bool valid = true;
    //! storage for an inline short string
    char buf[8];

    //! converts the value to a key string in the default encoding
    DLLLOCAL void setupSlow(const QoreValue& n, ExceptionSink* xsink);

    //! converts the string to the default encoding
    DLLLOCAL void setupSlow(const QoreString& str, ExceptionSink* xsink);

    DLLLOCAL QoreHashKeyHelper(const QoreHashKeyHelper&) = delete;
    DLLLOCAL QoreHashKeyHelper& operator=(const QoreHashKeyHelper&) = delete;
    DLLLOCAL void* operator new(size_t) = delete;
};

#endif
