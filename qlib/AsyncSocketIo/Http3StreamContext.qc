# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file Http3StreamContext.qc HTTP/3 stream I/O helper

/*  Http3StreamContext.qc Copyright 2026 Qore Technologies, s.r.o.

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
*/

#! The AsyncSocketIo namespace
/** All public declarations in the %AsyncSocketIo module are defined in this namespace
*/
public namespace AsyncSocketIo {

#! HTTP/3 stream I/O helper for extended CONNECT bidirectional tunnels (RFC 9220)
/** Provides stream-scoped send/receive helpers for HTTP/3 QUIC streams,
    used primarily for WebSocket over HTTP/3 (RFC 9220).

    Send-side: uses @ref Qore::Socket::submitQuicStreamData() to push data
    into the QUIC stream buffer, with backpressure handling via
    @ref Qore::Socket::waitForQuicStreamDrain().

    Receive-side: uses @ref Qore::Socket::readQuicConnectStreamData() to
    read tunnel data buffered by the QUIC session.
*/
public class Http3StreamContext inherits AbstractStreamContext {
    #! Maximum number of drain retries before throwing a backpressure timeout
    const MaxDrainRetries = 10;

    private {
        #! True once the end of the stream has been sent
        bool ended = False;

        #! True once the stream has been reset by abort()
        bool reset = False;

        Socket sock;
        int session_id;
        int stream_id;
        #! Optional callback to wake the I/O controller after queuing data
        *code<nothing()> wake_callback;
        #! Optional Queue for receiving data from the QUIC receive callback
        /** When set, @ref readData() reads from this Queue instead of calling
            @ref Qore::Socket::readQuicConnectStreamData().  Data is pushed by
            h3RecvDataCallback() in the C++ QUIC session.

            When @ref uses_frame_state is True, the same Queue carries typed
            frame hashes (one per complete WebSocket message) instead of raw
            binary chunks, and @ref readFrame() must be used to drain it.
        */
        *Queue data_queue;
        #! Set to True when a NOTHING sentinel is received from the Queue (stream closed)
        bool closed = False;
        #! True when data_queue contains pre-decoded WebSocket frame hashes
        /** Set when the underlying transport is feeding a C++ WebSocketStreamFrameState
            rather than a raw-data queue.  Consumers MUST use @ref readFrame()
            in this mode — @ref readData() throws.
        */
        bool uses_frame_state = False;
    }

    #! Creates an HTTP/3 stream context
    /** @param sock the QUIC socket
        @param session_id the QUIC session ID
        @param stream_id the HTTP/3 stream ID (must be non-negative)
        @param wake_callback optional callback to wake the I/O controller after writes
        @param data_queue optional Queue for receiving data from the QUIC receive callback
        @param uses_frame_state when True, @p data_queue carries typed WS frame
            hashes (one per complete message) and @ref readFrame() must be used
    */
    constructor(Socket sock, int session_id, int stream_id,
            *code<nothing()> wake_callback, *Queue data_queue,
            bool uses_frame_state = False) {
        @assert(sock, "socket cannot be null");
        @assert(session_id > 0, "session_id must be positive");
        @assert(stream_id >= 0, "stream_id must be non-negative");
        self.sock = sock;
        self.session_id = session_id;
        self.stream_id = stream_id;
        self.wake_callback = wake_callback;
        self.data_queue = data_queue;
        self.uses_frame_state = uses_frame_state;
        if (uses_frame_state) {
            ws_close_record = sock.getQuicStreamWebSocketCloseRecord(session_id, stream_id);
        }
    }

    #! Returns True when the transport feeds pre-decoded WebSocket frame hashes
    bool usesFrameState() {
        return uses_frame_state;
    }

    #! Drains one pre-decoded WebSocket frame hash from the frame queue (non-blocking)
    /** Only valid when @ref usesFrameState() is True.  The returned hash has
        the shape @c {type: string, data: binary, [code: int, reason: string]}
        — see @c WebSocketStreamFrameState for semantics.

        Returns @ref nothing when the queue is drained.  If a NOTHING sentinel
        is pulled, the context is marked closed and @ref isOpen() returns False.
    */
    *hash<auto> readFrame() {
        if (!uses_frame_state) {
            throw "HTTP3-STREAM-ERROR",
                "readFrame() requires frame-state mode — use readData() for raw streams";
        }
        if (!data_queue) {
            return NOTHING;
        }
        auto result = data_queue.tryGet();
        if (!exists result) {
            return NOTHING;
        }
        if (!exists result.value || result.value.typeCode() != NT_HASH) {
            closed = True;
            return NOTHING;
        }
        return result.value;
    }

    string getProtocol() {
        return "http3";
    }

    int getSessionId() {
        return session_id;
    }

    int getStreamId() {
        return stream_id;
    }

    Socket getSocket() {
        return sock;
    }

    bool isOpen() {
        return !closed && sock.isOpen();
    }

