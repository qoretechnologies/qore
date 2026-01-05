/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    ContextStatement.h

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

#ifndef _QORE_CONTEXTSTATEMENT_H

#define _QORE_CONTEXTSTATEMENT_H

#include "qore/intern/AbstractStatement.h"

#include <qore/safe_dslist>

// forward references
class StatementBlock;
class LVList;

// context mod types defined in Context.h

class ContextMod {
public:
    int type;
    QoreValue exp{};

    DLLLOCAL ContextMod(int t, QoreValue e) : type(t), exp(e) {
    }

    DLLLOCAL ~ContextMod() {
        exp.discard(nullptr);
    }
};

typedef safe_dslist<ContextMod*> cxtmod_list_t;

class ContextModList : public cxtmod_list_t {
public:
    DLLLOCAL ContextModList(ContextMod *cm);
    DLLLOCAL ~ContextModList();
    DLLLOCAL void addContextMod(ContextMod *cm);
};

class ContextStatement : public AbstractStatement {
public:
    char *name;
    QoreValue exp, where_exp, sort_ascending, sort_descending;
    StatementBlock* code;
    LVList* lvars = nullptr;

    DLLLOCAL ContextStatement(int start_line, int end_line, char* n, QoreValue expr, ContextModList* cm,
            StatementBlock* cd);
    DLLLOCAL virtual ~ContextStatement();

    // faked here; checked at runtime
    DLLLOCAL virtual bool hasFinalReturn() const {
        return false;
    }

    DLLLOCAL virtual void parseCommit(QoreProgram* pgm);

protected:
    DLLLOCAL virtual int execImpl(RuntimeConfig& rc, QoreValue& return_value, ExceptionSink* xsink);
    DLLLOCAL virtual int parseInitImpl(QoreParseContext& parse_context);
};

#endif
