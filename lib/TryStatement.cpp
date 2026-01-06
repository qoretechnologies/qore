/*
    TryStatement.cpp

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
#include "qore/intern/TryStatement.h"
#include "qore/intern/StatementBlock.h"

TryStatement::TryStatement(const QoreProgramLocation* loc, StatementBlock* t, StatementBlock* c, char* p,
        const QoreTypeInfo* typeInfo, QoreParseTypeInfo* parseTypeInfo, const QoreProgramLocation* vloc)
        : AbstractStatement(loc), try_block(t), catch_block(c), param(p), typeInfo(typeInfo),
        parseTypeInfo(parseTypeInfo), loc(vloc) {
}

TryStatement::~TryStatement() {
    if (param)
        free(param);
    delete try_block;
    delete catch_block;
    delete parseTypeInfo;
}

int TryStatement::execImpl(QoreValue& return_value, ExceptionSink *xsink) {
    ValueHolder trv(xsink);

    QORE_TRACE("TryStatement::execImpl()");
    int rc = 0;
    if (try_block)
        rc = try_block->execImpl(*trv, xsink);

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

    if (!trv->isNothing()) {
        if (return_value.isNothing())
            return_value = trv.release();
    }

    return rc;
}

int TryStatement::parseInitImpl(QoreParseContext& parse_context) {
    // turn off top-level flag for statement vars
    QoreParseContextFlagHelper fh(parse_context);
    fh.unsetFlags(PF_TOP_LEVEL);

    int err = 0;

    if (try_block) {
        err = try_block->parseInitImpl(parse_context);
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
    }

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