    #! Send data on the QUIC stream with backpressure handling
    /** @param data the binary payload to send
        @param end_stream if True, signals end of stream after this data
    */
    sendData(binary data, bool end_stream = False) {
        checkWebSocketDataFrame(data);
        # Guarantee wake callback fires even if waitForQuicStreamDrain() throws
        on_exit {
            if (wake_callback) {
                wake_callback();
            }
        }
        int retries = 0;
        while (sock.submitQuicStreamData(session_id, stream_id, data, end_stream) == 1) {
            if (++retries > MaxDrainRetries) {
                throw "QUIC-HTTP3-ERROR", sprintf("stream %d: backpressure timeout after %d retries "
                    "(%d bytes)", stream_id, MaxDrainRetries, data.size());
            }
            # Wake the I/O controller to flush queued packets before waiting for
            # drain — otherwise the controller may be idle and the buffer never drains
            if (wake_callback) {
                wake_callback();
            }
            sock.waitForQuicStreamDrain(session_id, stream_id, 5s);
        }
        if (end_stream) {
            ended = True;
        }
    }

    #! Sends data on the QUIC stream, waiting at most the given time for a full send buffer to drain
    /** @ref Qore::Socket::submitQuicStreamData() "submitQuicStreamData()" refuses data while the stream send buffer
        is full; this method then wakes the I/O controller to send it and waits for the stream to drain before trying
        again, so a peer that reads more slowly than data is sent slows the sender down instead of losing the data.

        @param data the binary payload to send
        @param timeout_ms the maximum time that the stream send buffer may make no progress: the time starts again
        whenever the peer has acknowledged more of the stream's data (see
        @ref Qore::Socket::getQuicStreamAckedBytes()), so that a large send that a slow or loaded peer takes does not
        fail while it is taken; a value less than or equal to zero waits without a limit
        @param end_stream if @ref True, signals end of stream after this data

        @throw STREAM-SEND-TIMEOUT the peer acknowledged no data for this time, or the stream ended with its send
        buffer full; the data was not sent
        @throw SOCKET-SYNC-ON-IO-THREAD-ERROR the stream send buffer is full and this method was called on the async
        I/O controller's I/O path, where it cannot wait

        @note other exceptions are thrown as by @ref sendData()

        @since AsyncSocketIo 1.0
    */
    sendDataWithTimeout(binary data, timeout timeout_ms, bool end_stream = False) {
        checkWebSocketDataFrame(data);
        # the wake callback runs even if the wait throws, so the data already queued is sent
        on_exit {
            if (wake_callback) {
                wake_callback();
            }
        }
        # the wait is bounded by the timeout of STALLED time, not of total time: a large message that the peer takes
        # slowly (a loaded host) makes progress all the while (issue #7252, as on the client: #6968).  The deadline
        # moves only when the count grows, so an unknown count (-1: the stream or session is gone) ends the wait
        # on the monotonic clock, in milliseconds
        *int stall_deadline;
        int last_acked = -1;
        # the waits in a row that found the stream ended, with the send still refused
        int ended_waits = 0;
        # True once this send has waited with a minimum rate set: the time after it is not waiting for the peer
        bool rate_wait;
        on_exit if (rate_wait) {
            try {
                send_rate.update(sock.getQuicStreamAckedBytes(session_id, stream_id), False);
            } catch () {
                # the stream or its session is gone: nothing more is measured
            }
        }
        while (sock.submitQuicStreamData(session_id, stream_id, data, end_stream) == 1) {
            # the data already queued must be sent before the stream can drain
            if (wake_callback) {
                wake_callback();
            }
            int wait_ms = -1;
            if (timeout_ms > 0 || send_rate) {
                int now = clock_get_monotonic_us() / 1000;
                int acked = sock.getQuicStreamAckedBytes(session_id, stream_id);
                if (timeout_ms > 0) {
                    if (!stall_deadline || acked > last_acked) {
                        last_acked = acked;
                        stall_deadline = now + timeout_ms;
                    }
                    wait_ms = stall_deadline - now;
                    if (wait_ms <= 0) {
                        throw "STREAM-SEND-TIMEOUT", sprintf("HTTP/3 stream %d: the send buffer did not drain within "
                            "%d ms and the peer acknowledged no data meanwhile; %d bytes were not sent", stream_id,
                            timeout_ms, data.size());
                    }
                }
                # the rate of the peer over the window, which the waits of all the sends on the stream share (#7280)
                if (send_rate) {
                    rate_wait = True;
                    if (send_rate.update(acked, True, now)) {
                        throw "STREAM-SEND-TIMEOUT", sprintf("HTTP/3 stream %d: the peer took less than the minimum "
                            "rate of %d bytes/s over %d ms; %d bytes were not sent", stream_id,
                            send_rate.getMinRate(), send_rate.getWindow(), data.size());
                    }
                    int check_ms = send_rate.getCheckInterval();
                    wait_ms = wait_ms < 0 ? check_ms : min(wait_ms, check_ms);
                }
            }
            # 0: drained, 1: timed out, -1: the stream has no data staged any more (sent or closed); the send is made
            # again in each case; a stream that has ended and still refuses it will never take it
            if (sock.waitForQuicStreamDrain(session_id, stream_id, wait_ms) >= 0) {
                ended_waits = 0;
            } else if (++ended_waits > 1) {
                throw "STREAM-SEND-TIMEOUT", sprintf("HTTP/3 stream %d: the stream ended with its send buffer full; "
                    "%d bytes were not sent", stream_id, data.size());
            }
        }
        # the stream was ended with the data: it is not ended again
        if (end_stream) {
            ended = True;
        }
    }

