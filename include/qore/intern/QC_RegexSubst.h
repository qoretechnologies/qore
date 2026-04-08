/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_RegexSubst.h

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

#ifndef _QORE_CLASS_REGEXSUBST_H

#define _QORE_CLASS_REGEXSUBST_H

#include "qore/intern/QoreRegexSubst.h"

DLLEXPORT extern qore_classid_t CID_REGEXSUBST;
DLLLOCAL extern QoreClass* QC_REGEXSUBST;
DLLLOCAL QoreClass* initRegexSubstClass(QoreNamespace& ns);

class QoreRegexSubstClass : public AbstractPrivateData {
protected:
    QoreRegexSubst* regex;
    QoreString pattern;
    QoreString replacement;
    int64 options;

    DLLLOCAL virtual ~QoreRegexSubstClass() {
        if (regex) {
            regex->deref();
        }
    }

public:
    DLLLOCAL QoreRegexSubstClass(const QoreString& pat, const QoreString& repl, int64 opts, ExceptionSink* xsink)
            : pattern(pat), replacement(repl), options(opts) {
        // Filter out the global flag since it's > 32 bits and not a PCRE option
        int pcre_opts = (int)(opts & ~QRE_GLOBAL);
        regex = new QoreRegexSubst(&pat, pcre_opts, xsink);
        if (*xsink) {
            regex->deref();
            regex = nullptr;
            return;
        }
        // Set global flag if requested
        if (opts & QRE_GLOBAL) {
            regex->setGlobal();
        }
    }

    DLLLOCAL QoreRegexSubstClass(const QoreRegexSubstClass& old)
            : pattern(old.pattern), replacement(old.replacement), options(old.options) {
        regex = old.regex ? old.regex->refSelf() : nullptr;
    }

    DLLLOCAL QoreStringNode* subst(const QoreString* target, ExceptionSink* xsink) const {
        return regex ? regex->exec(target, &replacement, xsink) : nullptr;
    }

    DLLLOCAL QoreStringNode* subst(const QoreString* target, const QoreString* repl, ExceptionSink* xsink) const {
        return regex ? regex->exec(target, repl, xsink) : nullptr;
    }

    DLLLOCAL QoreStringNode* substCallback(const QoreString* target,
            const ResolvedCallReferenceNode* callback, ExceptionSink* xsink) const {
        return regex ? regex->execWithCallback(target, callback, xsink) : nullptr;
    }

    DLLLOCAL const QoreString* getPattern() const {
        return &pattern;
    }

    DLLLOCAL const QoreString* getReplacement() const {
        return &replacement;
    }

    DLLLOCAL int64 getOptions() const {
        return options;
    }
};

#endif // _QORE_CLASS_REGEXSUBST_H
