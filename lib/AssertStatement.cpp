/*
    AssertStatement.cpp

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
#include "qore/intern/AssertStatement.h"

AssertStatement::AssertStatement(int start_line, int end_line, QoreValue cond, QoreValue msg)
        : AbstractStatement(start_line, end_line), condition(cond), message(msg) {
}

AssertStatement::~AssertStatement() {
    condition.discard(nullptr);
    message.discard(nullptr);
}

int AssertStatement::execImpl(QoreValue& return_value, ExceptionSink* xsink) {
    // Evaluate the condition
    ValueEvalOptimizedRefHolder cond_val(condition, xsink);
    if (*xsink) {
        return 0;
    }

    // Check if condition is true
    if (!cond_val->getAsBool()) {
        // Assertion failed - get the message if provided
        QoreStringNode* msg_str = nullptr;
        if (message) {
            ValueEvalOptimizedRefHolder msg_val(message, xsink);
            if (*xsink) {
                return 0;
            }
            if (msg_val->getType() == NT_STRING) {
                msg_str = msg_val.takeReferencedValue().get<QoreStringNode>();
            } else {
                bool del;
                QoreString* tmp = msg_val->getAsString(del, 0, xsink);
                if (*xsink) {
                    return 0;
                }
                msg_str = del ? static_cast<QoreStringNode*>(tmp) : new QoreStringNode(*tmp);
            }
        } else {
            msg_str = new QoreStringNode("assertion failed");
        }

        xsink->raiseException("ASSERTION-ERROR", msg_str);
    }

    return 0;
}

int AssertStatement::parseInitImpl(QoreParseContext& parse_context) {
    // Turn off top-level flag for statement vars
    QoreParseContextFlagHelper fh(parse_context);
    fh.unsetFlags(PF_TOP_LEVEL);

    int err = 0;

    // Parse the condition
    if (condition) {
        parse_context.typeInfo = nullptr;
        err = parse_init_value(condition, parse_context);
    }

    // Parse the optional message
    if (message) {
        parse_context.typeInfo = nullptr;
        if (parse_init_value(message, parse_context) && !err) {
            err = -1;
        }
    }

    return err;
}
