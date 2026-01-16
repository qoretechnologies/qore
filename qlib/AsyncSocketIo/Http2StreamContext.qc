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

    @since AsyncSocketIo 2.1
*/
public class Http2StreamContext {
    private {
        Socket sock;
        int stream_id;
    }

    constructor(Socket sock, int stream_id) {
        @assert(sock, "socket cannot be null");
        @assert(sock.isOpen(), "socket must be open");
        @assert(stream_id > 0, "stream_id must be positive");
        self.sock = sock;
        self.stream_id = stream_id;
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
        sock.sendHttp2StreamData(stream_id, data, end_stream, 0);
    }

    #! Read data from this HTTP/2 stream.
    /** Reads data for this stream from the underlying HTTP/2 socket.

        @param max_bytes
        the maximum number of bytes to read; if 0, the socket decides how much data to return

        @return
        the binary data returned by Socket::readHttp2StreamData()
    */
    *binary readData(int max_bytes = 0) {
        return sock.readHttp2StreamData(stream_id, max_bytes);
    }
}

} # namespace AsyncSocketIo
