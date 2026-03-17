# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file AbstractStreamContext.qc Protocol-agnostic stream I/O interface

/*  AbstractStreamContext.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Protocol-agnostic stream I/O interface for extended CONNECT tunnels
/** Abstract base class for stream contexts used by WebSocket over HTTP/2 (RFC 8441)
    and HTTP/3 (RFC 9220). Provides a unified API for bidirectional stream I/O,
    enabling protocol-independent WebSocket frame handling.

    Subclasses:
    - @ref Http2StreamContext — HTTP/2 stream I/O via nghttp2
    - @ref Http3StreamContext — HTTP/3 stream I/O via ngtcp2/nghttp3

    @since AsyncSocketIo 1.0
*/
public class AbstractStreamContext {
    #! Returns the protocol identifier
    /** @return \c "http2" or \c "http3"
    */
    abstract string getProtocol();

    #! Returns the stream ID
    /** @return the protocol-specific stream identifier
    */
    abstract int getStreamId();

    #! Returns the underlying socket
    /** @return the socket used for stream I/O
    */
    abstract Socket getSocket();

    #! Returns whether the stream is still open
    /** @return @ref True if the underlying socket is open
    */
    abstract bool isOpen();

    #! Send data on the stream
    /** @param data the binary payload to send
        @param end_stream if @ref True, signals end of stream after this data
    */
    abstract sendData(binary data, bool end_stream = False);

    #! Read data from the stream
    /** @return binary data if available, or @ref nothing if no data is buffered
    */
    abstract *binary readData(int max_bytes = 0);

    #! Close the stream
    /** Default implementation is a no-op. HTTP/2 streams die with the TCP connection;
        HTTP/3 streams require explicit END_STREAM signaling.
    */
    close() {
        # no-op by default; Http3StreamContext overrides
    }
}

} # namespace AsyncSocketIo
