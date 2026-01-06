/*
    QoreRangeOperatorNode.cpp

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
#include "qore/intern/qore_program_private.h"

QoreString QoreRangeOperatorNode::op_str(".. (range) operator expression");

int QoreRangeOperatorNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    // turn off "return value ignored" flags
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

    // see if any of the arguments cannot be converted to an integer, if so raise a parse exception
    if (!QoreTypeInfo::canConvertToScalar(lti)) {
        parseException(*loc, "PARSE-TYPE-ERROR", "the start expression of the 'range' operator (..) expression is " \
            "type '%s', which does not evaluate to a numeric type, therefore will always evaluate to 0 at runtime",
            QoreTypeInfo::getName(lti));
        if (!err) {
            err = -1;
        }
    }
    if (!QoreTypeInfo::canConvertToScalar(rti)) {
        parseException(*loc, "PARSE-TYPE-ERROR", "the end expression of the 'range' operator (..) expression is " \
            "type '%s', which does not evaluate to a numeric type, therefore will always evaluate to 0 at runtime",
            QoreTypeInfo::getName(rti));
        if (!err) {
            err = -1;
        }
    }

    // do not evaluate at parse time, even if the arguments are both constant values, so we can support lazy
    // evaluation with functional operators

    parse_context.typeInfo = qore_get_complex_list_type(bigIntTypeInfo);
    return err;
}

QoreValue QoreRangeOperatorNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    FunctionalValueType value_type;
    std::unique_ptr<FunctionalOperatorInterface> fit(getFunctionalIterator(value_type, xsink));
    if (*xsink || value_type != list)
        return QoreValue();

    ReferenceHolder<QoreListNode> rv(new QoreListNode(fit->getValueType()), xsink);

    while (true) {
        ValueOptionalRefHolder val(xsink);
        if (fit->getNext(val, xsink))
            break;

        if (xsink && *xsink)
            return QoreValue();

        rv->push(val.takeReferencedValue(), xsink);
    }

    return rv.release();
}

FunctionalOperatorInterface* QoreRangeOperatorNode::getFunctionalIteratorImpl(FunctionalValueType& value_type,
        ExceptionSink* xsink) const {
    ValueEvalOptimizedRefHolder lh(left, xsink);
    if (*xsink)
        return nullptr;
    int64 start = lh->getAsBigInt();

    ValueEvalOptimizedRefHolder rh(right, xsink);
    if (*xsink)
        return nullptr;
    int64 stop = rh->getAsBigInt();

    value_type = list;
    if (!(runtime_get_parse_options() & PO_BROKEN_RANGE)) {
        if (start <= stop) {
            ++stop;
        } else {
            --stop;
        }
    }
    return new QoreFunctionalRangeOperator(start, stop, xsink);
}

bool QoreFunctionalRangeOperator::getNextImpl(ValueOptionalRefHolder& val, ExceptionSink* xsink) {
    if (!next())
        return true;

    val.setValue(getValue(xsink), true);
    return false;
}