    #! Sends the close frame of a WebSocket connection on the stream, unless one was sent on it before
    /** See @ref Qore::Socket::submitQuicStreamWebSocketClose() "Socket::submitQuicStreamWebSocketClose()"

        @param frame the encoded close frame
        @param end_stream if @ref True, ends the stream after the frame (also if the frame is not sent)

        @return @ref True if the frame was sent; @ref False if a close frame was sent on the stream before

        @since AsyncSocketIo 1.0
    */
    bool sendWebSocketClose(binary frame, bool end_stream = False) {
        # the connection's record decides before the stream or the session, which may have ended
        if (isWebSocketCloseSent() && !end_stream) {
            return False;
        }
        on_exit {
            if (wake_callback) {
                wake_callback();
            }
        }
        int rc = sock.submitQuicStreamWebSocketClose(session_id, stream_id, frame, end_stream);
        if (end_stream) {
            ended = True;
        }
        # a close frame was staged on the stream, now or before
        ws_close_sent = True;
        return !rc;
    }

    #! Flush pending HTTP/3 writes for this session through the async I/O controller
    /** @throw QUIC-ERROR if no QUIC session with the context's session ID is active
        @throw QUIC-SEND-ERROR if the UDP send fails
    */
    flushPendingData() {
        sock.flushQuicPendingData(session_id);
    }

    #! Attempts to send data on the QUIC stream without blocking on backpressure
    /** Performs a single non-blocking @ref Qore::Socket::submitQuicStreamData() "submitQuicStreamData()" call,
        which returns @c 1 (and discards the frame) when the stream send buffer is full, rather than looping and
        waiting for the buffer to drain the way @ref sendData() does.  This lets a WebSocket broadcast drop a slow
        or non-reading client immediately instead of stalling for up to @ref MaxDrainRetries drain waits.

        @param data the binary payload to send
        @param end_stream if @ref True, signals end of stream after this data

        @return @ref True if the data was accepted; @ref False if the stream send buffer is full (backpressure)

        @since AsyncSocketIo 1.0
    */
    bool trySendData(binary data, bool end_stream = False) {
        checkWebSocketDataFrame(data);
        int rc = sock.submitQuicStreamData(session_id, stream_id, data, end_stream);
        # wake the I/O controller so queued packets are flushed on its next tick
        if (wake_callback) {
            wake_callback();
        }
        return rc != 1;
    }

    #! Read data from the extended CONNECT tunnel
    /** When a data Queue is registered, reads non-blockingly from the Queue
        (data is pushed by h3RecvDataCallback in the C++ QUIC session).
        Falls back to @ref Qore::Socket::readQuicConnectStreamData() when no
        Queue is available.

        @param max_bytes ignored for HTTP/3 — all buffered data is returned

        @return binary data if available, or @ref nothing if no data is buffered
    */
    *binary readData(int max_bytes = 0) {
        if (data_queue) {
            # Read from Queue populated by h3RecvDataCallback.
            # tryGet() returns {"value": data} or NOTHING if queue is empty,
            # disambiguating empty queue from a NOTHING sentinel value.
            auto result = data_queue.tryGet();
            if (!exists result) {
                return NOTHING;
            }
            # NOTHING sentinel means stream closed
            if (exists result && !(result.value instanceof binary)) {
                closed = True;
                return NOTHING;
            }
            return result.value;
        }
        return sock.readQuicConnectStreamData(session_id, stream_id);
    }

    #! Close the stream by sending end-of-stream
    close() {
        # the end of the stream is sent once
        if (ended) {
            return;
        }
        ended = True;
        try {
            sock.submitQuicStreamData(session_id, stream_id, NOTHING, True);
            if (wake_callback) {
                wake_callback();
            }
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err != "SOCKET-NOT-OPEN" && ex.err != "QUIC-ERROR"
                    && ex.err != "QUIC-HTTP3-ERROR") {
                rethrow;
            }
        }
    }

    #! Aborts the stream with \c RESET_STREAM and \c STOP_SENDING and releases its state and the data not yet sent
    /** Also for a stream that was ended already: an end of the stream (\c FIN) that is staged behind data that the
        peer acknowledges none of never reaches it (qore#7325).  The stream is reset once; the errors of a stream or
        session that has ended already are ignored.

        @since AsyncSocketIo 1.0
    */
    abort() {
        if (reset) {
            return;
        }
        reset = True;
        ended = True;
        try {
            sock.cancelQuicStream(session_id, stream_id);
            if (wake_callback) {
                wake_callback();
            }
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err != "SOCKET-NOT-OPEN" && ex.err != "QUIC-ERROR"
                    && ex.err != "QUIC-HTTP3-ERROR") {
                rethrow;
            }
        }
    }
}

} # namespace AsyncSocketIo
