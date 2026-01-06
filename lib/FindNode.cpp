/*
    FindNode.cpp

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
#include "qore/intern/FindNode.h"

FindNode::FindNode(const QoreProgramLocation* loc, QoreValue expr, QoreValue find_expr, QoreValue w)
        : ParseNode(loc, NT_FIND) {
    exp = expr;
    find_exp = find_expr;
    where = w;
}

FindNode::~FindNode() {
    find_exp.discard(nullptr);
    exp.discard(nullptr);
    where.discard(nullptr);
}

// get string representation (for %n and %N), foff is for multi-line formatting offset, -1 = no line breaks
// the ExceptionSink is only needed for QoreObject where a method may be executed
// use the QoreNodeAsStringHelper class (defined in QoreStringNode.h) instead of using these functions directly
// returns -1 for exception raised, 0 = OK
int FindNode::getAsString(QoreString &qstr, int foff, ExceptionSink *xsink) const {
    qstr.sprintf("find expression (%p)", this);
    return 0;
}

// if del is true, then the returned QoreString * should be deleted, if false, then it must not be
QoreString* FindNode::getAsString(bool &del, int foff, ExceptionSink *xsink) const {
    del = true;
    QoreString *rv = new QoreString;
    getAsString(*rv, foff, xsink);
    return rv;
}

// returns the type name as a c string
const char* FindNode::getTypeName() const {
    return "find expression";
}

QoreValue FindNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    ValueHolder rv(xsink);
    ReferenceHolder<Context> context(new Context(nullptr, xsink, find_exp), xsink);
    if (*xsink)
        return QoreValue();

    QoreListNode* lrv = nullptr;
    for (context->pos = 0; context->pos < context->max_pos && !xsink->isEvent(); ++context->pos) {
        printd(4, "FindNode::eval() checking %d/%d\n", context->pos, context->max_pos);
        bool b = context->check_condition(where, xsink);
        if (*xsink)
            return QoreValue();
        if (!b)
            continue;

        printd(4, "FindNode::eval() GOT IT: %d\n", context->pos);
        ValueEvalOptimizedRefHolder result(exp, xsink);
        //ValueHolder result(exp->eval(xsink), xsink);
        if (*xsink) {
            return QoreValue();
        }
        if (!rv->isNothing()) {
            if (!lrv) {
                lrv = new QoreListNode(autoTypeInfo);
                lrv->push(rv.release(), xsink);
                lrv->push(result.takeReferencedValue(), xsink);
                rv = lrv;
            } else {
                lrv->push(result.takeReferencedValue(), xsink);
                assert(!*xsink);
            }
        } else {
            rv = result.takeReferencedValue();
        }
    }

    return rv.release();
}

int FindNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    push_cvar(nullptr);

    parse_context.typeInfo = nullptr;
    int err = parse_init_value(find_exp, parse_context);
    if (where) {
        parse_context.typeInfo = nullptr;
        if (parse_init_value(where, parse_context) && !err) {
            err = -1;
        }
    }
    if (exp) {
        parse_context.typeInfo = nullptr;
        if (parse_init_value(exp, parse_context) && !err) {
            err = -1;
        }
    }
    pop_cvar();

    parse_context.typeInfo = nullptr;
    return err;
}
