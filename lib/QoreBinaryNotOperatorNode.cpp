/*
    QoreBinaryNotOperatorNode.cpp

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

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
#include "qore/intern/qore_string_private.h"
#include "qore/intern/qore_number_private.h"

QoreString QoreBinaryNotOperatorNode::BinaryNot_str("binary not (~) operator expression");

// if del is true, then the returned QoreString * should be deleted, if false, then it must not be
QoreString *QoreBinaryNotOperatorNode::getAsString(bool& del, int foff, ExceptionSink* xsink) const {
    del = false;
    return &BinaryNot_str;
}

int QoreBinaryNotOperatorNode::getAsString(QoreString& str, int foff, ExceptionSink* xsink) const {
    qore_string_private::get(str)->concat(&BinaryNot_str);
    return 0;
}

QoreValue QoreBinaryNotOperatorNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    ValueEvalOptimizedRefHolder v(exp, xsink);
    if (*xsink)
        return QoreValue();

    return ~v->getAsBigInt();
}

int QoreBinaryNotOperatorNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    // turn off "return value ignored" flags
    QoreParseContextFlagHelper fh(parse_context);
    fh.unsetFlags(PF_RETURN_VALUE_IGNORED);

    assert(exp);

    parse_context.typeInfo = nullptr;
    QoreParseAnalysis operand_analysis;
    int err = 0;
    {
        QoreParseContextAnalysisHelper ah(parse_context);
        err = parse_init_value(exp, parse_context);
        operand_analysis = parse_context.analysis;
    }

    if (!QoreTypeInfo::canConvertToScalar(parse_context.typeInfo)) {
        parse_context.typeInfo->doNonNumericWarning(loc, "the operand of the 'binary not' operator (^) expression " \
            "is ");
    }

    // see if the argument is a constant value, then eval immediately and substitute this node with the result
    if (!err && exp.isValue()) {
        SimpleRefHolder<QoreBinaryNotOperatorNode> del(this);
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
            } else {
                parse_context.typeInfo = val.getFullTypeInfo();
                parse_context.analysis.clear();
                parse_context.analysis.setFlag(QoreParseAnalysis::KnownTypeInfo);
                parse_context.analysis.setFlag(QoreParseAnalysis::DefinitelyAssigned);
                if (!val.isNothing()) {
                    parse_context.analysis.setFlag(QoreParseAnalysis::NeverNothing);
                }
                parse_context.analysis.known_type = parse_context.typeInfo;
            }
        } else {
            // constants not resolved - skip parse-time folding, let runtime handle it
            del.release();
        }
    }

    parse_context.typeInfo = bigIntTypeInfo;
    parse_context.analysis.clear();
    parse_context.analysis.setFlag(QoreParseAnalysis::KnownTypeInfo);
    parse_context.analysis.setFlag(QoreParseAnalysis::NeverNothing);
    parse_context.analysis.known_type = parse_context.typeInfo;
    if (operand_analysis.hasFlag(QoreParseAnalysis::DefinitelyAssigned)) {
        parse_context.analysis.setFlag(QoreParseAnalysis::DefinitelyAssigned);
    }
    return err;
}
