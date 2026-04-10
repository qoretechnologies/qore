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
#include <qore/QoreThreadLock.h>
#include "qore/intern/qore_socket_private.h"
#include "qore/intern/SocketSyncPoll.h"

#include <cassert>
#include <vector>

#ifdef HAVE_POLL
#include <poll.h>
#endif

int SocketSyncPoll::run(qore_socket_private& sock, AbstractPollState& state,
        int timeout_ms, const char* cname, const char* mname, ExceptionSink* xsink) {
    // Sync loops on the async I/O thread deadlock the controller — every
    // sync Socket API that ends up here must have been called from a
    // handler thread.  Debug-only for now; Phase 4 promotes this to a
    // raised exception in release builds.
    assertNotOnIoThread(cname, mname, xsink);

    while (true) {
        int rc = state.continuePoll(xsink);
        if (rc == 0) {
            // goal reached
            return 0;
        }
        if (rc < 0) {
            // poll-state raised the exception
            assert(*xsink || rc == QSE_TIMEOUT);
            return rc;
        }

        // SOCK_POLLIN / SOCK_POLLOUT — wait for the requested readiness.
        // A poll-state may legally ask for both (bitwise OR) on the same
        // call; we wait for whichever arrives first via asyncIoWait() so
        // SSL handshakes that toggle direction don't stall.
        bool want_read = (rc & SOCK_POLLIN) != 0;
        bool want_write = (rc & SOCK_POLLOUT) != 0;
        assert(want_read || want_write);

        // If the poll-state has extra fds to watch (e.g. QUIC migration
        // candidate), build a combined pollfd array and poll() directly
        // so a readiness event on any fd wakes us.
        std::vector<ExtraWaitFd> extras = state.getExtraWaitFds();
        int wait_rc;
        if (!extras.empty()) {
            wait_rc = waitMultiFd(sock, extras, want_read, want_write,
                timeout_ms, cname, mname, xsink);
        } else if (want_read && want_write) {
            // Ambivalent readiness — use the lower-level asyncIoWait()
            // directly to wait for either direction simultaneously.
            wait_rc = sock.asyncIoWait(timeout_ms, true, true, cname, mname, xsink);
        } else if (want_read) {
            // Use isDataAvailable() rather than asyncIoWait() so that
            // SSL-buffered bytes (which poll() cannot see) count as
            // readable and avoid a spurious timeout.
            wait_rc = sock.isDataAvailable(timeout_ms, mname, xsink) ? 1 : 0;
        } else {
            wait_rc = sock.isWriteFinished(timeout_ms, mname, xsink) ? 1 : 0;
        }

        if (*xsink) {
            return -1;
        }
        if (wait_rc <= 0) {
            // Timed out — raise a method-appropriate timeout exception.
            // Negative timeout_ms means "wait forever" and asyncIoWait()
            // will only return on error, which we would have caught via
            // *xsink above — so a timeout from the wait step always
            // means the caller explicitly set a finite budget.
            se_timeout(cname, mname, timeout_ms, xsink);
            return QSE_TIMEOUT;
        }

        // wait_rc > 0 — retry continuePoll()
    }
}

int SocketSyncPoll::waitMultiFd(qore_socket_private& sock,
        const std::vector<ExtraWaitFd>& extras,
        bool primary_want_read, bool primary_want_write,
        int timeout_ms, const char* cname, const char* mname,
        ExceptionSink* xsink) {
    assert(xsink);
#ifdef HAVE_POLL
    // Build pollfd array: primary fd (if open) + each extra fd.
    std::vector<struct pollfd> pfds;
    pfds.reserve(extras.size() + 1);
    if (sock.sock != QORE_INVALID_SOCKET) {
        struct pollfd p = {};
        p.fd = sock.sock;
        if (primary_want_read) {
            p.events |= POLLIN;
        }
        if (primary_want_write) {
            p.events |= POLLOUT;
        }
        if (p.events) {
            pfds.push_back(p);
        }
    }
    for (const auto& e : extras) {
        if (e.fd < 0) {
            continue;
        }
        struct pollfd p = {};
        p.fd = e.fd;
        if (e.want_read) {
            p.events |= POLLIN;
        }
        if (e.want_write) {
            p.events |= POLLOUT;
        }
        if (p.events) {
            pfds.push_back(p);
        }
    }
    if (pfds.empty()) {
        se_not_open(cname, mname, xsink, "waitMultiFd");
        return -1;
    }

    int pto = (timeout_ms < 0) ? -1 : timeout_ms;
    int rc;
    while (true) {
        rc = ::poll(pfds.data(), pfds.size(), pto);
        if (rc >= 0) {
            break;
        }
        if (errno == EINTR) {
            continue;
        }
        qore_socket_error(xsink, "SOCKET-SELECT-ERROR",
            "error in poll() during multi-fd wait", mname);
        return -1;
    }
    return rc;
#else
    // Platforms without poll() fall back to the primary fd only.
    (void)extras;
    return sock.asyncIoWait(timeout_ms, primary_want_read,
        primary_want_write, cname, mname, xsink);
#endif
}

int SocketSyncPoll::waitReleasingLock(qore_socket_private& sock, QoreThreadLock& outer_lock,
        bool want_read, bool want_write, int timeout_ms,
        const char* cname, const char* mname, ExceptionSink* xsink) {
    assert(xsink);
    assert(want_read || want_write);
    // Caller must hold the outer lock on entry.
    assert(outer_lock.trylock());

    // Snapshot the fd and its generation so we can detect close/fd-swap
    // by another thread during the unlocked wait window.
    const uint32_t gen_before = sock.fd_generation;
    const int saved_fd = sock.sock;
    if (saved_fd == QORE_INVALID_SOCKET) {
        se_not_open(cname, mname, xsink, "waitReleasingLock");
        return -1;
    }

    int wait_rc;
    {
        // Release the Socket's outer mutex for the duration of the wait
        // so other threads (including the async I/O controller) can
        // operate on the same Socket concurrently.  Re-acquired on
        // scope exit even if asyncIoWait() throws.
        AutoUnlocker au(outer_lock);
        wait_rc = sock.asyncIoWait(timeout_ms, want_read, want_write, cname, mname, xsink);
    }

    // After re-acquiring the lock, verify the socket wasn't closed or
    // had its fd swapped out from under us during the wait.  If it
    // was, the caller must abort its retry loop — the fd we were
    // waiting on no longer corresponds to what it had before.
    if (sock.fd_generation != gen_before || sock.sock != saved_fd) {
        if (!*xsink) {
            xsink->raiseException("SOCKET-CLOSED",
                "the underlying socket was closed or replaced while %s::%s() "
                "was waiting for I/O readiness", cname, mname);
        }
        return -1;
    }

    if (*xsink) {
        return -1;
    }
    return wait_rc;
}

void SocketSyncPoll::assertNotOnIoThread(const char* cname, const char* mname,
        ExceptionSink* xsink) {
    if (!qore_on_async_io_thread()) {
        return;
    }
    // A sync Socket API is running on the async I/O controller's
    // I/O thread.  This would deadlock the controller — the sync wait
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
            "thread — this would deadlock the controller; sync socket "
            "APIs must only be called from handler/worker threads",
            cname, mname);
    }
#ifdef DEBUG
    fprintf(stderr,
        "FATAL: sync %s::%s() called from the async I/O controller "
        "thread — this would deadlock the controller.  Sync socket "
        "APIs must only be called from handler/worker threads.\n",
        cname, mname);
    assert(false && "sync socket API called from async I/O thread");
#endif
}
