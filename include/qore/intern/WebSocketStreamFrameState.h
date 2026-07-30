/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    WebSocketStreamFrameState.h

    Qore Programming Language

    Copyright 2026 Qore Technologies, s.r.o.

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

#ifndef _QORE_WEBSOCKETSTREAMFRAMESTATE_H
#define _QORE_WEBSOCKETSTREAMFRAMESTATE_H

#include <qore/WebSocketFrameCodec.h>

#include <cstdint>
#include <deque>
#include <functional>
#include <string>

class Queue;

//! Per-stream WebSocket frame decode/reassemble/dispatch state machine
/** Designed to be driven from the I/O thread of a multiplexed transport
    (HTTP/2 over TCP, HTTP/3 over QUIC) that delivers raw tunnel bytes for
    one extended-CONNECT WebSocket stream.  The caller feeds bytes in via
    feedData() and:

    - Decoded data frames (text/binary) are reassembled from fragments and
      pushed to the caller-supplied `msg_queue` as Qore `BinaryNode`s,
      one Queue entry per **complete** WebSocket message.
    - Control frames (ping/pong/close) are handled automatically:
      - ping  -> pong response encoded and handed to the caller via the
        send callback, then a "ping" notification is pushed to msg_queue
        so the handler can observe pings if it cares.
      - pong  -> pushed to msg_queue as a "pong" notification.
      - close -> close notification pushed to msg_queue; the caller is
        responsible for initiating the close echo on the transport.
    - All frame-size limits, protocol-violation checks and UTF-8 validation
      happen in pure C++.
    - No Qore interpreter reentry from feedData(): the msg_queue push is
      the only Qore call (Queue is thread-safe) and all encoding is
      pre-computed BinaryNode bytes.

    The send callback exists so control-frame responses (pong/close-echo)
    can be shipped in the same I/O-thread cycle as the recv that triggered
    them, avoiding a round-trip through the worker pool.

    Back-pressure: msg_queue is ordinarily **unbounded**; the caller is
    expected to police memory via `setMaxMessageSize()` (frame-size limit,
    enforced during reassembly) and by applying transport-level flow
    control (e.g. skipping `ngtcp2_conn_extend_max_stream_offset()` when
    the Queue is full) outside this class.

    Thread safety:
    - feedData(), processPending(), and all state accessors are intended
      to run on the I/O thread only.  They are NOT re-entrant.
    - msg_queue is a thread-safe Qore Queue; the consumer drains it from
      any thread.
    - The send callback is invoked synchronously from feedData()/
      processPending() on the I/O thread; the caller must be prepared
      to send (or enqueue for later send) without blocking.

    @since %Qore 3.0
*/
class WebSocketStreamFrameState {
public:
    //! Callback type for outgoing encoded frame bytes
    /** The callback takes ownership of the BinaryNode reference and must
        deref it after shipping the bytes (or queuing them for later send).
    */
    using SendCallback = std::function<void(BinaryNode* /* frame_bytes */)>;

    //! Message kinds pushed to msg_queue as type tag
    enum class MessageKind {
        Text,        //!< complete text message (BinaryNode payload, UTF-8)
        Binary,      //!< complete binary message (BinaryNode payload)
        Ping,        //!< ping received (BinaryNode payload, may be empty)
        Pong,        //!< pong received (BinaryNode payload, may be empty)
        Close,       //!< close frame received (code in close_code, reason in close_reason)
        Error,       //!< protocol violation; stream should be closed
    };

    //! Creates a new frame state
    /** @param msg_queue the Queue to push completed messages to (ref
            not taken here — caller owns and ensures lifetime).
        @param send_cb  callback invoked for pong / close-echo outgoing frames.
        @param is_server true for server-side (we receive masked frames from
            client, send unmasked to client); false for client-side.
    */
    DLLLOCAL WebSocketStreamFrameState(Queue* msg_queue, SendCallback send_cb, bool is_server);
    DLLLOCAL ~WebSocketStreamFrameState();

    //! Feeds raw stream bytes (decoded H2/H3 DATA payload) into the state machine
    /** Pushes any newly-completed messages to msg_queue and invokes the send
        callback for any control-frame responses that must be sent.

        @param data pointer to received bytes (not retained)
        @param len  number of bytes
        @return number of messages pushed to msg_queue during this call
                (useful for dispatch-notification counters)
    */
    DLLLOCAL int feedData(const void* data, size_t len);

    //! Returns and clears the messages-pushed counter
    /** For integration with AsyncIoController's `getAndClearItemsPushed()`
        dispatch path (mirrors HttpWebSocketPollOperationPriv).
    */
    DLLLOCAL int getAndClearMessagesPushed();

