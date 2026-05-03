# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file AsyncHttp2ServerStream.qc Async-only HTTP/2 server-side stream handle

/*  AsyncHttp2ServerStream.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Async-only HTTP/2 server-side stream handle
/** Purpose-built handle for protocols like gRPC that drive an HTTP/2 server response
    through headers → DATA* → trailers and must not acquire the socket wrapper lock
    (@c my_socket_priv::m) from the handler thread while the I/O controller is
    driving the same socket.  All write methods route through the @c *Async
    variants on @ref Qore::Socket, which delegate nghttp2 / session-state work
    to the async I/O controller.

    Construction requires:
    - the HTTP/2 Socket that carries the stream,
    - the stream ID received via the async request-dispatch path
      (e.g. @ref HttpServerAsyncIo::Http2PollOperation),
    - a @ref Qore::Queue populated with inbound DATA payloads by the I/O
      controller; a NOTHING sentinel enqueued by the I/O thread signals END_STREAM.

    The lifecycle is linear:
    - @ref sendHeaders() — exactly once, before any data or trailers.
    - @ref sendData() — zero or more times.
    - @ref sendTrailers() — exactly once; carries END_STREAM.  For trailers-only
      error responses (e.g. gRPC status != OK with no body) call @ref sendHeaders()
      and then @ref sendTrailers() directly, skipping @ref sendData().

    The read side uses @ref read() / @ref readsDone().  No explicit cleanup is
    needed on the async path — nghttp2 recycles the stream ID when END_STREAM is
    emitted, and the Queue is released when this object is destroyed.

    @par Thread safety
    A single handler thread owns this object.  Read and write methods are not
    safe to call concurrently from multiple threads on the same instance.
*/
public class AsyncHttp2ServerStream {
    private {
        #! The HTTP/2 Socket (held for the lifetime of the stream)
        Socket sock;

        #! The HTTP/2 stream ID (positive)
        int stream_id;

        #! Queue fed by the I/O controller with inbound DATA payloads
        /** The I/O thread pushes binary payloads per DATA frame and pushes
            a NOTHING sentinel when END_STREAM is observed.
        */
        Queue inbound;

        #! Set to True after a NOTHING sentinel is pulled from @ref inbound
        bool reads_done = False;

        #! Set to True after @ref sendHeaders() has been called
        bool headers_sent = False;

        #! Set to True after @ref sendTrailers() (or @ref sendData() with end_stream=True) has run
        bool writes_done = False;

        #! Optional callback invoked after each write to wake the I/O thread
        /** The @c *Async Socket methods enqueue data on the HTTP/2 session but
            do not wake the I/O controller.  For the controller to flush
            pending frames promptly, handlers must call a wake closure
            (typically @c sub () { controller.wakeSocket(sock); }).
            HttpServerAsyncIo provides exactly this closure via
            @c HttpConnectionInfo.header_info.io_wake; pass it here.  If
            unset, writes still succeed but are only flushed on the
            controller's next scheduled tick.
        */
        *code<nothing()> wake_callback;
    }

    #! Construct an async server-side HTTP/2 stream handle
    /** @param sock the open HTTP/2 Socket carrying the stream
        @param stream_id the HTTP/2 stream ID from the async dispatch
        @param inbound the Queue populated by the I/O controller with DATA payloads
        @param wake_callback optional I/O-controller wake closure; see the
        @ref wake_callback member for the contract
    */
    constructor(Socket sock, int stream_id, Queue inbound,
            *code<nothing()> wake_callback) {
        @assert(sock, "socket cannot be null");
        @assert(sock.isOpen(), "socket must be open");
        @assert(stream_id > 0, "stream_id must be positive");
        @assert(inbound, "inbound queue cannot be null");
        self.sock = sock;
        self.stream_id = stream_id;
        self.inbound = inbound;
        self.wake_callback = wake_callback;
    }

    #! Returns the HTTP/2 stream ID
    int getStreamId() {
        return stream_id;
    }

    #! Returns the underlying Socket
    Socket getSocket() {
        return sock;
    }

    #! Returns True if the peer has not yet sent END_STREAM
    /** @return True until @ref read() has seen the NOTHING sentinel on the
        inbound Queue, or an explicit close has been observed.
    */
    bool isReadOpen() {
        return !reads_done;
    }

    #! Returns True after END_STREAM has been observed on the read side
    /** Equivalent to the sync-path @c Socket::isHttp2StreamComplete() probe
        against this stream.
    */
    bool readsDone() {
        return reads_done;
    }

    #! Reads the next inbound DATA payload, blocking up to the given timeout
    /** Blocks on the inbound Queue until a payload is available, the peer
        sends END_STREAM, or the timeout elapses.  A NOTHING return can mean
        either "timed out" or "END_STREAM observed" — use @ref readsDone() to
        disambiguate.

        @param t maximum time to wait (default 5s)
        @return binary payload, or @ref nothing on timeout or END_STREAM
    */
    *binary read(timeout t = 5s) {
        if (reads_done) {
            return NOTHING;
        }
        try {
            auto v = inbound.get(t);
            if (!(v instanceof binary)) {
                # NOTHING sentinel pushed by the I/O thread means END_STREAM
                reads_done = True;
                return NOTHING;
            }
            return v;
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "QUEUE-TIMEOUT") {
                return NOTHING;
            }
            rethrow;
        }
    }

    #! Sends HTTP/2 response HEADERS on the stream
    /** Must be called exactly once, before any @ref sendData() or @ref sendTrailers().
        The END_STREAM flag is NOT set — trailers carry END_STREAM per gRPC convention.

        @param status HTTP status code (typically 200 for gRPC)
        @param headers response headers (e.g. \c "content-type": "application/grpc+proto")
    */
    sendHeaders(int status, hash<string, string> headers) {
        @assert(!headers_sent, "sendHeaders() already called on this stream");
        sock.submitHttp2StreamingResponseHeaders(stream_id, status, headers);
        headers_sent = True;
        if (wake_callback) {
            wake_callback();
        }
    }

    #! Sends a DATA frame payload on the stream
    /** @param data the binary payload (framed as appropriate for the protocol on top)
        @param end_stream if True, the last DATA frame carries END_STREAM.  For gRPC
        this is normally False — END_STREAM rides on the subsequent trailer HEADERS.

        @throw HTTP2-FLOW-CONTROL the stream's send buffer is full; drain via
        @ref Qore::Socket::waitForHttp2StreamDrain() and retry
    */
    sendData(binary data, bool end_stream = False) {
        @assert(headers_sent, "sendHeaders() must be called before sendData()");
        @assert(!writes_done, "stream write side already closed");
        sock.sendHttp2StreamData(stream_id, data, end_stream);
        if (end_stream) {
            writes_done = True;
        }
        if (wake_callback) {
            wake_callback();
        }
    }

    #! Sends HTTP/2 trailers on the stream
    /** Carries END_STREAM.  For trailers-only error responses call @ref sendHeaders()
        first with no intervening @ref sendData().

        @param trailers trailer header name/value pairs (e.g. \c grpc-status, \c grpc-message)
    */
    sendTrailers(hash<string, string> trailers) {
        @assert(headers_sent, "sendHeaders() must be called before sendTrailers()");
        @assert(!writes_done, "stream write side already closed");
        sock.sendHttp2Trailers(stream_id, trailers);
        writes_done = True;
        if (wake_callback) {
            wake_callback();
        }
    }
}

} # namespace AsyncSocketIo
