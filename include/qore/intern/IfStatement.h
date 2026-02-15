/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    IfStatement.h

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

#ifndef _QORE_IFSTATEMENT_H

#define _QORE_IFSTATEMENT_H

#include "qore/intern/AbstractStatement.h"

class RuntimeConfig;

class IfStatement : public AbstractStatement {
public:
    DLLLOCAL IfStatement(int start_line, int end_line, QoreValue c, StatementBlock* i, StatementBlock* e = nullptr);
    DLLLOCAL virtual ~IfStatement();

    DLLLOCAL virtual bool hasFinalReturn() const {
        return if_code && if_code->hasFinalReturn() && else_code && else_code->hasFinalReturn();
    }
    DLLLOCAL virtual void parseCommit(QoreProgram* pgm);

    DLLLOCAL QoreValue getCond() const {
        return cond;
    }

    DLLLOCAL StatementBlock* getIfCode() const {
        return if_code;
    }

    DLLLOCAL StatementBlock* getElseCode() const {
        return else_code;
    }

    DLLLOCAL const LVList* getLVList() const {
        return lvars;
    }

private:
    QoreValue cond{};
    StatementBlock* if_code;
    StatementBlock* else_code;
    LVList* lvars = nullptr;

    DLLLOCAL virtual int execImpl(QoreValue& return_value, ExceptionSink* xsink);
    DLLLOCAL virtual int execImpl(RuntimeConfig& rc, QoreValue& return_value, ExceptionSink* xsink);
    DLLLOCAL virtual int parseInitImpl(QoreParseContext& parse_context);
};

#endif
