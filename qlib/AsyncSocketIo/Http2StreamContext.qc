# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file Http2StreamContext.qc HTTP/2 stream I/O helper

/*  Http2StreamContext.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! HTTP/2 stream I/O helper
/** Provides stream-scoped send/receive helpers for HTTP/2 sockets.

    An external I/O thread reads HTTP/2 frames and populates a Queue for the stream. The
    context reads from the Queue via @ref readData() and sends via the socket directly.
*/
public class Http2StreamContext inherits AbstractStreamContext {
    private {
        #! True once the end of the stream has been sent
        bool ended = False;

        #! True once the stream has been reset by abort()
        bool reset = False;

        Socket sock;
        int stream_id;
        #! Queue for receiving data from the I/O controller
        /** @ref readData() reads from this Queue (non-blocking). The I/O controller's
            @c Http2PollOperation drains data
            from the per-stream buffer to this Queue after each poll cycle.
        */
        Queue data_queue;

        #! Set to True when a NOTHING sentinel is received from the Queue (stream closed)
        bool closed = False;

        #! True when data_queue contains pre-decoded WebSocket frame hashes
        /** Set when the underlying transport is feeding a C++
            WebSocketStreamFrameState rather than a raw-data queue.  Consumers
            MUST use @ref readFrame() in this mode — @ref readData() throws.
        */
        bool uses_frame_state = False;

        #! Optional callback to wake the I/O controller after queuing data
        /** When set, @ref sendData() and @ref sendTrailers() invoke this
            closure after enqueuing HTTP/2 frames so the I/O thread flushes
            them on its next tick rather than on the next timeout.
        */
        *code<nothing()> wake_callback;
    }

