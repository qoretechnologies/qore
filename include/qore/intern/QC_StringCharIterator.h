/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_StringCharIterator.h

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

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

#ifndef _QORE_QC_STRINGCHARITERATOR_H
#define _QORE_QC_STRINGCHARITERATOR_H

#include <qore/QoreIteratorBase.h>

DLLEXPORT extern QoreClass* QC_STRINGCHARITERATOR;
DLLEXPORT extern qore_classid_t CID_STRINGCHARITERATOR;
DLLEXPORT extern QoreClass* QC_STRINGITERATOR;
DLLEXPORT extern qore_classid_t CID_STRINGITERATOR;

//! Lazy forward iterator over a string's Unicode codepoints.
/** Each call to next() advances by one character (codepoint), and getValue()
    returns the codepoint at the current position as an integer.  Backed by
    @ref QoreString::getUnicodePointFromBytePos() so any encoding Qore
    supports works correctly (UTF-8 / UTF-16LE / UTF-16BE / Latin-1 / etc).

    Use this for general character iteration where the source string may
    contain non-ASCII codepoints.  ASCII bytes in ASCII-compatible encodings
    take a direct fast path; byte-oriented parsing can still use
    @ref Qore::zzz8stringzzz9::getByte() to avoid iterator/value overhead.
*/
class StringCharIterator : public QoreIteratorBase {
public:
    DLLLOCAL StringCharIterator(const QoreStringNode* s);
    DLLLOCAL StringCharIterator(const StringCharIterator& old);
    DLLLOCAL virtual ~StringCharIterator();

    //! Advances to the next codepoint; returns true if a codepoint is available, false at end.
    /** May raise an encoding-error exception via xsink for malformed input. */
    DLLLOCAL bool next(ExceptionSink* xsink);

    //! Returns the current codepoint as an integer.
    DLLLOCAL QoreValue getValue(ExceptionSink* xsink);

    DLLLOCAL bool valid() const { return m_valid; }

    DLLLOCAL void reset();

    DLLLOCAL virtual const char* getName() const override { return "StringCharIterator"; }

    DLLLOCAL virtual const QoreTypeInfo* getElementType() const override;

    // Native fast-path: next(xsink) + getValue(xsink), thread-checked inline.
    QORE_NATIVE_FAST_PATH_NEXT_XSINK()

private:
    // Source string; ref'd in ctor, deref'd in dtor.
    QoreStringNode* src;

    // Cached source buffer metadata for fast ASCII access.
    const char* src_buf = nullptr;
    size_t src_size = 0;
    bool ascii_compat = false;

    // Byte offset of the next codepoint to decode.
    size_t cursor = 0;

    // Cached current codepoint (set by next(), consumed by getValue()).
    unsigned int current_cp = 0;

    // true after next() has positioned us on a codepoint.
    bool m_valid = false;

    // true once iteration has reached end-of-string.
    bool exhausted = false;
};

//! Lazy forward iterator over a string's Unicode characters as char values.
class StringIterator : public QoreIteratorBase {
public:
    DLLLOCAL StringIterator(const QoreStringNode* s);
    DLLLOCAL StringIterator(const StringIterator& old);
    DLLLOCAL virtual ~StringIterator();

    DLLLOCAL bool next(ExceptionSink* xsink);

    //! Returns the current codepoint as a char value.
    DLLLOCAL QoreValue getValue(ExceptionSink* xsink);

    DLLLOCAL bool valid() const { return m_valid; }

    DLLLOCAL void reset();

    DLLLOCAL virtual const char* getName() const override { return "StringIterator"; }

    DLLLOCAL virtual const QoreTypeInfo* getElementType() const override;

    QORE_NATIVE_FAST_PATH_NEXT_XSINK()

private:
    QoreStringNode* src;
    const char* src_buf = nullptr;
    size_t src_size = 0;
    bool ascii_compat = false;
    size_t cursor = 0;
    unsigned int current_cp = 0;
    bool m_valid = false;
    bool exhausted = false;
};

#endif
