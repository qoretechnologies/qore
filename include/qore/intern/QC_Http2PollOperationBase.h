/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_Http2PollOperationBase.h

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

#ifndef _QORE_CLASS_HTTP2POLLOPERATIONBASE_H

#define _QORE_CLASS_HTTP2POLLOPERATIONBASE_H

#include "qore/SocketPollOperationBase.h"
#include "qore/QoreSocketObject.h"

#include <unordered_map>
#include <vector>

// Forward declarations for stream queue support
class Queue;
class QoreEventNotifier;

//! C++ private data for Http2PollOperationBase
/** Core state machine and inner op lifecycle for HTTP/2 server-side poll operations.

    State machine:
    - INIT: initial state, first read operation injected
    - READING: delegating to inner read operation (SocketHttp2ServerPollOperation)
    - WAIT_READ: empty read (control frames only), waiting for new socket data
    - REQUEST_READY: a complete HTTP/2 request is available via getOutput()
    - SENDING: delegating to inner send operation (response or streaming response)
    - SENT: response send completed
    - CLOSED: connection closed or fatal error

    The C++ base drives inner ops (read/send) that are injected from the Qore wrapper
    via injectReadOp() / injectSendOp().  Inner ops are created by Qore Socket methods
    (startPollReadHttp2Request, startPollSendHttp2Response, etc.) and the C++ base
    extracts the SocketPollOperationBase priv to drive continuePoll() directly.

    Thread safety: continuePoll() runs on the I/O thread; handler-thread methods
    (getOutput, injectSendOp, injectReadOp, continueReading) acquire op_lock.

    The Qore wrapper (Http2PollOperation) adds:
    - Stream queue management for CONNECT handlers
    - Completion context (setCompletionContext, onComplete)
    - Push promises (pushResource, getPushResourceCallback)

    @since %Qore 2.3
*/
class Http2PollOperationPriv : public SocketPollOperationBase {
public:
    //! HTTP/2 server poll operation states
    enum class H2ServerState {
        INIT,           //!< Initial state, first read op injected
        READING,        //!< Delegating to inner read operation
        WAIT_READ,      //!< Empty read, waiting for new socket data
        REQUEST_READY,  //!< Complete request available via getOutput()
        SENDING,        //!< Delegating to inner send operation
        SENT,           //!< Response send completed
        CLOSED          //!< Connection closed or fatal error
    };

    //! Creates the poll operation
    /** @param self the QoreObject wrapping this private data
        @param sock the client socket (ref'd, ownership transferred)
        @param initial_op the initial read SocketPollOperationBase (ref'd, ownership transferred)
        @param initial_op_obj the QoreObject wrapping initial_op (ref'd for DGC)
        @param headers_only if true, dispatch requests on HEADERS (before END_STREAM)
    */
    DLLLOCAL Http2PollOperationPriv(QoreObject* self, QoreSocketObject* sock,
        SocketPollOperationBase* initial_op, QoreObject* initial_op_obj, bool headers_only);

    DLLLOCAL ~Http2PollOperationPriv();

    //! Returns true when a request is ready or a send is complete
    DLLLOCAL bool goalReached() const override;

    //! Drives the state machine: delegates to inner ops, transitions on completion
    DLLLOCAL QoreHashNode* continuePoll(ExceptionSink* xsink) override;

    //! Aborts the operation: aborts inner op, transitions to CLOSED
    DLLLOCAL void abort(ExceptionSink* xsink) override;

    //! Returns the HTTP/2 request hash when ready (acquires op_lock)
    /** Clears request_pending so that concurrent continueReading() calls
        correctly throw instead of clearing a request being dispatched.
    */
    DLLLOCAL QoreValue getOutput() const override;

    //! Injects a send inner op (from Qore wrapper's startSendResponse)
    /** Called from the handler thread under op_lock.
        @param send_op the SocketPollOperationBase priv (ref'd, ownership transferred)
        @param send_op_obj the QoreObject wrapping send_op (ref'd for DGC)
        @param xsink exception sink
    */
    DLLLOCAL void injectSendOp(SocketPollOperationBase* send_op, QoreObject* send_op_obj,
        ExceptionSink* xsink);

    //! Injects a read inner op (from Qore wrapper's startReadNextRequest)
    /** Called from the handler thread under op_lock.
        @param read_op the SocketPollOperationBase priv (ref'd, ownership transferred)
        @param read_op_obj the QoreObject wrapping read_op (ref'd for DGC)
        @param xsink exception sink
    */
    DLLLOCAL void injectReadOp(SocketPollOperationBase* read_op, QoreObject* read_op_obj,
        ExceptionSink* xsink);

    //! Continues reading without creating a new read operation
    /** Called from the handler thread after a request has been dispatched in
        multiplexing mode.  The same underlying SocketHttp2ServerPollOperation
        continues reading frames.

        Thread safety: rejects the call if state is REQUEST_READY but the
        request hasn't been consumed via getOutput() yet.
    */
    DLLLOCAL void continueReading(ExceptionSink* xsink);

    //! Returns true if the connection is closed
    DLLLOCAL bool isClosed() const {
        return h2_state == H2ServerState::CLOSED;
    }

    //! Returns true if an error occurred
    DLLLOCAL bool hasError() const {
        return error_info != nullptr;
    }

    //! Returns error information if an error occurred (ref'd)
    DLLLOCAL QoreHashNode* getErrorInfo() const {
        if (error_info) {
            error_info->ref();
        }
        return error_info;
    }

