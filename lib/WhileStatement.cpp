/*
    WhileStatement.cpp

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
#include "qore/intern/WhileStatement.h"
#include "qore/intern/StatementBlock.h"

WhileStatement::WhileStatement(int start_line, int end_line, QoreValue c, StatementBlock* cd)
        : AbstractStatement(start_line, end_line), cond(c), code(cd) {
}

WhileStatement::~WhileStatement() {
    cond.discard(nullptr);
    delete code;
    delete lvars;
}

int WhileStatement::execImpl(QoreValue& return_value, ExceptionSink *xsink) {
    // instantiate local variables
    LVListInstantiator lvi(xsink, lvars, pwo.parse_options);

    int rc = 0;

    while (true) {
        // Check for sandbox interrupt
        QoreSandboxManager* sm = runtime_get_sandbox_manager();
        if (sm && sm->isInterruptRequested()) {
            xsink->raiseException("PROGRAM-INTERRUPTED", "program execution was interrupted");
            break;
        }

        ValueEvalOptimizedRefHolder val(cond, xsink);
        if (*xsink) {
            break;
        }

        if (!val->getAsBool()) {
            break;
        }

        if (code) {
            rc = code->execImpl(return_value, xsink);
            if (*xsink || rc == RC_BREAK) {
                rc = 0;
                break;
            }
            if (rc == RC_RETURN) {
                break;
            }
            if (rc == RC_CONTINUE) {
                rc = 0;
            }
        }
    }

    return rc;
}

int WhileStatement::execImpl(RuntimeConfig& rc, QoreValue& return_value, ExceptionSink* xsink) {
    // instantiate local variables
    LVListInstantiator lvi(xsink, lvars, pwo.parse_options);

    int rc_state = 0;

    while (true) {
        // Check for sandbox interrupt
        QoreSandboxManager* sm = runtime_get_sandbox_manager();
        if (sm && sm->isInterruptRequested()) {
            xsink->raiseException("PROGRAM-INTERRUPTED", "program execution was interrupted");
            break;
        }

        ValueEvalRefHolder val(rc, cond, xsink);
        if (*xsink) {
            break;
        }

        if (!val->getAsBool()) {
            break;
        }

        if (code) {
            rc_state = code->execImpl(rc, return_value, xsink);
            if (*xsink || rc_state == RC_BREAK) {
                rc_state = 0;
                break;
            }
            if (rc_state == RC_RETURN) {
                break;
            }
            if (rc_state == RC_CONTINUE) {
                rc_state = 0;
            }
        }
    }

    return rc_state;
}

int WhileStatement::parseInitImpl(QoreParseContext& parse_context) {
    // turn off top-level flag for statement vars
    QoreParseContextFlagHelper fh(parse_context);
    fh.unsetFlags(PF_TOP_LEVEL);

   // saves local variables after parsing
    QoreParseContextLvarHelper lh(parse_context, lvars);

    int err = 0;
    if (cond) {
        parse_context.typeInfo = nullptr;
        err = parse_init_value(cond, parse_context);
        // FIXME: raise a parse warning if cond cannot be converted to a bool (i.e. always false)
    }
    if (code) {
        QoreParseContextFlagHelper fh0(parse_context);
        fh0.setFlags(PF_BREAK_OK | PF_CONTINUE_OK);

        if (code->parseInitImpl(parse_context) && !err) {
            err = -1;
        }
    }

    return err;
}

void WhileStatement::parseCommit(QoreProgram* pgm) {
    AbstractStatement::parseCommit(pgm);
    if (code) {
        code->parseCommit(pgm);
    }
}
