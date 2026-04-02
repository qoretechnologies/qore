/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreAsyncIoLogger.h

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

#ifndef _QORE_QOREASYNCIOLOGGER_H

#define _QORE_QOREASYNCIOLOGGER_H

#include <cstdarg>

//! Sets the global async I/O logger
DLLLOCAL void qore_set_async_io_logger(QoreObject* logger_obj, ExceptionSink* xsink);

//! Returns the wrapped logger object (referenced), or nullptr
DLLLOCAL QoreObject* qore_get_async_io_logger_object();

//! Logs a message via the global async I/O logger (no-op if not set)
/** Uses snapshot+ref pattern: briefly locks to ref the logger, releases lock,
    then calls Qore methods (isEnabledFor, logArgs) outside the lock.
    Safe to call while holding other locks.
*/
DLLLOCAL void qore_async_io_log(int level, const char* fmt, ...);

//! va_list variant for forwarding from other variadic functions
DLLLOCAL void qore_async_io_log_v(int level, const char* fmt, va_list args);

//! Cleanup (called from qore_cleanup())
DLLLOCAL void qore_async_io_logger_cleanup();

// --- Low-level C trace for async I/O debugging ---
//
// Usage:  ASYNC_IO_TRACE("Phase2 SKIP key='%s' ready=%d\n", key.c_str(), ready);
//
// Compiled in when DEBUG or DEBUG_ASYNC_IO is defined.
// DEBUG_ASYNC_IO can be enabled independently in optimized builds via:
//   cmake -DCMAKE_CXX_FLAGS="-DDEBUG_ASYNC_IO" ...
//
// Enabled at runtime by setting QORE_ASYNC_IO_TRACE=1 env var.
// Output goes to stderr with a "[ASYNC-IO]" prefix and microsecond timestamp.
// When compiled out (default optimized builds), zero cost.

#if defined(DEBUG) || defined(DEBUG_ASYNC_IO)

//! Returns true if async I/O tracing is enabled (cached env var check)
DLLLOCAL bool qore_async_io_trace_enabled();

//! Writes a trace line to stderr (only if tracing is enabled)
DLLLOCAL void qore_async_io_trace(const char* fmt, ...)
#ifdef __GNUC__
    __attribute__((format(printf, 1, 2)))
#endif
    ;

#define ASYNC_IO_TRACE(...) do { \
    if (qore_async_io_trace_enabled()) { \
        qore_async_io_trace(__VA_ARGS__); \
    } \
} while (0)

#else
#define ASYNC_IO_TRACE(...) ((void)0)
#endif

#endif // _QORE_QOREASYNCIOLOGGER_H
