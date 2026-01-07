/*
    QoreLogicalComparisonOperatorNode.cpp

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

#include <cmath>

QoreString QoreLogicalComparisonOperatorNode::logical_comparison_str("logical comparison (<=>) operator expression");

QoreValue QoreLogicalComparisonOperatorNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    ValueEvalOptimizedRefHolder l(left, xsink);
    if (*xsink)
        return QoreValue();

    ValueEvalOptimizedRefHolder r(right, xsink);
    if (*xsink)
        return QoreValue();

    return doComparison(*l, *r, xsink);
}

int QoreLogicalComparisonOperatorNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    // turn off "return value ignored" flags
    QoreParseContextFlagHelper fh(parse_context);
    fh.unsetFlags(PF_RETURN_VALUE_IGNORED);

    parse_context.typeInfo = nullptr;
    int err = parse_init_value(left, parse_context);
    parse_context.typeInfo = nullptr;
    if (parse_init_value(right, parse_context) && !err) {
        err = -1;
    }

    // FIXME: check args to see if comparisons are possible and issue warnings / errors as appropriate

    // see if both arguments are constant values, then eval immediately and substitute this node with the result
    if (!err && left.isValue() && right.isValue()) {
        SimpleRefHolder<QoreLogicalComparisonOperatorNode> del(this);
        ParseExceptionSink xsink;
        ValueEvalOptimizedRefHolder v(this, *xsink);
        QoreValue result = v.takeReferencedValue();
        // only use parse-time folding if we got a valid result
        // (constants may not be fully resolved at parse time, resulting in NOTHING)
        if (!result.isNothing() || **xsink) {
            val = result;
            if (**xsink) {
                err = -1;
            }
        } else {
            // constants not resolved - skip parse-time folding, let runtime handle it
            del.release();
        }
    }

    parse_context.typeInfo = bigIntTypeInfo;
    return err;
}

int QoreLogicalComparisonOperatorNode::doComparison(const QoreValue& left, const QoreValue& right,
        ExceptionSink* xsink) {
    qore_type_t lt = left.getType();
    qore_type_t rt = right.getType();

    if (lt == NT_STRING) {
        const QoreStringNode* l = left.get<const QoreStringNode>();
        if (rt == NT_STRING) {
            const QoreStringNode* r = right.get<const QoreStringNode>();
            if (l->getEncoding() != r->getEncoding()) {
                QoreStringValueHelper rstr(r, l->getEncoding(), xsink);
                if (*xsink)
                    return 0;
                return l->compare(*rstr);
            }
            return l->compare(r);
        }
        QoreStringValueHelper r(right, l->getEncoding(), xsink);
        if (*xsink)
            return 0;
        return l->compare(*r);
    }
    else if (rt == NT_STRING) {
        const QoreStringNode* r = right.get<const QoreStringNode>();
        QoreStringValueHelper l(left, r->getEncoding(), xsink);
        if (*xsink)
            return 0;
        return l->compare(r);
    }

    if (lt == NT_NUMBER) {
        const QoreNumberNode* l = left.get<const QoreNumberNode>();
        if (l->nan()) {
            xsink->raiseException("NAN-COMPARE-ERROR", "NaN in arbitrary-precision value on left hand side of logical comparison operator");
            return 0;
        }
        switch (rt) {
            case NT_NUMBER: {
                const QoreNumberNode* r = right.get<const QoreNumberNode>();
                if (r->nan()) {
                    xsink->raiseException("NAN-COMPARE-ERROR", "NaN in arbitrary-precision value on right hand side of logical comparison operator");
                    return 0;
                }
                if (l->lessThan(*r))
                    return -1;

                return l->equals(*r) ? 0 : 1;
            }
            case NT_FLOAT: {
                float f = right.getAsFloat();
                if (std::isnan(f)) {
                    xsink->raiseException("NAN-COMPARE-ERROR", "NaN in floating-point value on right hand side of logical comparison operator");
                    return 0;
                }

                if (l->lessThan(f))
                    return -1;

                return l->equals(f) ? 0 : 1;
            }
            case NT_INT:
            case NT_BOOLEAN: {
                int64 r = right.getAsBigInt();

                if (l->lessThan(r))
                    return -1;

                return l->equals(r) ? 0 : 1;
            }
            default: {
                ReferenceHolder<QoreNumberNode> rn(new QoreNumberNode(right), xsink);

                if (l->lessThan(**rn))
                    return -1;

                return l->equals(**rn) ? 0 : 1;
            }
        }
    }

    if (rt == NT_NUMBER) {
        assert(lt != NT_NUMBER);

        const QoreNumberNode* r = right.get<const QoreNumberNode>();
        if (r->nan()) {
            xsink->raiseException("NAN-COMPARE-ERROR", "NaN in arbitrary-precision value on right hand side of logical comparison operator");
            return 0;
        }

        switch (lt) {
            case NT_FLOAT: {
                float l = left.getAsFloat();
                if (std::isnan(l)) {
                    xsink->raiseException("NAN-COMPARE-ERROR", "NaN in floating-point value on left hand side of logical comparison operator");
                    return 0;
                }

                if (r->greaterThan(l))
                    return -1;

                return r->equals(l) ? 0 : 1;
            }
            case NT_INT:
            case NT_BOOLEAN: {
                int64 l = left.getAsBigInt();

                if (r->greaterThan(l))
                    return -1;

                return r->equals(l) ? 0 : 1;
            }
            default: {
                ReferenceHolder<QoreNumberNode> ln(new QoreNumberNode(left), xsink);

                if (ln->lessThan(*r))
                    return -1;

                return ln->equals(*r) ? 0 : 1;
            }
        }
    }

    if (lt == NT_FLOAT || rt == NT_FLOAT) {
        float l = left.getAsFloat();
        if (std::isnan(l)) {
            xsink->raiseException("NAN-COMPARE-ERROR", "NaN in floating-point value on left hand side of logical comparison operator");
            return 0;
        }

        float r = right.getAsFloat();
        if (std::isnan(r)) {
            xsink->raiseException("NAN-COMPARE-ERROR", "NaN in floating-point value on right hand side of logical comparison operator");
            return 0;
        }

        if (l < r)
            return -1;
        return l == r ? 0 : 1;
    }

    if (lt == NT_INT || rt == NT_INT) {
        int64 l = left.getAsBigInt();
        int64 r = right.getAsBigInt();

        if (l < r)
            return -1;
        return l == r ? 0 : 1;
    }

    DateTimeValueHelper l(left);
    DateTimeValueHelper r(right);

    return (int)DateTime::compareDates(*l, *r);
}
