/*
    QoreLogicalLessThanOperatorNode.cpp

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

QoreString QoreLogicalLessThanOperatorNode::op_str("< operator expression");

QoreValue QoreLogicalLessThanOperatorNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    if (pfunc) {
        return (this->*pfunc)(xsink);
    }

    ValueEvalOptimizedRefHolder lh(left, xsink);
    if (*xsink)
        return QoreValue();
    ValueEvalOptimizedRefHolder rh(right, xsink);
    if (*xsink)
        return QoreValue();

    return doLessThan(*lh, *rh, xsink);
}

int QoreLogicalLessThanOperatorNode::parseInitIntern(const char* name, QoreValue& val, QoreParseContext& parse_context) {
    // turn off "return value ignored" flags
    QoreParseContextFlagHelper fh(parse_context);
    fh.unsetFlags(PF_RETURN_VALUE_IGNORED);

    parse_context.typeInfo = nullptr;
    int err = parse_init_value(left, parse_context);
    const QoreTypeInfo* lti = parse_context.typeInfo;
    parse_context.typeInfo = nullptr;
    if (parse_init_value(right, parse_context) && !err) {
        err = -1;
    }
    const QoreTypeInfo* rti = parse_context.typeInfo;

    parse_context.typeInfo = boolTypeInfo;

    // see if both arguments are constants, then eval immediately and substitute this node with the result
    if (!err && left.isValue() && right.isValue()) {
        SimpleRefHolder<QoreLogicalLessThanOperatorNode> del(this);
        ParseExceptionSink xsink;
        val = doLessThan(left, right, *xsink);
        return **xsink ? -1 : 0;
    }

    // check for optimizations based on type; but only if types are known on both sides, although the highest priority
    // (float) can be assigned if either side is a float
    if (!QoreTypeInfo::isType(lti, NT_NUMBER) && !QoreTypeInfo::isType(rti, NT_NUMBER)) {
        if (QoreTypeInfo::isType(lti, NT_FLOAT) || QoreTypeInfo::isType(rti, NT_FLOAT))
            pfunc = &QoreLogicalLessThanOperatorNode::floatLessThan;
        else if (QoreTypeInfo::hasType(lti) && QoreTypeInfo::hasType(rti)) {
            if (QoreTypeInfo::isType(lti, NT_INT)) {
                if (QoreTypeInfo::isType(rti, NT_INT))
                    pfunc = &QoreLogicalLessThanOperatorNode::bigIntLessThan;
            }
            // FIXME: check for invalid operation here
        }
    }

    return err;
}

bool QoreLogicalLessThanOperatorNode::floatLessThan(ExceptionSink* xsink) const {
    ValueEvalOptimizedRefHolder lh(left, xsink);
    if (*xsink) return false;
    ValueEvalOptimizedRefHolder rh(right, xsink);
    if (*xsink) return false;

    return lh->getAsFloat() < rh->getAsFloat();
}

bool QoreLogicalLessThanOperatorNode::bigIntLessThan(ExceptionSink* xsink) const {
    ValueEvalOptimizedRefHolder lh(left, xsink);
    if (*xsink) return false;
    ValueEvalOptimizedRefHolder rh(right, xsink);
    if (*xsink) return false;

    return lh->getAsBigInt() < rh->getAsBigInt();
}

bool QoreLogicalLessThanOperatorNode::doLessThan(const QoreValue& lh, const QoreValue& rh,
        ExceptionSink* xsink) {
    qore_type_t lt = lh.getType();
    qore_type_t rt = rh.getType();

    if (lt == NT_NUMBER) {
        switch (rt) {
            case NT_NUMBER:
                return lh.get<const QoreNumberNode>()->lessThan(*rh.get<const QoreNumberNode>());
            case NT_FLOAT:
                return lh.get<const QoreNumberNode>()->lessThan(rh.getAsFloat());
            case NT_BOOLEAN:
            case NT_INT:
                return lh.get<const QoreNumberNode>()->lessThan(rh.getAsBigInt());
            default: {
                ReferenceHolder<QoreNumberNode> rn(new QoreNumberNode(rh.getInternalNode()), xsink);
                return lh.get<const QoreNumberNode>()->lessThan(**rn);
            }
        }
    }

    if (rt == NT_NUMBER) {
        assert(lt != NT_NUMBER);
        switch (lt) {
            case NT_FLOAT:
                return rh.get<const QoreNumberNode>()->greaterThan(lh.getAsFloat());
            case NT_BOOLEAN:
            case NT_INT:
                return rh.get<const QoreNumberNode>()->greaterThan(lh.getAsBigInt());
            default: {
                ReferenceHolder<QoreNumberNode> ln(new QoreNumberNode(lh.getInternalNode()), xsink);
                return rh.get<const QoreNumberNode>()->greaterThan(**ln);
            }
        }
    }

    if (lt == NT_FLOAT || rt == NT_FLOAT)
        return lh.getAsFloat() < rh.getAsFloat();

    if (lt == NT_INT || rt == NT_INT)
        return lh.getAsBigInt() < rh.getAsBigInt();

    if (lt == NT_STRING || rt == NT_STRING) {
        QoreStringValueHelper ls(lh);
        QoreStringValueHelper rs(rh, ls->getEncoding(), xsink);
        if (*xsink)
            return false;
        return ls->compare(*rs) < 0;
    }

    if (lt == NT_DATE || rt == NT_DATE) {
        DateTimeValueHelper ld(lh);
        DateTimeValueHelper rd(rh);
        return DateTime::compareDates(*ld, *rd) < 0;
    }

    return lh.getAsFloat() < rh.getAsFloat();
}
