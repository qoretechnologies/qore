/* -*- indent-tabs-mode: nil -*- */
/*
    QoreRegexSubstOperatorNode.cpp

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

QoreString QoreRegexSubstOperatorNode::op_str("regex subst (=~ s///) operator expression");

QoreValue QoreRegexSubstOperatorNode::evalImpl(bool& needs_deref, ExceptionSink *xsink) const {
    // get ptr to current value (lvalue is locked for the scope of the LValueHelper object)
    LValueHelper v(exp, xsink);
    if (!v)
        return QoreValue();

    // if it's not a string, then do nothing
    if (!v.checkType(NT_STRING))
        return QoreValue();

    const QoreStringNode* str = v.getValue().get<const QoreStringNode>();

    // get new value
    QoreStringNode* nv = regex->exec(str, xsink);

    // if there is an exception above, nv = 0
    if (*xsink) {
        assert(!nv);
        return QoreValue();
    }

    // assign new value to lvalue (no exception possible here)
    v.assign(nv);
    assert(!*xsink);

    // reference for return value if necessary
    return ref_rv ? nv->refSelf() : QoreValue();
}

int QoreRegexSubstOperatorNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    // turn off "return value ignored" flags
    QoreParseContextFlagHelper fh(parse_context);
    fh.unsetFlags(PF_RETURN_VALUE_IGNORED);

    assert(!parse_context.typeInfo);
    int err = parse_init_value(exp, parse_context);

    if (!QoreTypeInfo::parseAcceptsReturns(parse_context.typeInfo, NT_STRING)) {
        // FIXME: raise an exception with %strict-types
        QoreStringNode* desc = new QoreStringNode("the lvalue expression with the ");
        desc->sprintf("%s operator is ", op_str.c_str());
        QoreTypeInfo::getThisType(parse_context.typeInfo, *desc);
        desc->sprintf(", therefore this operation will have no effect on the lvalue and will always return " \
            "NOTHING; this operator only works on strings");
        qore_program_private::makeParseWarning(parse_context.pgm, *loc, QP_WARN_INVALID_OPERATION,
            "INVALID-OPERATION", desc);
        typeInfo = nothingTypeInfo;
    } else {
        typeInfo = stringTypeInfo;
    }

    if (exp && !err) {
        err = checkLValue(exp, parse_context.pflag);
    }

    parse_context.typeInfo = typeInfo;
    return err;
}
