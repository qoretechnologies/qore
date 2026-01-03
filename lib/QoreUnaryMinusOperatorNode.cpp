/*
    QoreUnaryMinusOperatorNode.cpp

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
#include "qore/intern/qore_number_private.h"
#include "qore/intern/qore_program_private.h"

QoreString QoreUnaryMinusOperatorNode::unaryminus_str("unary minus (-) operator expression");

// if del is true, then the returned QoreString * should be deleted, if false, then it must not be
QoreString *QoreUnaryMinusOperatorNode::getAsString(bool& del, int foff, ExceptionSink* xsink) const {
    del = false;
    return &unaryminus_str;
}

int QoreUnaryMinusOperatorNode::getAsString(QoreString& str, int foff, ExceptionSink* xsink) const {
    str.concat(&unaryminus_str);
    return 0;
}

QoreValue QoreUnaryMinusOperatorNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    ValueEvalOptimizedRefHolder v(exp, xsink);
    if (*xsink)
        return QoreValue();

    switch (v->getType()) {
        case NT_NUMBER: {
            needs_deref = true;
            return static_cast<QoreNumberNode *>(v->getInternalNode())->negate();
        }

        case NT_FLOAT: {
            return -(v->getAsFloat());
        }

        case NT_DATE: {
            return v->get<const DateTimeNode>()->unaryMinus();
        }

        case NT_INT: {
            return -(v->getAsBigInt());
        }
    }

    return QoreValue(0ll);
}

int QoreUnaryMinusOperatorNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    assert(!parse_context.typeInfo);
    int err = parse_init_value(exp, parse_context);

    // see if the argument is a constant value, then eval immediately and substitute this node with the result
    if (!err & exp.isValue()) {
        SimpleRefHolder<QoreUnaryMinusOperatorNode> del(this);
        ParseExceptionSink xsink;
        ValueEvalOptimizedRefHolder v(this, *xsink);
        assert(!**xsink);
        //printd(5, "QoreUnaryMinusOperatorNode::parseInitImpl() parse type: '%s' runtype: '%s'\n", QoreTypeInfo::getName(typeInfo), v->getTypeName());
        val = v.takeReferencedValue();
        parse_context.typeInfo = val.getFullTypeInfo();
        return 0;
    }

    if (QoreTypeInfo::hasType(parse_context.typeInfo)) {
        qore_type_t t = QoreTypeInfo::getSingleReturnType(parse_context.typeInfo);
        switch (t) {
            case NT_NUMBER:
                parse_context.typeInfo = numberTypeInfo;
                break;
            case NT_FLOAT:
                parse_context.typeInfo = floatTypeInfo;
                break;
            case NT_INT:
                parse_context.typeInfo = bigIntTypeInfo;
                break;
            case NT_DATE:
                parse_context.typeInfo = dateTypeInfo;
                break;
            default:
                if (!QoreTypeInfo::parseReturns(parse_context.typeInfo, NT_NUMBER)
                    && !QoreTypeInfo::parseReturns(parse_context.typeInfo, NT_FLOAT)
                    && !QoreTypeInfo::parseReturns(parse_context.typeInfo, NT_INT)
                    && !QoreTypeInfo::parseReturns(parse_context.typeInfo, NT_DATE)) {
                    // FIXME: raise exceptions with %strict-types
                    QoreStringNode* edesc = new QoreStringNode("the expression with the unary minus '-' operator " \
                        "is ");
                    QoreTypeInfo::getThisType(parse_context.typeInfo, *edesc);
                    edesc->concat(" and so this expression will always return 0; the unary minus '-' operator only " \
                        "returns a value with integers, floats, numbers, and relative date/time values");
                    qore_program_private::makeParseWarning(getProgram(), *loc, QP_WARN_INVALID_OPERATION, "INVALID-" \
                        "OPERATION", edesc);
                    parse_context.typeInfo = bigIntTypeInfo;
                }
                break;
        }
    }

    returnTypeInfo = parse_context.typeInfo;
    return err;
}
