/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreAsciiCompatStringHelper.h

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

#ifndef _QORE_INTERN_QOREASCIICOMPATSTRINGHELPER_H

#define _QORE_INTERN_QOREASCIICOMPATSTRINGHELPER_H

#include <qore/Qore.h>
#include "qore/intern/qore_string_private.h"
#include "qore/intern/qore_encoding_private.h"

#include <cassert>
#include <cstddef>
#include <memory>

#if defined(__GNUC__) || defined(__clang__)
#define QORE_ASCII_COMPAT_NOINLINE __attribute__((noinline))
#else
#define QORE_ASCII_COMPAT_NOINLINE
#endif

//! Provides the text of a string in an ASCII-compatible encoding, for parsing numbers, booleans, and dates
/** Numbers, booleans, and dates are parsed from the bytes of a string with ASCII syntax (digits, signs, \c "true",
    date separators, ...), which is only valid for a string in an ASCII-compatible encoding.  Every conversion of a
    string to a number, boolean, or date gets the text to parse through this class, so that a string in an encoding
    that is not ASCII-compatible (UTF-16, UTF-16LE, UTF-16BE) gives the same result as the same characters in UTF-8.

    A string in an ASCII-compatible encoding is used as it is, without conversion or allocation.  Any other string is
    decoded to UTF-8; a byte order mark at the start of a string in the generic \c "UTF-16" encoding gives the byte
    order and is dropped, and a byte sequence that is not a valid character is decoded as U+FFFD (the Unicode
    replacement character), which is never part of a number, boolean, or date, so parsing stops there as it does at
    any other character that cannot be parsed.  No exception is raised, since the conversion APIs that use this class
    (ex: QoreStringNode::getAsBigInt()) cannot raise one.

    Stack only; it may not be dynamically allocated.

    @code
    QoreAsciiCompatStringHelper str(*this);
    return strtoll(str.c_str(), nullptr, 10);
    @endcode
*/
class QoreAsciiCompatStringHelper {
public:
    //! sets up the text for the given string
    DLLLOCAL explicit QoreAsciiCompatStringHelper(const QoreString& str) {
        const qore_string_private* p = qore_string_private::get(str);
        if (isAsciiCompat(p->encoding)) {
            buf = p->effective_buf();
            len = p->len;
            enc = p->encoding;
            return;
        }
        setupSlow(*p);
    }

    //! calls the given function with the text of the string in an ASCII-compatible encoding and returns its result
    /** For the hot conversion paths: a string in an ASCII-compatible encoding is passed with no helper object, so
        the call costs nothing more than with the string itself.

        @param str the string
        @param f the function to call with the null-terminated text and its byte length
    */
    template <typename F>
    DLLLOCAL static auto withText(const QoreString& str, F&& f) -> decltype(f(static_cast<const char*>(nullptr),
            size_t())) {
        const qore_string_private* p = qore_string_private::get(str);
        if (isAsciiCompat(p->encoding)) {
            return f(p->effective_buf(), p->len);
        }
        return withDecodedText(*p, f);
    }

    //! returns true if text in the given encoding can be parsed as it is
    DLLLOCAL static bool isAsciiCompat(const QoreEncoding* enc) {
        assert(enc);
        return qore_encoding_private::get(*enc)->isAsciiCompat();
    }

    //! returns the null-terminated text in an ASCII-compatible encoding
    DLLLOCAL const char* c_str() const {
        return buf;
    }

    //! returns the byte length of the text
    DLLLOCAL size_t size() const {
        return len;
    }

    //! returns the encoding of the text; always an ASCII-compatible encoding
    DLLLOCAL const QoreEncoding* getEncoding() const {
        return enc;
    }

    //! appends the characters of a string in an encoding that is not ASCII-compatible to a UTF-8 string
    /** @param enc the encoding of the source string; must not be ASCII-compatible
        @param p the source string
        @param size the byte length of the source string
        @param out the UTF-8 string to append to

        A byte order mark at the start of a string in the generic \c "UTF-16" encoding gives the byte order and is
        dropped; an invalid byte sequence is decoded as U+FFFD.
    */
    DLLLOCAL static void decodeToUtf8(const QoreEncoding* enc, const char* p, size_t size, QoreString& out);

private:
    //! calls the given function with the string decoded to UTF-8 and returns its result
    /** Not inlined, so that the fast path in withText() needs no stack frame for the decoded string.
    */
    template <typename F>
    DLLLOCAL static QORE_ASCII_COMPAT_NOINLINE auto withDecodedText(const qore_string_private& str, F& f)
            -> decltype(f(static_cast<const char*>(nullptr), size_t())) {
        QoreString tmp(QCS_UTF8);
        decodeToUtf8(str.encoding, str.effective_buf(), str.len, tmp);
        return f(tmp.c_str(), tmp.size());
    }

    //! the text in an ASCII-compatible encoding
    const char* buf = "";
    //! the byte length of the text
    size_t len = 0;
    //! the encoding of the text
    const QoreEncoding* enc = nullptr;
    //! the decoded text, if the string was not in an ASCII-compatible encoding
    std::unique_ptr<QoreString> tmp;

    //! decodes the string to UTF-8
    DLLLOCAL void setupSlow(const qore_string_private& str);

    DLLLOCAL QoreAsciiCompatStringHelper(const QoreAsciiCompatStringHelper&) = delete;
    DLLLOCAL QoreAsciiCompatStringHelper& operator=(const QoreAsciiCompatStringHelper&) = delete;
    DLLLOCAL void* operator new(size_t) = delete;
};

#endif
