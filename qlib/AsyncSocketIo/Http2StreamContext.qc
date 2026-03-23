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

    @since AsyncSocketIo 1.0
*/
public class Http2StreamContext inherits AbstractStreamContext {
    private {
        Socket sock;
        int stream_id;
        #! Queue for receiving data from the I/O controller
        /** @ref readData() reads from this Queue (non-blocking). The I/O controller's
            @ref HttpServerAsyncIo::Http2PollOperation "Http2PollOperation" drains data
            from the per-stream buffer to this Queue after each poll cycle.

            @since AsyncSocketIo 1.1
        */
        Queue data_queue;

        #! Set to True when a NOTHING sentinel is received from the Queue (stream closed)
        /** @since AsyncSocketIo 1.1
        */
        bool closed = False;
    }

    #! Creates an HTTP/2 stream context with Queue-based async I/O
    /** @param sock the HTTP/2 socket (used for sending; the I/O controller reads)
        @param stream_id the HTTP/2 stream ID (must be positive)
        @param data_queue the Queue populated by the I/O controller with incoming data;
        @ref readData() reads from this Queue non-blockingly

        @since AsyncSocketIo 1.1
    */
    constructor(Socket sock, int stream_id, Queue data_queue) {
        @assert(sock, "socket cannot be null");
        @assert(sock.isOpen(), "socket must be open");
        @assert(stream_id > 0, "stream_id must be positive");
        self.sock = sock;
        self.stream_id = stream_id;
        self.data_queue = data_queue;
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
    /** Sends binary DATA frames for this HTTP/2 stream using the underlying socket.

        @param data
        the binary payload to send on the stream

        @param end_stream
        if True, the END_STREAM flag is set on the DATA frame, signaling no more data will be sent on this stream

        @throw
        any exception thrown by Socket::sendHttp2StreamData()
    */
    sendData(binary data, bool end_stream = False) {
        sock.sendHttp2StreamData(stream_id, data, end_stream);
    }

    #! Read data from this HTTP/2 stream.
    /** Reads from the Queue non-blockingly. The I/O controller populates the Queue via
        @ref HttpServerAsyncIo::Http2PollOperation::drainStreamQueues()
        "Http2PollOperation::drainStreamQueues()".

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
    }
}

} # namespace AsyncSocketIo
