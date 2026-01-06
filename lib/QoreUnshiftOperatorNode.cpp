/*
    QoreUnshiftOperatorNode.cpp

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

QoreString QoreUnshiftOperatorNode::unshift_str("unshift operator expression");

int QoreUnshiftOperatorNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    // turn off "return value ignored" flags
    QoreParseContextFlagHelper fh(parse_context);
    fh.unsetFlags(PF_RETURN_VALUE_IGNORED);

    assert(!parse_context.typeInfo);
    int err;
    {
        QoreParseContextFlagHelper fh(parse_context);
        fh.setFlags(PF_FOR_ASSIGNMENT);
        err = parse_init_value(left, parse_context);
    }
    const QoreTypeInfo* leftTypeInfo = parse_context.typeInfo;

    parse_context.typeInfo = nullptr;
    if (parse_init_value(right, parse_context) && !err) {
        err = -1;
    }

    if (left) {
        if (!err && checkLValue(left, parse_context.pflag)) {
            err = -1;
        }

        if (!QoreTypeInfo::parseAcceptsReturns(leftTypeInfo, NT_LIST)) {
            // only raise a parse exception if parse exceptions are enabled
            if (getProgram()->getParseExceptionSink()) {
                QoreStringNode* edesc = new QoreStringNode("the lvalue expression with the ");
                edesc->sprintf("'%s' operator is ", getTypeName());
                QoreTypeInfo::getThisType(leftTypeInfo, *edesc);
                edesc->sprintf(" therefore this operation is invalid and would throw an exception at run-time; the " \
                    "'%s' operator can only operate on lists", getTypeName());
                qore_program_private::makeParseException(getProgram(), *loc, "PARSE-TYPE-ERROR", edesc);
            }
            if (!err) {
                err = -1;
            }
        } else {
            returnTypeInfo = listTypeInfo;
        }
    }
    parse_context.typeInfo = returnTypeInfo;
    return err;
}

QoreValue QoreUnshiftOperatorNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    ValueEvalOptimizedRefHolder res(right, xsink);
    if (*xsink)
        return QoreValue();

    // get ptr to current value (lvalue is locked for the scope of the LValueHelper object)
    LValueHelper val(left, xsink);
    if (!val)
        return QoreValue();

    // assign to a blank list if the lvalue has no value yet but is typed as a list or a softlist
    if (val.getType() == NT_NOTHING) {
        const QoreTypeInfo* vti = val.getTypeInfo();
        if (QoreTypeInfo::parseAcceptsReturns(vti, NT_LIST)) {
            // issue #3317: if the lvar can be a list, assign the current runtime type based on the declared complex
            // list type
            const QoreTypeInfo* lti = vti == autoTypeInfo
                ? autoTypeInfo
                : QoreTypeInfo::getReturnComplexListOrNothing(vti);
            if (val.assign(new QoreListNode(lti))) {
                assert(*xsink);
                return QoreValue();
            }
        }
    }

    // value is not a list, so throw exception
    if (val.getType() != NT_LIST) {
        // no need to check for PO_STRICT_ARGS; this exception was always thrown
        xsink->raiseException(*loc, "UNSHIFT-ERROR", QoreValue(), "the lvalue argument to unshift is type \"%s\"; " \
            "expecting \"list\"", val.getTypeName());
        return QoreValue();
    }

    // no exception can occur here
    val.ensureUnique();

    QoreListNode* l = val.getValue().get<QoreListNode>();

    l->insert(res.takeReferencedValue(), xsink);

    // reference for return value
    return ref_rv ? l->refSelf() : QoreValue();
}
