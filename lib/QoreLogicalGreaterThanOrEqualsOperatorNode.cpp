/*
    QoreLogicalGreaterThanOrEqualsOperatorNode.cpp

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

QoreString QoreLogicalGreaterThanOrEqualsOperatorNode::op_str(">= operator expression");

static void set_binary_analysis_ge(QoreParseContext& parse_context,
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

QoreValue QoreLogicalGreaterThanOrEqualsOperatorNode::evalImpl(bool& needs_deref, ExceptionSink *xsink) const {
   if (pfunc)
      return (this->*pfunc)(xsink);

   ValueEvalOptimizedRefHolder lh(left, xsink);
   if (*xsink)
      return QoreValue();
   ValueEvalOptimizedRefHolder rh(right, xsink);
   if (*xsink)
      return QoreValue();

   return doGreaterThanOrEquals(*lh, *rh, xsink);
}

int QoreLogicalGreaterThanOrEqualsOperatorNode::parseInitIntern(const char* name, QoreValue& val,
        QoreParseContext& parse_context) {
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

    parse_context.typeInfo = boolTypeInfo;

    // see if both arguments are constants, then eval immediately and substitute this node with the result
    if (!err && left.isValue() && right.isValue()) {
        SimpleRefHolder<QoreLogicalGreaterThanOrEqualsOperatorNode> del(this);
        ParseExceptionSink xsink;
        val = doGreaterThanOrEquals(left, right, *xsink);
        set_binary_analysis_ge(parse_context, left_analysis, right_analysis);
        return **xsink ? -1 : 0;
    }

    // check for optimizations based on type; but only if types are known on both sides, although the highest priority
    // (float) can be assigned if either side is a float
    if (!QoreTypeInfo::isType(lti, NT_NUMBER) && !QoreTypeInfo::isType(rti, NT_NUMBER)) {
        if (QoreTypeInfo::isType(lti, NT_FLOAT) || QoreTypeInfo::isType(rti, NT_FLOAT))
            pfunc = &QoreLogicalGreaterThanOrEqualsOperatorNode::floatGreaterThanOrEquals;
        else if (QoreTypeInfo::hasType(lti) && QoreTypeInfo::hasType(rti)) {
            if (QoreTypeInfo::isType(lti, NT_INT)) {
                if (QoreTypeInfo::isType(rti, NT_INT))
                    pfunc = &QoreLogicalGreaterThanOrEqualsOperatorNode::bigIntGreaterThanOrEquals;
            }
            // FIXME: check for invalid operation here
        }
    }

    set_binary_analysis_ge(parse_context, left_analysis, right_analysis);
    return err;
}

bool QoreLogicalGreaterThanOrEqualsOperatorNode::floatGreaterThanOrEquals(ExceptionSink *xsink) const {
    ValueEvalOptimizedRefHolder lh(left, xsink);
    if (*xsink) return false;
    ValueEvalOptimizedRefHolder rh(right, xsink);
    if (*xsink) return false;

    return lh->getAsFloat() >= rh->getAsFloat();
}

bool QoreLogicalGreaterThanOrEqualsOperatorNode::bigIntGreaterThanOrEquals(ExceptionSink *xsink) const {
    ValueEvalOptimizedRefHolder lh(left, xsink);
    if (*xsink) return false;
    ValueEvalOptimizedRefHolder rh(right, xsink);
    if (*xsink) return false;

    return lh->getAsBigInt() >= rh->getAsBigInt();
}

bool QoreLogicalGreaterThanOrEqualsOperatorNode::doGreaterThanOrEquals(const QoreValue& lh,
        const QoreValue& rh, ExceptionSink* xsink) {
    // Unwrap TAG_ENUM to base values for soft comparison
    QoreValue l = lh.isEnum() ? lh.getEnumMember()->getValue() : lh;
    QoreValue r = rh.isEnum() ? rh.getEnumMember()->getValue() : rh;

    qore_type_t lt = l.getType();
    qore_type_t rt = r.getType();

    if (lt == NT_NUMBER) {
        switch (rt) {
            case NT_NUMBER:
                return l.get<const QoreNumberNode>()->greaterThanOrEqual(*r.get<const QoreNumberNode>());
            case NT_FLOAT:
                return l.get<const QoreNumberNode>()->greaterThanOrEqual(r.getAsFloat());
            case NT_BOOLEAN:
            case NT_INT:
                return l.get<const QoreNumberNode>()->greaterThanOrEqual(r.getAsBigInt());
            default: {
                ReferenceHolder<QoreNumberNode> rn(new QoreNumberNode(r.getInternalNode()), xsink);
                return l.get<const QoreNumberNode>()->greaterThanOrEqual(**rn);
            }
        }
    }

    if (rt == NT_NUMBER) {
        assert(lt != NT_NUMBER);
        switch (lt) {
            case NT_FLOAT:
                return r.get<const QoreNumberNode>()->lessThanOrEqual(l.getAsFloat());
            case NT_BOOLEAN:
            case NT_INT:
                return r.get<const QoreNumberNode>()->lessThanOrEqual(l.getAsBigInt());
            default: {
                ReferenceHolder<QoreNumberNode> ln(new QoreNumberNode(l.getInternalNode()), xsink);
                return r.get<const QoreNumberNode>()->lessThanOrEqual(**ln);
            }
        }
    }

    if (lt == NT_FLOAT || rt == NT_FLOAT) {
        return l.getAsFloat() >= r.getAsFloat();
    }

    if (lt == NT_INT || rt == NT_INT) {
        return l.getAsBigInt() >= r.getAsBigInt();
    }

    if (lt == NT_STRING || rt == NT_STRING) {
        QoreStringValueHelper ls(l);
        QoreStringValueHelper rs(r, ls->getEncoding(), xsink);
        if (*xsink) {
            return false;
        }
        return ls->compare(*rs) >= 0;
    }

    if (lt == NT_DATE || rt == NT_DATE) {
        DateTimeValueHelper ld(l);
        DateTimeValueHelper rd(r);
        return DateTime::compareDates(*ld, *rd) >= 0;
    }

    return l.getAsFloat() >= r.getAsFloat();
}
