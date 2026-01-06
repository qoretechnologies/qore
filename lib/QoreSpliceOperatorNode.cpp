/*
    QoreSpliceOperatorNode.cpp

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

QoreString QoreSpliceOperatorNode::splice_str("splice operator expression");

// if del is true, then the returned QoreString * should be spliced, if false, then it must not be
QoreString *QoreSpliceOperatorNode::getAsString(bool &del, int foff, ExceptionSink *xsink) const {
   del = false;
   return &splice_str;
}

int QoreSpliceOperatorNode::getAsString(QoreString &str, int foff, ExceptionSink *xsink) const {
   str.concat(&splice_str);
   return 0;
}

int QoreSpliceOperatorNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    assert(!parse_context.typeInfo);
    // turn off "return value ignored" flags
    QoreParseContextFlagHelper fh(parse_context);
    fh.unsetFlags(PF_RETURN_VALUE_IGNORED);

    int err;
    {
        // check lvalue expression
        QoreParseContextFlagHelper fh0(parse_context);
        fh0.setFlags(PF_FOR_ASSIGNMENT);
        err = parse_init_value(lvalue_exp, parse_context);
    }
    const QoreTypeInfo* expTypeInfo = parse_context.typeInfo;

    if (!err) {
        if (checkLValue(lvalue_exp, parse_context.pflag)) {
            err = -1;
        } else {
            if (QoreTypeInfo::hasType(expTypeInfo)) {
                if (!QoreTypeInfo::parseAcceptsReturns(expTypeInfo, NT_LIST)
                    && !QoreTypeInfo::parseAcceptsReturns(expTypeInfo, NT_BINARY)
                    && !QoreTypeInfo::parseAcceptsReturns(expTypeInfo, NT_STRING)) {
                    QoreStringNode *desc = new QoreStringNode("the lvalue expression (1st position) with the " \
                        "'splice' operator is ");
                    QoreTypeInfo::getThisType(expTypeInfo, *desc);
                    desc->sprintf(", therefore this operation is invalid and would throw an exception at run-time; " \
                        "the 'splice' operator only operates on lists, strings, and binary objects");
                    qore_program_private::makeParseException(getProgram(), *loc, "PARSE-TYPE-ERROR", desc);
                } else {
                    returnTypeInfo = expTypeInfo;
                }
            }
        }
    }

    // check offset expression
    parse_context.typeInfo = nullptr;
    if (parse_init_value(offset_exp, parse_context) && !err) {
        err = -1;
    }
    if (!QoreTypeInfo::canConvertToScalar(parse_context.typeInfo)) {
        // FIXME: raise exceptions with %strict-types
        parse_context.typeInfo->doNonNumericWarning(loc, "the offset expression (2nd position) with the 'splice' " \
            "operator is ");
    }

    // check length expression, if any
    if (length_exp) {
        parse_context.typeInfo = nullptr;
        if (parse_init_value(length_exp, parse_context) && !err) {
            err = -1;
        }
        if (!QoreTypeInfo::canConvertToScalar(parse_context.typeInfo)) {
            // FIXME: raise exceptions with %strict-types
            expTypeInfo->doNonNumericWarning(loc, "the length expression (3nd position) with the 'splice' operator " \
                "is ");
        }
    }

    // check new value expression, if any
    if (new_exp) {
        parse_context.typeInfo = nullptr;
        if (parse_init_value(new_exp, parse_context) && !err) {
            err = -1;
        }
    }

    parse_context.typeInfo = returnTypeInfo;
    return err;
}

QoreValue QoreSpliceOperatorNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    printd(5, "QoreSpliceOperatorNode::splice() lvalue_exp: %s, offset_exp: %s, length_exp: %s, new_exp: %s, " \
        "isEvent: %d\n", lvalue_exp.getTypeName(), offset_exp.getTypeName(), length_exp.getTypeName(),
        new_exp.getTypeName(), xsink->isEvent());

    // evaluate arguments
    ValueEvalOptimizedRefHolder eoffset(offset_exp, xsink);
    if (*xsink)
        return QoreValue();

    ValueEvalOptimizedRefHolder elength(length_exp, xsink);
    if (*xsink)
        return QoreValue();

    ValueEvalOptimizedRefHolder exp(new_exp, xsink);
    if (*xsink)
        return QoreValue();

    // ensure that any list elements removed are removed outside the LValueHelper lock
    ReferenceHolder<QoreListNode> holder(xsink);

    // get ptr to current value (lvalue is locked for the scope of the LValueHelper object)
    LValueHelper val(lvalue_exp, xsink);
    if (!val)
        return QoreValue();

    // if value is not a list or string, throw exception
    qore_type_t vt = val.getType();

    if (vt == NT_NOTHING) {
        // see if the lvalue has a default type
        const QoreTypeInfo *typeInfo = val.getTypeInfo();
        if (typeInfo == softListTypeInfo || typeInfo == listTypeInfo || typeInfo == stringTypeInfo
            || typeInfo == softStringTypeInfo) {
            if (val.assign(QoreTypeInfo::getDefaultQoreValue(typeInfo)))
                return QoreValue();
            vt = val.getType();
        }
    }

    if (vt != NT_LIST && vt != NT_STRING && vt != NT_BINARY) {
        xsink->raiseException(*loc, "EXTRACT-ERROR", QoreValue(), "first (lvalue) argument to the extract operator " \
            "is not a list, string, or binary object");
        return QoreValue();
    }

    // no exception can occur here
    val.ensureUnique();

    size_t offset = (size_t)eoffset->getAsBigInt();

#ifdef DEBUG
    if (vt == NT_LIST) {
        QoreListNode *vl = val.getValue().get<QoreListNode>();
        printd(5, "op_splice() val: %p (size: " QSD ") offset: " QSD "\n", vl, vl->size(), offset);
    }
    else if (vt == NT_STRING) {
        QoreStringNode *vs = val.getValue().get<QoreStringNode>();
        printd(5, "op_splice() val: %p (strlen: " QSD ") offset: " QSD "\n", vs, vs->strlen(), offset);
    }
    else if (vt == NT_STRING) {
        QoreStringNode *vs = val.getValue().get<QoreStringNode>();
        printd(5, "op_splice() val: %p (strlen: " QSD ") offset: " QSD "\n", vs, vs->strlen(), offset);
    }
#endif

    if (vt == NT_LIST) {
        QoreListNode *vl = val.getValue().get<QoreListNode>();
        if (!length_exp && !new_exp) {
            holder = vl->splice(offset);
        } else {
            size_t length = (size_t)elength->getAsBigInt();
            if (!new_exp) {
                holder = vl->splice(offset, length);
            } else {
                holder = vl->splice(offset, length, *exp, xsink);
            }
        }
    } else if (vt == NT_STRING) {
        QoreStringNode *vs = val.getValue().get<QoreStringNode>();
        if (!length_exp && !new_exp)
            vs->splice(offset, xsink);
        else {
            size_t length = (size_t)elength->getAsBigInt();
            if (!new_exp)
                vs->splice(offset, length, xsink);
            else
                vs->splice(offset, length, *exp, xsink);
        }
    } else { // must be a binary
        BinaryNode* b = val.getValue().get<BinaryNode>();
        if (!length_exp && !new_exp)
            b->splice(offset, b->size());
        else {
            size_t length = (size_t)elength->getAsBigInt();
            if (!new_exp)
                b->splice(offset, length);
            else {
                qore_type_t t = exp->getType();
                if (t == NT_BINARY) {
                    const BinaryNode* b1 = exp->get<const BinaryNode>();
                    b->splice(offset, length, b1->getPtr(), b1->size());
                } else {
                    QoreStringNodeValueHelper sv(*exp);
                    if (!sv->strlen())
                        b->splice(offset, length);
                    else
                        b->splice(offset, length, sv->getBuffer(), sv->size());
                }
            }
        }
    }

    // reference for return value
    if (!ref_rv || *xsink)
        return QoreValue();

    return val.getReferencedValue();
}
