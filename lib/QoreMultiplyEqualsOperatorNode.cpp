/*
    QoreMultiplyEqualsOperatorNode.cpp

    Qore Programming Language

    Copyright (C) 2003 - 2024 Qore Techologies, s.r.o.

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

QoreString QoreMultiplyEqualsOperatorNode::op_str("*= operator expression");

int QoreMultiplyEqualsOperatorNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
   return parseInitIntern(op_str.c_str(), parse_context);
}

QoreValue QoreMultiplyEqualsOperatorNode::evalImpl(bool& needs_deref, ExceptionSink *xsink) const {
    ValueEvalOptimizedRefHolder res(right, xsink);
    if (*xsink)
        return QoreValue();

    // get ptr to current value (lvalue is locked for the scope of the LValueHelper object)
    LValueHelper v(left, xsink);
    if (!v)
        return QoreValue();

    // is either side a number?
    if (v.getType() == NT_NUMBER || res->getType() == NT_NUMBER) {
        v.multiplyEqualsNumber(*res, "<*= operator>");
        if (ref_rv && !*xsink)
            return v.getReferencedValue();

        return QoreValue();
    }

    // is either side a float?
    if (v.getType() == NT_FLOAT || res->getType() == NT_FLOAT)
        return v.multiplyEqualsFloat(res->getAsFloat(), "<*= operator>");

    // get operand
    int64 y = res->getAsBigInt();

    // do integer multiply equals
    if (!v.getAsBigInt() || !y) {
        // no need to multiply something by zero
        v.assign(0ll);
        return 0ll;
    }

    return v.multiplyEqualsBigInt(res->getAsBigInt(), "<*= operator>");
}
