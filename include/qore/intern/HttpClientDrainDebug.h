/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    HttpClientDrainDebug.h

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

#ifndef _QORE_INTERN_HTTPCLIENTDRAINDEBUG_H
#define _QORE_INTERN_HTTPCLIENTDRAINDEBUG_H

#include <qore/Qore.h>

#include <atomic>
#include <cstddef>

#ifdef DEBUG
//! The state of the debug hooks that hold the responses of HTTP/2 and HTTP/3 clients (issue #5839)
/** While responses are held, the HTTP/2 and HTTP/3 client poll operations read and process the data of their
    connections but dispatch no response, so that the completed responses accumulate; a test waits for them with
    dbg_http_client_wait_held_outputs(), and the read pass after the hold dispatches them all at once.  The hold is
    process-wide and covers every response of a client, including the response to an extended CONNECT request and,
    for HTTP/2, the data of CONNECT tunnels.
*/
class QoreHttpClientDrainDebug {
public:
    //! The drain limit set by dbg_http_client_drain_limit(); 0: the limit of the operation
    std::atomic<int> drain_limit{0};

    //! True while the responses of clients are held; checked without the lock first
    std::atomic<bool> hold{false};

    //! The read passes that returned with responses left and no immediate re-poll requested for them
    std::atomic<int64_t> stranded_passes{0};

    //! Returns the state, which is never destroyed
    DLLLOCAL static QoreHttpClientDrainDebug& get() {
        static QoreHttpClientDrainDebug* state = new QoreHttpClientDrainDebug;
        return *state;
    }

    //! Records the number of completed responses that a read pass found held
    DLLLOCAL void recordHeld(size_t count) {
        AutoLocker al(lock);
        held = count;
        cond.broadcast();
    }

    //! Waits until a read pass found at least the given number of completed responses held
    /** @return false if the wait timed out
    */
    DLLLOCAL bool waitHeld(size_t count, int64_t timeout_ms) {
        int64_t deadline = q_clock_getmillis() + timeout_ms;
        AutoLocker al(lock);
        while (held < count) {
            int64_t remaining = deadline - q_clock_getmillis();
            if (remaining <= 0) {
                return false;
            }
            cond.wait(lock, static_cast<int>(remaining > INT_MAX ? INT_MAX : remaining));
        }
        return true;
    }

    //! Holds the responses of clients from now on, or dispatches them again
    DLLLOCAL void setHold(bool h) {
        AutoLocker al(lock);
        held = 0;
        hold.store(h, std::memory_order_release);
    }

private:
    QoreThreadLock lock;
    QoreCondition cond;
    //! the number of completed responses that the last read pass found held
    size_t held = 0;
};
#endif

//! Returns the number of responses that an HTTP/2 or HTTP/3 client poll operation dispatches in one read pass
/** @param dflt the limit of the operation

    In a library-debug build, a test can lower it with dbg_http_client_drain_limit() (issue #5839).
*/
DLLLOCAL inline int qore_http_client_drain_limit(int dflt) {
#ifdef DEBUG
    int limit = QoreHttpClientDrainDebug::get().drain_limit.load(std::memory_order_acquire);
    if (limit > 0) {
        return limit;
    }
#endif
    return dflt;
}

//! Requests an immediate re-poll of a client read pass that returns with responses left (issue #5839)
/** No socket event or timer may come for the responses that a read pass leaves in its operation; the pass after
    this one dispatches them.

    @param poll_info the poll info that the pass returns; may be nullptr
    @param remaining true if the pass leaves responses that it has not dispatched
    @param xsink for exceptions
*/
DLLLOCAL inline QoreHashNode* qore_http_client_finish_read_pass(QoreHashNode* poll_info, bool remaining,
        ExceptionSink* xsink) {
    if (poll_info && remaining) {
        poll_info->setKeyValue("poll_timeout_ms", 0, xsink);
    }
#ifdef DEBUG
    // a test checks that no pass leaves responses without a re-poll; a pass without a poll info ended in an error
    if (remaining && poll_info && poll_info->getKeyValue("poll_timeout_ms").getAsBigInt() != 0) {
        QoreHttpClientDrainDebug::get().stranded_passes.fetch_add(1, std::memory_order_relaxed);
    }
#endif
    return poll_info;
}

#endif
