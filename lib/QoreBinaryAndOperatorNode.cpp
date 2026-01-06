/*
    QoreBinaryAndOperatorNode.cpp

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

QoreString QoreBinaryAndOperatorNode::op_str("& (binary and) operator expression");

QoreValue QoreBinaryAndOperatorNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    ValueEvalOptimizedRefHolder lh(left, xsink);
    if (*xsink) return QoreValue();
    ValueEvalOptimizedRefHolder rh(right, xsink);
    if (*xsink) return QoreValue();

    return lh->getAsBigInt() & rh->getAsBigInt();
}

int QoreBinaryAndOperatorNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    // turn off "return value ignored" flags
    QoreParseContextFlagHelper fh(parse_context);
    fh.unsetFlags(PF_RETURN_VALUE_IGNORED);

    parse_context.typeInfo = nullptr;
    int err = parse_init_value(left, parse_context);
    const QoreTypeInfo *lti = parse_context.typeInfo;
    parse_context.typeInfo = nullptr;
    if (parse_init_value(right, parse_context) && !err) {
        err = -1;
    }

    if (!err) {
        // see if any of the arguments cannot be converted to an integer, if so generate a warning
        if (!QoreTypeInfo::canConvertToScalar(lti))
            lti->doNonNumericWarning(loc, "the left hand expression of the 'binary and' operator (&) expression is ");
        if (!QoreTypeInfo::canConvertToScalar(parse_context.typeInfo))
            parse_context.typeInfo->doNonNumericWarning(loc, "the right hand expression of the 'binary and' " \
                "operator (&) expression is ");
    }

    // see if both arguments are constant values, then eval immediately and substitute this node with the result
    if (!err && left.isValue() && right.isValue()) {
        SimpleRefHolder<QoreBinaryAndOperatorNode> del(this);
        ParseExceptionSink xsink;
        ValueEvalOptimizedRefHolder v(this, *xsink);
        assert(!**xsink);
        QoreValue result = v.takeReferencedValue();
        // only use parse-time folding if we got a valid result
        // (constants may not be fully resolved at parse time, resulting in NOTHING)
        if (!result.isNothing() || **xsink) {
            val = result;
            if (**xsink) {
                err = -1;
            }
        } else {
            // constants not resolved - skip parse-time folding, let runtime handle it
            del.release();
        }
    }

    parse_context.typeInfo = bigIntTypeInfo;
    return err;
}
