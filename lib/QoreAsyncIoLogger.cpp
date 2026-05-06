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
#include <unistd.h>
#include <sys/syscall.h>
#include <cstring>

static QoreThreadLock async_io_logger_lock;
static QoreLoggerBridge* async_io_logger = nullptr;

void qore_set_async_io_logger(QoreObject* logger_obj, ExceptionSink* xsink) {
    // Type safety is enforced at the Qore level via *LoggerInterfaceBase parameter type
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
    // Format and log.  QoreString::vsprintf returns -1 (no retry) when its
    // buffer is too small; re-copy the va_list and retry until it fits.
    // Without the loop, large substituted values silently render as empty
    // log messages.
    QoreStringNode* msg = new QoreStringNode();
    while (true) {
        va_list iter_args;
        va_copy(iter_args, args);
        int rc = msg->vsprintf(fmt, iter_args);
        va_end(iter_args);
        if (!rc) {
            break;
        }
    }
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

// --- Low-level C trace for async I/O debugging ---
#if defined(DEBUG) || defined(DEBUG_ASYNC_IO)

bool qore_async_io_trace_enabled() {
    static int enabled = -1;
    if (enabled < 0) {
        const char* val = getenv("QORE_ASYNC_IO_TRACE");
        enabled = (val && val[0] == '1') ? 1 : 0;
    }
    return enabled != 0;
}

void qore_async_io_trace(const char* fmt, ...) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tm;
    localtime_r(&ts.tv_sec, &tm);
    long tid = (long)syscall(SYS_gettid);
    char prefix[128];
    int plen = snprintf(prefix, sizeof(prefix),
        "[ASYNC-IO %04d-%02d-%02d %02d:%02d:%02d.%06ld tid=%ld] ",
        tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
        tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec / 1000, tid);
    if (plen < 0) plen = 0;
    if (plen >= (int)sizeof(prefix)) plen = sizeof(prefix) - 1;
    char body[4096];
    va_list args;
    va_start(args, fmt);
    int blen = vsnprintf(body, sizeof(body), fmt, args);
    va_end(args);
    if (blen < 0) blen = 0;
    if (blen >= (int)sizeof(body)) blen = sizeof(body) - 1;
    // Single atomic write — no interleaving between threads
    char combined[4096 + 256];
    int total = plen + blen;
    if (total >= (int)sizeof(combined)) total = sizeof(combined) - 1;
    memcpy(combined, prefix, plen);
    memcpy(combined + plen, body, (size_t)(total - plen));
    combined[total] = '\0';
    fputs(combined, stderr);
}

#endif // DEBUG || DEBUG_ASYNC_IO
