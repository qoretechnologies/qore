/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_HttpWebSocketPollOperationBase.h

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

#ifndef _QORE_CLASS_HTTPWEBSOCKETPOLLOPERATIONBASE_H

#define _QORE_CLASS_HTTPWEBSOCKETPOLLOPERATIONBASE_H

#include "qore/SocketPollOperationBase.h"
#include "qore/SocketPollOperation.h"
#include "qore/QoreSocketObject.h"
#include "qore/QoreQueue.h"
#include "qore/WebSocketFrameCodec.h"

//! C++ private data for HttpWebSocketPollOperationBase
/** Core WebSocket frame I/O state machine in pure C++, enabling the
    AsyncIoController I/O thread to drive the poll without Qore interpreter
    overhead.

    The C++ base handles the hot path:
    - Read from socket -> decode frames via WebSocketFrameDecoder/Reassembler
    - Push decoded frames to recv_queue as QoreHashNode entries
    - Dequeue pre-encoded frame data from send_queue -> send via inner op
    - Auto ping/pong (pong sent automatically on ping)
    - Close handshake (echo close frame if not initiated)

    State machine: CONNECTED -> CLOSING -> CLOSED / ERROR

    Thread safety:
    - continuePoll() runs on the I/O thread only
    - sendText/sendBinary/sendPing/sendClose push to send_queue (thread-safe Queue)
    - getReceivedFrame/hasReceivedFrames read from recv_queue (thread-safe Queue)

    The Qore wrapper (HttpWebSocketPollOperation) adds:
    - Channel-based API (sendText, sendBinary, sendPing, getReceivedFrame)
    - Completion context for HttpConnectionPollOperationBase

    @since %Qore 2.3
*/
class HttpWebSocketPollOperationPriv : public SocketPollOperationBase {
public:
    //! WebSocket connection states
    enum class WsState {
        CONNECTED,  //!< WebSocket connection is active
        CLOSING,    //!< Close handshake in progress
        CLOSED,     //!< Connection cleanly closed
        ERROR       //!< Fatal error occurred
    };

    //! Creates the WebSocket poll operation
    /** @param self the QoreObject wrapping this private data
        @param sock the WebSocket socket (ref'd, ownership transferred)
        @param recv_queue the Queue for pushing received frames (ref'd, ownership transferred)
        @param recv_queue_obj the QoreObject wrapping recv_queue (ref'd for DGC)
        @param send_queue the Queue for dequeuing frames to send (ref'd, ownership transferred)
        @param send_queue_obj the QoreObject wrapping send_queue (ref'd for DGC)
    */
    DLLLOCAL HttpWebSocketPollOperationPriv(QoreObject* self, QoreSocketObject* sock,
        Queue* recv_queue, QoreObject* recv_queue_obj,
        Queue* send_queue, QoreObject* send_queue_obj);

    DLLLOCAL ~HttpWebSocketPollOperationPriv();

    //! Returns true when the connection is closed or in error
    DLLLOCAL bool goalReached() const override;

    //! Drives the WebSocket I/O state machine
    /** 1. Check state (CLOSED/ERROR -> return nullptr)
        2. If send_op active: drive it; when done, release it
        3. Try non-blocking recv via inner recv_op
        4. When recv data arrives: feed to decoder, process frames
        5. If send_queue has data and no send_op: create SocketSendPollOperation
        6. Return POLLIN | (POLLOUT if sending)
    */
    DLLLOCAL QoreHashNode* continuePoll(ExceptionSink* xsink) override;

    //! Aborts the operation: transitions to CLOSED
    DLLLOCAL void abort(ExceptionSink* xsink) override;

    //! Returns the output of the operation
    DLLLOCAL QoreValue getOutput() const override;

    //! Returns true if the connection is closed
    DLLLOCAL bool isClosed() const {
        return ws_state == WsState::CLOSED || ws_state == WsState::ERROR;
    }

    //! Returns the WebSocket close code
    DLLLOCAL uint16_t getCloseCode() const {
        return close_code;
    }

    //! Returns the WebSocket close reason
    DLLLOCAL const char* getCloseReason() const {
        return close_reason.c_str();
    }

