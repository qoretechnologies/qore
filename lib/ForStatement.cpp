/*
    ForStatement.cpp

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

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
#include "qore/intern/ForStatement.h"
#include "qore/intern/StatementBlock.h"

ForStatement::ForStatement(int start_line, int end_line, QoreValue a, QoreValue c, QoreValue i, StatementBlock* cd)
        : AbstractStatement(start_line, end_line), assignment(a), cond(c), iterator(i), code(cd) {
}

ForStatement::~ForStatement() {
    assignment.discard(nullptr);
    cond.discard(nullptr);
    iterator.discard(nullptr);
    delete code;
    delete lvars;
}

int ForStatement::execImpl(QoreValue& return_value, ExceptionSink *xsink) {
    // instantiate local variables
    LVListInstantiator lvi(xsink, lvars, pwo.parse_options);

    // evaluate assignment expression and discard results if any
    if (assignment) {
        ValueEvalOptimizedRefHolder tmp(assignment, xsink);
        if (*xsink) {
            return 0;
        }
    }

    int rc = 0;

    // execute "for" body
    while (!*xsink) {
        // Check for cancellation or program interrupt
        if (qore_check_cancel(xsink, "for loop")) {
            break;
        }

        // check conditional expression, exit "for" loop if condition is false
        if (cond) {
            ValueEvalOptimizedRefHolder val(cond, xsink);
            if (*xsink || !val->getAsBool()) {
                break;
            }
        }

        // otherwise, execute "for" body
        if (code) {
            rc = code->execImpl(return_value, xsink);
            if (*xsink || rc == RC_BREAK) {
                rc = 0;
                break;
            }

            if (rc == RC_RETURN) {
                break;
            } else if (rc == RC_CONTINUE) {
                rc = 0;
            }
        }

        // evaluate iterator expression and discard results if any
        if (iterator) {
            ValueEvalOptimizedRefHolder tmp(iterator, xsink);
            if (*xsink) {
                break;
            }
        }
    }

    return rc;
}

int ForStatement::execImpl(RuntimeConfig& rc, QoreValue& return_value, ExceptionSink* xsink) {
    // instantiate local variables
    LVListInstantiator lvi(xsink, lvars, pwo.parse_options);

    // evaluate assignment expression and discard results if any
    if (assignment) {
        ValueEvalRefHolder tmp(rc, assignment, xsink);
        if (*xsink) {
            return 0;
        }
    }

    int rc_state = 0;

    // execute "for" body
    while (!*xsink) {
        // Check for cancellation or program interrupt
        if (qore_check_cancel(xsink, "for loop")) {
            break;
        }

        // check conditional expression, exit "for" loop if condition is false
        if (cond) {
            ValueEvalRefHolder val(rc, cond, xsink);
            if (*xsink || !val->getAsBool()) {
                break;
            }
        }

        // otherwise, execute "for" body
        if (code) {
            rc_state = code->execImpl(rc, return_value, xsink);
            if (*xsink || rc_state == RC_BREAK) {
                rc_state = 0;
                break;
            }

            if (rc_state == RC_RETURN) {
                break;
            } else if (rc_state == RC_CONTINUE) {
                rc_state = 0;
            }
        }

        // evaluate iterator expression and discard results if any
        if (iterator) {
            ValueEvalRefHolder tmp(rc, iterator, xsink);
            if (*xsink) {
                break;
            }
        }
    }

    return rc_state;
}

int ForStatement::parseInitImpl(QoreParseContext& parse_context) {
    // turn off top-level flag for statement vars
    QoreParseContextFlagHelper fh(parse_context);
    fh.unsetFlags(PF_TOP_LEVEL);

    // saves local variables after parsing
    QoreParseContextLvarHelper lh(parse_context, lvars);

    int err = 0;

    //const QoreTypeInfo* argTypeInfo = nullptr;
    if (assignment) {
        parse_context.typeInfo = nullptr;
        QoreParseContextFlagHelper fh0(parse_context);
        fh0.setFlags(PF_RETURN_VALUE_IGNORED);
        err = parse_init_value(assignment, parse_context);
        // enable optimizations when return value is ignored for operator expressions
        ignore_return_value(assignment);
    }
    if (cond) {
        parse_context.typeInfo = nullptr;
        if (parse_init_value(cond, parse_context) && !err) {
            err = -1;
        }
        // FIXME: raise a parse warning if cond cannot be converted to a bool (i.e. always false)
    }
    if (iterator) {
        parse_context.typeInfo = nullptr;
        QoreParseContextFlagHelper fh0(parse_context);
        fh0.setFlags(PF_RETURN_VALUE_IGNORED);
        err = parse_init_value(iterator, parse_context);
        // enable optimizations when return value is ignored for operator expressions
        ignore_return_value(iterator);
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

void ForStatement::parseCommit(QoreProgram* pgm) {
    AbstractStatement::parseCommit(pgm);
    if (code) {
        code->parseCommit(pgm);
    }
}
