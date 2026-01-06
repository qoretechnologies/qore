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
#include "qore/intern/SummarizeStatement.h"
#include "qore/intern/StatementBlock.h"

int SummarizeStatement::execImpl(QoreValue& return_value, ExceptionSink* xsink) {
    int rc = 0;
    QoreValue sort = sort_ascending ? sort_ascending : sort_descending;
    int sort_type = sort_ascending ? CM_SORT_ASCENDING : (sort_descending ? CM_SORT_DESCENDING : -1);

    // instantiate local variables
    LVListInstantiator lvi(xsink, lvars, pwo.parse_options);

    // create the context
    ReferenceHolder<Context> context(new Context(name, xsink, exp, where_exp, sort_type, sort, summarize), xsink);

    // execute the statements
    if (code) {
        if (context->max_group_pos && !xsink->isEvent())
        do {
            if (((rc = code->execImpl(return_value, xsink)) == RC_BREAK) || xsink->isEvent()) {
                rc = 0;
                break;
            } else if (rc == RC_RETURN) {
                break;
            } else if (rc == RC_CONTINUE) {
                rc = 0;
            }
        } while (!xsink->isEvent() && context->next_summary());
    }

    return rc;
}

int SummarizeStatement::parseInitImpl(QoreParseContext& parse_context) {
    QORE_TRACE("SummarizeStatement::parseInit()");

    // turn off top-level flag for statement vars
    QoreParseContextFlagHelper fh(parse_context);
    fh.unsetFlags(PF_TOP_LEVEL);

    // saves local variables after parsing
    QoreParseContextLvarHelper lh(parse_context, lvars);

    parse_context.typeInfo = nullptr;
    int err = parse_init_value(exp, parse_context);

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
    if (summarize) {
        parse_context.typeInfo = nullptr;
        if (parse_init_value(summarize, parse_context) && !err) {
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