    //! Returns the current state as a string
    DLLLOCAL const char* getWsState() const {
        switch (ws_state) {
            case WsState::CONNECTED: return "connected";
            case WsState::CLOSING:   return "closing";
            case WsState::CLOSED:    return "closed";
            case WsState::ERROR:     return "error";
            default:                 return "unknown";
        }
    }

    //! Returns true if the error state is set
    DLLLOCAL bool hasError() const {
        return error_info != nullptr;
    }

    //! Returns ref'd error info hash or nullptr
    DLLLOCAL QoreHashNode* getErrorInfo() const {
        if (error_info) {
            error_info->ref();
        }
        return error_info;
    }

    //! Queues a text frame for sending (thread-safe)
    /** @param text the UTF-8 text to send
        @param xsink exception sink
    */
    DLLLOCAL void queueSendText(const QoreStringNode* text, ExceptionSink* xsink);

    //! Queues a binary frame for sending (thread-safe)
    /** @param data the binary data to send
        @param xsink exception sink
    */
    DLLLOCAL void queueSendBinary(const BinaryNode* data, ExceptionSink* xsink);

    //! Queues a ping frame for sending (thread-safe)
    /** @param data optional ping payload
        @param xsink exception sink
    */
    DLLLOCAL void queueSendPing(const BinaryNode* data, ExceptionSink* xsink);

    //! Initiates the close handshake (thread-safe)
    /** @param code the close code
        @param reason the close reason string
        @param xsink exception sink
    */
    DLLLOCAL void queueSendClose(uint16_t code, const char* reason, ExceptionSink* xsink);

    //! Returns the next received frame from recv_queue (non-blocking)
    /** @param xsink exception sink
        @return the frame hash or QoreValue() if none available
    */
    DLLLOCAL QoreValue getReceivedFrame(ExceptionSink* xsink);

    //! Returns true if recv_queue has frames available
    DLLLOCAL bool hasReceivedFrames() const {
        return !recv_queue->empty();
    }

    //! Queues a pre-encoded raw frame for sending (thread-safe)
    /** @param data the pre-encoded frame bytes
        @param xsink exception sink
    */
    DLLLOCAL void queueSendRawFrame(const BinaryNode* data, ExceptionSink* xsink);

    //! Sets the frame callback (called on I/O thread when a frame is received)
    /** @param cb the callback reference (ref'd), or nullptr to clear
        @param xsink exception sink
    */
    DLLLOCAL void setFrameCallback(ResolvedCallReferenceNode* cb, ExceptionSink* xsink);

    //! Sets the heartbeat interval in milliseconds (0 = disabled)
    DLLLOCAL void setHeartbeat(int64 ms) {
        heartbeat_interval_ms = ms;
        if (ms > 0 && last_recv_activity_ms == 0) {
            last_recv_activity_ms = q_clock_getmillis();
        }
    }

    //! Sets the send timeout in milliseconds
    DLLLOCAL void setSendTimeout(int64 ms) {
        send_timeout_ms = ms;
    }

    //! Returns the send timeout in milliseconds
    DLLLOCAL int64 getSendTimeout() const {
        return send_timeout_ms;
    }

    //! Returns and clears the number of items pushed during the last continuePoll() cycle
    /** Used by the async I/O controller to dispatch notifications when frames arrive.
    */
    DLLLOCAL int getAndClearItemsPushed() override {
        int result = frames_pushed_in_cycle;
        frames_pushed_in_cycle = 0;
        return result;
    }

    //! Releases all internal references
    DLLLOCAL void cleanup(ExceptionSink* xsink);

protected:
    DLLLOCAL const char* getStateImpl() const override;

private:
    QoreSocketObject* sock;                 //!< ref'd WebSocket socket
    Queue* recv_queue;                      //!< ref'd, for pushing received frames
    QoreObject* recv_queue_obj;             //!< ref'd QoreObject wrapping recv_queue (for DGC)
    Queue* send_queue;                      //!< ref'd, for dequeuing frames to send
    QoreObject* send_queue_obj;             //!< ref'd QoreObject wrapping send_queue (for DGC)

