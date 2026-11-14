/*
    TryStatement.cpp

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
#include "qore/intern/TryStatement.h"
#include "qore/intern/StatementBlock.h"
#include "qore/intern/QoreDeferredRelease.h"

TryStatement::TryStatement(const QoreProgramLocation* loc, StatementBlock* t, StatementBlock* c, char* p,
        const QoreTypeInfo* typeInfo, QoreParseTypeInfo* parseTypeInfo, const QoreProgramLocation* vloc)
        : AbstractStatement(loc), try_block(t), catch_block(c), param(p), typeInfo(typeInfo),
        parseTypeInfo(parseTypeInfo), loc(vloc) {
}

TryStatement::~TryStatement() {
    if (param)
        free(param);
    qore_delete_statement(try_block);
    qore_delete_statement(catch_block);
    delete parseTypeInfo;
}

//! Releases the value of a return in the try block that an exception interrupted
/** A return that an exception interrupts gives no value: the value of a return statement whose value the declared
    return type rejects, or of a return statement after which an on_exit handler raised an exception, is released
    while the exception is still pending, so that an exception that releasing it raises follows it, and the code
    continues after the catch block as if the return had not completed
*/
static void qore_try_release_interrupted_return(ValueHolder& trv, ExceptionSink* xsink) {
    if (*xsink && !trv->isNothing()) {
        ValueHolder interrupted(trv.release(), xsink);
    }
}

//! Gives the value of a return in the try or catch block to the enclosing block
/** A return in the try statement replaces a value that a return in the block that encloses the statement left, as
    a return statement replaces it (ReturnStatement::execImpl()): in a handler of a block (on_exit, on_success,
    on_error), which runs with the value of the return that left the block, the try statement's return gives the
    return value, and the replaced value is released when the try statement ends (#5992)
*/
static void qore_try_give_return(int rc, ValueHolder& trv, QoreValue& return_value, ExceptionSink* xsink) {
    if (rc == RC_RETURN && !*xsink) {
        ValueHolder replaced(return_value, xsink);
        return_value = trv.release();
        return;
    }
    if (!trv->isNothing() && return_value.isNothing()) {
        return_value = trv.release();
    }
}

bool qore_catch_thread_exit(ExceptionSink* xsink) {
    if (!xsink || !xsink->isThreadExit()) {
        return false;
    }
    // the catch block would not run while the thread is exiting (StatementBlock::execIntern()), so the exception is
    // only caught and deleted, as execImpl() below does
    if (xsink->isException()) {
        xsink->catchException()->del(xsink);
    }
    return true;
}

int TryStatement::execImpl(QoreValue& return_value, ExceptionSink *xsink) {
    ValueHolder trv(xsink);

    QORE_TRACE("TryStatement::execImpl()");
    int rc = 0;
    if (try_block)
        rc = try_block->execImpl(*trv, xsink);

    // a return that an exception interrupted gives no value: the value of a return statement that raised the
    // exception (a value that the declared return type rejects) or that an exception raised after it (in an on_exit
    // handler) interrupted is released, and the code continues as if the return had not completed
    qore_try_release_interrupted_return(trv, xsink);

    QoreException* except = xsink->catchException();
    if (except) {
        printd(5, "TryStatement::execImpl() entering catch handler, e=%p\n", except);

        if (catch_block) {
            CatchExceptionHelper ceh(except);

            // instantiate exception information parameter
            if (param)
                id->instantiate(except->makeExceptionObject());

            rc = catch_block->execImpl(*trv, xsink);

            // uninstantiate extra args
            if (param)
                id->uninstantiate(xsink);
        } else
            rc = 0;

        // delete exception chain
        except->del(xsink);
    }

    qore_try_give_return(rc, trv, return_value, xsink);

    return rc;
}

