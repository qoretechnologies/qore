# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file Http2ClientConnectionImpl.qc HTTP/2 client connection implementation

/** Http2ClientConnectionImpl.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Main namespace
public namespace HttpClientIo {

#! HTTP/2 client connection implementation
/** This class wraps a Socket + Http2ClientPollOperation to provide a
    higher-level interface for HTTP/2 multiplexed client connections.

    A single connection can handle multiple concurrent streams, with
    responses dispatched to callbacks by stream ID.

    @par Thread Safety
    This class is thread-safe. Stream submission uses the nghttp2 session's
    internal mutex for synchronization.
*/
public class Http2ClientConnection inherits HttpClientConnection {
    private {
        #! Poll operation for this connection
        Http2ClientPollOperation poll_op;
    }

    #! Copying is not supported
    private copy() {
        throw "COPY-ERROR", "Http2ClientConnection objects cannot be copied";
    }

    #! Creates a new HTTP/2 client connection
    /** @param url_info parsed URL info with host, port, scheme
        @param connect_timeout timeout for connection establishment
        @param logger optional logger for debug output
        @param max_streams maximum concurrent streams per connection
        @param ssl_verify_mode SSL verification mode (optional)
        @param accept_all_certs if True, accept all SSL certificates including self-signed
        @param proxy_info parsed HTTP proxy info (optional)
        @param client_cert X.509 client certificate for mutual TLS (optional)
        @param client_pk private key matching \a client_cert (optional)
        @param ssl_ca_file a PEM file with the CA certificates to verify the server with instead of the default CAs
        @param ssl_ca_path a directory with the CA certificates to verify the server with instead of the default CAs
        @param offer_http11 if @ref True, TLS ALPN offers \c http/1.1 after \c h2 (for an automatic protocol
        selection that falls back to HTTP/1.1)
    */
    constructor(hash<auto> url_info, timeout connect_timeout = DefaultConnectTimeout,
            *LoggerInterface logger, int max_streams = DefaultMaxStreamsPerConnection,
            *int ssl_verify_mode, bool accept_all_certs = False,
            *hash<auto> proxy_info,
            *SSLCertificate client_cert, *SSLPrivateKey client_pk, *string ssl_ca_file, *string ssl_ca_path,
            bool offer_http11 = False)
            : HttpClientConnection(url_info, connect_timeout, logger,
                    max_streams, proxy_info) {

        log(LoggerLevel::DEBUG, "creating connection to %s:%d (ssl=%y max_streams=%d proxy=%y mtls=%y)",
            url_info.host, url_info.port, url_info.protocol == "https", max_streams,
            proxy_info ? sprintf("%s:%d", proxy_info.host, proxy_info.port) : NOTHING,
            client_cert ? True : False);

        poll_op = new Http2ClientPollOperation(url_info, connect_timeout, logger,
            ssl_verify_mode, accept_all_certs, proxy_info, self, client_cert, client_pk, ssl_ca_file, ssl_ca_path,
            offer_http11);
    }

    #! Returns the protocol identifier
    string getProtocol() {
        return "h2";
    }

    #! Returns the poll operation for this connection
    HttpClientPollOperation getPollOperation() {
        return poll_op;
    }

    #! Returns the underlying socket
    Socket getSocket() {
        return poll_op.getSocket();
    }

    #! Submits a request on this connection (protocol-specific)
    /** The capacity check is performed atomically inside poll_op.submitRequest()
        under the stream_lock.
        @throw HTTPCLIENT-REQUEST-BODY-ERROR a GET request specifies a body or enables request-body streaming
    */
    int submitRequestImpl(string method, string path, *hash<auto> headers,
            *data body, Promise promise) {
        checkRequestBody(method, headers, body);
        return poll_op.submitRequest(method, path, headers, body, promise,
            max_concurrent_streams);
    }