    WebSocketFrameDecoder decoder;          //!< stateful frame decoder
    WebSocketFrameReassembler reassembler;  //!< fragment reassembler

    //! Current send inner operation (nullptr when not sending)
    SocketSendPollOperation* send_op = nullptr;

    //! Current recv inner operation (recreated each read cycle)
    SocketRecvDataPollOperation* recv_op = nullptr;

    WsState ws_state = WsState::CONNECTED;
    uint16_t close_code = 1000;
    std::string close_reason;
    bool close_initiated = false;           //!< true if we initiated the close handshake
    size_t max_frame_size = WS_DEFAULT_MAX_FRAME_SIZE;

    //! Error information (ref'd or nullptr)
    QoreHashNode* error_info = nullptr;

    //! Frame callback — invoked on I/O thread when a frame is pushed to recv_queue
    ResolvedCallReferenceNode* frame_callback = nullptr;

    //! Send timeout in milliseconds (0 = no timeout)
    int64 send_timeout_ms = 0;

    //! Timestamp when current send operation started (milliseconds since epoch)
    int64 send_op_start_ms = 0;

    //! Number of frames pushed during the current continuePoll() cycle
    int frames_pushed_in_cycle = 0;

    //! Heartbeat interval in milliseconds (0 = disabled)
    int64 heartbeat_interval_ms = 0;

    //! Timestamp of last recv activity (milliseconds since epoch)
    int64 last_recv_activity_ms = 0;

    //! Pending send data — pre-encoded frame bytes ready to send
    /** Populated from send_queue when no send_op is active.
        Multiple queued frames are concatenated to reduce syscalls.
    */
    SimpleRefHolder<BinaryNode> pending_send;

    //! Process all complete frames in the decoder buffer
    /** Decodes frames, handles control frames (ping/pong/close),
        pushes data frames to recv_queue.
        @param xsink exception sink
    */
    DLLLOCAL void processDecodedFrames(ExceptionSink* xsink);

    //! Handles a single decoded frame
    /** @param frame the decoded WebSocket frame
        @param xsink exception sink
    */
    DLLLOCAL void handleFrame(const WsFrame& frame, ExceptionSink* xsink);

    //! Pushes a frame info hash to recv_queue
    /** @param type frame type string ("text", "binary", "ping", "pong", "close")
        @param data the payload (ref transferred to hash)
        @param rsv the RSV bits from the frame header
        @param xsink exception sink
    */
    DLLLOCAL void pushRecvFrame(const char* type, AbstractQoreNode* data, uint8_t rsv, ExceptionSink* xsink);

    //! Pushes a close frame info hash to recv_queue
    /** @param code the close code
        @param reason the close reason
        @param xsink exception sink
    */
    DLLLOCAL void pushRecvCloseFrame(uint16_t code, const char* reason, ExceptionSink* xsink);

    //! Drains send_queue and concatenates into pending_send
    /** @param xsink exception sink
    */
    DLLLOCAL void drainSendQueue(ExceptionSink* xsink);

    //! Releases the current send inner operation
    /** @param xsink exception sink
    */
    DLLLOCAL void releaseSendOp(ExceptionSink* xsink);

    //! Releases the current recv inner operation
    /** @param xsink exception sink
    */
    DLLLOCAL void releaseRecvOp(ExceptionSink* xsink);

    //! Creates a new recv inner operation for the next read cycle
    /** @param xsink exception sink
        @return 0 on success, -1 on error
    */
    DLLLOCAL int startRecvOp(ExceptionSink* xsink);

    //! Creates a send inner operation from pending_send data
    /** @param xsink exception sink
        @return 0 on success, -1 on error
    */
    DLLLOCAL int startSendOp(ExceptionSink* xsink);

    //! Sets error state and stores exception info
    /** @param xsink exception sink containing the error
    */
    DLLLOCAL void setError(ExceptionSink* xsink);

    //! Transitions to CLOSED state with the given code and reason
    DLLLOCAL void setClosed(uint16_t code, const char* reason);
};

DLLLOCAL QoreClass* initHttpWebSocketPollOperationBaseClass(QoreNamespace& qorens);

#endif // _QORE_CLASS_HTTPWEBSOCKETPOLLOPERATIONBASE_H
