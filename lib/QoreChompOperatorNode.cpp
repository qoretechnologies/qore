/*
    QoreChompOperatorNode.cpp

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
#include "qore/intern/QoreHashNodeIntern.h"
#include "qore/intern/qore_list_private.h"

QoreString QoreChompOperatorNode::chomp_str("chomp operator expression");

// if del is true, then the returned QoreString*  should be chompd, if false, then it must not be
QoreString* QoreChompOperatorNode::getAsString(bool& del, int foff, ExceptionSink* xsink) const {
    del = false;
    return &chomp_str;
}

int QoreChompOperatorNode::getAsString(QoreString& str, int foff, ExceptionSink* xsink) const {
    str.concat(&chomp_str);
    return 0;
}

QoreValue QoreChompOperatorNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    LValueHelper val(exp, xsink);
    if (!val)
        return QoreValue();

    qore_type_t vtype = val.getType();
    if (vtype != NT_LIST && vtype != NT_STRING && vtype != NT_HASH)
        return QoreValue();

    // note that no exception can happen here
    val.ensureUnique();
    assert(!*xsink);

    if (vtype == NT_STRING)
        return val.getValue().get<QoreStringNode>()->chomp();

    int64 count = 0;

    if (vtype == NT_LIST) {
        QoreListNode* l = val.getValue().get<QoreListNode>();
        qore_list_private* ll = qore_list_private::get(*l);
        ListIterator li(l);
        while (li.next()) {
            QoreValue& v = ll->getEntryReference(li.index());
            if (v.getType() == NT_STRING) {
                // note that no exception can happen here
                ensure_unique(v, xsink);
                assert(!*xsink);
                QoreStringNode* vs = v.get<QoreStringNode>();
                count += vs->chomp();
            }
        }
        return count;
    }

    // must be a hash
    QoreHashNode* vh = val.getValue().get<QoreHashNode>();
    HashIterator hi(vh);
    while (hi.next()) {
        if (hi.get().getType() == NT_STRING) {
            QoreValue& v = (*qhi_priv::get(hi)->i)->val;
            QoreStringNode* vs = v.get<QoreStringNode>();
            if (!vs->is_unique()) {
                QoreStringNode* old = vs;
                vs = vs->copy();
                old->deref();
                v = vs;
            }
            count += vs->chomp();
        }
    }
    return count;
}

int QoreChompOperatorNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    assert(!parse_context.typeInfo);
    QoreParseContextFlagHelper fh(parse_context);
    fh.setFlags(PF_FOR_ASSIGNMENT);
    int err = parse_init_value(exp, parse_context);
    if (!err && exp && checkLValue(exp, parse_context.pflag)) {
        err = -1;
    }

    if (QoreTypeInfo::hasType(parse_context.typeInfo)
        && !QoreTypeInfo::parseAcceptsReturns(parse_context.typeInfo, NT_STRING)
        && !QoreTypeInfo::parseAcceptsReturns(parse_context.typeInfo, NT_LIST)
        && !QoreTypeInfo::parseAcceptsReturns(parse_context.typeInfo, NT_HASH)) {
        // FIXME: raise an error with %strict-types
        QoreStringNode* desc = new QoreStringNode("the lvalue expression with the chomp operator is ");
        QoreTypeInfo::getThisType(parse_context.typeInfo, *desc);
        desc->sprintf(", therefore this operation will have no effect on the lvalue and will always return " \
            "NOTHING; this operator only works on strings, lists, and hashes");
        qore_program_private::makeParseWarning(getProgram(), *loc, QP_WARN_INVALID_OPERATION, "INVALID-OPERATION",
            desc);
    }

    parse_context.typeInfo = bigIntTypeInfo;
    return err;
}
