/*
    QoreElementsOperatorNode.cpp

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

QoreString QoreElementsOperatorNode::Elements_str("elements operator expression");

// if del is true, then the returned QoreString * should be deleted, if false, then it must not be
QoreString *QoreElementsOperatorNode::getAsString(bool& del, int foff, ExceptionSink* xsink) const {
    del = false;
    return &Elements_str;
}

int QoreElementsOperatorNode::getAsString(QoreString& str, int foff, ExceptionSink* xsink) const {
    str.concat(&Elements_str);
    return 0;
}

QoreValue QoreElementsOperatorNode::evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const {
    ValueEvalOptimizedRefHolder v(rc, exp, xsink);
    if (*xsink)
        return QoreValue();

    switch (v->getType()) {
        case NT_LIST: return v->get<const QoreListNode>()->size();
        // return the number of characters in a string (not bytes)
        case NT_STRING: return v->get<const QoreStringNode>()->length();
        case NT_HASH: return v->get<const QoreHashNode>()->size();
        case NT_OBJECT: return v->get<const QoreObject>()->size(xsink);
        case NT_BINARY: return v->get<const BinaryNode>()->size();
    }

    return 0;
}

int QoreElementsOperatorNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    // turn off "return value ignored" flags
    QoreParseContextFlagHelper fh(parse_context);
    fh.unsetFlags(PF_RETURN_VALUE_IGNORED);

    assert(!parse_context.typeInfo);
    int err = parse_init_value(exp, parse_context);

    if (QoreTypeInfo::hasType(parse_context.typeInfo)
        && !QoreTypeInfo::parseAccepts(listTypeInfo, parse_context.typeInfo)
        && !QoreTypeInfo::parseAccepts(hashTypeInfo, parse_context.typeInfo)
        && !QoreTypeInfo::parseAccepts(stringTypeInfo, parse_context.typeInfo)
        && !QoreTypeInfo::parseAccepts(binaryTypeInfo, parse_context.typeInfo)
        && !QoreTypeInfo::parseAccepts(objectTypeInfo, parse_context.typeInfo)) {
        // FIXME: this should be an error with %strict-types
        QoreStringNode* edesc = new QoreStringNode("the argument given to the 'elements' operator is ");
        QoreTypeInfo::getThisType(parse_context.typeInfo, *edesc);
        edesc->concat(", so this expression will always return 0; the 'elements' operator can only return a value " \
            "with lists, hashes, strings, binary objects, and objects");
        qore_program_private::makeParseWarning(getProgram(), *loc, QP_WARN_INVALID_OPERATION, "INVALID-OPERATION",
            edesc);
    }

    // see the argument is a constant value, then eval immediately and substitute this node with the result
    if (!err && exp.isValue()) {
        SimpleRefHolder<QoreElementsOperatorNode> del(this);
        ParseExceptionSink xsink;
        RuntimeConfig parse_rc = rc_get_parse_time();
        ValueEvalOptimizedRefHolder v(parse_rc, this, *xsink);
        assert(!**xsink);
        QoreValue result = v.takeReferencedValue();
        // only use parse-time folding if we got a valid result
        // (constants may not be fully resolved at parse time, resulting in NOTHING)
        if (!result.isNothing()) {
            val = result;
        } else {
            // constants not resolved - skip parse-time folding, let runtime handle it
            del.release();
        }
    }

    parse_context.typeInfo = bigIntTypeInfo;
    return err;
}
