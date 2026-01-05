/*
    QoreShiftRightOperatorNode.cpp

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

QoreString QoreShiftRightOperatorNode::op_str(">> (shift right) operator expression");

QoreValue QoreShiftRightOperatorNode::evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const {
    ValueEvalOptimizedRefHolder lh(rc, left, xsink);
    if (*xsink) return QoreValue();
    ValueEvalOptimizedRefHolder rh(rc, right, xsink);
    if (*xsink) return QoreValue();

    return lh->getAsBigInt() >> rh->getAsBigInt();
}

int QoreShiftRightOperatorNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    // turn off "return value ignored" flags
    QoreParseContextFlagHelper fh(parse_context);
    fh.unsetFlags(PF_RETURN_VALUE_IGNORED);

    assert(!parse_context.typeInfo);
    int err = parse_init_value(left, parse_context);
    const QoreTypeInfo* lti = parse_context.typeInfo;
    parse_context.typeInfo = nullptr;
    if (parse_init_value(right, parse_context) && !err) {
        err = -1;
    }
    const QoreTypeInfo* rti = parse_context.typeInfo;

    // see if any of the arguments cannot be converted to an integer, if so generate a warning
    // FIXME: raise exceptions with %strict-types
    if (!QoreTypeInfo::canConvertToScalar(lti)) {
        lti->doNonNumericWarning(loc, "the left hand expression of the 'shift right' operator (>>) expression is ");
    }
    if (!QoreTypeInfo::canConvertToScalar(rti)) {
        QoreStringNode* desc = new QoreStringNode("the right hand side of the 'shift right' (>>) expression is ");
        QoreTypeInfo::getThisType(rti, *desc);
        desc->concat(", which cannot be converted to an integer, therefore the entire expression will always " \
            "return the integer value of the left hand side");
        qore_program_private::makeParseWarning(getProgram(), *loc, QP_WARN_INVALID_OPERATION, "INVALID-OPERATION",
            desc);
    }

    // see if both arguments are constant values, then eval immediately and substitute this node with the result
    if (!err && left.isValue() && right.isValue()) {
        SimpleRefHolder<QoreShiftRightOperatorNode> del(this);
        ParseExceptionSink xsink;
        RuntimeConfig parse_rc = rc_get_parse_time();
        ValueEvalOptimizedRefHolder v(parse_rc, this, *xsink);
        assert(!**xsink);
        QoreValue result = v.takeReferencedValue();
        // only use parse-time folding if we got a valid result
        // (constants may not be fully resolved at parse time, resulting in NOTHING)
        if (!result.isNothing()) {
            val = result;
        } else {
            // constants not resolved - skip parse-time folding, let runtime handle it
            del.release();
        }
    }

    parse_context.typeInfo = bigIntTypeInfo;
    return err;
}
