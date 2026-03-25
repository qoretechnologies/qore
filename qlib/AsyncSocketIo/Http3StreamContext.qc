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

    @since AsyncSocketIo 1.0
*/
public class Http3StreamContext inherits AbstractStreamContext {
    #! Maximum number of drain retries before throwing a backpressure timeout
    const MaxDrainRetries = 10;

    private {
        Socket sock;
        int session_id;
        int stream_id;
        #! Optional callback to wake the I/O controller after queuing data
        *code<nothing()> wake_callback;
        #! Optional Queue for receiving data from the QUIC receive callback
        /** When set, @ref readData() reads from this Queue instead of calling
            @ref Qore::Socket::readQuicConnectStreamData().  Data is pushed by
            h3RecvDataCallback() in the C++ QUIC session.

            @since AsyncSocketIo 1.2
        */
        *Queue data_queue;
        #! Set to True when a NOTHING sentinel is received from the Queue (stream closed)
        /** @since AsyncSocketIo 1.2
        */
        bool closed = False;
    }

    #! Creates an HTTP/3 stream context
    /** @param sock the QUIC socket
        @param session_id the QUIC session ID
        @param stream_id the HTTP/3 stream ID (must be non-negative)
        @param wake_callback optional callback to wake the I/O controller after writes
        @param data_queue optional Queue for receiving data from the QUIC receive callback
    */
    constructor(Socket sock, int session_id, int stream_id,
            *code<nothing()> wake_callback, *Queue data_queue) {
        @assert(sock, "socket cannot be null");
        @assert(session_id > 0, "session_id must be positive");
        @assert(stream_id >= 0, "stream_id must be non-negative");
        self.sock = sock;
        self.session_id = session_id;
        self.stream_id = stream_id;
        self.wake_callback = wake_callback;
        self.data_queue = data_queue;
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
    }

    #! Read data from the extended CONNECT tunnel
    /** When a data Queue is registered, reads non-blockingly from the Queue
        (data is pushed by h3RecvDataCallback in the C++ QUIC session).
        Falls back to @ref Qore::Socket::readQuicConnectStreamData() when no
        Queue is available.

        @param max_bytes ignored for HTTP/3 — all buffered data is returned

        @return binary data if available, or @ref nothing if no data is buffered

        @since AsyncSocketIo 1.2 Queue-based reading
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
}

} # namespace AsyncSocketIo
