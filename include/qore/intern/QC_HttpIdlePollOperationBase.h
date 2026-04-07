/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_HttpIdlePollOperationBase.h

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

#ifndef _QORE_CLASS_HTTPIDLEPOLLOPERATIONBASE_H

#define _QORE_CLASS_HTTPIDLEPOLLOPERATIONBASE_H

#include "qore/SocketPollOperationBase.h"
#include "qore/QoreSocketObject.h"

//! C++ private data for HttpIdlePollOperationBase
/** Monitors an idle HTTP keep-alive connection for:
    - New data arriving (indicating a new request)
    - Connection close by peer
    - Timeout expiration

    @since %Qore 2.3
*/
class HttpIdlePollOperationPriv : public SocketPollOperationBase {
public:
    //! Idle connection states
    enum class IdleState {
        IDLE,               //!< Waiting for data or timeout
        DATA_AVAILABLE,     //!< Data arrived on the socket
        CLOSED,             //!< Socket was closed by peer
        TIMEOUT             //!< Keep-alive timeout expired
    };

    //! Creates the idle poll operation
    /** @param self the QoreObject wrapping this private data
        @param sock the socket to monitor (will be ref'd)
        @param ttl_us keep-alive timeout in microseconds
    */
    DLLLOCAL HttpIdlePollOperationPriv(QoreObject* self, QoreSocketObject* sock, int64_t ttl_us);

    DLLLOCAL ~HttpIdlePollOperationPriv();

    //! Returns true when the operation is no longer idle
    DLLLOCAL bool goalReached() const override;

    //! Continues the poll operation
    /** Checks state, deadline, socket liveness, and data availability.
        @return poll info hash if more polling needed, or nullptr if done
    */
    DLLLOCAL QoreHashNode* continuePoll(ExceptionSink* xsink) override;

    //! Aborts the operation by setting state to CLOSED
    DLLLOCAL void abort(ExceptionSink* xsink) override;

    //! Returns output hash with sock, state, timed_out, closed, data_available
    DLLLOCAL QoreValue getOutput() const override;

    //! Returns true if the connection timed out
    DLLLOCAL bool timedOut() const {
        return idle_state == IdleState::TIMEOUT;
    }

    //! Returns true if data is available for reading
    DLLLOCAL bool hasData() const {
        return idle_state == IdleState::DATA_AVAILABLE;
    }

    //! Returns true if the connection was closed
    DLLLOCAL bool wasClosed() const {
        return idle_state == IdleState::CLOSED;
    }

    //! Returns true if the socket should be auto-closed on completion
    /** Terminal states (CLOSED, TIMEOUT) mean the connection is dead.
        DATA_AVAILABLE means the socket will be reused for request handling.
    */
    DLLLOCAL bool needsCloseOnComplete() const override {
        return idle_state == IdleState::CLOSED || idle_state == IdleState::TIMEOUT;
    }

    //! Resets the timeout deadline from now
    DLLLOCAL void resetTimeout();

    //! Releases the socket reference
    DLLLOCAL void cleanup(ExceptionSink* xsink);

protected:
    DLLLOCAL const char* getStateImpl() const override;

private:
    QoreSocketObject* sock_obj;         //!< ref'd socket
    IdleState idle_state = IdleState::IDLE;
    int64_t deadline_us;                //!< absolute deadline in microseconds since epoch
    int64_t ttl_us;                     //!< keep-alive timeout duration in microseconds

    //! Returns current time in microseconds since epoch
    DLLLOCAL static int64_t nowUs() {
        int us;
        int64_t secs = q_epoch_us(us);
        return secs * 1000000LL + us;
    }
};

DLLLOCAL QoreClass* initHttpIdlePollOperationBaseClass(QoreNamespace& qorens);

#endif // _QORE_CLASS_HTTPIDLEPOLLOPERATIONBASE_H
