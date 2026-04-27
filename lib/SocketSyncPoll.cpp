/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    SocketSyncPoll.cpp

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
#include "qore/intern/qore_socket_private.h"
#include "qore/intern/SocketSyncPoll.h"

#include <cassert>

void SocketSyncPoll::assertNotOnIoThread(const char* cname, const char* mname,
        ExceptionSink* xsink) {
    if (!qore_on_async_io_thread()) {
        return;
    }
    // A sync Socket API is running on the async I/O controller's
    // I/O thread.  This would deadlock the controller: the sync wait
    // primitives block the very thread that is supposed to deliver the
    // readiness events.  Raise a fail-fast exception with a clear
    // message so the bug surfaces as close to its origin as possible.
    //
    // In debug builds we also abort() so the stack trace points directly
    // at the offending call site; in release builds the raised exception
    // is sufficient.
    if (xsink) {
        xsink->raiseException("SOCKET-SYNC-ON-IO-THREAD-ERROR",
            "synchronous %s::%s() called from the async I/O controller "
            "thread: this would deadlock the controller; sync socket "
            "APIs must only be called from handler/worker threads",
            cname, mname);
    }
#ifdef DEBUG
    fprintf(stderr,
        "FATAL: sync %s::%s() called from the async I/O controller "
        "thread: this would deadlock the controller.  Sync socket "
        "APIs must only be called from handler/worker threads.\n",
        cname, mname);
    assert(false && "sync socket API called from async I/O thread");
#endif
}
