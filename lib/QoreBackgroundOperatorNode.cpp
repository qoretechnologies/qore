/*
    QoreBackgroundOperatorNode.cpp

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

#include <qore/Qore.h>
#include "qore/intern/qore_program_private.h"

QoreString QoreBackgroundOperatorNode::name("background operator expression");

QoreValue QoreBackgroundOperatorNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    return do_op_background(exp, xsink);
}

int QoreBackgroundOperatorNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    int err = 0;
    if (parse_context.pflag & PF_CONST_EXPRESSION) {
        parseException(*loc, "ILLEGAL-OPERATION", "the background operator may not be used in an expression " \
            "initializing a constant value executed at parse time");
        err = -1;
    }

    // issue #2993: turn on "return value ignored" flag when parsing background expressions
    QoreParseContextFlagHelper fh(parse_context);
    fh.setFlags(PF_BACKGROUND | PF_RETURN_VALUE_IGNORED);
    parse_context.typeInfo = nullptr;
    if (parse_init_value(exp, parse_context) && !err) {
        err = -1;
    }

    parse_context.typeInfo = bigIntTypeInfo;
    return err;
}
