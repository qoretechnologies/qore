/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreChannel.h

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

#ifndef _QORE_INTERN_QORECHANNEL_H
#define _QORE_INTERN_QORECHANNEL_H

#include <qore/Qore.h>
#include <qore/QoreCondition.h>
#include <qore/QoreReferenceCounter.h>
#include <qore/AbstractPrivateData.h>

#include <deque>
#include <vector>

//! Heap-allocated, refcounted waiter for channel_select deterministic wakeup
/** channel_select creates one SelectWaiter, registers it with all channels.
    Channel operations copy the pointer (bumping refcount), release channel lock,
    then signal under the waiter's own lock.  Refcounting prevents use-after-free
    when channel_select returns while in-flight notifications hold pointers.
*/
struct SelectWaiter : public QoreReferenceCounter {
    QoreThreadLock lock;
    QoreCondition cond;
    bool notified = false;

    void ref() { ROreference(); }
    void deref() {
        if (ROdereference()) {
            delete this;
        }
    }
};

//! RAII helper that collects SelectWaiter pointers and notifies them on destruction
/** Declared before AutoLocker in channel methods so it destructs *after* the
    channel lock is released, ensuring notifications fire outside the channel lock.
*/
class WaiterNotifyHelper {
    std::vector<SelectWaiter*> waiters;

public:
    ~WaiterNotifyHelper() {
        flush();
    }

    //! Fire notifications immediately and clear the list
    void flush() {
        for (auto* w : waiters) {
            {
                AutoLocker al(&w->lock);
                w->notified = true;
                w->cond.broadcast();
            }
            w->deref();
        }
        waiters.clear();
    }

    //! Copy all registered external waiters (bumps refcount on each)
    void addAll(const std::vector<SelectWaiter*>& external_waiters) {
        for (auto* w : external_waiters) {
            w->ref();
            waiters.push_back(w);
        }
    }
};

//! A typed channel for inter-thread communication with close semantics
class QoreChannel : public AbstractPrivateData {
public:
    DLLLOCAL QoreChannel(int cap) : cap(cap), closed(false), deleted(false),
        send_waiting(0), recv_waiting(0) {
        // For unbuffered (cap=0): we use a single-slot handoff mechanism
        if (cap == 0) {
            unbuffered_has_value = false;
        }
    }

    //! Sends a value into the channel
    /** @param val the value to send
        @param timeout_ms timeout in milliseconds; 0 = infinite
        @param xsink exception sink
        @return 0 on success, -1 on error/timeout
    */
    DLLLOCAL int send(QoreValue val, int64 timeout_ms, ExceptionSink* xsink) {
        // Convention: timeout_ms <= 0 means infinite; waitWithInterrupt uses -1 for infinite
        int64 cond_timeout = (timeout_ms <= 0) ? -1 : timeout_ms;

        if (cap == 0) {
            // Unbuffered: synchronous handoff — two-phase with deferred notification
            // Phase 1: Wait for slot to be empty, place value, notify external waiters
            {
                WaiterNotifyHelper wnh;
                AutoLocker al(&lck);

                if (closed) {
                    val.discard(xsink);
                    xsink->raiseException("CHANNEL-CLOSED", "cannot send on a closed channel");
                    return -1;
                }

                ++send_waiting;
                while (unbuffered_has_value && !deleted && !closed) {
                    int rc = send_cond.waitWithInterrupt(&lck, cond_timeout, xsink);
                    if (rc == QORE_COND_RESULT_INTERRUPTED) {
                        --send_waiting;
                        val.discard(xsink);
                        return -1;
                    }
                    if (deleted || closed || !unbuffered_has_value) {
                        break;
                    }
                    if (rc == QORE_COND_RESULT_TIMEOUT) {
                        --send_waiting;
                        val.discard(xsink);
                        xsink->raiseException("CHANNEL-TIMEOUT",
                            "timed out after " QLLD "ms waiting to send on channel", timeout_ms);
                        return -1;
                    }
                }
                --send_waiting;

                if (deleted) {
                    val.discard(xsink);
                    xsink->raiseException("CHANNEL-ERROR", "Channel has been deleted in another thread");
                    return -1;
                }
                if (closed) {
                    val.discard(xsink);
                    xsink->raiseException("CHANNEL-CLOSED", "cannot send on a closed channel");
                    return -1;
                }

                unbuffered_value = val;
                unbuffered_has_value = true;
                recv_cond.signal();
                wnh.addAll(external_waiters);
            }
            // AutoLocker releases channel lock first, then WaiterNotifyHelper fires notifications

            // Phase 2: Wait for receiver to take the value (infinite wait — handoff must complete)
            {
                AutoLocker al(&lck);
                while (unbuffered_has_value && !deleted) {
                    int rc = send_cond.waitWithInterrupt(&lck, -1, xsink);
                    if (rc == QORE_COND_RESULT_INTERRUPTED) {
                        return -1;
                    }
                    if (deleted || !unbuffered_has_value) {
                        break;
                    }
                }

                if (deleted) {
                    xsink->raiseException("CHANNEL-ERROR", "Channel has been deleted in another thread");
                    return -1;
                }
            }

            return 0;
        }

        // Buffered: wait for space (cap < 0 means unlimited — never wait)
        WaiterNotifyHelper wnh;
        AutoLocker al(&lck);

        if (closed) {
            val.discard(xsink);
            xsink->raiseException("CHANNEL-CLOSED", "cannot send on a closed channel");
            return -1;
        }

        ++send_waiting;
        while (cap > 0 && (int)buffer.size() >= cap && !deleted && !closed) {
            int rc = send_cond.waitWithInterrupt(&lck, cond_timeout, xsink);
            if (rc == QORE_COND_RESULT_INTERRUPTED) {
                --send_waiting;
                val.discard(xsink);
                return -1;
            }
            if (deleted || closed || (int)buffer.size() < cap) {
                break;
            }
            if (rc == QORE_COND_RESULT_TIMEOUT) {
                --send_waiting;
                val.discard(xsink);
                xsink->raiseException("CHANNEL-TIMEOUT",
                    "timed out after " QLLD "ms waiting to send on channel", timeout_ms);
                return -1;
            }
        }
        --send_waiting;

        if (deleted) {
            val.discard(xsink);
            xsink->raiseException("CHANNEL-ERROR", "Channel has been deleted in another thread");
            return -1;
        }
        if (closed) {
            val.discard(xsink);
            xsink->raiseException("CHANNEL-CLOSED", "cannot send on a closed channel");
            return -1;
        }

        buffer.push_back(val);
        recv_cond.signal();
        wnh.addAll(external_waiters);
        return 0;
    }

