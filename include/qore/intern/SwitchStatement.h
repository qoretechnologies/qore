/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    SwitchStatement.h

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

#ifndef _QORE_SWITCHSTATEMENT_H

#define _QORE_SWITCHSTATEMENT_H

#include "qore/intern/AbstractStatement.h"
#include "qore/intern/StatementBlock.h"

class RuntimeConfig;

class CaseNode {
private:
    DLLLOCAL virtual bool isCaseNodeImpl() const;

public:
    const QoreProgramLocation* loc;
    QoreValue val{};
    StatementBlock* code;
    CaseNode* next = nullptr;
    bool def;

    DLLLOCAL CaseNode(const QoreProgramLocation* loc, QoreValue v, StatementBlock* c, bool def = false)
            : loc(loc), val(v), code(c), def(def) {
    }

    DLLLOCAL virtual ~CaseNode();

    DLLLOCAL virtual bool matches(QoreValue lhs_value, ExceptionSink* xsink) const;

    DLLLOCAL virtual bool isDefault() const {
        return def;
    }

    DLLLOCAL bool isCaseNode() const;
};

class SwitchStatement : public AbstractStatement {
public:
    LVList* lvars = nullptr;

    // start and end line are set later
    DLLLOCAL SwitchStatement(CaseNode* f);
    DLLLOCAL virtual ~SwitchStatement();
    DLLLOCAL void setSwitch(QoreValue s);
    DLLLOCAL void addCase(CaseNode* c);

    DLLLOCAL QoreValue getSwitchExp() const {
        return sexp;
    }

    DLLLOCAL const CaseNode* getCases() const {
        return head;
    }

    // fake it here and let it be checked at runtime
    DLLLOCAL virtual bool hasFinalReturn() const {
        return true;
    }
    DLLLOCAL virtual void parseCommit(QoreProgram* pgm);

private:
    CaseNode* head = nullptr, *tail = nullptr;
    QoreValue sexp{};
    CaseNode* deflt = nullptr;

    DLLLOCAL virtual int parseInitImpl(QoreParseContext& parse_context0);
    DLLLOCAL virtual int execImpl(QoreValue& return_value, ExceptionSink* xsink);
    DLLLOCAL virtual int execImpl(RuntimeConfig& rc, QoreValue& return_value, ExceptionSink* xsink);
};

#endif