    #! Submits a request using CompositeAction (Promise + EventNotifier) — HTTP/2
    int submitRequestWithNotifierImpl(string method, string path, *hash<auto> headers,
            *data body, Promise promise, EventNotifier notifier) {
        checkRequestBody(method, headers, body);
        return poll_op.submitRequestWithNotifier(method, path, headers, body, promise,
            notifier, max_concurrent_streams);
    }

    #! Cancels a request (protocol-specific)
    cancelRequestImpl(int stream_id) {
        poll_op.cancelRequest(stream_id);
    }

    #! Closes the connection (protocol-specific)
    closeImpl(*timeout close_timeout) {
        self.setClosed();
        *AsyncSocketIoController ctl;
        {
            AutoLock al(lock);
            ctl = controller_ref;
        }
        if (ctl) {
            try {
                ctl.cancelAndClose(sock: self.getSocket());
            } catch () {
                # Ignore close errors during close
            }
        } else {
            try {
                poll_op.abort();
            } catch () {
                # Ignore abort errors during close
            }
        }
        try {
            self.getSocket().close();
        } catch () {
            # Ignore close errors during close
        }
    }

    #! Sends data on a stream (HTTP/2-specific streaming support)
    /** @param stream_id the stream ID to send data on
        @param data the data to send (can be string or binary)
        @param end_stream if True, signals end of stream
        @param drain_timeout the time to wait for the stream's send buffer to drain when it is full (the peer has not
        granted enough flow-control credit for the data buffered): the data is sent once there is room; with a
        timeout of zero or less, a full buffer raises \c HTTP2-FLOW-CONTROL at once.  A send that raises took none of
        the data, so it can be made again

        @throw HTTP2-ERROR if the stream ID is not found
        @throw HTTP2-FLOW-CONTROL if the stream's send buffer did not drain within \a drain_timeout; nothing was sent
        @throw HTTP2-STATE-ERROR if the connection is neither ready nor draining (a draining connection completes the
        streams in progress)
        @throw HTTP2-STREAM-CLOSED if the stream closed without an error and \a data is not empty or
        \a end_stream is not set; an empty send with \a end_stream set on such a stream is a no-op
        @throw HTTP2-STREAM-RESET if the server reset the stream with an error
    */
    sendStreamData(int stream_id, data data, bool end_stream = False, timeout drain_timeout = 0) {
        # a draining connection completes the streams in progress
        if (!self.canSendOnStreams()) {
            throw "HTTP2-STATE-ERROR",
                sprintf("cannot send data in state: %y", self.getConnectionState());
        }
        *AsyncSocketIoController ctl;
        {
            AutoLock al(lock);
            touchActivityLocked();
            ctl = controller_ref;
        }

        log(LoggerLevel::DEBUG, "sending data on stream %d", stream_id);
        # a full send buffer takes nothing: the send waits for the buffer to drain and is made again, as on HTTP/3;
        # the wait is woken by the I/O thread when it hands the stream's data to the transport, and when the stream
        # closes or is reset, so a cancel or a reset ends it at once (issue #5879)
        *date deadline;
        while (True) {
            try {
                poll_op.sendStreamData(stream_id, data, end_stream);
                break;
            } catch (hash<ExceptionInfo> ex) {
                if (ex.err != "HTTP2-FLOW-CONTROL" || drain_timeout <= 0) {
                    rethrow;
                }
                if (!deadline) {
                    deadline = now_us() + milliseconds(drain_timeout);
                }
                int remaining = (deadline - now_us()).durationMilliseconds();
                if (remaining <= 0) {
                    throw "HTTP2-FLOW-CONTROL", sprintf("stream %d: the send buffer did not drain within %d ms; the "
                        "data was not sent", stream_id, drain_timeout);
                }
                # the buffered data is sent by the I/O thread
                if (ctl) {
                    ctl.wakeSocket(sock: getSocket());
                }
                # 0: drained, 1: timed out, -1: the stream has no data buffered any more (sent or closed); the send
                # is made again in each case, and reports a closed or reset stream
                try {
                    getSocket().waitForHttp2StreamDrain(stream_id, remaining);
                } catch (hash<ExceptionInfo> wait_ex) {
                    # the wait failed (for example, the session is gone, or the wait was made on the I/O thread):
                    # the send is made once more, so that it reports the stream's state, or the data is sent; if
                    # the buffer is still full, HTTP2-FLOW-CONTROL is raised with the reason the wait failed
                    try {
                        poll_op.sendStreamData(stream_id, data, end_stream);
                        break;
                    } catch (hash<ExceptionInfo> ex2) {
                        if (ex2.err == "HTTP2-FLOW-CONTROL") {
                            throw "HTTP2-FLOW-CONTROL", sprintf("stream %d: the send buffer is full and cannot be "
                                "waited for (%s: %s); the data was not sent", stream_id, wait_ex.err, wait_ex.desc);
                        }
                        rethrow;
                    }
                }
            }
        }

        if (ctl) {
            ctl.wakeSocket(sock: getSocket());
        }
    }