    //! Returns the current H2 state as a string
    DLLLOCAL const char* getH2State() const {
        switch (h2_state) {
            case H2ServerState::INIT:           return "init";
            case H2ServerState::READING:        return "reading";
            case H2ServerState::WAIT_READ:      return "wait_read";
            case H2ServerState::REQUEST_READY:  return "request_ready";
            case H2ServerState::SENDING:        return "sending";
            case H2ServerState::SENT:           return "sent";
            case H2ServerState::CLOSED:         return "closed";
            default:                            return "unknown";
        }
    }

    //! Returns true if the connection should be kept alive
    DLLLOCAL bool shouldKeepAlive() const {
        return keep_alive && h2_state != H2ServerState::CLOSED && !error_info;
    }

    //! Information for a registered CONNECT stream queue
    struct StreamQueueInfo {
        Queue* queue;                       //!< ref'd Queue for data delivery
        QoreObject* queue_obj;              //!< ref'd QoreObject wrapping queue (for DGC)
        QoreEventNotifier* notifier;        //!< ref'd EventNotifier for wake-up, or nullptr
        QoreObject* notifier_obj;           //!< ref'd QoreObject wrapping notifier, or nullptr
    };

    //! Registers a Queue (and optional EventNotifier) for a CONNECT stream
    /** Called from the Qore wrapper when dispatching an extended CONNECT handler.
        The Queue is used for non-blocking data delivery from the I/O thread.
        The EventNotifier is signaled after data is pushed to the Queue so the
        handler thread can wake up.

        @param stream_id the HTTP/2 stream ID
        @param queue the Queue for data delivery (ref transferred)
        @param queue_obj the QoreObject wrapping queue (ref transferred)
        @param notifier optional EventNotifier for wake-up (ref transferred), or nullptr
        @param notifier_obj optional QoreObject wrapping notifier (ref transferred), or nullptr
    */
    DLLLOCAL void registerStreamQueue(int32_t stream_id, Queue* queue, QoreObject* queue_obj,
        QoreEventNotifier* notifier, QoreObject* notifier_obj);

    //! Returns and clears the list of stream IDs that had data drained
    /** Called by the controller after continuePoll() returns. For each
        stream ID, the controller dispatches onStreamData() to the worker pool.
    */
    DLLLOCAL std::vector<int32_t> getAndClearDataReadyStreams() {
        std::vector<int32_t> result;
        result.swap(data_ready_streams);
        return result;
    }

    //! Drains all registered stream queues after a read poll cycle
    /** Reads all available data from each registered stream's per-stream buffer
        and pushes it to the corresponding Queue.  When a stream is closed, a
        NOTHING sentinel is pushed and the registration is removed.

        Pure C++ — no Qore interpreter calls.

        @param xsink exception sink
    */
    DLLLOCAL void drainStreamQueues(ExceptionSink* xsink);

    //! Clears all stream queue registrations (called in abort/cleanup)
    /** Pushes NOTHING sentinels to all queues, notifies all EventNotifiers,
        and derefs all queue/notifier objects.

        @param xsink exception sink
    */
    DLLLOCAL void clearStreamQueues(ExceptionSink* xsink);

    //! Returns the headers_only flag
    DLLLOCAL bool isHeadersOnly() const {
        return headers_only;
    }

    //! Releases all internal references
    DLLLOCAL void cleanup(ExceptionSink* xsink);

protected:
    DLLLOCAL const char* getStateImpl() const override;

private:
    QoreSocketObject* sock;                     //!< ref'd client socket
    SocketPollOperationBase* current_op;         //!< ref'd inner poll operation (read or send)
    QoreObject* current_op_obj;                 //!< ref'd QoreObject wrapping current_op (for DGC)
    mutable QoreHashNode* request;              //!< ref'd, last completed request
    QoreHashNode* error_info;                   //!< ref'd error info
    H2ServerState h2_state;
    bool headers_only;                          //!< dispatch on HEADERS before END_STREAM
    mutable bool request_pending;               //!< set by continuePoll, cleared by getOutput
    bool keep_alive;                            //!< HTTP/2 connections are persistent by default
    int empty_read_count;                       //!< consecutive empty reads (control frames only)
    mutable QoreThreadLock op_lock;             //!< protects handler-thread methods

    //! Registered stream queues for CONNECT stream data delivery (stream_id -> info)
    std::unordered_map<int32_t, StreamQueueInfo> stream_queues;

    //! Stream IDs that had data drained in the last continuePoll cycle
    /** Populated by drainStreamQueues(), consumed by the controller after
        continuePoll() returns. The controller dispatches onStreamData()
        to the worker pool for each stream ID.
    */
    std::vector<int32_t> data_ready_streams;

    //! Maximum consecutive empty reads before closing the connection
    static constexpr int MAX_EMPTY_READS = 100;

    //! Releases the current inner operation and its wrapper object
    DLLLOCAL void releaseCurrentOp(ExceptionSink* xsink);

    //! Drives the read state (INIT, READING, WAIT_READ)
    /** @return poll info hash if more polling needed, nullptr if done
    */
    DLLLOCAL QoreHashNode* continueReadPoll(ExceptionSink* xsink);

    //! Drives the send state (SENDING)
    /** @return poll info hash if more polling needed, nullptr if done
    */
    DLLLOCAL QoreHashNode* continueSendPoll(ExceptionSink* xsink);
};

DLLLOCAL QoreClass* initHttp2PollOperationBaseClass(QoreNamespace& qorens);

#endif // _QORE_CLASS_HTTP2POLLOPERATIONBASE_H
