/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_HttpKeepAlivePollOperationBase.h

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

#ifndef _QORE_CLASS_HTTPKEEPALIVEPOLLOPERATIONBASE_H

#define _QORE_CLASS_HTTPKEEPALIVEPOLLOPERATIONBASE_H

#include "qore/SocketPollOperationBase.h"
#include "qore/SocketPollOperation.h"
#include "qore/QoreSocketObject.h"

//! Default header read timeout: 5 seconds in microseconds (slow-loris protection)
constexpr int64_t DEFAULT_HEADER_READ_TIMEOUT_US = 5000000LL;

//! C++ private data for HttpKeepAlivePollOperationBase
/** Combined idle + header read poll operation for HTTP keep-alive connections.

    This operation has two phases:
    1. **Idle phase**: Monitors for incoming data (like HttpIdlePollOperation) —
       checks timeout, socket liveness, data availability. Returns POLLIN with
       poll_timeout_ms for the idle deadline.
    2. **Header phase**: Delegates to SocketReadHttpHeaderPollOperation (C++ inner op).
       Has its own timeout for slow-loris protection.

    When data arrives during idle, transitions to header reading inline
    (no return to controller).

    @since %Qore 2.3
*/
class HttpKeepAlivePollOperationPriv : public SocketPollOperationBase {
public:
    //! Operation phases
    enum class Phase {
        IDLE,           //!< Waiting for data or timeout
        HEADER          //!< Reading HTTP headers
    };

    //! Operation states
    enum class State {
        IDLE,               //!< Waiting for data or timeout (idle phase)
        HEADER_READING,     //!< Reading HTTP headers (header phase)
        HEADER_COMPLETE,    //!< Headers successfully read
        CLOSED,             //!< Socket was closed by peer or aborted
        TIMEOUT             //!< Timeout expired (idle or header)
    };

    //! Creates the keep-alive poll operation
    /** @param self the QoreObject wrapping this private data
        @param sock the socket to monitor (will be ref'd)
        @param ttl_us keep-alive timeout in microseconds
    */
    DLLLOCAL HttpKeepAlivePollOperationPriv(QoreObject* self, QoreSocketObject* sock, int64_t ttl_us);

    DLLLOCAL ~HttpKeepAlivePollOperationPriv();

    //! Returns true when the operation is complete
    DLLLOCAL bool goalReached() const override;

    //! Continues the poll operation — dispatches on phase (idle vs header)
    DLLLOCAL QoreHashNode* continuePoll(ExceptionSink* xsink) override;

    //! Aborts the operation by setting state to CLOSED
    DLLLOCAL void abort(ExceptionSink* xsink) override;

    //! Returns header output if available, else state hash
    DLLLOCAL QoreValue getOutput() const override;

    //! Returns true if the operation timed out (idle or header)
    DLLLOCAL bool timedOut() const {
        return op_state == State::TIMEOUT;
    }

    //! Returns true if headers were successfully read
    DLLLOCAL bool headersReady() const {
        return op_state == State::HEADER_COMPLETE;
    }

    //! Returns true if the connection was closed
    DLLLOCAL bool wasClosed() const {
        return op_state == State::CLOSED;
    }

    //! Returns true if the socket should be auto-closed on completion
    /** Terminal states (CLOSED, TIMEOUT) mean the connection is dead.
        HEADER_COMPLETE means the socket will be reused for request handling.
    */
    DLLLOCAL bool needsCloseOnComplete() const override {
        return op_state == State::CLOSED || op_state == State::TIMEOUT;
    }

    //! Returns the current phase as a string
    DLLLOCAL const char* getPhase() const {
        switch (phase) {
            case Phase::IDLE:   return "idle";
            case Phase::HEADER: return "header";
            default:            return "unknown";
        }
    }

    //! Resets all state for another idle cycle (persistent registration)
    DLLLOCAL void resetForIdle();

    //! Releases internal references
    DLLLOCAL void cleanup(ExceptionSink* xsink);

protected:
    DLLLOCAL const char* getStateImpl() const override;

private:
    QoreSocketObject* sock_obj;                     //!< ref'd socket
    SocketReadHttpHeaderPollOperation* header_op;    //!< ref'd inner op for header phase
    QoreHashNode* header_output;                     //!< ref'd result from header read
    Phase phase;
    State op_state;
    int64_t deadline_us;                            //!< absolute deadline for idle phase
    int64_t header_deadline_us;                     //!< absolute deadline for header phase
    int64_t ttl_us;                                 //!< keep-alive timeout duration in microseconds
    bool registered_pollin = false;                  //!< True after first POLLIN registration

    //! Idle phase polling — register POLLIN on first call, transition to header on subsequent
    DLLLOCAL QoreHashNode* continueIdlePoll(ExceptionSink* xsink);

    //! Returns POLLIN poll_info with idle deadline timeout
    DLLLOCAL QoreHashNode* getIdlePollInfo(ExceptionSink* xsink);

    //! Transitions from idle to header reading phase
    DLLLOCAL QoreHashNode* transitionToHeaderPhase(ExceptionSink* xsink);

    //! Header phase polling — delegates to inner SocketReadHttpHeaderPollOperation
    DLLLOCAL QoreHashNode* continueHeaderPoll(ExceptionSink* xsink);

    //! Returns current time in microseconds since epoch
    DLLLOCAL static int64_t nowUs() {
        int us;
        int64_t secs = q_epoch_us(us);
        return secs * 1000000LL + us;
    }
};

DLLLOCAL QoreClass* initHttpKeepAlivePollOperationBaseClass(QoreNamespace& qorens);

#endif // _QORE_CLASS_HTTPKEEPALIVEPOLLOPERATIONBASE_H