    #! Creates an HTTP/2 stream context with Queue-based async I/O
    /** @param sock the HTTP/2 socket (used for sending; the I/O controller reads)
        @param stream_id the HTTP/2 stream ID (must be positive)
        @param data_queue the Queue populated by the I/O controller with incoming data;
        @ref readData() reads from this Queue non-blockingly
        @param uses_frame_state when True, @p data_queue carries typed WS frame
            hashes (one per complete message) and @ref readFrame() must be used
        @param wake_callback optional closure to wake the I/O controller after writes
    */
    constructor(Socket sock, int stream_id, Queue data_queue,
            bool uses_frame_state = False, *code<nothing()> wake_callback) {
        @assert(sock, "socket cannot be null");
        @assert(sock.isOpen(), "socket must be open");
        @assert(stream_id > 0, "stream_id must be positive");
        self.sock = sock;
        self.stream_id = stream_id;
        self.data_queue = data_queue;
        self.uses_frame_state = uses_frame_state;
        self.wake_callback = wake_callback;
        if (uses_frame_state) {
            ws_close_record = sock.getHttp2StreamWebSocketCloseRecord(stream_id);
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
            throw "HTTP2-STREAM-ERROR",
                "readFrame() requires frame-state mode — use readData() for raw streams";
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
        return "http2";
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

    #! Send DATA frames on the associated HTTP/2 stream.
    /** Enqueues binary DATA frames for this HTTP/2 stream; the I/O controller
        flushes them on its next tick (or immediately when @c wake_callback is
        set).

        @param data
        the binary payload to send on the stream

        @param end_stream
        if True, the END_STREAM flag is set on the DATA frame, signaling no more data will be sent on this stream

        @throw
        any exception thrown by Socket::sendHttp2StreamData()
    */
    sendData(binary data, bool end_stream = False) {
        checkWebSocketDataFrame(data);
        sock.sendHttp2StreamData(stream_id, data, end_stream);
        if (end_stream) {
            ended = True;
        }
        if (wake_callback) {
            wake_callback();
        }
    }

    #! Sends DATA frames on the HTTP/2 stream, waiting at most the given time for a full send buffer to drain
    /** @ref Qore::Socket::sendHttp2StreamData() "sendHttp2StreamData()" refuses data while more than the stream's
        backpressure threshold is still waiting to be sent; this method then wakes the I/O controller to send it and
        waits for the stream to drain before trying again, so a peer that reads more slowly than data is sent slows
        the sender down instead of losing the data.

        @param data the binary payload to send
        @param timeout_ms the maximum time that the stream send buffer may make no progress: the time starts again
        whenever the connection has taken more of the stream's buffered data (see
        @ref Qore::Socket::getHttp2StreamTakenBytes()), so that a large send that a slow or loaded peer takes does not
        fail while it is taken; a value less than or equal to zero waits without a limit
        @param end_stream if @ref True, signals end of stream after this data

        @throw STREAM-SEND-TIMEOUT the connection took no data of the stream for this time, or the stream ended with
        its send buffer full; the data was not sent
        @throw SOCKET-SYNC-ON-IO-THREAD-ERROR the stream send buffer is full and this method was called on the async
        I/O controller's I/O path, where it cannot wait

        @note other exceptions are thrown as by @ref sendData()

        @since AsyncSocketIo 1.0
    */
    sendDataWithTimeout(binary data, timeout timeout_ms, bool end_stream = False) {
        checkWebSocketDataFrame(data);
        # the wait is bounded by the timeout of STALLED time, not of total time: a large message that the peer takes
        # slowly (a loaded host) makes progress all the while (issue #7252, as on the client: #6968).  The deadline
        # moves only when the count grows, so an unknown count (-1: the stream or session is gone) ends the wait
        # on the monotonic clock, in milliseconds
        *int stall_deadline;
        int last_taken = -1;
        # the waits in a row that found the stream ended, with the send still refused
        int ended_waits = 0;
        while (True) {
            try {
                sock.sendHttp2StreamData(stream_id, data, end_stream);
                break;
            } catch (hash<ExceptionInfo> ex) {
                if (ex.err != "HTTP2-FLOW-CONTROL") {
                    rethrow;
                }
            }
            # the data already queued must be sent before the stream can drain
            if (wake_callback) {
                wake_callback();
            }
            int wait_ms = -1;
            if (timeout_ms > 0) {
                int now = clock_get_monotonic_us() / 1000;
                int taken = getTakenBytes();
                if (!stall_deadline || taken > last_taken) {
                    last_taken = taken;
                    stall_deadline = now + timeout_ms;
                }
                wait_ms = stall_deadline - now;
                if (wait_ms <= 0) {
                    throw "STREAM-SEND-TIMEOUT", sprintf("HTTP/2 stream %d: the send buffer did not drain within %d "
                        "ms and the peer granted no credit meanwhile; %d bytes were not sent", stream_id, timeout_ms,
                        data.size());
                }
            }
            # 0: drained, 1: timed out, -1: the stream has no data buffered any more (sent or closed); the send is
            # made again in each case; a stream that has ended and still refuses it will never take it
            if (sock.waitForHttp2StreamDrain(stream_id, wait_ms) >= 0) {
                ended_waits = 0;
            } else if (++ended_waits > 1) {
                throw "STREAM-SEND-TIMEOUT", sprintf("HTTP/2 stream %d: the stream ended with its send buffer full; "
                    "%d bytes were not sent", stream_id, data.size());
            }
        }
        # the stream was ended with the data: it is not ended again
        if (end_stream) {
            ended = True;
        }
        if (wake_callback) {
            wake_callback();
        }
    }

    #! Returns the bytes of the stream's buffered data that the connection has taken, or -1 if that is not known
    /** A session that has ended has no count: the send that is made next reports the state of the stream
    */
    private:internal int getTakenBytes() {
        try {
            return sock.getHttp2StreamTakenBytes(stream_id);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err != "HTTP2-ERROR") {
                rethrow;
            }
            return -1;
        }
    }

    #! Sends the close frame of a WebSocket connection on the stream, unless one was sent on it before
    /** See @ref Qore::Socket::sendHttp2StreamWebSocketClose() "Socket::sendHttp2StreamWebSocketClose()"

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
        int rc = sock.sendHttp2StreamWebSocketClose(stream_id, frame, end_stream);
        if (end_stream) {
            ended = True;
        }
        # a close frame was sent on the stream, now or before
        ws_close_sent = True;
        return !rc;
    }

    #! Attempts to send data on the HTTP/2 stream without blocking on backpressure
    /** @ref Qore::Socket::sendHttp2StreamData() "sendHttp2StreamData()" discards the frame and throws
        @c HTTP2-FLOW-CONTROL when the stream send buffer is full; this method maps that condition to a @ref False
        return so a caller (e.g. a WebSocket broadcast) can drop a slow or non-reading client immediately.  Any
        other exception propagates unchanged.

        @param data the binary payload to send
        @param end_stream if @ref True, signals end of stream after this data

        @return @ref True if the data was accepted; @ref False if the stream send buffer is full (backpressure)

        @since AsyncSocketIo 1.0
    */
    bool trySendData(binary data, bool end_stream = False) {
        checkWebSocketDataFrame(data);
        try {
            sock.sendHttp2StreamData(stream_id, data, end_stream);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "HTTP2-FLOW-CONTROL") {
                return False;
            }
            rethrow;
        }
        if (wake_callback) {
            wake_callback();
        }
        return True;
    }