    #! Releases the records that the session of the connection keeps for a stream that the caller is done with
    /** Called by the stream handle when it is destroyed; see
        @ref Qore::Http2ClientPollOperationBase::releaseStream() "Http2ClientPollOperationBase::releaseStream()"

        @param stream_id the ID of the stream

        @since HttpClientIo 1.0
    */
    releaseStream(int stream_id) {
        poll_op.releaseStream(stream_id);
    }

    #! Sends trailers that end the request body of a streaming request
    /** @param stream_id the HTTP/2 stream ID
        @param trailers the trailer fields

        @throw HTTP2-STATE-ERROR if the connection is neither ready nor draining
        @throw HTTP2-ERROR if the trailers cannot be sent

        @since HttpClientIo 1.0
    */
    sendTrailers(int stream_id, hash<string, string> trailers) {
        # a draining connection completes the streams in progress
        if (!self.canSendOnStreams()) {
            throw "HTTP2-STATE-ERROR",
                sprintf("cannot send trailers in state: %y", self.getConnectionState());
        }
        *AsyncSocketIoController ctl;
        {
            AutoLock al(lock);
            touchActivityLocked();
            ctl = controller_ref;
        }
        log(LoggerLevel::DEBUG, "sending trailers on stream %d", stream_id);
        getSocket().sendHttp2Trailers(stream_id, trailers);
        if (ctl) {
            ctl.wakeSocket(sock: getSocket());
        }
    }

    #! Registers a Queue for async stream data delivery
    /** @param stream_id the HTTP/2 stream ID
        @param queue the Queue for data delivery; a Queue with a maximum size is supported (the I/O thread never
        waits for room in it; the data that it has no room for stays in the stream)
    */
    registerStreamQueue(int stream_id, Queue queue) {
        poll_op.registerStreamQueue(stream_id, queue);
    }

    #! Registers a listener to be notified when stream data arrives
    /** @param stream_id the HTTP/2 stream ID
        @param listener an object with a notifyIo() method
    */
    registerStreamListener(int stream_id, object listener) {
        poll_op.registerStreamListener(stream_id, listener);
    }

    #! Records on the stream's frame state that the application sends its WebSocket close frame itself
    /** See
        @ref Qore::Http2ClientPollOperationBase::markFrameStateCloseSent() "Http2ClientPollOperationBase::markFrameStateCloseSent()"

        @return 0 if the stream has no frame state, 1 if the close is recorded (the caller sends its close frame), 2
        if the frame state sent a close frame before (the caller sends none)

        @since HttpClientIo 1.0
    */
    int markFrameStateCloseSent(int stream_id) {
        return poll_op.markFrameStateCloseSent(stream_id);
    }

