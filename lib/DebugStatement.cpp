/*
    DebugStatement.cpp

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

#include <qore/Qore.h>
#include "qore/intern/DebugStatement.h"

DebugStatement::DebugStatement(int start_line, int end_line, QoreValue exp)
        : AbstractStatement(start_line, end_line), expression(exp) {
}

DebugStatement::~DebugStatement() {
    expression.discard(nullptr);
}

int DebugStatement::execImpl(QoreValue& return_value, ExceptionSink* xsink) {
    // Simply evaluate the expression for its side effects (e.g., logging)
    ValueEvalOptimizedRefHolder val(expression, xsink);
    // The result is discarded; we just want the side effects
    return *xsink ? -1 : 0;
}

int DebugStatement::parseInitImpl(QoreParseContext& parse_context) {
    // Turn off top-level flag for statement vars
    QoreParseContextFlagHelper fh(parse_context);
    fh.unsetFlags(PF_TOP_LEVEL);

    int err = 0;

    // Parse the expression
    if (expression) {
        parse_context.typeInfo = nullptr;
        err = parse_init_value(expression, parse_context);
    }

    return err;
}