    #! Read data from this HTTP/2 stream.
    /** Reads from the Queue non-blockingly. The I/O controller populates the Queue via
        @c Http2PollOperation::drainStreamQueues().

        @param max_bytes
        ignored; present for API compatibility

        @return
        binary data if available, or @ref nothing if no data is buffered
    */
    *binary readData(int max_bytes = 0) {
        # Read from Queue populated by the I/O controller.
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

    #! Send HTTP/2 trailer headers on this stream
    /** Sends a trailing HEADERS frame with END_STREAM containing the given trailer fields.
        The stream's data provider is configured so the last DATA frame uses
        NGHTTP2_DATA_FLAG_NO_END_STREAM, and the trailer HEADERS frame carries END_STREAM.

        This is required for protocols like gRPC that communicate status via trailers.

        @param trailers
        trailer header name/value pairs (e.g., \c "grpc-status", \c "grpc-message")

        @throw HTTP2-ERROR
        if no HTTP/2 session is active or trailer submission fails
    */
    sendTrailers(hash<string, string> trailers) {
        sock.sendHttp2Trailers(stream_id, trailers);
        # trailers end the stream
        ended = True;
        if (wake_callback) {
            wake_callback();
        }
    }

    #! Ends the stream by sending END_STREAM, unless it has been sent already
    /** The orderly close of a stream (RFC 8441 section 5); errors are ignored, as the peer may have reset the stream
        already

        @since AsyncSocketIo 1.0
    */
    close() {
        if (ended) {
            return;
        }
        ended = True;
        try {
            sock.sendHttp2StreamData(stream_id, binary(), True);
            if (wake_callback) {
                wake_callback();
            }
        } catch (hash<ExceptionInfo> ex) {
        }
    }

    #! Aborts the stream with \c RST_STREAM (\c CANCEL) and releases its state and the data not yet sent
    /** Also for a stream that was ended already: an end of the stream that is queued behind data that the peer
        grants no credit for never reaches it (qore#7325).  The stream is reset once; errors are ignored, as the peer
        or the session may have ended it already.

        @since AsyncSocketIo 1.0
    */
    abort() {
        if (reset) {
            return;
        }
        reset = True;
        ended = True;
        try {
            sock.resetHttp2Stream(stream_id);
            if (wake_callback) {
                wake_callback();
            }
        } catch (hash<ExceptionInfo> ex) {
        }
    }
}

} # namespace AsyncSocketIo
