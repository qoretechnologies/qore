/* -*- indent-tabs-mode: nil -*- */
/*
    QoreAsyncIoLogger.cpp

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
#include "qore/intern/QoreAsyncIoLogger.h"

static QoreThreadLock async_io_logger_lock;
static QoreLoggerBridge* async_io_logger = nullptr;

void qore_set_async_io_logger(QoreObject* logger_obj, ExceptionSink* xsink) {
    if (logger_obj) {
        // Validate the logger object has the required methods
        const QoreClass* cls = logger_obj->getClass();
        if (!cls->findMethod("logArgs")) {
            xsink->raiseException("ASYNC-IO-LOGGER-ERROR",
                "logger object of class '%s' does not implement logArgs() method",
                cls->getName());
            return;
        }
        if (!cls->findMethod("isEnabledFor")) {
            xsink->raiseException("ASYNC-IO-LOGGER-ERROR",
                "logger object of class '%s' does not implement isEnabledFor() method",
                cls->getName());
            return;
        }
    }

    QoreLoggerBridge* old_logger;
    {
        AutoLocker al(async_io_logger_lock);
        old_logger = async_io_logger;
        async_io_logger = logger_obj ? new QoreLoggerBridge(logger_obj) : nullptr;
    }
    // Deref old bridge outside lock — destructor may call Qore user code
    if (old_logger) {
        old_logger->deref(xsink);
    }
}

QoreObject* qore_get_async_io_logger_object() {
    AutoLocker al(async_io_logger_lock);
    if (async_io_logger) {
        QoreObject* obj = async_io_logger->getObject();
        obj->ref();
        return obj;
    }
    return nullptr;
}

void qore_async_io_log_v(int level, const char* fmt, va_list args) {
    QoreLoggerBridge* lgr;
    {
        AutoLocker al(async_io_logger_lock);
        if (!async_io_logger) {
            return;   // fast path: no logger set
        }
        lgr = async_io_logger;
        lgr->ref();
    }
    // Check level outside lock (calls Qore method)
    if (!lgr->isEnabledFor(level)) {
        ExceptionSink xsink;
        lgr->deref(&xsink);
        return;
    }
    // Format and log
    QoreStringNode* msg = new QoreStringNode();
    msg->vsprintf(fmt, args);
    ExceptionSink xsink;
    lgr->logArgs(level, msg, nullptr, &xsink);
    msg->deref();
    lgr->deref(&xsink);
    if (xsink) {
        xsink.clear();
    }
}

void qore_async_io_log(int level, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    qore_async_io_log_v(level, fmt, args);
    va_end(args);
}

void qore_async_io_logger_cleanup() {
    QoreLoggerBridge* old_logger;
    {
        AutoLocker al(async_io_logger_lock);
        old_logger = async_io_logger;
        async_io_logger = nullptr;
    }
    // Deref outside lock — destructor may call Qore user code
    if (old_logger) {
        ExceptionSink xsink;
        old_logger->deref(&xsink);
    }
}
