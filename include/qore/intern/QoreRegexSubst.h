/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreRegexSubst.h

    regular expression substitution node definition

    Qore Programming Language

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

#ifndef _QORE_QOREREGEXSUBST_H

#define _QORE_QOREREGEXSUBST_H

#include "qore/intern/QoreRegexBase.h"
#include <sys/types.h>

class QoreRegexSubst : public QoreRegexBase, public QoreReferenceCounter {
public:
    DLLLOCAL QoreRegexSubst();

    // used at run-time
    DLLLOCAL QoreRegexSubst(const QoreString* pstr, int opts, ExceptionSink* xsink);
    // pstr must be in UTF-8 encoding
    DLLLOCAL QoreRegexSubst(const char* pstr, int opts, ExceptionSink* xsink);

    DLLLOCAL ~QoreRegexSubst();

    DLLLOCAL int parseRT(const QoreString* pstr, ExceptionSink* xsink);
    // pstr must be in UTF-8 encoding
    DLLLOCAL int parseRT(const char* pstr, ExceptionSink* xsink);
    DLLLOCAL int parse();
    DLLLOCAL QoreStringNode* exec(const QoreString* target, ExceptionSink* xsink) const;
    DLLLOCAL QoreStringNode* exec(const QoreString* target, const QoreString* newstr, ExceptionSink* xsink) const;
    DLLLOCAL void concatSource(char c);
    DLLLOCAL void concatTarget(char c);
    DLLLOCAL void setGlobal();
    DLLLOCAL QoreString* getPattern() const;

    DLLLOCAL void ref() const {
        ROreference();
    }

    DLLLOCAL void deref() {
        if (ROdereference()) {
            delete this;
        }
    }

    DLLLOCAL QoreRegexSubst* refSelf() const {
        ref();
        return const_cast<QoreRegexSubst*>(this);
    }

    //! Returns the replacement string (for AOT serialization)
    DLLLOCAL const QoreString* getNewStr() const { return newstr; }

    //! Returns the global flag (for AOT serialization)
    DLLLOCAL bool isGlobal() const { return global; }

protected:
private:
    bool global = false;
    QoreString* newstr = nullptr;

    DLLLOCAL static int concat(ExceptionSink& xsink, QoreString* str, PCRE2_SIZE* ovector, int olen, const char* ptr,
            const char* target, int rc);
};

#endif // _QORE_QOREREGEXSUBST_H
