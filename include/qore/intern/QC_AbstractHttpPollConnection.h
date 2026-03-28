/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_AbstractHttpPollConnection.h

    Qore Programming Language

    Copyright (C) 2026 Qore Technologies, s.r.o.

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

#ifndef _QORE_CLASS_ABSTRACTHTTPPOLLCONNECTION_H

#define _QORE_CLASS_ABSTRACTHTTPPOLLCONNECTION_H

#include <qore/AbstractPrivateData.h>
#include <qore/QoreThreadLock.h>
#include <qore/QoreCondition.h>

#include <atomic>

//! C++ base class providing connection state management for HTTP poll connections
/** This class implements the connection state machine (CONNECTING, READY,
    DRAINING, CLOSED) entirely in C++, enabling:

    1. **I/O thread safety**: onConnectionReady() is pure C++ — no Qore
       interpreter involvement on the I/O thread (no Mutex, no evalMethod).

    2. **Lock-free state reads**: isReady(), isClosed(), isDraining() use
       atomic loads — no lock contention from app threads.

    3. **DGC-visible ownership**: Poll ops store the connection in a Qore
       internal_members slot (DGC can follow the ref) and keep only a raw
       C++ pointer to this priv data for I/O-thread calls.

    State values match Qore's HttpClientConnectionState enum:
    - CONNECTING = 0
    - READY = 1
    - DRAINING = 2
    - CLOSED = 3

    @par Lock ordering
    AbstractHttpPollConnectionPriv::lock (outermost) →
    Http{2,3}ClientPollOperationPriv::stream_lock (inner).
    onConnectionReady() only acquires this lock, never stream_lock.

    @since %Qore 2.3
*/
class AbstractHttpPollConnectionPriv : public AbstractPrivateData {
public:
    //! Connection states — values match Qore HttpClientConnectionState enum
    enum State : int {
        CONNECTING = 0,
        READY = 1,
        DRAINING = 2,
        CLOSED = 3
    };

    // --- I/O thread safe (pure C++, no Qore interpreter) ---

    //! Called by poll op from I/O thread when connection is established
    /** Pure C++: atomic state set + condition broadcast.
        No evalMethod, no Qore Mutex — safe on the I/O thread.
    */
    DLLLOCAL void onConnectionReady() {
        AutoLocker al(lock);
        if (state.load(std::memory_order_acquire) == CONNECTING) {
            state.store(READY, std::memory_order_release);
            ready_cond.broadcast();
        }
    }

    // --- App thread ---

    //! Blocks until state leaves CONNECTING or timeout expires
    /** @param timeout_ms timeout in milliseconds (-1 = no timeout)
        @return true if state is READY after wait
    */
    DLLLOCAL bool waitForReady(int64_t timeout_ms) {
        AutoLocker al(lock);
        if (timeout_ms < 0) {
            // No timeout — wait indefinitely
            while (state.load(std::memory_order_acquire) == CONNECTING) {
                ready_cond.wait(lock);
            }
        } else {
            // Compute absolute deadline
            int us;
            int64_t deadline_us = q_epoch_us(us) * 1000000LL + us + timeout_ms * 1000;
            while (state.load(std::memory_order_acquire) == CONNECTING) {
                int64_t now_us = q_epoch_us(us) * 1000000LL + us;
                int64_t remaining_us = deadline_us - now_us;
                if (remaining_us <= 0) {
                    return false;
                }
                int64_t remaining_ms = (remaining_us + 999) / 1000;
                if (ready_cond.wait2(lock, remaining_ms)) {
                    break;  // timeout
                }
            }
        }
        return state.load(std::memory_order_acquire) == READY;
    }

    //! Transitions to DRAINING (GOAWAY received)
    DLLLOCAL void setDraining() {
        AutoLocker al(lock);
        State cur = static_cast<State>(state.load(std::memory_order_acquire));
        if (cur == READY) {
            state.store(DRAINING, std::memory_order_release);
            ready_cond.broadcast();
        }
    }

    //! Transitions to CLOSED
    DLLLOCAL void setClosed() {
        AutoLocker al(lock);
        state.store(CLOSED, std::memory_order_release);
        ready_cond.broadcast();
    }

    // --- Lock-free accessors ---

    DLLLOCAL bool isReady() const {
        return state.load(std::memory_order_acquire) == READY;
    }

    DLLLOCAL bool isClosed() const {
        return state.load(std::memory_order_acquire) == CLOSED;
    }

    DLLLOCAL bool isDraining() const {
        return state.load(std::memory_order_acquire) == DRAINING;
    }

    //! Returns the current state as an int matching HttpClientConnectionState enum
    DLLLOCAL int getState() const {
        return state.load(std::memory_order_acquire);
    }

    DLLLOCAL void deref(ExceptionSink* xsink) {
        if (ROdereference()) {
            delete this;
        }
    }

private:
    //! Connection state — values match Qore HttpClientConnectionState enum
    std::atomic<int> state{CONNECTING};

    //! Lock for state transitions + condition broadcasts
    QoreThreadLock lock;

    //! Condition for waitForReady() and state transitions
    QoreCondition ready_cond;
};

DLLLOCAL QoreClass* initAbstractHttpPollConnectionClass(QoreNamespace& qorens);

#endif // _QORE_CLASS_ABSTRACTHTTPPOLLCONNECTION_H
