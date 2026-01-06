/*
    ExpressionStatement.cpp

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
#include "qore/intern/ExpressionStatement.h"

ExpressionStatement::ExpressionStatement(const QoreProgramLocation* loc, QoreValue v)
        : AbstractStatement(loc), exp(v) {
    // if it is a global variable declaration, then do not register
    if (exp.getType() == NT_VARREF) {
        VarRefNode* vr = exp.get<VarRefNode>();
        // used by QoreProgram to detect invalid top-level statements
        is_declaration = !vr->has_effect();
        // used in parsing to eliminate noops from the parse tree
        is_parse_declaration = !vr->stayInTree();
        return;
    }

    QoreParseListNode *l = exp.getType() == NT_PARSE_LIST ? exp.get<QoreParseListNode>() : nullptr;
    if (l && l->isVariableList()) {
        is_declaration = true;
        is_parse_declaration = l->get(0).get<VarRefNode>()->getType() == VT_GLOBAL ? true : false;
        return;
    }

    is_declaration = false;
    is_parse_declaration = false;
}

ExpressionStatement::~ExpressionStatement() {
    //printd(5, "ExpressionStatement::~ExpressionStatement() this=%p exp=%p (%s)\n", this, exp,
    //  exp ? exp->getTypeName() : "n/a");
    // this should never be 0, but in case the implementation changes...
    exp.discard(nullptr);
}

int ExpressionStatement::execImpl(QoreValue& return_value, ExceptionSink* xsink) {
    ValueEvalOptimizedRefHolder erh(exp, xsink);
    return 0;
}

int ExpressionStatement::parseInitImpl(QoreParseContext& parse_context) {
    //printd(5, "ExpressionStatement::parseInitImpl() exp=%p (%s)\n", exp, exp->getTypeName());
    int err = 0;
    if (exp) {
        QoreParseContextFlagHelper fh(parse_context);
        fh.setFlags(PF_RETURN_VALUE_IGNORED);

        parse_context.typeInfo = nullptr;
        err = parse_init_value(exp, parse_context);
    }
    //printd(5, "ExpressionStatement::parseInitImpl() this=%p exp=%p (%s) lvids=%d\n", this, exp, exp->getTypeName(),
    //  parse_context.lvids);
    return err;
}
