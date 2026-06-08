/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    SocketSyncPoll.h

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

#ifndef _QORE_INTERN_SOCKETSYNCPOLL_H
#define _QORE_INTERN_SOCKETSYNCPOLL_H

#include <qore/common.h>

//! Guard helpers for legacy sync-over-async call sites.
/**
    Socket sync APIs should delegate to the async I/O controller.  This
    class only retains the fail-fast I/O-thread guard used by legacy
    HTTPClient/FtpClient sync entry points that block waiting for
    controller completion.

    @since %Qore 3.0
*/
class SocketSyncPoll {
public:
    //! Asserts the current thread is not an async I/O controller thread.
    /** Sync socket API entry points call this before doing any blocking
        work: sync-on-I/O-thread is a correctness bug that leads to
        deadlock of the controller (the wait primitives block the very
        thread that is supposed to deliver the readiness events).

        On violation:
        - Raises @c SOCKET-SYNC-ON-IO-THREAD-ERROR on @a xsink in both
          debug and release builds, so the bug surfaces as a clean
          exception at the offending call site instead of a deadlock.
        - In debug builds additionally `abort()`s after logging the
          violation to `stderr`, so the stack trace in the core file
          points directly at the offending frame.

        Safe to call from any code path; has no side effects when
        running on a handler/worker thread.

        @param cname class name for the exception message (e.g. `"Socket"`,
            `"HTTPClient"`, `"FtpClient"`)
        @param mname method name for the exception message
        @param xsink exception sink (may be NULL for callers that cannot
            raise; the debug abort() still fires in that case)
    */
    DLLLOCAL static void assertNotOnIoThread(const char* cname,
                                             const char* mname,
                                             ExceptionSink* xsink);
};

#endif // _QORE_INTERN_SOCKETSYNCPOLL_H
