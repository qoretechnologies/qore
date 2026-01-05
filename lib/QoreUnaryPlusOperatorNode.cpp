/*
    QoreUnaryPlusOperatorNode.cpp

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

QoreString QoreUnaryPlusOperatorNode::unaryplus_str("unary plus (+) operator expression");

// if del is true, then the returned QoreString * should be deleted, if false, then it must not be
QoreString *QoreUnaryPlusOperatorNode::getAsString(bool &del, int foff, ExceptionSink *xsink) const {
    del = false;
    return &unaryplus_str;
}

int QoreUnaryPlusOperatorNode::getAsString(QoreString &str, int foff, ExceptionSink *xsink) const {
    str.concat(&unaryplus_str);
    return 0;
}

QoreValue QoreUnaryPlusOperatorNode::evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink*xsink) const {
    ValueEvalOptimizedRefHolder v(rc, exp, xsink);
    if (*xsink)
        return QoreValue();

    switch (v->getType()) {
        case NT_INT:
        case NT_FLOAT:
        case NT_DATE:
        case NT_NUMBER: {
            needs_deref = v.isTemp();
            v.clearTemp();
            return *v;
        }
    }

    return QoreValue(0ll);
}

int QoreUnaryPlusOperatorNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    assert(!parse_context.typeInfo);
    int err = parse_init_value(exp, parse_context);

    // see if the argument is a constant value, then eval immediately and substitute this node with the result
    if (!err && exp.isValue()) {
        SimpleRefHolder<QoreUnaryPlusOperatorNode> del(this);
        ParseExceptionSink xsink;
        RuntimeConfig parse_rc = rc_get_parse_time();
        ValueEvalOptimizedRefHolder v(parse_rc, this, *xsink);
        assert(!**xsink);
        QoreValue result = v.takeReferencedValue();
        // only use parse-time folding if we got a valid result
        // (constants may not be fully resolved at parse time, resulting in NOTHING)
        if (!result.isNothing()) {
            val = result;
            parse_context.typeInfo = val.getFullTypeInfo();
            return 0;
        }
        // constants not resolved - skip parse-time folding, let runtime handle it
        del.release();
    }

    if (QoreTypeInfo::hasType(parse_context.typeInfo)) {
        int tcnt = 0;
        if (QoreTypeInfo::parseAccepts(bigIntTypeInfo, parse_context.typeInfo)) {
            returnTypeInfo = bigIntTypeInfo;
            ++tcnt;
        }

        if (QoreTypeInfo::parseAccepts(floatTypeInfo, parse_context.typeInfo)) {
            returnTypeInfo = floatTypeInfo;
            ++tcnt;
        }

        if (QoreTypeInfo::parseAccepts(numberTypeInfo, parse_context.typeInfo)) {
            returnTypeInfo = numberTypeInfo;
            ++tcnt;
        }

        if (QoreTypeInfo::parseAccepts(dateTypeInfo, parse_context.typeInfo)) {
            returnTypeInfo = dateTypeInfo;
            ++tcnt;
        }

        // if multiple types match, then set to no type (FIXME: can't currently handle multiple possible types)
        if (tcnt > 0) {
            returnTypeInfo = nullptr;
        } else if (!tcnt) {
            // FIXME: raise exceptions with %strict-types
            QoreStringNode* edesc = new QoreStringNode("the expression with the unary plus '+' operator is ");
            QoreTypeInfo::getThisType(parse_context.typeInfo, *edesc);
            edesc->concat(" and so this expression will always return 0; the unary plus '+' operator only returns " \
                "a value with integers, floats, numbers, and relative date/time values");
            qore_program_private::makeParseWarning(getProgram(), *loc, QP_WARN_INVALID_OPERATION, "INVALID-OPERATION",
                edesc);
            returnTypeInfo = bigIntTypeInfo;
        }
    }

    parse_context.typeInfo = returnTypeInfo;
    return err;
}
