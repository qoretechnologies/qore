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

#endif // _QORE_QOREASYNCIOLOGGER_H
