/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_StringRegexSplitIterator.h

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

#ifndef _QORE_QC_STRINGREGEXSPLITITERATOR_H
#define _QORE_QC_STRINGREGEXSPLITITERATOR_H

#include <qore/QoreIteratorBase.h>
#include <qore/intern/QoreRegex.h>

DLLEXPORT extern QoreClass* QC_STRINGREGEXSPLITITERATOR;
DLLEXPORT extern qore_classid_t CID_STRINGREGEXSPLITITERATOR;

//! Lazy iterator over a QoreStringNode split by a PCRE2 regex.
/** Per-call pcre2_match() from the current offset; getValue() returns a
    zero-copy view over the UTF-8 subject.  Iteration matches eager
    split-by-regex semantics: trailing-empty-piece for a terminal match,
    whole string when the pattern never matches.

    Primary wins over the eager ListIterator wrapper:
      - early termination (select / foldl short-circuiting)
      - O(1) memory: pieces are views, none are materialised up front
      - streaming over very large inputs

    Eager mode still wins on pure throughput when every piece is consumed
    (PCRE2 setup cost is amortised across the whole string).  Callers that
    want throughput should keep using splitLinesRegex's eager variant;
    this class is the functional/streaming counterpart.
*/
class StringRegexSplitIterator : public QoreIteratorBase {
public:
    //! Takes refs on both the source string and the regex.
    /** The source string is converted to UTF-8 if necessary; the converted
        copy is held for the iterator's lifetime and all yielded views are
        carved out of it (matches the encoding convention of
        @ref QoreRegex::extractWithPattern).
    */
    DLLLOCAL StringRegexSplitIterator(const QoreStringNode* src, const QoreRegex* re, bool with_separator,
            ExceptionSink* xsink);

    //! Copy constructor — returns a fresh iterator over the same source + regex, reset to the start.
    DLLLOCAL StringRegexSplitIterator(const StringRegexSplitIterator& old, ExceptionSink* xsink);

    DLLLOCAL virtual ~StringRegexSplitIterator();

    //! Advances to the next piece; returns true if a piece is available, false at end.
    DLLLOCAL bool next(ExceptionSink* xsink);

    //! Returns the current piece as a zero-copy view onto the (UTF-8) subject.
    DLLLOCAL QoreValue getValue(ExceptionSink* xsink);

    //! True if next() has yielded a piece and a further piece may or may not follow.
    DLLLOCAL bool valid() const { return m_valid; }

    //! Resets to the initial state; a subsequent next() yields the first piece again.
    DLLLOCAL void reset();

    DLLLOCAL virtual const char* getName() const override { return "StringRegexSplitIterator"; }

    DLLLOCAL virtual const QoreTypeInfo* getElementType() const override;

    // Native fast-path overrides — see QC_StringSplitIterator.h for the
    // rationale.  Thread-check in check() is replicated inline.
    DLLLOCAL bool supportsNativeIteration() const override { return true; }

    DLLLOCAL bool nativeNext(ExceptionSink* xsink) override {
        if (check(xsink)) {
            return false;
        }
        return next(xsink);
    }

    DLLLOCAL QoreValue nativeGetValue(ExceptionSink* xsink) override {
        if (check(xsink)) {
            return QoreValue();
        }
        return getValue(xsink);
    }

private:
    // UTF-8 subject string; ref'd on construction, deref'd in dtor.
    // May be a fresh copy if the caller passed a non-UTF-8 string, or the
    // caller's object itself if it was already UTF-8.
    QoreStringNode* subject;

    // The regex; ref'd on construction, deref'd in dtor.
    QoreRegex* regex;

    // Owned match data, allocated once from the compiled pattern.
    pcre2_match_data* md = nullptr;

    bool with_separator;

    // Byte offset in subject for the next pcre2_match() call.
    size_t offset = 0;

    // Byte range of the current piece (set by next(), consumed by getValue()).
    size_t piece_off = 0;
    size_t piece_len = 0;

    // true after next() has produced a piece.
    bool m_valid = false;

    // true once the final piece has been produced; next() will return false.
    bool exhausted = false;
};

#endif
