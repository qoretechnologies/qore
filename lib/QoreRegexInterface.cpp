/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreRegexInterface.cpp

    Copyright (C) 2003 - 2024 Qore Technologies, s.r.o.

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

#include "qore/Qore.h"
#include "qore/intern/QoreRegex.h"
#include "qore/intern/QoreRegexSubst.h"

class qore_regex_private : public QoreRegex {
public:
    DLLLOCAL qore_regex_private(const QoreString& str, int64 options, ExceptionSink* xsink)
            : QoreRegex(str, options, xsink) {
    }

    DLLLOCAL qore_regex_private(const char* str, int64 options, ExceptionSink* xsink)
            : QoreRegex(str, options, xsink) {
    }
};

class qore_regex_subst_private : public QoreRegexSubst {
public:
    DLLLOCAL qore_regex_subst_private(const QoreString& str, int64 options, ExceptionSink* xsink)
            : QoreRegexSubst(&str, options, xsink) {
    }

    DLLLOCAL qore_regex_subst_private(const char* str, int64 options, ExceptionSink* xsink)
            : QoreRegexSubst(str, options, xsink) {
    }
};

QoreRegexInterface::QoreRegexInterface(ExceptionSink* xsink, const QoreString& pattern)
        : QoreRegexInterface(xsink, pattern, 0) {
}

QoreRegexInterface::QoreRegexInterface(ExceptionSink* xsink, const QoreString& pattern, int64 opts)
        : priv(new qore_regex_private(pattern, opts, xsink)) {
}

QoreRegexInterface::QoreRegexInterface(ExceptionSink* xsink, const char* pattern)
        : QoreRegexInterface(xsink, pattern, 0) {
}

QoreRegexInterface::QoreRegexInterface(ExceptionSink* xsink, const char* pattern, int64 opts)
        : priv(new qore_regex_private(pattern, opts, xsink)) {
}

QoreRegexInterface::~QoreRegexInterface() {
    delete priv;
}

bool QoreRegexInterface::match(const QoreString& target, ExceptionSink* xsink) const {
    return priv->exec(&target, xsink);
}

QoreListNode* QoreRegexInterface::extractSubstrings(const QoreString& target, ExceptionSink* xsink) const {
    return priv->extractSubstrings(&target, xsink);
}

QoreRegexSubstInterface::QoreRegexSubstInterface(ExceptionSink* xsink, const QoreString& pattern)
        : QoreRegexSubstInterface(xsink, pattern, 0) {
}

QoreRegexSubstInterface::QoreRegexSubstInterface(ExceptionSink* xsink, const QoreString& pattern, int64 opts)
        : priv(new qore_regex_subst_private(pattern, opts, xsink)) {
    if (opts & QRE_GLOBAL) {
        priv->setGlobal();
    }
}

QoreRegexSubstInterface::QoreRegexSubstInterface(ExceptionSink* xsink, const char* pattern)
        : QoreRegexSubstInterface(xsink, pattern, 0) {
}

QoreRegexSubstInterface::QoreRegexSubstInterface(ExceptionSink* xsink, const char* pattern, int64 opts)
        : priv(new qore_regex_subst_private(pattern, opts, xsink)) {
    if (opts & QRE_GLOBAL) {
        priv->setGlobal();
    }
}

QoreRegexSubstInterface::~QoreRegexSubstInterface() {
    delete priv;
}

void QoreRegexSubstInterface::setGlobal() {
    priv->setGlobal();
}

QoreStringNode* QoreRegexSubstInterface::subst(const QoreString& target, const QoreString& newstr,
        ExceptionSink* xsink) const {
    return priv->exec(&target, &newstr, xsink);
}

// replaces with an empty string
QoreStringNode* QoreRegexSubstInterface::subst(const QoreString& target, ExceptionSink* xsink) const {
    return priv->exec(&target, xsink);
}