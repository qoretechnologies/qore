/*
    ContextStatement.cpp

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
#include "qore/intern/ContextStatement.h"
#include "qore/intern/StatementBlock.h"

ContextModList::ContextModList(ContextMod *cm) {
    push_back(cm);
}

ContextModList::~ContextModList() {
    cxtmod_list_t::iterator i;
    while ((i = begin()) != end()) {
        //printd(5, "CML::~CML() %d (%p)\n", (*i)->getType(), (*i)->exp);
        if (*i) {
            delete *i;
        }
        erase(i);
    }
}

void ContextModList::addContextMod(ContextMod *cm) {
    push_back(cm);
    //printd(5, "CML::CML() %d (%p)\n", cm->getType(), cm->exp);
}

ContextStatement::ContextStatement(int start_line, int end_line, char* n, QoreValue expr, ContextModList* mods,
        StatementBlock* cd)
        : AbstractStatement(start_line, end_line),
            name(n), exp(expr), code(cd) {
    if (mods) {
        for (cxtmod_list_t::iterator i = mods->begin(); i != mods->end(); i++) {
            switch ((*i)->type) {
                case CM_WHERE_NODE:
                    if (!where_exp) {
                        where_exp = (*i)->exp;
                        (*i)->exp.clear();
                    }
                    else {
                        parseException(*loc, "CONTEXT-PARSE-ERROR", "multiple where conditions found for context " \
                            "statement");
                    }
                    break;
                case CM_SORT_ASCENDING:
                    if (!sort_ascending && !sort_descending) {
                        sort_ascending = (*i)->exp;
                        (*i)->exp.clear();
                    }
                    else {
                        parseException(*loc, "CONTEXT-PARSE-ERROR", "multiple sort conditions found for context " \
                            "statement");
                    }
                    break;
                case CM_SORT_DESCENDING:
                    if (!sort_descending && !sort_ascending) {
                        sort_descending = (*i)->exp;
                        (*i)->exp.clear();
                    }
                    else {
                        parseException(*loc, "CONTEXT-PARSE-ERROR", "multiple sort conditions found for context " \
                            "statement");
                    }
                    break;
            }
        }
        delete mods;
    }
}

ContextStatement::~ContextStatement() {
    if (name) {
        free(name);
    }
    exp.discard(nullptr);
    delete code;
    delete lvars;
    where_exp.discard(nullptr);
    sort_ascending.discard(nullptr);
    sort_descending.discard(nullptr);
}

// FIXME: local vars should only be instantiated if there is a non-null context
int ContextStatement::execImpl(QoreValue& return_value, ExceptionSink *xsink) {
    int rc = 0;
    QoreValue sort = sort_ascending ? sort_ascending : sort_descending;
    int sort_type = sort_ascending ? CM_SORT_ASCENDING : (sort_descending ? CM_SORT_DESCENDING : -1);

    // instantiate local variables
    LVListInstantiator lvi(xsink, lvars, pwo.parse_options);

    // create the context
    ReferenceHolder<Context> context(new Context(name, xsink, exp, where_exp, sort_type, sort), xsink);
    if (*xsink || !code)
        return rc;

    // execute the statements
    for (context->pos = 0; context->pos < context->max_pos && !xsink->isEvent(); ++context->pos) {
        printd(4, "ContextStatement::exec() iteration %d/%d\n", context->pos, context->max_pos);
        if (((rc = code->execImpl(return_value, xsink)) == RC_BREAK) || *xsink) {
            rc = 0;
            break;
        }
        else if (rc == RC_RETURN) {
            break;
        }
        else if (rc == RC_CONTINUE) {
            rc = 0;
        }
    }

    return rc;
}

int ContextStatement::parseInitImpl(QoreParseContext& parse_context) {
    QORE_TRACE("ContextStatement::parseInitImpl()");

    // turn off top-level flag for statement vars
    QoreParseContextFlagHelper fh(parse_context);
    fh.unsetFlags(PF_TOP_LEVEL);

    int err = 0;

    if (!exp && !getCVarStack()) {
        parse_error(*loc, "subcontext statement out of context");
        err = -1;
    }

    // saves local variables after parsing
    QoreParseContextLvarHelper lh(parse_context, lvars);

    // initialize context expression
    parse_context.typeInfo = nullptr;
    if (parse_init_value(exp, parse_context) && !err) {
        err = -1;
    }
    //const QoreTypeInfo* argTypeInfo = parse_context.typeInfo;

    // need to push something on the stack even if the context is not named
    push_cvar(name);

    if (where_exp) {
        parse_context.typeInfo = nullptr;
        if (parse_init_value(where_exp, parse_context) && !err) {
            err = -1;
        }
    }
    if (sort_ascending) {
        parse_context.typeInfo = nullptr;
        if (parse_init_value(sort_ascending, parse_context) && !err) {
            err = -1;
        }
    }
    if (sort_descending) {
        parse_context.typeInfo = nullptr;
        if (parse_init_value(sort_descending, parse_context) && !err) {
            err = -1;
        }
    }

    // initialize statement block
    if (code) {
        QoreParseContextFlagHelper fh0(parse_context);
        fh0.setFlags(PF_BREAK_OK | PF_CONTINUE_OK);
        if (code->parseInitImpl(parse_context) && !err) {
            err = -1;
        }
    }

    pop_cvar();

    return err;
}

void ContextStatement::parseCommit(QoreProgram* pgm) {
    AbstractStatement::parseCommit(pgm);
    if (code) {
        code->parseCommit(pgm);
    }
}
