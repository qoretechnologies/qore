/*
    QoreMinusOperatorNode.cpp

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

#include "qore/Qore.h"
#include "qore/intern/qore_program_private.h"

QoreString QoreMinusOperatorNode::minus_str("- operator expression");

QoreValue QoreMinusOperatorNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    ValueEvalOptimizedRefHolder lh(left, xsink);
    if (*xsink) {
        return QoreValue();
    }
    ValueEvalOptimizedRefHolder rh(right, xsink);
    if (*xsink) {
        return QoreValue();
    }

    qore_type_t lt = lh->getType();
    qore_type_t rt = rh->getType();

    if (rt == NT_NOTHING) {
        return lh.takeReferencedValue();
    }

    // issue #3157: try to handle timeout - date and vice-versa specially
    if (check_timeout_date_variant && lt == NT_INT && rt == NT_DATE) {
        int64 secs = lh->getAsBigInt() / 1000;
        int64 ms = lh->getAsBigInt() - (secs * 1000);
        SimpleRefHolder<DateTimeNode> l(DateTimeNode::makeRelativeFromSeconds(secs, static_cast<int>(ms)));
        return l->subtractBy(rh->get<DateTimeNode>());
    }

    if (check_date_timeout_variant && lt == NT_DATE && rt == NT_INT) {
        int64 secs = rh->getAsBigInt() / 1000;
        int64 ms = rh->getAsBigInt() - (secs * 1000);
        DateTime r;
        r.setRelativeDateSeconds(secs, static_cast<int>(ms));
        return lh->get<DateTimeNode>()->subtractBy(r);
    }

    if (lt == NT_DATE || rt == NT_DATE) {
        DateTimeNodeValueHelper l(*lh);
        DateTimeValueHelper r(*rh);
        return l->subtractBy(*r);
    }

    if (lt == NT_NUMBER || rt == NT_NUMBER) {
        QoreNumberNodeHelper l(*lh);
        QoreNumberNodeHelper r(*rh);
        return l->doMinus(**r);
    }

    if (lt == NT_FLOAT || rt == NT_FLOAT) {
        return lh->getAsFloat() - rh->getAsFloat();
    }

    if (lt == NT_INT || rt == NT_INT) {
        return lh->getAsBigInt() - rh->getAsBigInt();
    }

    if (lt == NT_HASH) {
        if (rt == NT_LIST) {
            ReferenceHolder<QoreHashNode> nh(lh->get<const QoreHashNode>()->copy(), xsink);
            ConstListIterator li(rh->get<const QoreListNode>());
            while (li.next()) {
                QoreStringValueHelper val(li.getValue());

                nh->removeKey(*val, xsink);
                if (*xsink)
                    return 0;
            }
            return nh.release();
        }
        if (rt == NT_STRING) {
            ReferenceHolder<QoreHashNode> nh(lh->get<const QoreHashNode>()->copy(), xsink);
            nh->removeKey(rh->get<const QoreStringNode>(), xsink);
            if (*xsink)
                return QoreValue();
            return nh.release();
        }
    }

    return QoreValue();
}

int QoreMinusOperatorNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    // turn off "return value ignored" flags
    QoreParseContextFlagHelper fh(parse_context);
    fh.unsetFlags(PF_RETURN_VALUE_IGNORED);

    assert(!parse_context.typeInfo);

    parse_context.typeInfo = nullptr;
    int err = parse_init_value(left, parse_context);
    const QoreTypeInfo* leftTypeInfo = parse_context.typeInfo;
    parse_context.typeInfo = nullptr;
    if (parse_init_value(right, parse_context) && !err) {
        err = -1;
    }
    const QoreTypeInfo* rightTypeInfo = parse_context.typeInfo;

    // see if both arguments are constants, then eval immediately and substitute this node with the result
    if (!err && right.isValue() && left.isValue()) {
        SimpleRefHolder<QoreMinusOperatorNode> del(this);
        ParseExceptionSink xsink;
        ValueEvalOptimizedRefHolder rv(this, *xsink);
        QoreValue result = rv.takeReferencedValue();
        // only use parse-time folding if we got a valid result
        // (constants may not be fully resolved at parse time, resulting in NOTHING)
        if (!result.isNothing() || **xsink) {
            val = result;
            parse_context.typeInfo = val.getFullTypeInfo();
            return **xsink ? -1 : 0;
        }
        // constants not resolved - skip parse-time folding, let runtime handle it
        del.release();
    }

    // issue #4834: if the rhs is NOTHING, then return the type of the lhs and raise a warning
    if (QoreTypeInfo::isType(rightTypeInfo, NT_NOTHING)) {
        returnTypeInfo = leftTypeInfo;
        QoreStringNode* edesc = new QoreStringNode("subtracting NOTHING from ");
        QoreTypeInfo::getThisType(leftTypeInfo, *edesc);
        edesc->concat(" is a noop; the result of the operation is always the same value on the left-hand side of "
            "the subtraction operator (-)");
        qore_program_private::makeParseWarning(getProgram(), *loc, QP_WARN_INVALID_OPERATION, "INVALID-OPERATION",
            edesc);
    } else {
        // issue #3157: try to handle timeout + date specially
        if (QoreTypeInfo::equal(leftTypeInfo, timeoutTypeInfo)
            && QoreTypeInfo::parseReturns(rightTypeInfo, NT_DATE)) {
            check_timeout_date_variant = true;
        } else if (QoreTypeInfo::equal(rightTypeInfo, timeoutTypeInfo)
            && QoreTypeInfo::parseReturns(leftTypeInfo, NT_DATE)) {
            check_date_timeout_variant = true;
        }

        // if either side is a date, then the return type is date (highest priority)
        if (QoreTypeInfo::isType(leftTypeInfo, NT_DATE)
            || QoreTypeInfo::isType(rightTypeInfo, NT_DATE)) {
            returnTypeInfo = dateTypeInfo;
        // otherwise we have to make sure types are known on both sides of the expression
        } else if (QoreTypeInfo::hasType(leftTypeInfo) && QoreTypeInfo::hasType(rightTypeInfo)) {
            if (QoreTypeInfo::isType(leftTypeInfo, NT_NUMBER)
                    || QoreTypeInfo::isType(rightTypeInfo, NT_NUMBER)) {
                returnTypeInfo = numberTypeInfo;
            } else if (QoreTypeInfo::isType(leftTypeInfo, NT_FLOAT)
                    || QoreTypeInfo::isType(rightTypeInfo, NT_FLOAT)) {
                returnTypeInfo = floatTypeInfo;
            } else if (QoreTypeInfo::isType(leftTypeInfo, NT_INT)
                    || QoreTypeInfo::isType(rightTypeInfo, NT_INT)) {
                returnTypeInfo = bigIntTypeInfo;
            } else if (QoreTypeInfo::isType(leftTypeInfo, NT_HASH)
                && (QoreTypeInfo::isType(rightTypeInfo, NT_STRING)
                    || QoreTypeInfo::isType(rightTypeInfo, NT_LIST))) {
                returnTypeInfo = leftTypeInfo;
            } else if (QoreTypeInfo::isType(leftTypeInfo, NT_OBJECT)
                    && (QoreTypeInfo::isType(rightTypeInfo, NT_STRING)
                    || QoreTypeInfo::isType(rightTypeInfo, NT_LIST))) {
                returnTypeInfo = autoHashTypeInfo;
            } else if (QoreTypeInfo::returnsSingle(leftTypeInfo) && QoreTypeInfo::returnsSingle(rightTypeInfo)) {
                QoreStringNode* edesc = new QoreStringNode("subtracting ");
                QoreTypeInfo::getThisType(rightTypeInfo, *edesc);
                edesc->concat(" from ");
                QoreTypeInfo::getThisType(leftTypeInfo, *edesc);
                edesc->concat(" is an invalid operation and always returns NOTHING");
                qore_program_private::makeParseWarning(getProgram(), *loc, QP_WARN_INVALID_OPERATION,
                    "INVALID-OPERATION", edesc);
                // only return type nothing if both types are available and return a single type
                returnTypeInfo = nothingTypeInfo;
            }
        }
    }

    parse_context.typeInfo = returnTypeInfo;
    return err;
}
