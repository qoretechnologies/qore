/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_RegexExtract.h

    Qore Programming Language

    Copyright (C) 2025 - 2026 Qore Technologies, s.r.o.

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

#ifndef _QORE_CLASS_REGEXEXTRACT_H

#define _QORE_CLASS_REGEXEXTRACT_H

#include "qore/intern/QoreRegex.h"

DLLEXPORT extern qore_classid_t CID_REGEXEXTRACT;
DLLLOCAL extern QoreClass* QC_REGEXEXTRACT;
DLLLOCAL QoreClass* initRegexExtractClass(QoreNamespace& ns);

class QoreRegexExtractClass : public AbstractPrivateData {
protected:
    QoreRegex* regex;
    QoreString pattern;
    int64 options;

    DLLLOCAL virtual ~QoreRegexExtractClass() {
        if (regex) {
            regex->deref();
        }
    }

public:
    DLLLOCAL QoreRegexExtractClass(const QoreString& pat, int64 opts, ExceptionSink* xsink)
            : pattern(pat), options(opts) {
        regex = new QoreRegex(pat, opts, xsink);
        if (*xsink) {
            regex->deref();
            regex = nullptr;
        }
    }

    DLLLOCAL QoreRegexExtractClass(const QoreRegexExtractClass& old) : pattern(old.pattern), options(old.options) {
        regex = old.regex ? old.regex->refSelf() : nullptr;
    }

    DLLLOCAL QoreListNode* extract(const QoreString* target, ExceptionSink* xsink) const {
        return regex ? regex->extractSubstrings(target, xsink) : nullptr;
    }

    DLLLOCAL bool match(const QoreString* target, ExceptionSink* xsink) const {
        return regex ? regex->exec(target, xsink) : false;
    }

    DLLLOCAL int64 countMatches(const QoreString* target, ExceptionSink* xsink) const {
        return regex ? regex->countMatches(target, xsink) : 0;
    }

    DLLLOCAL QoreValue extractNamedGroups(const QoreString* target, ExceptionSink* xsink) const {
        return regex ? regex->extractNamedGroups(target, xsink) : QoreValue();
    }

    DLLLOCAL QoreListNode* extractDetailed(const QoreString* target, ExceptionSink* xsink) const {
        return regex ? regex->extractDetailed(target, xsink) : nullptr;
    }

    DLLLOCAL const QoreString* getPattern() const {
        return &pattern;
    }

    DLLLOCAL int64 getOptions() const {
        return options;
    }
};

#endif // _QORE_CLASS_REGEXEXTRACT_H
