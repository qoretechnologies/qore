/* -*- indent-tabs-mode: nil -*- */
/*
    QoreLoggerBridge.cpp

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
#include "qore/intern/QoreLoggerBridge.h"
#include "qore/intern/qore_thread_intern.h"

namespace {
//! Enters the logger object's program context when the calling thread has none
/** The bridge is called from native threads that never entered a %Qore program: the async I/O
    controller's I/O threads log directly from their event loop (ex:
    AsyncIoControllerPriv::ioThread() reporting an event loop error), and every logged message is
    rendered with q_sprintf(), which reads parse options from the current program.  With no program
    context the current program is nullptr, and running the logger's %Qore code from such a thread
    crashes.

    Entering the context also adds this thread to the program's thread count, so the program cannot
    complete teardown while its logger is running.

    If the context cannot be entered - the program is already past its teardown gate - the helper is
    invalid and the caller must drop the message; executing %Qore code in the program is no longer
    safe at that point.

    A thread that already has a program context is left alone: switching it to the logger's program
    would change the behavior of ordinary %Qore-thread logging.
*/
class LoggerProgramContextHelper {
public:
    DLLLOCAL LoggerProgramContextHelper(QoreProgram* pgm) {
        if (!pgm || getProgram()) {
            return;
        }
        // use a temporary sink: a failure to enter the context is not the caller's exception, and
        // the caller's sink may already hold an unrelated exception
        ExceptionSink xsink;
        pch.set(&xsink, pgm, true);
        if (xsink) {
            xsink.clear();
            valid = false;
        }
    }

    DLLLOCAL operator bool() const {
        return valid;
    }

private:
    ProgramThreadCountContextHelper pch;
    bool valid = true;
};
}

QoreLoggerBridge::QoreLoggerBridge(QoreObject* logger_obj)
    : logger_obj(logger_obj), pgm(logger_obj->getProgram()), logArgsMethod(nullptr),
        isEnabledForMethod(nullptr) {
    assert(logger_obj);
    logger_obj->ref();
    const QoreClass* cls = logger_obj->getClass();
    logArgsMethod = cls->findMethod("logArgs");
    isEnabledForMethod = cls->findMethod("isEnabledFor");
}

QoreLoggerBridge::~QoreLoggerBridge() {
}

void QoreLoggerBridge::logArgs(int level, const QoreStringNode* msg,
        const QoreListNode* args, ExceptionSink* xsink) {
    if (!logArgsMethod) {
        return;
    }
    LoggerProgramContextHelper pch(pgm);
    if (!pch) {
        return;
    }
    ReferenceHolder<QoreListNode> call_args(new QoreListNode(autoTypeInfo), xsink);
    call_args->push(level, xsink);
    if (msg) {
        call_args->push(msg->refSelf(), xsink);
    } else {
        call_args->push(QoreValue(), xsink);
    }
    if (args) {
        call_args->push(args->refSelf(), xsink);
    }
    ValueHolder rv(logger_obj->evalMethod(*logArgsMethod, *call_args, xsink), xsink);
}

bool QoreLoggerBridge::isEnabledFor(int level) const {
    if (!isEnabledForMethod) {
        return false;
    }
    LoggerProgramContextHelper pch(pgm);
    if (!pch) {
        return false;
    }
    ExceptionSink xsink;
    ReferenceHolder<QoreListNode> call_args(new QoreListNode(autoTypeInfo), &xsink);
    call_args->push(level, &xsink);
    ValueHolder rv(logger_obj->evalMethod(*isEnabledForMethod, *call_args, &xsink), &xsink);
    if (xsink) {
        return false;
    }
    return rv->getAsBool();
}

void QoreLoggerBridge::deref(ExceptionSink* xsink) {
    if (ROdereference()) {
        // releasing the last reference runs the logger object's destructor, which is Qore code, and
        // this can happen on an I/O thread (AsyncIoControllerPriv::log() derefs its snapshot of the
        // logger there).  Unlike a log call this cannot be skipped when the context cannot be
        // entered: the reference must be released either way
        LoggerProgramContextHelper pch(pgm);
        logger_obj->deref(xsink);
        logger_obj = nullptr;
        delete this;
    }
}
