/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_StringSplitIterator.h

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

#ifndef _QORE_QC_STRINGSPLITITERATOR_H
#define _QORE_QC_STRINGSPLITITERATOR_H

#include <qore/QoreIteratorBase.h>

#include <string>

DLLEXPORT extern QoreClass* QC_STRINGSPLITITERATOR;
DLLEXPORT extern qore_classid_t CID_STRINGSPLITITERATOR;

//! Lazy iterator over a QoreStringNode split by a byte-wise delimiter.
/** Each call to next() advances to the next piece; getValue() returns a
    zero-copy QoreStringNodeView over the parent string.  Iteration matches
    eager split() semantics: "a,b,".split(",") yields "a", "b", "".
*/
class StringSplitIterator : public QoreIteratorBase {
public:
    //! Mode flags passed to the ctor.
    enum Mode {
        MODE_DEFAULT         = 0,
        //! When set, each yielded piece has a trailing '\r' byte trimmed so
        //! a "\r\n"-delimited text file iterates identically to a "\n" one.
        STRIP_TRAILING_CR    = 1,
        //! When set, the separator is ignored and each yielded piece is a
        //! single character (codepoint) from the source. ASCII bytes in
        //! ASCII-compatible encodings take a direct fast path; other
        //! characters use QoreString::getUnicodePointFromBytePos().
        SPLIT_CHARS          = 2,
    };

    //! Takes a reference on the source string; the caller retains the ref it passed in.
    DLLLOCAL StringSplitIterator(const QoreStringNode* src, const QoreString* sep, int mode = MODE_DEFAULT);

    //! Copy constructor — returns a fresh iterator over the same source + delimiter, reset to the start.
    DLLLOCAL StringSplitIterator(const StringSplitIterator& old);

    DLLLOCAL virtual ~StringSplitIterator();

    //! Advances to the next piece; returns true if a piece is available, false at end.
    /** The xsink is only used by @ref SPLIT_CHARS mode when advancing through
        a non-UTF-8 source raises an encoding error; byte-wise modes ignore it.
     */
    DLLLOCAL bool next(ExceptionSink* xsink);

    //! Returns the current piece as a zero-copy view onto the source string.
    DLLLOCAL QoreValue getValue(ExceptionSink* xsink);

    //! True if next() has yielded a piece and a further piece may or may not follow.
    DLLLOCAL bool valid() const { return m_valid; }

    //! Resets to the initial state; a subsequent next() yields the first piece again.
    DLLLOCAL void reset();

    DLLLOCAL virtual const char* getName() const override { return "StringSplitIterator"; }

    DLLLOCAL virtual const QoreTypeInfo* getElementType() const override;

    // Native fast-path: next() takes xsink (used by SPLIT_CHARS mode).
    QORE_NATIVE_FAST_PATH_NEXT_XSINK()

private:
    // Source string (ref'd on construction, deref'd in dtor).  Declared as the
    // non-const storage we actually hold; the ctor accepts const and casts
    // away for ref() per the Qore convention that refcount is conceptually
    // const-safe.
    QoreStringNode* src;

    // Cached source buffer metadata for byte and ASCII-char fast paths.
    const char* src_buf = nullptr;
    size_t src_size = 0;
    bool ascii_compat = false;

    // Delimiter copied into our own storage so the caller's QoreString may
    // be freed or reused without affecting us.  Empty delimiter splits the
    // string into one-char pieces (matches Python's str.split("") raising —
    // we instead treat empty as "yield the whole string once" for symmetry
    // with QoreString::split(), which does the same).
    std::string sep;

    // Byte position of the start of the next piece (initialised to 0).
    size_t cursor = 0;

    // Byte range of the current piece (set by next(), consumed by getValue()).
    size_t piece_off = 0;
    size_t piece_len = 0;

    // true after next() has produced a piece; false on construction and
    // after exhaustion.
    bool m_valid = false;

    // true once the final piece has been produced; next() will return false.
    bool exhausted = false;

    // Combination of Mode flag bits.
    int mode = MODE_DEFAULT;
};

#endif
