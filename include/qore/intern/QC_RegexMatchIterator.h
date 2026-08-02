/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_RegexMatchIterator.h

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

#ifndef _QORE_QC_REGEXMATCHITERATOR_H
#define _QORE_QC_REGEXMATCHITERATOR_H

#include <qore/QoreIteratorBase.h>
#include <qore/intern/QoreRegex.h>

DLLEXPORT extern QoreClass* QC_REGEXMATCHITERATOR;
DLLEXPORT extern qore_classid_t CID_REGEXMATCHITERATOR;

//! Lazy iterator over the matches of a compiled regular expression.
class RegexMatchIterator : public QoreIteratorBase {
public:
    //! Takes references on both arguments and converts the subject to UTF-8 when necessary.
    DLLLOCAL RegexMatchIterator(const QoreStringNode* subject, const QoreRegex* regex,
            ExceptionSink* xsink);

    //! Creates a fresh iterator over the same subject and compiled pattern.
    DLLLOCAL RegexMatchIterator(const RegexMatchIterator& old, ExceptionSink* xsink);

    DLLLOCAL virtual ~RegexMatchIterator();

    DLLLOCAL bool next(ExceptionSink* xsink);
    DLLLOCAL QoreValue getValue(ExceptionSink* xsink);
    DLLLOCAL QoreValue getMatch(ExceptionSink* xsink) const;
    DLLLOCAL int64 getOffset(ExceptionSink* xsink) const;
    DLLLOCAL int64 getLength(ExceptionSink* xsink) const;
    DLLLOCAL int64 getCaptureCount(ExceptionSink* xsink) const;
    DLLLOCAL QoreValue getGroup(int64 index, ExceptionSink* xsink) const;
    DLLLOCAL QoreValue getGroupOffset(int64 index, ExceptionSink* xsink) const;
    DLLLOCAL QoreValue getGroupLength(int64 index, ExceptionSink* xsink) const;
    DLLLOCAL QoreValue getGroups(ExceptionSink* xsink) const;
    DLLLOCAL QoreValue getNamedGroup(const char* name, ExceptionSink* xsink) const;
    DLLLOCAL QoreValue getNamedGroups(ExceptionSink* xsink) const;

    DLLLOCAL bool valid() const {
        return m_valid;
    }

    DLLLOCAL void reset();

    DLLLOCAL virtual const char* getName() const override {
        return "RegexMatchIterator";
    }

    DLLLOCAL virtual const QoreTypeInfo* getElementType() const override;

    QORE_NATIVE_FAST_PATH_NEXT_XSINK()

private:
    DLLLOCAL bool checkValid(ExceptionSink* xsink) const;
    DLLLOCAL QoreValue getCapture(int capture, ExceptionSink* xsink) const;
    DLLLOCAL int namedCapture(const char* name, ExceptionSink* xsink) const;
    DLLLOCAL size_t advanceCodepoint(size_t byte_offset) const;

    QoreStringNode* subject = nullptr;
    QoreRegex* regex = nullptr;
    pcre2_match_data* md = nullptr;

    uint32_t name_count = 0;
    uint32_t name_entry_size = 0;
    PCRE2_SPTR name_table = nullptr;

    size_t next_offset = 0;
    size_t match_offset = 0;
    size_t match_length = 0;
    int match_count = 0;
    bool m_valid = false;
    bool exhausted = false;
    bool utf_validated = false;
};

#endif
