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

#include <qore/AbstractPollState.h>

class qore_socket_private;

//! Canonical sync-over-async bridge for socket I/O.
/**
    Runs an @ref AbstractPollState synchronously to completion by looping
    continuePoll() and waiting for the requested socket readiness between
    iterations.  This is the single sanctioned path for implementing sync
    Socket APIs in terms of async poll-state primitives.

    The rationale and invariants are described in
    `design/async-socket-io.md` §"Sync I/O on Born-Non-Blocking Sockets".

    @note The long-term direction is a fully async core with sync APIs
    implemented as thin wrappers over internal async calls.  This helper
    is deliberately `intern/` only — external binary modules should use
    @ref QoreSocket's sync methods (which route through this helper
    internally) or, better, poll-state primitives directly.

    @since %Qore 2.3
*/
class SocketSyncPoll {
public:
    //! Runs a poll-state to completion synchronously.
    /** Loops:
        -# Call `state->continuePoll(xsink)`.
        -# If the return is `0`, the operation is complete — return `0`.
        -# If the return is `< 0`, an exception was raised — return the
           error code.
        -# If the return is @ref SOCK_POLLIN, wait for the socket to
           become readable via `isDataAvailable(timeout_ms, ...)` then
           retry.
        -# If the return is @ref SOCK_POLLOUT, wait for the socket to
           become writable via `isWriteFinished(timeout_ms, ...)` then
           retry.
        -# If any wait step times out, raise a timeout exception via
           `se_timeout()` and return @ref QSE_TIMEOUT.  A negative
           @a timeout_ms means "wait forever" — the underlying async
           wait primitives already honor this.

        Asserts (debug builds) that the caller is **not** running on an
        async I/O controller thread — sync loops on the I/O thread are a
        correctness bug and will deadlock the controller.

        @param sock the socket the poll-state is operating on; used for
            the readable/writable wait and for the I/O-thread assertion
        @param state the poll-state to run to completion; caller retains
            ownership and is responsible for its lifetime
        @param timeout_ms total timeout budget in milliseconds for the
            entire operation; negative means "no timeout"
        @param cname class name for exception messages (e.g. `"Socket"`)
        @param mname method name for exception messages (e.g. `"accept"`)
        @param xsink exception sink

        @return `0` on success, `< 0` on error (exception raised via
            @a xsink).  Callers that need structured output from the
            poll-state should invoke `state->takeOutput()` on success.

        @note If the wait step detects a closed socket (fd became
        invalid while waiting), the error propagates from the underlying
        wait primitive — callers do not need to handle that case.
    */
    DLLLOCAL static int run(qore_socket_private& sock,
                            AbstractPollState& state,
                            int timeout_ms,
                            const char* cname,
                            const char* mname,
                            ExceptionSink* xsink);

    //! Asserts the current thread is not an async I/O controller thread.
    /** Sync socket API entry points call this before doing any blocking
        work — sync-on-I/O-thread is a correctness bug that leads to
        deadlock of the controller.  In debug builds this is a hard
        assert; in release builds it's currently a no-op (Phase 4 of the
        sync-elimination plan will promote it to a raised exception).

        Safe to call from any code path; has no side effects beyond the
        assertion.

        @param cname class name for future exception text
        @param mname method name for future exception text
        @param xsink currently unused; reserved for the Phase 4 exception
    */
    DLLLOCAL static void assertNotOnIoThread(const char* cname,
                                             const char* mname,
                                             ExceptionSink* xsink);
};

#endif // _QORE_INTERN_SOCKETSYNCPOLL_H