    //! Returns true if the remote peer has sent a CLOSE frame
    DLLLOCAL bool peerClosed() const { return peer_closed; }

    //! Close code from the peer's CLOSE frame (only valid after peerClosed())
    DLLLOCAL uint16_t getCloseCode() const { return close_code; }

    //! Close reason from the peer's CLOSE frame
    DLLLOCAL const std::string& getCloseReason() const { return close_reason; }

    //! Returns true if an unrecoverable protocol error has been observed
    DLLLOCAL bool hasError() const { return error_state; }

    //! Error message (only valid after hasError())
    DLLLOCAL const std::string& getErrorMessage() const { return error_msg; }

    //! Sets the maximum per-message (post-reassembly) payload size in bytes
    /** Defaults to @ref WS_DEFAULT_MAX_FRAME_SIZE.  When exceeded, the
        state machine transitions to error and sends a close with code 1009.
    */
    DLLLOCAL void setMaxMessageSize(size_t size) { max_message_size = size; }

    //! Encodes and enqueues (via send callback) a text message from the caller
    /** Thread-safe: uses the send callback which the caller must make
        thread-safe on their side.
    */
    DLLLOCAL void sendText(const char* text, size_t len);

    //! Encodes and enqueues a binary message from the caller
    DLLLOCAL void sendBinary(const void* data, size_t len);

    //! Encodes and enqueues a ping from the caller
    DLLLOCAL void sendPing(const void* data, size_t len);

    //! Encodes and enqueues a close handshake initiation from the caller
    /** After calling this, further sends are suppressed and incoming
        frames will be processed until the peer's CLOSE is received.
    */
    DLLLOCAL void sendClose(uint16_t code, const char* reason);

    //! Pushes a NOTHING sentinel to msg_queue to signal transport close
    /** Called from the transport layer (e.g. QuicSession::streamCloseCallback)
        when the underlying stream is torn down before a proper WebSocket
        close handshake completed.  The handler uses the sentinel to detect
        stream termination alongside the normal "close" message kind.
    */
    DLLLOCAL void pushCloseSentinel();

    //! Returns the (non-owning) msg_queue pointer
    /** Callers may use the pointer to push auxiliary entries (e.g., error
        hashes from an AsyncCompletionAction::executeError() path) alongside
        the typed frame hashes produced by the state machine itself.  The
        queue stays alive as long as the frame state does.
    */
    DLLLOCAL Queue* getMsgQueue() const { return msg_queue; }

private:
    Queue* msg_queue;
    SendCallback send_cb;
    bool is_server;

    WebSocketFrameDecoder decoder;
    WebSocketFrameReassembler reassembler;

    size_t max_message_size = WS_DEFAULT_MAX_FRAME_SIZE;
    int messages_pushed = 0;

    bool peer_closed = false;
    uint16_t close_code = WSCC_NormalClosure;
    std::string close_reason;

    bool close_sent = false;      //!< true once sendClose() has been called
    bool error_state = false;
    std::string error_msg;

    //! Drains any complete frames from the decoder and dispatches them
    DLLLOCAL int processFrames();

    //! Handles a single decoded frame (control frames handled here;
    //! data frames passed to reassembler)
    DLLLOCAL void handleFrame(WsFrame& frame);

    //! Pushes a message hash {type, data, rsv1?, rsv2?, rsv3?, code?, reason?} to msg_queue
    /** For Text/Binary messages the rsv1/rsv2/rsv3 keys carry the RSV bits
        from the first frame of the (possibly fragmented) message — extension
        consumers (e.g. permessage-deflate) need them to decide whether to
        decompress the payload.  For Close messages, code and reason are set.

        All keys are simple — the hash shape is compatible with the H1 async
        path for consistency with existing handler code.

        @param kind     message kind tag
        @param payload  completed message bytes (ref transferred); nullptr for close/error
        @param rsv      RSV bits byte from the first frame (bit 2 = RSV1,
                        bit 1 = RSV2, bit 0 = RSV3) — only used for data frames
        @param code     close status code (Close only)
        @param reason   close reason string (Close only) */
    DLLLOCAL void pushMessage(MessageKind kind, BinaryNode* payload /* ref transferred */,
        uint8_t rsv = 0, uint16_t code = 0, const char* reason = nullptr);

    //! Transitions to error state with the given message
    DLLLOCAL void setError(const std::string& msg, uint16_t wire_code = WSCC_ProtocolError);

    //! Non-copyable
    DLLLOCAL WebSocketStreamFrameState(const WebSocketStreamFrameState&) = delete;
    DLLLOCAL WebSocketStreamFrameState& operator=(const WebSocketStreamFrameState&) = delete;
};

#endif // _QORE_WEBSOCKETSTREAMFRAMESTATE_H
