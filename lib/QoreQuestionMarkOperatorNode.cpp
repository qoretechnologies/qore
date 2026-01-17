/*
    QoreQuestionMarkOperatorNode.cpp

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

QoreString QoreQuestionMarkOperatorNode::question_mark_str("question mark (?:) operator expression");

int QoreQuestionMarkOperatorNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    assert(!parse_context.typeInfo);
    int err = parse_init_value(e[0], parse_context);

    if (!QoreTypeInfo::canConvertToScalar(parse_context.typeInfo)
        && parse_check_parse_option(PO_STRICT_BOOLEAN_EVAL)) {
        // FIXME: raise an error here with strict-types
        parse_context.typeInfo->doNonBooleanWarning(loc, "the initial expression with the '?:' operator is ");
    }

    parse_context.typeInfo = nullptr;
    if (parse_init_value(e[1], parse_context) && !err) {
        err = -1;
    }
    const QoreTypeInfo* leftTypeInfo = parse_context.typeInfo;

    parse_context.typeInfo = nullptr;
    if (parse_init_value(e[2], parse_context) && !err) {
        err = -1;
    }
    const QoreTypeInfo* rightTypeInfo = parse_context.typeInfo;

    // see if all arguments are constant values, then eval immediately and substitute this node with the result
    if (!err && e[0].isValue() && e[1].isValue() && e[2].isValue()) {
        SimpleRefHolder<QoreQuestionMarkOperatorNode> del(this);
        ParseExceptionSink xsink;
        ValueEvalOptimizedRefHolder v(this, *xsink);
        assert(!**xsink);
        QoreValue result = v.takeReferencedValue();
        // only use parse-time folding if we got a valid result
        // (constants may not be fully resolved at parse time, resulting in NOTHING)
        if (!result.isNothing()) {
            val = result;
            typeInfo = val.getTypeInfo();
            return 0;
        }
        // constants not resolved - skip parse-time folding, let runtime handle it
        del.release();
    }

    // FIXME: find common type if l != r type
    parse_context.typeInfo = QoreTypeInfo::isOutputIdentical(leftTypeInfo, rightTypeInfo) ? leftTypeInfo : nullptr;
    return err;
}

QoreValue QoreQuestionMarkOperatorNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    RuntimeConfig& rc = rc_get_current_ref();
    return evalImpl(rc, needs_deref, xsink);
}

QoreValue QoreQuestionMarkOperatorNode::evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const {
    ValueEvalRefHolder b(rc, e[0], xsink);
    if (*xsink)
        return QoreValue();

    QoreValue exp = b->getAsBool() ? e[1] : e[2];

    ValueEvalRefHolder rv(rc, exp, xsink);
    if (*xsink)
        return QoreValue();

    return rv.takeValue(needs_deref);
}
