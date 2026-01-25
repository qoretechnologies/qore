/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    CaseNodeRegex.h

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

#ifndef QORE_CASENODEREGEX_H
#define QORE_CASENODEREGEX_H

#include "qore/intern/SwitchStatement.h"
#include "qore/intern/QoreRegex.h"

// Class supporting:
// switch ($a) {
// case ~= /regex_exp/: ..
//
class CaseNodeRegex : public CaseNode {
public:
    DLLLOCAL CaseNodeRegex(const QoreProgramLocation* loc, QoreRegex *m_re, StatementBlock *blk);

    DLLLOCAL virtual ~CaseNodeRegex() {
        if (re) {
            re->deref();
        }
    }

    DLLLOCAL virtual bool matches(QoreValue lhs_value, ExceptionSink* xsink);

    DLLLOCAL QoreRegex* getRegex() const {
        return re;
    }

protected:
    QoreRegex* re;

    DLLLOCAL virtual bool isCaseNodeImpl() const {
        return false;
    }
    DLLLOCAL virtual bool isDefault() const {
        return false;
    }
};

class CaseNodeNegRegex : public CaseNodeRegex {
public:
    DLLLOCAL CaseNodeNegRegex(const QoreProgramLocation* loc, QoreRegex* m_re, StatementBlock* blk) : CaseNodeRegex(loc, m_re, blk) {
    }

    DLLLOCAL virtual bool matches(QoreValue lhs_value, ExceptionSink* xsink);
};

#endif
