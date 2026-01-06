/*
    ForEachStatement.cpp

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
#include <qore/QoreSandboxManager.h>
#include "qore/intern/ForEachStatement.h"
#include "qore/intern/StatementBlock.h"
#include "qore/intern/AbstractIteratorHelper.h"

#include <memory>

ForEachStatement::ForEachStatement(int start_line, int end_line, QoreValue v, QoreValue l, StatementBlock* cd) : AbstractStatement(start_line, end_line), var(v), list(l), code(cd) {
}

ForEachStatement::~ForEachStatement() {
    var.discard(nullptr);
    list.discard(nullptr);
    delete code;
    delete lvars;
}

int ForEachStatement::execImpl(QoreValue& return_value, ExceptionSink* xsink) {
    if (is_ref)
        return execRef(return_value, xsink);

    // instantiate local variables
    LVListInstantiator lvi(xsink, lvars, pwo.parse_options);

    // get iterator object
    FunctionalOperator::FunctionalValueType value_type;
    std::unique_ptr<FunctionalOperatorInterface> f(iterator_func ? iterator_func->getFunctionalIterator(value_type, xsink) : FunctionalOperatorInterface::getFunctionalIterator(value_type, list, true, "foreach statement", xsink));
    if (*xsink || value_type == FunctionalOperator::nothing || !code)
        return 0;

    // execute "foreach" body
    unsigned i = 0;

    int rc = 0;

    while (true) {
        // Check for sandbox interrupt
        QoreSandboxManager* sm = runtime_get_sandbox_manager();
        if (sm && sm->isInterruptRequested()) {
            xsink->raiseException("PROGRAM-INTERRUPTED", "program execution was interrupted");
            break;
        }

        {
            // get first value
            ValueOptionalRefHolder iv(xsink);
            if (f->getNext(iv, xsink))
                break;
            if (*xsink)
                break;

            LValueHelper n(var, xsink);
            if (!n)
                break;

            // assign variable to current value
            if (n.assign(iv.takeReferencedValue(), "<foreach lvalue assignment>"))
                break;
        }

        // set offset in thread-local data for "$#"
        ImplicitElementHelper eh(i++);

        // execute "foreach" body
        if (((rc = code->execImpl(return_value, xsink)) == RC_BREAK) || *xsink) {
            rc = 0;
            break;
        }

        if (rc == RC_RETURN)
            break;
        else if (rc == RC_CONTINUE)
            rc = 0;
    }

    return rc;
}

int ForEachStatement::execRef(QoreValue& return_value, ExceptionSink* xsink) {
    int rc = 0;

    // instantiate local variables
    LVListInstantiator lvi(xsink, lvars, pwo.parse_options);

    ParseReferenceNode* r = list.get<ParseReferenceNode>();

    // here we get the runtime reference
    ReferenceHolder<ReferenceNode> vr(r->evalToRef(xsink), xsink);
    if (*xsink) {
        return 0;
    }

    // get the current value of the lvalue expression
    ValueHolder tlist(vr->eval(xsink), xsink);
    if (!code || *xsink || tlist->isNothing()) {
        return 0;
    }

    QoreListNode* l_tlist = tlist->getType() == NT_LIST ? tlist->get<QoreListNode>() : nullptr;
    if (l_tlist && l_tlist->empty()) {
        return 0;
    }

    // execute "foreach" body
    ValueHolder ln(xsink);
    unsigned i = 0;

    if (l_tlist) {
        ln = new QoreListNode(autoTypeInfo);
    }
    //printd(5, "ForEachStatement::execRef() l_tlist: %p ln: %s\n", l_tlist, ln->getFullTypeName());

    while (true) {
        // Check for sandbox interrupt
        QoreSandboxManager* sm = runtime_get_sandbox_manager();
        if (sm && sm->isInterruptRequested()) {
            xsink->raiseException("PROGRAM-INTERRUPTED", "program execution was interrupted");
            break;
        }

        {
            LValueHelper n(var, xsink);
            if (!n) {
                return 0;
            }

            // assign variable to current value in list
            if (n.assign(l_tlist ? l_tlist->getReferencedEntry(i) : tlist.release())) {
                return 0;
            }
        }

        // set offset in thread-local data for "$#"
        ImplicitElementHelper eh(l_tlist ? (int)i : 0);

        // execute "for" body
        rc = code->execImpl(return_value, xsink);
        if (*xsink) {
            return 0;
        }

        // get value of foreach variable
        ValueEvalOptimizedRefHolder nv(var, xsink);
        if (*xsink) {
            return 0;
        }

        // assign new value to temporary variable for later assignment to referenced lvalue
        if (l_tlist) {
            ln->get<QoreListNode>()->push(nv.takeReferencedValue(), nullptr);
        } else {
            ln = nv.takeReferencedValue();
        }

        if (rc == RC_BREAK) {
            // assign remaining values to list unchanged
            if (l_tlist) {
                while (++i < l_tlist->size()) {
                    ln->get<QoreListNode>()->push(l_tlist->getReferencedEntry(i), nullptr);
                }
            }

            rc = 0;
            break;
        }

        if (rc == RC_RETURN) {
            break;
        } else if (rc == RC_CONTINUE) {
            rc = 0;
        }
        ++i;

        // break out of loop if appropriate
        if (!l_tlist || i == l_tlist->size()) {
            break;
        }
    }

    // write the value back to the lvalue
    LValueHelper val(**vr, xsink);
    if (!val) {
        return 0;
    }

    if (val.assign(ln.release())) {
        return 0;
    }

    return rc;
}

int ForEachStatement::parseInitImpl(QoreParseContext& parse_context) {
    // turn off top-level flag for statement vars
    QoreParseContextFlagHelper fh(parse_context);
    fh.unsetFlags(PF_TOP_LEVEL);

    // saves local variables after parsing
    QoreParseContextLvarHelper lh(parse_context, lvars);

    int err = 0;

    parse_context.typeInfo = nullptr;
    err = parse_init_value(var, parse_context);

    qore_type_t t = var.getType();
    if (!err && t != NT_VARREF && t != NT_SELF_VARREF) {
        parse_error(*loc, "foreach variable expression is not a variable reference (got type '%s' instead)",
            var.getTypeName());
        err = -1;
    }

    parse_context.typeInfo = nullptr;
    if (parse_init_value(list, parse_context) && !err) {
        err = -1;
    }

    if (code) {
        QoreParseContextFlagHelper fh0(parse_context);
        fh0.setFlags(PF_BREAK_OK | PF_CONTINUE_OK);

        err = code->parseInitImpl(parse_context);
    }

    qore_type_t typ = list.getType();
    is_ref = (typ == NT_PARSEREFERENCE);
    // use lazy evaluation if the iterator expression supports it
    iterator_func = dynamic_cast<FunctionalOperator*>(list.getInternalNode());

    return err;
}

void ForEachStatement::parseCommit(QoreProgram* pgm) {
    AbstractStatement::parseCommit(pgm);
    if (code) {
        code->parseCommit(pgm);
    }
}
