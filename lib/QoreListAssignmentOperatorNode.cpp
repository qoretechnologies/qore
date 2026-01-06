/*
    QoreListAssignmentOperatorNode.cpp

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

QoreString QoreListAssignmentOperatorNode::op_str("list assignment operator expression");

int QoreListAssignmentOperatorNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    // turn off "return value ignored" flag
    QoreParseContextFlagHelper fh(parse_context);
    fh.unsetFlags(PF_RETURN_VALUE_IGNORED);

    assert(left.getType() == NT_PARSE_LIST);
    QoreParseListNode* l = left.get<QoreParseListNode>();

    QoreParseListNodeParseInitHelper li(l);
    QorePossibleListNodeParseInitHelper ri(right, parse_context);

    int err = li.hasError() || ri.hasError() ? -1 : 0;

    const QoreTypeInfo* argInfo;
    while (li.next()) {
        ri.next();

        QoreValue v = li.parseInit(parse_context);
        const QoreTypeInfo* prototypeInfo = parse_context.typeInfo;
        //printd(5, "QoreListAssignmentOperatorNode::parseInitImpl() paI: '%s' (%s)\n", QoreTypeInfo::getName(prototypeInfo),
        //    v.getFullTypeName());
        if (check_lvalue(v)) {
            if (!li.hasError()) {
                parse_error(*loc, "expecting lvalue in position %d of left-hand-side list in list assignment, got " \
                    "'%s' instead", li.index() + 1, v.getTypeName());
                if (!err) {
                    err = -1;
                }
            }
        } else if ((parse_context.pflag & PF_BACKGROUND) && v.getType() == NT_VARREF
            && v.get<const VarRefNode>()->getType() == VT_LOCAL) {
            parse_error(*loc, "illegal local variable modification with the background operator in position %d " \
                "of left-hand-side list in list assignment", li.index() + 1);
            if (!err) {
                err = -1;
            }
        }

        // check for illegal assignment to $self
        if (parse_context.oflag) {
            check_self_assignment(loc, v, parse_context.oflag);
        }

        ri.parseInit(parse_context);
        argInfo = parse_context.typeInfo;

        if (QoreTypeInfo::hasType(prototypeInfo)) {
            bool may_not_match = false;
            bool may_need_filter = false;
            qore_type_result_e max_result = QTI_NOT_EQUAL;
            // only set the initial assignment flag if the lvalue is a declaration
            bool initial_assignment = (v.getType() == NT_VARREF && v.get<VarRefNode>()->parseIsDecl());
            qore_type_result_e res = QoreTypeInfo::parseAccepts(prototypeInfo, argInfo, may_not_match,
                may_need_filter, max_result, initial_assignment);
            //printd(5, "QoreListAssignmentOperatorNode::parseInitImpl() proto: %s <- %s: %d\n",
            //    QoreTypeInfo::getName(prototypeInfo), QoreTypeInfo::getName(argInfo), res);
            if (!res && !ri.hasError()) {
                // raise an exception only if parse exceptions are not disabled
                if (parse_context.pgm->getParseExceptionSink()) {
                    QoreStringNode* edesc = new QoreStringNode("lvalue for ListAssignment operator in position ");
                    edesc->sprintf("%d of list ListAssignment expects ", li.index() + 1);
                    QoreTypeInfo::getThisType(prototypeInfo, *edesc);
                    edesc->concat(", but right-hand side is ");
                    QoreTypeInfo::getThisType(argInfo, *edesc);
                    qore_program_private::makeParseException(parse_context.pgm, *loc, "PARSE-TYPE-ERROR", edesc);
                    if (!err) {
                        err = -1;
                    }
                }
            }
        }
    }

    while (ri.next()) {
        ri.parseInit(parse_context);
    }

    return li.hasError() || ri.hasError() || err ? -1 : 0;
}

QoreValue QoreListAssignmentOperatorNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    assert(left.getType() == NT_PARSE_LIST);
    const QoreParseListNode* llv = left.get<const QoreParseListNode>();

    /* assign new value, this value gets referenced with the
        eval(xsink) call, so there's no need to reference it again
        for the variable assignment - however it does need to be
        copied/referenced for the return value
    */
    ValueEvalOptimizedRefHolder new_value(right, xsink);
    if (*xsink)
        return QoreValue();

    const QoreListNode* nv = (new_value->getType() == NT_LIST ? new_value->get<QoreListNode>() : nullptr);

    // get values and save
    for (unsigned i = 0; i < llv->size(); i++) {
        QoreValue lv = llv->get(i);

        // get ptr to current value (lvalue is locked for the scope of the LValueHelper object)
        LValueHelper v(lv, xsink);
        if (!v)
            return QoreValue();

        // if there's only one value, then save it
        if (nv) { // assign to list position
            v.assign(nv->getReferencedEntry(i));
        } else {
            if (!i) {
                v.assign(new_value->refSelf());
            } else {
                v.assign(QoreValue());
            }
        }
        if (*xsink) {
            return QoreValue();
        }
    }

    return ref_rv ? new_value.takeValue(needs_deref) : QoreValue();
}
