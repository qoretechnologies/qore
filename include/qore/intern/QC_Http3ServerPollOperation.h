/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_Http3ServerPollOperation.h

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

#ifndef _QORE_CLASS_HTTP3SERVERPOLLOPERATION_H

#define _QORE_CLASS_HTTP3SERVERPOLLOPERATION_H

#include "qore/intern/QC_SocketPollOperationBase.h"
#include "qore/intern/QC_SocketPollOperation.h"

#include <atomic>
#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>

// Forward declarations
class Queue;

//! C++ implementation of Http3ServerPollOperation for I/O thread execution
/** Manages an HTTP/3 server connection lifecycle entirely in C++ so that
    continuePoll() runs on the I/O thread (same thread every time).  This fixes
    the STREAM-THREAD-ERROR caused by running Qore poll ops on random worker
    threads, where the one-shot reassignThread() on BinaryInputStream fails
    after the first call.

    State machine:
    - INIT -> READING -> REQUEST_READY -> SENDING -> SENT -> READING (loop)
    - DRAINING -> CLOSED (graceful shutdown via GOAWAY)

    Thread safety:
    - continuePoll() runs on the I/O thread (via SocketPollOperationBase fast path)
    - Handler-thread methods (startSendResponse, continueReading, shutdown) modify
      state under op_lock.  The I/O thread also acquires op_lock in continuePoll().
    - Completion context methods and stream registration are called from Qore code
      on handler/callback threads.

    Ownership model:
    - This class owns (refs): sock_obj, read_op, send_op, cached_request, error_info
    - cleanup() releases ALL refs — called from QPP destructor before deref()
    - getOutput() transfers cached_request ownership to caller
    - Stream queue and listener objects are stored as QoreObject* refs in the QPP layer

    @since %Qore 2.3
*/
class Http3ServerPollOperationPriv : public SocketPollOperationBase {
public:
    //! HTTP/3 server connection states
    enum class H3State {
        INIT,
        READING,
        WAIT_READ,
        REQUEST_READY,
        SENDING,
        SENT,
        DRAINING,
        CLOSED
    };

    //! Creates the poll operation
    /** @param self the QoreObject wrapping this private data
        @param sock the QoreSocketObject (ref'd by caller, ownership transferred)
        @param drain_timeout drain timeout
        @param headers_only if true, dispatch on HEADERS before full body
    */
    DLLLOCAL Http3ServerPollOperationPriv(QoreObject* self, QoreSocketObject* sock,
            std::chrono::microseconds drain_timeout, bool headers_only);

    DLLLOCAL virtual ~Http3ServerPollOperationPriv();

    // --- SocketPollOperationBase overrides ---

    DLLLOCAL bool goalReached() const override {
        H3State s = h3_state.load(std::memory_order_acquire);
        return s == H3State::REQUEST_READY || s == H3State::SENT;
    }

    DLLLOCAL QoreHashNode* continuePoll(ExceptionSink* xsink) override;
    DLLLOCAL void abort(ExceptionSink* xsink) override;
    DLLLOCAL QoreValue getOutput() const override;

    // --- Handler-thread methods (called from Qore code under op_lock) ---

    //! Starts sending an HTTP/3 response
    /** Creates a SocketQuicSendResponsePollOperation and transitions to SENDING.
        @param status_code HTTP status code
        @param headers response headers (may be nullptr)
        @param body response body (may be nullptr)
        @param xsink exception sink
    */
    DLLLOCAL void startSendResponse(int status_code, const QoreHashNode* headers,
            const AbstractQoreNode* body, ExceptionSink* xsink);

    //! Starts sending a streaming HTTP/3 response
    /** Creates a SocketQuicSendStreamingResponsePollOperation and transitions to SENDING.
        @param status_code HTTP status code
        @param headers response headers (may be nullptr)
        @param body_stream InputStream providing body data (ownership transferred)
        @param body_obj QoreObject wrapping the InputStream (ref'd, ownership transferred)
        @param chunk_size chunk size for reading from the InputStream
        @param xsink exception sink
    */
    DLLLOCAL void startSendStreamingResponse(int status_code, const QoreHashNode* headers,
            InputStream* body_stream, QoreObject* body_obj, int64_t chunk_size,
            ExceptionSink* xsink);

    //! Starts reading the next request (after sending a response)
    DLLLOCAL void startReadNextRequest(ExceptionSink* xsink);

    //! Continues reading without creating a new read operation
    DLLLOCAL void continueReading(ExceptionSink* xsink);