    //! Receives a value from the channel
    /** @param timeout_ms timeout in milliseconds; 0 = infinite
        @param xsink exception sink
        @param timed_out set to true on timeout
        @param has_value set to true if a value was actually received (distinguishes NOTHING value from closed+drained)
        @return the received value, or NOTHING if channel is closed and drained
    */
    DLLLOCAL QoreValue recv(int64 timeout_ms, ExceptionSink* xsink, bool& timed_out, bool& has_value) {
        // Convention: timeout_ms <= 0 means infinite; waitWithInterrupt uses -1 for infinite
        int64 cond_timeout = (timeout_ms <= 0) ? -1 : timeout_ms;
        timed_out = false;
        has_value = false;

        WaiterNotifyHelper wnh;
        AutoLocker al(&lck);

        if (cap == 0) {
            // Unbuffered: wait for sender to provide a value
            ++recv_waiting;
            while (!unbuffered_has_value && !deleted && !closed) {
                int rc = recv_cond.waitWithInterrupt(&lck, cond_timeout, xsink);
                if (rc == QORE_COND_RESULT_INTERRUPTED) {
                    --recv_waiting;
                    return QoreValue();
                }
                if (deleted || closed || unbuffered_has_value) {
                    break;
                }
                if (rc == QORE_COND_RESULT_TIMEOUT) {
                    --recv_waiting;
                    timed_out = true;
                    return QoreValue();
                }
            }
            --recv_waiting;

            if (deleted) {
                xsink->raiseException("CHANNEL-ERROR", "Channel has been deleted in another thread");
                return QoreValue();
            }

            if (!unbuffered_has_value) {
                // Channel closed with no value available
                return QoreValue();
            }

            QoreValue rv = unbuffered_value;
            unbuffered_value = QoreValue();
            unbuffered_has_value = false;
            has_value = true;
            send_cond.signal();
            wnh.addAll(external_waiters);
            return rv;
        }

        // Buffered: wait for data
        ++recv_waiting;
        while (buffer.empty() && !deleted && !closed) {
            int rc = recv_cond.waitWithInterrupt(&lck, cond_timeout, xsink);
            if (rc == QORE_COND_RESULT_INTERRUPTED) {
                --recv_waiting;
                return QoreValue();
            }
            if (deleted || closed || !buffer.empty()) {
                break;
            }
            if (rc == QORE_COND_RESULT_TIMEOUT) {
                --recv_waiting;
                timed_out = true;
                return QoreValue();
            }
        }
        --recv_waiting;

        if (deleted) {
            xsink->raiseException("CHANNEL-ERROR", "Channel has been deleted in another thread");
            return QoreValue();
        }

        if (buffer.empty()) {
            // Channel closed with no buffered data
            return QoreValue();
        }

        QoreValue rv = buffer.front();
        buffer.pop_front();
        has_value = true;
        send_cond.signal();
        wnh.addAll(external_waiters);
        return rv;
    }

