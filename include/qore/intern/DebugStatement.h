/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    DebugStatement.h

    Qore Programming Language

    Copyright (C) 2003 - 2025 Qore Technologies, s.r.o.

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

#ifndef _QORE_DEBUGSTATEMENT_H

#define _QORE_DEBUGSTATEMENT_H

#include "qore/intern/AbstractStatement.h"

class DebugStatement : public AbstractStatement {
public:
    //! Creates a debug statement with an expression to evaluate
    /** @param start_line the starting line number
        @param end_line the ending line number
        @param exp the expression to evaluate (for side effects like logging)
    */
    DLLLOCAL DebugStatement(int start_line, int end_line, QoreValue exp);

    //! Creates a debug statement with a statement block to execute
    /** @param start_line the starting line number
        @param end_line the ending line number
        @param block the statement block to execute
    */
    DLLLOCAL DebugStatement(int start_line, int end_line, class StatementBlock* block);

    DLLLOCAL virtual ~DebugStatement();

    DLLLOCAL virtual void parseCommit(QoreProgram* pgm);

private:
    QoreValue expression{};           //!< The expression to evaluate (if expression form)
    class StatementBlock* code;     //!< The statement block to execute (if block form)

    DLLLOCAL virtual int execImpl(QoreValue& return_value, ExceptionSink* xsink);

    DLLLOCAL virtual int parseInitImpl(QoreParseContext& parse_context);
};

#endif
