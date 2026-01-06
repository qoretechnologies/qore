/*
    QoreDivisionOperatorNode.cpp

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

QoreString QoreDivisionOperatorNode::op_str("/ operator expression");

QoreValue QoreDivisionOperatorNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
   if (pfunc)
      return (this->*pfunc)(xsink);

   ValueEvalOptimizedRefHolder lh(left, xsink);
   if (*xsink)
      return QoreValue();
   ValueEvalOptimizedRefHolder rh(right, xsink);
   if (*xsink)
      return QoreValue();

   return doDivision(*lh, *rh, xsink);
}

int QoreDivisionOperatorNode::parseInitIntern(const char* name, QoreValue& val, QoreParseContext& parse_context) {
    // turn off "reference ok" and "return value ignored" flags
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

    // see if both arguments are constants, then eval immediately and substitute this node with the result
    if (!err && right.isValue()) {
        if (left.isValue()) {
            SimpleRefHolder<QoreDivisionOperatorNode> del(this);
            ParseExceptionSink xsink;
            val = doDivision(left, right, *xsink);
            return **xsink ? -1 : 0;
        }
        // check for division by zero here
        if (!right.getAsFloat()) {
            parse_error(*loc, "division by zero found in parse expression");
            return -1;
        }
    }

    // check for optimizations based on type; but only if types are known on both sides, although the highest
    // priority (number) can be assigned if either side is known to have it can be assigned if either side is a float
    if (QoreTypeInfo::isType(lti, NT_NUMBER) || QoreTypeInfo::isType(rti, NT_NUMBER)) {
        parse_context.typeInfo = numberTypeInfo;
    } else if (QoreTypeInfo::hasType(lti) && QoreTypeInfo::hasType(rti)) {
        if (QoreTypeInfo::isType(lti, NT_FLOAT) || QoreTypeInfo::isType(rti, NT_FLOAT)) {
            pfunc = &QoreDivisionOperatorNode::floatDivision;
            parse_context.typeInfo = floatTypeInfo;
        } else if (QoreTypeInfo::isType(lti, NT_INT) && QoreTypeInfo::isType(rti, NT_INT)) {
            pfunc = &QoreDivisionOperatorNode::bigIntDivision;
            parse_context.typeInfo = bigIntTypeInfo;
        } else {
            parse_context.typeInfo = floatTypeInfo;
        }
    }

    return err;
}

QoreValue QoreDivisionOperatorNode::floatDivision(ExceptionSink* xsink) const {
    ValueEvalOptimizedRefHolder lh(left, xsink);
    if (*xsink) return QoreValue();
    ValueEvalOptimizedRefHolder rh(right, xsink);
    if (*xsink) return QoreValue();

    double r = rh->getAsFloat();

    if (!r) {
        xsink->raiseException("DIVISION-BY-ZERO", "division by zero found in floating-point expression");
        return QoreValue();
    }

    return lh->getAsFloat() / r;
}

QoreValue QoreDivisionOperatorNode::bigIntDivision(ExceptionSink* xsink) const {
    ValueEvalOptimizedRefHolder lh(left, xsink);
    if (*xsink) return QoreValue();
    ValueEvalOptimizedRefHolder rh(right, xsink);
    if (*xsink) return QoreValue();

    int64 r = rh->getAsBigInt();

    if (!r) {
        xsink->raiseException("DIVISION-BY-ZERO", "division by zero found in integer expression");
        return QoreValue();
    }

    return lh->getAsBigInt() / r;
}

QoreValue QoreDivisionOperatorNode::doDivision(QoreValue lh, QoreValue rh, ExceptionSink* xsink) {
    qore_type_t lt = lh.getType();
    qore_type_t rt = rh.getType();

    if (lt == NT_NUMBER) {
        switch (rt) {
            case NT_NUMBER:
                return lh.get<const QoreNumberNode>()->doDivideBy(*rh.get<const QoreNumberNode>(), xsink);
            case NT_FLOAT:
                return lh.get<const QoreNumberNode>()->doDivideBy(rh.getAsFloat(), xsink);
            case NT_INT:
                return lh.get<const QoreNumberNode>()->doDivideBy(rh.getAsBigInt(), xsink);
            default: {
                SimpleRefHolder<QoreNumberNode> rn(new QoreNumberNode(rh));
                return lh.get<const QoreNumberNode>()->doDivideBy(**rn, xsink);
            }
        }
    }

    if (rt == NT_NUMBER) {
        assert(lt != NT_NUMBER);
        SimpleRefHolder<QoreNumberNode> ln(new QoreNumberNode(lh));
        return ln->doDivideBy(*rh.get<const QoreNumberNode>(), xsink);
    }

    if (lt == NT_INT && rt == NT_INT) {
        int64 r = rh.getAsBigInt();
        if (!r) {
            xsink->raiseException("DIVISION-BY-ZERO", "division by zero found in integer expression");
            return QoreValue();
        }
        return lh.getAsBigInt() / r;
    }

    double r = rh.getAsFloat();
    if (!r) {
        xsink->raiseException("DIVISION-BY-ZERO", "division by zero found in floating-point expression");
        return QoreValue();
    }
    return lh.getAsFloat() / r;
}
