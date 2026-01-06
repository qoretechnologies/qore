/*
    QoreLogicalAbsoluteEqualsOperatorNode.cpp

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

QoreString QoreLogicalAbsoluteEqualsOperatorNode::logical_absolute_equals_str("logical absolute equals (===) " \
    "operator expression");
QoreString QoreLogicalAbsoluteNotEqualsOperatorNode::logical_absolute_not_equals_str("logical absolute not equals " \
    "(!==) operator expression");

QoreValue QoreLogicalAbsoluteEqualsOperatorNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    ValueEvalOptimizedRefHolder l(left, xsink);
    if (*xsink)
        return QoreValue();

    ValueEvalOptimizedRefHolder r(right, xsink);
    if (*xsink)
        return QoreValue();

    return hardEqual(*l, *r, xsink);
}

int QoreLogicalAbsoluteEqualsOperatorNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    parse_context.typeInfo = nullptr;
    int err = parse_init_value(left, parse_context);
    parse_context.typeInfo = nullptr;
    if (parse_init_value(right, parse_context) && !err) {
        err = -1;
    }

    // FIXME: check type compatibility and issue a warning if the types can never be or are always equal

    parse_context.typeInfo = boolTypeInfo;

    // see if both arguments are constants, then eval immediately and substitute this node with the result
    if (!err && left.isValue() && right.isValue()) {
        SimpleRefHolder<QoreLogicalAbsoluteEqualsOperatorNode> del(this);
        val = hardEqual(left, right, nullptr);
        return 0;
    }

    return err;
}

bool QoreLogicalAbsoluteEqualsOperatorNode::hardEqual(const QoreValue& left, const QoreValue& right,
        ExceptionSink *xsink) {
    qore_type_t lt = left.getType();
    qore_type_t rt = right.getType();

    if (lt != rt) {
        return false;
    }

    return left.isEqualHard(right);
}
