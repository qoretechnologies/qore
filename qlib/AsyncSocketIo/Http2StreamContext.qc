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

    When \c direct_io is @ref True "True", the context manages HTTP/2 frame reading from the
    TCP socket itself (via @ref Qore::Socket::isDataAvailable() "Socket::isDataAvailable()"
    which triggers HTTP/2 frame processing). This is required when no I/O thread polls the
    socket for incoming HTTP/2 frames (e.g., dedicated WebSocket handler threads).

    When \c direct_io is @ref False "False" (default), an external I/O thread is expected to
    read HTTP/2 frames and populate the stream buffer. The context only reads from the buffer
    via @ref Qore::Socket::readHttp2StreamData() "Socket::readHttp2StreamData()".

    @since AsyncSocketIo 2.1
*/
public class Http2StreamContext {
    private {
        Socket sock;
        int stream_id;
        #! When True, readData() triggers HTTP/2 frame reading from TCP
        bool direct_io;
    }

    #! Creates an HTTP/2 stream context
    /** @param sock the HTTP/2 socket
        @param stream_id the HTTP/2 stream ID (must be positive)
        @param direct_io when @ref True "True", the context reads HTTP/2 frames directly from
        the TCP socket (for cases where no I/O thread manages frame reading)
    */
    constructor(Socket sock, int stream_id, bool direct_io = False) {
        @assert(sock, "socket cannot be null");
        @assert(sock.isOpen(), "socket must be open");
        @assert(stream_id > 0, "stream_id must be positive");
        self.sock = sock;
        self.stream_id = stream_id;
        self.direct_io = direct_io;
        if (direct_io) {
            # Set the active stream so isDataAvailable() knows which stream to check
            # and can trigger HTTP/2 frame reading from TCP
            sock.setHttp2ActiveStream(stream_id);
        }
    }

    int getStreamId() {
        return stream_id;
    }

    Socket getSocket() {
        return sock;
    }

    bool isOpen() {
        return sock.isOpen();
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
    /** Reads data for this stream from the underlying HTTP/2 socket.

        In direct I/O mode, this method first calls
        @ref Qore::Socket::isDataAvailable() "Socket::isDataAvailable()" with a zero timeout
        to trigger non-blocking HTTP/2 frame reading from the TCP socket. This populates the
        per-stream buffer that @ref Qore::Socket::readHttp2StreamData()
        "Socket::readHttp2StreamData()" reads from.

        @param max_bytes
        the maximum number of bytes to read; if 0, the socket decides how much data to return

        @return
        the binary data returned by Socket::readHttp2StreamData(), or @ref nothing if no data
        is available
    */
    *binary readData(int max_bytes = 0) {
        if (direct_io) {
            # Trigger HTTP/2 frame reading from TCP. isDataAvailable() with the active
            # stream set will: check the stream buffer, and if empty, read HTTP/2 frames
            # from the TCP socket (non-blocking with 0ms timeout) to populate the buffer.
            sock.isDataAvailable(0ms);
        }
        return sock.readHttp2StreamData(stream_id, max_bytes);
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