    //! Non-blocking send
    DLLLOCAL bool trySend(QoreValue val) {
        WaiterNotifyHelper wnh;
        AutoLocker al(&lck);
        if (closed || deleted) {
            return false;
        }
        if (cap == 0) {
            // Unbuffered: can only send if a receiver is waiting and slot is empty
            if (!unbuffered_has_value && recv_waiting > 0) {
                unbuffered_value = val;
                unbuffered_has_value = true;
                recv_cond.signal();
                wnh.addAll(external_waiters);
                return true;
            }
            return false;
        }
        if (cap > 0 && (int)buffer.size() >= cap) {
            return false;
        }
        buffer.push_back(val);
        recv_cond.signal();
        wnh.addAll(external_waiters);
        return true;
    }

    //! Non-blocking receive
    /** @param has_value set to true if a value was received
        @return the value or NOTHING
    */
    DLLLOCAL QoreValue tryRecv(bool& has_value) {
        WaiterNotifyHelper wnh;
        AutoLocker al(&lck);
        has_value = false;
        if (cap == 0) {
            if (unbuffered_has_value) {
                QoreValue rv = unbuffered_value;
                unbuffered_value = QoreValue();
                unbuffered_has_value = false;
                send_cond.signal();
                wnh.addAll(external_waiters);
                has_value = true;
                return rv;
            }
            return QoreValue();
        }
        if (buffer.empty()) {
            return QoreValue();
        }
        has_value = true;
        QoreValue rv = buffer.front();
        buffer.pop_front();
        send_cond.signal();
        wnh.addAll(external_waiters);
        return rv;
    }

    //! Closes the channel
    DLLLOCAL void close() {
        WaiterNotifyHelper wnh;
        AutoLocker al(&lck);
        closed = true;
        send_cond.broadcast();
        recv_cond.broadcast();
        wnh.addAll(external_waiters);
    }

    DLLLOCAL bool isClosed() const {
        AutoLocker al(&lck);
        return closed;
    }

    DLLLOCAL int size() const {
        AutoLocker al(&lck);
        if (cap == 0) {
            return unbuffered_has_value ? 1 : 0;
        }
        return (int)buffer.size();
    }

    DLLLOCAL int capacity() const {
        return cap;
    }

    DLLLOCAL bool empty() const {
        AutoLocker al(&lck);
        if (cap == 0) {
            return !unbuffered_has_value;
        }
        return buffer.empty();
    }

    //! Called from destructor
    DLLLOCAL void destructor(ExceptionSink* xsink) {
        WaiterNotifyHelper wnh;
        AutoLocker al(&lck);
        if (send_waiting > 0 || recv_waiting > 0) {
            xsink->raiseException("CHANNEL-ERROR",
                "Channel deleted while %d thread%s blocked on it",
                send_waiting + recv_waiting,
                (send_waiting + recv_waiting) == 1 ? " is" : "s are");
        }
        deleted = true;
        closed = true;
        send_cond.broadcast();
        recv_cond.broadcast();
        wnh.addAll(external_waiters);

        // Clean up any buffered values
        for (auto& v : buffer) {
            v.discard(xsink);
        }
        buffer.clear();
        if (unbuffered_has_value) {
            unbuffered_value.discard(xsink);
            unbuffered_has_value = false;
        }
    }

    //! Register an external waiter (for channel_select)
    DLLLOCAL void registerExternalWaiter(SelectWaiter* w) {
        w->ref();  // bump refcount for the channel's copy
        AutoLocker al(&lck);
        external_waiters.push_back(w);
    }

    //! Deregister an external waiter
    DLLLOCAL void deregisterExternalWaiter(SelectWaiter* w) {
        {
            AutoLocker al(&lck);
            for (auto it = external_waiters.begin(); it != external_waiters.end(); ++it) {
                if (*it == w) {
                    external_waiters.erase(it);
                    break;
                }
            }
        }
        w->deref();
    }

protected:
    DLLLOCAL virtual ~QoreChannel() {}

private:
    mutable QoreThreadLock lck;
    QoreCondition send_cond;
    QoreCondition recv_cond;
    std::deque<QoreValue> buffer;
    int cap;
    bool closed;
    bool deleted;
    int send_waiting;
    int recv_waiting;

    // Unbuffered mode
    QoreValue unbuffered_value;
    bool unbuffered_has_value = false;

    // External waiters for channel_select
    std::vector<SelectWaiter*> external_waiters;
};

#endif // _QORE_INTERN_QORECHANNEL_H
