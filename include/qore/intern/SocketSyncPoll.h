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
class QoreThreadLock;

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

    //! Single readiness-wait cycle with the outer Socket mutex released.
    /** Releases @a outer_lock, waits for the requested readiness on the
        socket's primary fd via @c asyncIoWait(), then re-acquires the
        lock.  Used by direct sync I/O code (brecv, sendIntern,
        accept_intern, doSSLRW) so concurrent async operations on the
        same Socket are not blocked by a long-running sync wait.

        After re-acquiring the lock the helper verifies that the
        socket's fd_generation counter hasn't changed — if it has, the
        socket was closed or had its fd swapped while waiting and the
        caller must abort rather than retry its I/O.

        @param sock the socket to wait on
        @param outer_lock the mutex to release during the wait (the
            caller must hold it on entry; it is held again on return)
        @param want_read true to wait for POLLIN
        @param want_write true to wait for POLLOUT
        @param timeout_ms wait budget in milliseconds; negative means
            "wait forever"
        @param cname class name for error/timeout messages
        @param mname method name for error/timeout messages
        @param xsink exception sink

        @return
          - `> 0`: readiness reached — caller may retry I/O
          - `0`: timeout (no exception raised; caller decides whether
            to propagate via `se_timeout()`)
          - `< 0`: socket was closed during the wait, or the wait
            itself failed with an error (exception raised via @a xsink)

        @note The helper intentionally does **not** call
        `se_timeout()` on a timeout — that decision belongs to the
        caller, which knows the full operation context (total budget
        left, whether this was the first iteration, etc.).

        @since %Qore 2.3
    */
    DLLLOCAL static int waitReleasingLock(qore_socket_private& sock,
                                          QoreThreadLock& outer_lock,
                                          bool want_read, bool want_write,
                                          int timeout_ms,
                                          const char* cname, const char* mname,
                                          ExceptionSink* xsink);

    //! Wait on the primary socket fd plus any extra fds a poll-state surfaces.
    /** Used by @ref run() when `state.getExtraWaitFds()` is non-empty, so
        multi-fd poll-states (e.g. QUIC migration candidates) can wake on
        a readiness event on any of their fds.  Builds a pollfd array with
        the primary fd (if still open) followed by each extra fd, and
        calls `::poll()` directly.

        @param sock the Socket whose primary fd is included in the wait
        @param extras extra fds from @ref AbstractPollState::getExtraWaitFds()
        @param primary_want_read request POLLIN on the primary fd
        @param primary_want_write request POLLOUT on the primary fd
        @param timeout_ms wait budget in ms; negative means "wait forever"
        @param cname class name for error messages
        @param mname method name for error messages
        @param xsink exception sink

        @return `> 0` when at least one fd is ready, `0` on timeout, `< 0`
            on error (exception raised)
    */
    DLLLOCAL static int waitMultiFd(qore_socket_private& sock,
                                    const std::vector<ExtraWaitFd>& extras,
                                    bool primary_want_read,
                                    bool primary_want_write,
                                    int timeout_ms,
                                    const char* cname, const char* mname,
                                    ExceptionSink* xsink);
};

#endif // _QORE_INTERN_SOCKETSYNCPOLL_H