int TryStatement::execImpl(RuntimeConfig& rc, QoreValue& return_value, ExceptionSink* xsink) {
    ValueHolder trv(xsink);

    QORE_TRACE("TryStatement::execImpl()");
    int rc_state = 0;
    if (try_block)
        rc_state = try_block->execImpl(rc, *trv, xsink);

    // a return that an exception interrupted gives no value (see above)
    qore_try_release_interrupted_return(trv, xsink);

    QoreException* except = xsink->catchException();
    if (except) {
        printd(5, "TryStatement::execImpl() entering catch handler, e=%p\n", except);

        if (catch_block) {
            CatchExceptionHelper ceh(except);

            // instantiate exception information parameter
            if (param)
                id->instantiate(except->makeExceptionObject());

            rc_state = catch_block->execImpl(rc, *trv, xsink);

            // uninstantiate extra args
            if (param)
                id->uninstantiate(xsink);
        } else
            rc_state = 0;

        // delete exception chain
        except->del(xsink);
    }

    qore_try_give_return(rc_state, trv, return_value, xsink);

    return rc_state;
}

int TryStatement::parseInitImpl(QoreParseContext& parse_context) {
    // turn off top-level flag for statement vars
    QoreParseContextFlagHelper fh(parse_context);
    fh.unsetFlags(PF_TOP_LEVEL);

    int err = 0;

    AssignedStateHelper ash;
    ash.saveState();

    if (try_block) {
        // the statements of the try block are in a try statement; those of the catch block are not, unless this
        // statement is
        QoreParseContextFlagHelper fht(parse_context);
        fht.setFlags(PF_IN_TRY);
        err = try_block->parseInitImpl(parse_context);
        ash.recordBranchAndRestore();
        // the catch block can follow any point of the try block that raises an exception (qore#5937)
        ash.applyChangesSinceSave();
    } else {
        ash.recordSavedAsImplicitBranch();
    }

    // prepare catch block and params
    if (param) {
        // initialize/check catch exception variable type (if any)
        if (parseTypeInfo) {
            typeInfo = QoreParseTypeInfo::resolveAndDelete(parseTypeInfo, loc, err);
            parseTypeInfo = nullptr;
        }

        if (!QoreTypeInfo::parseAccepts(typeInfo, hashdeclExceptionInfo->getTypeInfo())) {
            parse_error(*loc, "catch block exception variable is not compatible with exception hashdecl type '%s'",
                QoreTypeInfo::getName(typeInfo));
            if (!err) {
                err = -1;
            }
        }

        // push as if the variable is already referenced so no warning will be emitted
        // in case the variable is not actually referenced in the catch block
        id = push_local_var(param, loc, typeInfo, err, true, 1);
        printd(3, "TryStatement::parseInitImpl() reg. local var %s (id=%p)\n", param, id);
    } else {
        id = nullptr;
        assert(!parseTypeInfo);
        assert(!typeInfo);
    }

    // initialize code block
    if (catch_block) {
        // turn off top-level flag for statement vars
        QoreParseContextFlagHelper fh0(parse_context);
        fh0.setFlags(PF_RETHROW_OK);

        if (catch_block->parseInitImpl(parse_context) && !err) {
            err = -1;
        }
        ash.recordBranchAndRestore();
    } else {
        ash.recordSavedAsImplicitBranch();
    }

    ash.mergeAndApply();

    // pop local param from stack
    if (param) {
        pop_local_var();
    }

    return err;
}

bool TryStatement::hasFinalReturn() const {
    // this works because try and rethrow both return true for hasFinalReturn
    // because throwing an exception trumpts any return statement
    return try_block && try_block->hasFinalReturn() && catch_block && catch_block->hasFinalReturn();
}

void TryStatement::parseCommit(QoreProgram* pgm) {
    AbstractStatement::parseCommit(pgm);
    if (try_block) {
        try_block->parseCommit(pgm);
    }
    if (catch_block) {
        catch_block->parseCommit(pgm);
    }
}