    #! Installs a WebSocket frame-state decoder on an existing CONNECT stream
    /** @param stream_id the HTTP/2 stream ID
        @param msg_queue Queue to receive typed frame hashes
        @param initial_body optional body bytes already consumed before
        installing the frame state
    */
    installFrameState(int stream_id, Queue msg_queue, *data initial_body) {
        poll_op.installFrameState(stream_id, msg_queue, initial_body);
    }

    #! Installs an SSE parser on an existing streaming request
    /** @param stream_id the HTTP/2 stream ID
        @param queue Queue to receive parsed SSE events
        @param notifier optional EventNotifier signaled after queue updates
        @param initial_body optional body bytes already consumed before
        installing the parser
    */
    installSseState(int stream_id, Queue queue, *EventNotifier notifier,
            *data initial_body) {
        poll_op.installSseState(stream_id, queue, notifier, initial_body);
    }

    #! Submits a request with streaming support (HTTP/2-specific)
    /** @param method HTTP method
        @param path request path
        @param headers request headers
        @param body request body (optional)
        @param channel Channel for incremental response delivery
        @param streaming if True, enable streaming mode
        @param notifier optional EventNotifier signaled after channel updates
        @param sse_install_eligible permits installing the SSE parser for a qualifying response

        @return the stream ID

        @note The capacity check is performed atomically inside
        poll_op.submitRequestStreaming() under stream_lock, so there is no TOCTOU gap.
        @throw HTTPCLIENT-REQUEST-BODY-ERROR a GET request specifies a body or enables request-body streaming
    */
    int submitRequestStreaming(string method, string path, *hash<auto> headers, *data body,
            Channel channel, bool streaming = False, *EventNotifier notifier,
            bool sse_install_eligible = False) {
        checkRequestBody(method, headers, body, streaming);
        if (!self.isReady()) {
            throw "HTTPCLIENT-STATE-ERROR",
                sprintf("connection not ready: state=%y", self.getConnectionState());
        }
        *AsyncSocketIoController ctl;
        {
            AutoLock al(lock);
            touchActivityLocked();
            ctl = controller_ref;
        }

        log(LoggerLevel::DEBUG, "submitting request: %s %s (streaming=%y)", method, path, streaming);
        int stream_id = poll_op.submitRequestStreaming(method, path, headers, body, channel,
            max_concurrent_streams, streaming, notifier, sse_install_eligible);

        if (ctl) {
            ctl.wakeSocket(sock: getSocket());
        }

        return stream_id;
    }

    #! Submits an SSE request with Queue-based parsed event delivery
    /** Like submitRequestStreaming() but creates an SseAction that parses SSE
        events on the I/O thread and pushes parsed SseMessageInfo hashes to a Queue.

        @param method HTTP method
        @param path request path
        @param headers request headers (optional)
        @param body request body (optional)
        @param queue Queue for parsed SSE event delivery
        @param notifier optional EventNotifier signaled after queue updates

        @return the stream ID
        @throw HTTPCLIENT-REQUEST-BODY-ERROR a GET request specifies a body or enables request-body streaming
    */
    int submitRequestSse(string method, string path, *hash<auto> headers, *data body,
            Queue queue, *EventNotifier notifier) {
        checkRequestBody(method, headers, body);
        if (!self.isReady()) {
            throw "HTTPCLIENT-STATE-ERROR",
                sprintf("connection not ready: state=%y", self.getConnectionState());
        }
        *AsyncSocketIoController ctl;
        {
            AutoLock al(lock);
            touchActivityLocked();
            ctl = controller_ref;
        }

        log(LoggerLevel::DEBUG, "submitting SSE request: %s %s", method, path);
        int stream_id = poll_op.submitRequestSse(method, path, headers, body, queue,
            max_concurrent_streams, notifier);

        if (ctl) {
            ctl.wakeSocket(sock: getSocket());
        }

        return stream_id;
    }
}

} # namespace HttpClientIo
