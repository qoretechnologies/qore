/*
    QoreLogicalEqualsOperatorNode.cpp

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

QoreString QoreLogicalEqualsOperatorNode::logical_equals_str("logical equals operator expression");
QoreString QoreLogicalNotEqualsOperatorNode::logical_not_equals_str("logical not equals operator expression");

static void set_binary_analysis_eq(QoreParseContext& parse_context,
        const QoreParseAnalysis& left,
        const QoreParseAnalysis& right) {
    parse_context.analysis.clear();
    parse_context.analysis.setFlag(QoreParseAnalysis::KnownTypeInfo);
    parse_context.analysis.setFlag(QoreParseAnalysis::NeverNothing);
    parse_context.analysis.known_type = parse_context.typeInfo;
    if (left.hasFlag(QoreParseAnalysis::DefinitelyAssigned)
            && right.hasFlag(QoreParseAnalysis::DefinitelyAssigned)) {
        parse_context.analysis.setFlag(QoreParseAnalysis::DefinitelyAssigned);
    }
}

QoreValue QoreLogicalEqualsOperatorNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    if (pfunc)
        return (this->*pfunc)(xsink);

    ValueEvalOptimizedRefHolder l(left, xsink);
    if (*xsink)
        return QoreValue();

    ValueEvalOptimizedRefHolder r(right, xsink);
    if (*xsink)
        return QoreValue();

    return softEqual(*l, *r, xsink);
}

int QoreLogicalEqualsOperatorNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    // turn off "return value ignored" flags
    QoreParseContextFlagHelper fh(parse_context);
    fh.unsetFlags(PF_RETURN_VALUE_IGNORED);

    parse_context.typeInfo = nullptr;
    QoreParseAnalysis left_analysis;
    QoreParseAnalysis right_analysis;
    int err = 0;
    {
        QoreParseContextAnalysisHelper ah(parse_context);
        err = parse_init_value(left, parse_context);
        left_analysis = parse_context.analysis;
    }
    const QoreTypeInfo* lti = parse_context.typeInfo;
    parse_context.typeInfo = nullptr;
    {
        QoreParseContextAnalysisHelper ah(parse_context);
        if (parse_init_value(right, parse_context) && !err) {
            err = -1;
        }
        right_analysis = parse_context.analysis;
    }
    const QoreTypeInfo* rti = parse_context.typeInfo;

    // FIXME issue warnings or errors at parse time based on operand types

    parse_context.typeInfo = boolTypeInfo;

    // see if both arguments are constants, then eval immediately and substitute this node with the result
    if (!err && left.isValue() && right.isValue()) {
        SimpleRefHolder<QoreLogicalEqualsOperatorNode> del(this);
        ParseExceptionSink xsink;
        val = softEqual(left, right, *xsink);
        set_binary_analysis_eq(parse_context, left_analysis, right_analysis);
        return **xsink ? -1 : 0;
    }

    // check for optimizations based on type, but only assign if neither side is a string or number (highest priority)
    // and types are known for both operands (if not, QoreTypeInfo::parseReturns(type, NT_STRING) will return a
    // non-zero value
    if (!QoreTypeInfo::parseReturns(lti, NT_STRING) && !QoreTypeInfo::parseReturns(rti, NT_STRING)
        && !QoreTypeInfo::parseReturns(lti, NT_NUMBER) && !QoreTypeInfo::parseReturns(rti, NT_NUMBER)) {
        if (QoreTypeInfo::isType(lti, NT_FLOAT) || QoreTypeInfo::isType(rti, NT_FLOAT))
            pfunc = &QoreLogicalEqualsOperatorNode::floatSoftEqual;
        else if (QoreTypeInfo::isType(lti, NT_INT) || QoreTypeInfo::isType(rti, NT_INT))
            pfunc = &QoreLogicalEqualsOperatorNode::bigIntSoftEqual;
        else if (QoreTypeInfo::isType(lti, NT_BOOLEAN) || QoreTypeInfo::isType(rti, NT_BOOLEAN))
            pfunc = &QoreLogicalEqualsOperatorNode::boolSoftEqual;
    }

    set_binary_analysis_eq(parse_context, left_analysis, right_analysis);
    return err;
}

bool QoreLogicalEqualsOperatorNode::softEqual(const QoreValue& left, const QoreValue& right,
        ExceptionSink *xsink) {
    // Unwrap TAG_ENUM to base values for soft comparison
    QoreValue l = left.isEnum() ? left.getEnumMember()->getValue() : left;
    QoreValue r = right.isEnum() ? right.getEnumMember()->getValue() : right;

    qore_type_t lt = l.getType();
    qore_type_t rt = r.getType();

    //printf("QoreLogicalEqualsOperatorNode::softEqual() lt: %d rt: %d (%d %s)\n", lt, rt, right.type,
    //  right.getTypeName());

    if (lt == NT_STRING) {
        const QoreStringNode* ls = l.get<const QoreStringNode>();
        if (rt == NT_STRING) {
            return ls->equalSoft(*r.get<const QoreStringNode>(), xsink);
        }
        QoreStringValueHelper rs(r, ls->getEncoding(), xsink);
        if (*xsink) {
            return false;
        }
        return ls->equal(*rs);
    }

    if (rt == NT_STRING) {
        const QoreStringNode* rs = r.get<const QoreStringNode>();
        QoreStringValueHelper ls(l, rs->getEncoding(), xsink);
        if (*xsink) {
            return false;
        }
        return ls->equal(*rs);
    }

    if (lt == NT_NUMBER) {
        switch (rt) {
            case NT_NUMBER:
                return l.get<const QoreNumberNode>()->equals(*r.get<const QoreNumberNode>());
            case NT_FLOAT:
                return l.get<const QoreNumberNode>()->equals(r.getAsFloat());
            case NT_INT:
            case NT_BOOLEAN:
                return l.get<const QoreNumberNode>()->equals(r.getAsBigInt());
            default: {
                ReferenceHolder<QoreNumberNode> rn(new QoreNumberNode(r.getInternalNode()), xsink);
                return l.get<const QoreNumberNode>()->equals(**rn);
            }
        }
    }

    if (rt == NT_NUMBER) {
        assert(lt != NT_NUMBER);
        switch (lt) {
            case NT_FLOAT:
                return r.get<const QoreNumberNode>()->equals(l.getAsFloat());
            case NT_INT:
            case NT_BOOLEAN:
                return r.get<const QoreNumberNode>()->equals(l.getAsBigInt());
            default: {
                ReferenceHolder<QoreNumberNode> ln(new QoreNumberNode(l.getInternalNode()), xsink);
                return r.get<const QoreNumberNode>()->equals(**ln);
            }
        }
    }

    if (lt == NT_FLOAT || rt == NT_FLOAT) {
        return l.getAsFloat() == r.getAsFloat();
    }

    if (lt == NT_INT || rt == NT_INT) {
        return l.getAsBigInt() == r.getAsBigInt();
    }

    if (lt == NT_BOOLEAN || rt == NT_BOOLEAN) {
        return l.getAsBool() == r.getAsBool();
    }

    if (lt == NT_DATE || rt == NT_DATE) {
        DateTimeValueHelper ld(l);
        DateTimeValueHelper rd(r);
        return ld->isEqual(*rd);
    }

    const AbstractQoreNode* ln = l.getInternalNode();
    if (!ln) {
        ln = &Nothing;
    }
    const AbstractQoreNode* rn = r.getInternalNode();
    if (!rn) {
        rn = &Nothing;
    }

    return ln->is_equal_soft(rn, xsink);
}

bool QoreLogicalEqualsOperatorNode::floatSoftEqual(ExceptionSink *xsink) const {
    ValueEvalOptimizedRefHolder lh(left, xsink);
    if (*xsink) return false;
    ValueEvalOptimizedRefHolder rh(right, xsink);
    if (*xsink) return false;

    return lh->getAsFloat() == rh->getAsFloat();
}

bool QoreLogicalEqualsOperatorNode::bigIntSoftEqual(ExceptionSink *xsink) const {
    ValueEvalOptimizedRefHolder lh(left, xsink);
    if (*xsink) return false;
    ValueEvalOptimizedRefHolder rh(right, xsink);
    if (*xsink) return false;

    return lh->getAsBigInt() == rh->getAsBigInt();
}

bool QoreLogicalEqualsOperatorNode::boolSoftEqual(ExceptionSink *xsink) const {
    ValueEvalOptimizedRefHolder lh(left, xsink);
    if (*xsink) return false;
    ValueEvalOptimizedRefHolder rh(right, xsink);
    if (*xsink) return false;

    return lh->getAsBool() == rh->getAsBool();
}
