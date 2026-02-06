/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    CaseNodeWithOperator.h

    Qore Programming Language

    Copyright (C) 2006 - 2024 Qore Technologies, s.r.o.

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

#ifndef QORE_CASENODEWITHOPERATOR_H
#define QORE_CASENODEWITHOPERATOR_H

#include "qore/intern/SwitchStatement.h"

// Class supporting:
// switch ($a) {
// case > 1: ..
// case < 0.0: ...
// case >= 1.0: ...
// case <= -1: ...
//
class CaseNodeWithOperator : public CaseNode {
private:
    DLLLOCAL virtual bool isCaseNodeImpl() const;

private:
    op_log_func_t op_func;

public:
    DLLLOCAL CaseNodeWithOperator(const QoreProgramLocation* loc, QoreValue v, StatementBlock* c, op_log_func_t op);

    DLLLOCAL ~CaseNodeWithOperator() {}

    DLLLOCAL virtual bool matches(QoreValue lhs_value, ExceptionSink* xsink) const;

    DLLLOCAL op_log_func_t getOpFunc() const {
        return op_func;
    }
};

#endif