    //! Gracefully shuts down the QUIC connection
    /** @param session_id optional session ID; if 0, uses the cached request's session_id
    */
    DLLLOCAL void shutdown(int64_t session_id, ExceptionSink* xsink);

    // --- Stream data dispatch ---

    //! Registers a Queue for stream data delivery
    /** @param stream_key composite key "session_id:stream_id"
        @param queue the C++ QoreQueue pointer (not ref'd — QoreObject ref held in QPP layer)
    */
    DLLLOCAL void registerStreamQueue(const std::string& stream_key, Queue* queue) {
        AutoLocker al(op_lock);
        stream_queues[stream_key] = queue;
    }

    //! Returns and clears the list of stream keys with data available
    /** Called by the I/O controller after continuePoll().
    */
    DLLLOCAL std::vector<std::string> getAndClearDataReadyStreams() {
        std::vector<std::string> result;
        result.swap(data_ready_streams);
        return result;
    }

    // --- Accessors ---

    DLLLOCAL bool isClosed() const {
        return h3_state.load(std::memory_order_acquire) == H3State::CLOSED;
    }

    DLLLOCAL bool hasError() const {
        return error_info != nullptr;
    }

    DLLLOCAL QoreHashNode* getErrorInfo() const {
        if (error_info) {
            error_info->ref();
        }
        return error_info;
    }

    DLLLOCAL bool shouldKeepAlive() const {
        H3State s = h3_state.load(std::memory_order_acquire);
        return keep_alive && s != H3State::CLOSED && s != H3State::DRAINING && !error_info;
    }

    //! Cleanup all referenced objects (must be called before destructor)
    DLLLOCAL void cleanup(ExceptionSink* xsink);

    //! Starts the read request operation (creates a new SocketQuicServerPollOperation)
    /** Called from the QPP constructor and from startReadNextRequest().
    */
    DLLLOCAL void startReadRequest(ExceptionSink* xsink);

protected:
    DLLLOCAL const char* getStateImpl() const override;

private:
    //! Inner C++ read operation (ref'd, or nullptr)
    SocketQuicServerPollOperation* read_op = nullptr;

    //! Inner C++ send operation (ref'd, or nullptr; only during SENDING)
    SocketPollOperationBase* send_op = nullptr;

    //! The socket object (ref'd)
    QoreSocketObject* sock_obj = nullptr;

    //! Connection state (atomic for lock-free reads; mutable for getOutput() const)
    mutable std::atomic<H3State> h3_state{H3State::INIT};

    //! Mutex for thread safety between I/O thread and handler threads
    mutable QoreThreadLock op_lock;

    //! Cached completed request hash (ref'd, or nullptr)
    mutable QoreHashNode* cached_request = nullptr;

    //! Error info hash (ref'd, or nullptr)
    QoreHashNode* error_info = nullptr;

    //! Keep-alive flag (QUIC connections are persistent)
    bool keep_alive = true;

    //! Headers-only mode
    bool headers_only;

    //! Effective drain timeout
    std::chrono::microseconds drain_timeout;

    //! Consecutive empty reads counter
    mutable int empty_read_count = 0;

    //! Timestamp when draining started
    std::chrono::steady_clock::time_point drain_start;

    //! Session ID being drained
    int64_t drain_session_id = 0;

    //! Set to true by continuePoll() when a request is stored; cleared by getOutput()
    mutable bool request_pending = false;

    //! Registered stream data Queues (composite key -> C++ Queue*)
    /** The Queue objects are ref'd at the QoreObject level in the QPP layer.
        These raw pointers are used for empty() checks on the I/O thread
        without Qore interpreter overhead.
    */
    std::unordered_map<std::string, Queue*> stream_queues;

    //! Stream keys with data available (populated by continuePoll, consumed by controller)
    std::vector<std::string> data_ready_streams;

    static constexpr int MAX_EMPTY_READS = 100;

    // --- Internal methods (I/O thread only) ---

    DLLLOCAL QoreHashNode* handleReading(ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* handleSending(ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* handleDraining(ExceptionSink* xsink);
    DLLLOCAL void setError(const char* err, const char* desc, ExceptionSink* xsink);

    //! Check stream queues and populate data_ready_streams
    DLLLOCAL void checkStreamQueues();
};

DLLLOCAL QoreClass* initHttp3ServerPollOperationClass(QoreNamespace& qorens);

#endif // _QORE_CLASS_HTTP3SERVERPOLLOPERATION_H
