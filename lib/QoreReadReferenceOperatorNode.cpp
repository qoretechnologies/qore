/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreReadReferenceOperatorNode.cpp

    Qore Programming Language

    Copyright (C) 2026 Qore Technologies, s.r.o.

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
#include "qore/intern/LocalVar.h"

QoreString QoreReadReferenceOperatorNode::readreference_str("read through reference expression");

// if del is true, then the returned QoreString * should be deleted, if false, then it must not be
QoreString* QoreReadReferenceOperatorNode::getAsString(bool& del, int foff, ExceptionSink* xsink) const {
    del = false;
    return &readreference_str;
}

int QoreReadReferenceOperatorNode::getAsString(QoreString& str, int foff, ExceptionSink* xsink) const {
    qore_string_private::get(str)->concatAsciiName(readreference_str);
    return 0;
}

bool QoreReadReferenceOperatorNode::isReferenceType(const QoreTypeInfo* ti) {
    return QoreTypeInfo::isReference(ti);
}

const QoreTypeInfo* QoreReadReferenceOperatorNode::getTargetType(const QoreTypeInfo* ti) {
    const QoreTypeInfo* target = QoreTypeInfo::getReferenceTarget(ti);
    if (!target) {
        return ti;
    }
    // the value read through a *reference<T> is NOTHING when there is no reference (qore#6662)
    if (QoreTypeInfo::parseReturns(ti, NT_NOTHING) != QTI_NOT_EQUAL) {
        const QoreTypeInfo* or_nothing = get_or_nothing_type_check(target);
        if (or_nothing) {
            return or_nothing;
        }
    }
    return target;
}

QoreValue QoreReadReferenceOperatorNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    ValueEvalOptimizedRefHolder v(exp, xsink);
    if (*xsink) {
        return QoreValue();
    }

    // a container element that holds a reference gives the reference; its referenced value is read here, before the
    // assignment locks its target (qore#6662)
    if (v->getType() == NT_REFERENCE) {
        needs_deref = true;
        return qore_read_access_base(*v, xsink);
    }

    return v.takeValue(needs_deref);
}

int QoreReadReferenceOperatorNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    // the parser creates this node with its operand parse-initialized and marks it as parse-initialized itself
    // (ParseNode::setParseAnalysis()); this initializes a node created otherwise
    assert(!parse_context.typeInfo);
    int err = 0;
    {
        QoreParseContextAnalysisHelper ah(parse_context);
        err = parse_init_value(exp, parse_context);
    }

    returnTypeInfo = getTargetType(parse_context.typeInfo);
    parse_context.typeInfo = returnTypeInfo;
    parse_context.analysis.clear();
    // the referenced lvalue can have no value whatever its type (a missing hash key, for one), so the value is known
    // only to be of the or-nothing type, and never known to be assigned
    if (returnTypeInfo) {
        parse_context.analysis.setFlag(QoreParseAnalysis::KnownTypeInfo);
        parse_context.analysis.known_type = get_or_nothing_type_check(returnTypeInfo);
    }
    return err;
}
