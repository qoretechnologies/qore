/*
    IfStatement.cpp

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
#include "qore/intern/IfStatement.h"
#include "qore/intern/StatementBlock.h"

IfStatement::IfStatement(int start_line, int end_line, QoreValue c, StatementBlock* i, StatementBlock* e)
        : AbstractStatement(start_line, end_line), cond(c), if_code(i), else_code(e) {
}

IfStatement::~IfStatement() {
    cond.discard(nullptr);
    delete if_code;
    delete else_code;
    delete lvars;
}

// only executed by Statement::exec()
int IfStatement::execImpl(QoreValue& return_value, ExceptionSink *xsink) {
    // instantiate local variables
    LVListInstantiator lvi(xsink, lvars, pwo.parse_options);

    ValueEvalOptimizedRefHolder val(cond, xsink);
    if (*xsink) {
        return 0;
    }

    int rc = 0;
    if (val->getAsBool()) {
        if (if_code) {
            rc = if_code->execImpl(return_value, xsink);
        }
    } else {
        if (else_code) {
            rc = else_code->execImpl(return_value, xsink);
        }
    }

    return rc;
}

int IfStatement::parseInitImpl(QoreParseContext& parse_context) {
    // turn off top-level flag for statement vars
    QoreParseContextFlagHelper fh(parse_context);
    fh.unsetFlags(PF_TOP_LEVEL);

    // saves local variables after parsing
    QoreParseContextLvarHelper lh(parse_context, lvars);

    int err = 0;

    if (cond) {
        parse_context.typeInfo = nullptr;
        err = parse_init_value(cond, parse_context);
        // FIXME: generate a warning here if the type cannot be converted to a bool (i.e. always false)
    }

    // Track narrowed types across branches
    NarrowedTypeHelper nth;
    nth.saveState();

    if (if_code) {
        if (if_code->parseInitImpl(parse_context) && !err) {
            err = -1;
        }
        // Record narrowed types after if branch and restore for else branch
        nth.recordBranchAndRestore();
    } else {
        // Empty if block (e.g., "if (cond) ;") - record saved state as this branch
        nth.recordSavedAsImplicitBranch();
    }
    if (else_code) {
        if (else_code->parseInitImpl(parse_context) && !err) {
            err = -1;
        }
        // Record narrowed types after else branch
        nth.recordBranchAndRestore();
    } else {
        // No else branch - the implicit else path preserves the original type
        // Record the saved state as an implicit branch
        nth.recordSavedAsImplicitBranch();
    }

    // Merge types from all branches
    nth.mergeAndApply();

    return err;
}

void IfStatement::parseCommit(QoreProgram* pgm) {
    AbstractStatement::parseCommit(pgm);
    if (if_code) {
        if_code->parseCommit(pgm);
    }
    if (else_code) {
        else_code->parseCommit(pgm);
    }
}
