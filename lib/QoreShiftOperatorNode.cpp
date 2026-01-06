/*
    QoreShiftOperatorNode.cpp

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

QoreString QoreShiftOperatorNode::shift_str("shift operator expression");

// if del is true, then the returned QoreString * should be shiftd, if false, then it must not be
QoreString *QoreShiftOperatorNode::getAsString(bool& del, int foff, ExceptionSink* xsink) const {
    del = false;
    return &shift_str;
}

int QoreShiftOperatorNode::getAsString(QoreString& str, int foff, ExceptionSink* xsink) const {
    str.concat(&shift_str);
    return 0;
}

int QoreShiftOperatorNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    assert(!parse_context.typeInfo);

    // turn off "return value ignored" flags
    QoreParseContextFlagHelper fh(parse_context);
    fh.unsetFlags(PF_RETURN_VALUE_IGNORED);
    fh.setFlags(PF_FOR_ASSIGNMENT);

    int err = parse_init_value(exp, parse_context);

    if (exp) {
        const QoreTypeInfo* expTypeInfo = parse_context.typeInfo;
        if (!err && checkLValue(exp, parse_context.pflag)) {
            err = -1;
        }

        if (!QoreTypeInfo::parseAcceptsReturns(expTypeInfo, NT_LIST)) {
            // FIXME: raise an exception with %strict-types
            QoreStringNode* edesc = new QoreStringNode("the lvalue expression with the ");
            edesc->sprintf("'%s' operator is ", getTypeName());
            QoreTypeInfo::getThisType(expTypeInfo, *edesc);
            edesc->sprintf(" therefore this operation will have no effect on the lvalue and will always return " \
                "NOTHING; the '%s' operator can only operate on lists", getTypeName());
            qore_program_private::makeParseWarning(getProgram(), *loc, QP_WARN_INVALID_OPERATION, "INVALID-OPERATION",
                edesc);
            returnTypeInfo = nothingTypeInfo;
        } else {
            returnTypeInfo = QoreTypeInfo::getUniqueReturnComplexList(expTypeInfo);
            if (returnTypeInfo) {
                returnTypeInfo = get_or_nothing_type_check(returnTypeInfo);
            }
        }
    }

    parse_context.typeInfo = returnTypeInfo;
    return err;
}

QoreValue QoreShiftOperatorNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    //printd(5, "QoreShiftOperatorNode::evalImpl(%p, isEvent=%d)\n", exp, xsink->isEvent());

    // get ptr to current value (lvalue is locked for the scope of the LValueHelper object)
    LValueHelper val(exp, xsink);
    if (!val)
        return QoreValue();

    // return NOTHING if the lvalue has no value for backwards compatibility
    if (val.getType() == NT_NOTHING)
        return QoreValue();

    // value is not a list, so throw exception
    if (val.getType() != NT_LIST) {
        // only throw a runtime exception if %strict-args is in effect
        if (runtime_check_parse_option(PO_STRICT_ARGS))
            xsink->raiseException("SHIFT-ERROR", "the lvalue argument to shift is type \"%s\"; expecting \"list\"", val.getTypeName());
        return QoreValue();
    }

    // no exception can occur here
    val.ensureUnique();

    QoreListNode* l = val.getValue().get<QoreListNode>();

    printd(5, "QoreShiftOperatorNode::evalImpl() about to call QoreListNode::shift() on list node %p (%d)\n", l, l->size());
    // the list reference will now be the reference for the return value
    // therefore no need to reference again
    return l->shift();
}
