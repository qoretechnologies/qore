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
    */
    constructor(hash<auto> url_info, timeout connect_timeout = DefaultConnectTimeout,
            *Logger logger, int max_streams = DefaultMaxStreamsPerConnection,
            *int ssl_verify_mode, bool accept_all_certs = False) {
        self.url_info = url_info;
        self.connect_timeout = connect_timeout;
        self.logger = logger;
        self.max_concurrent_streams = max_streams;
        self.last_activity = now_us();
        connection_id = allocateConnectionId();

        log(LoggerLevel::DEBUG, "creating connection to %s:%d (ssl=%y max_streams=%d)",
            url_info.host, url_info.port, url_info.protocol == "https", max_streams);

        poll_op = new Http2ClientPollOperation(url_info, connect_timeout, logger,
            ssl_verify_mode, accept_all_certs);
        poll_op.setReadyCallback(\onConnectionReady());
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
    */
    int submitRequestImpl(string method, string path, *hash<auto> headers,
            *data body, code callback, *hash<auto> ctx) {
        return poll_op.submitRequest(method, path, headers, body, callback, ctx,
            max_concurrent_streams);
    }

    #! Cancels a request (protocol-specific)
    cancelRequestImpl(int stream_id) {
        poll_op.cancelRequest(stream_id);
    }

    #! Closes the connection (protocol-specific)
    /** @par Thread Safety
        Coordinates with the driving mechanism: sets state to Closed,
        waits for any active driver thread to finish, then calls abort().
    */
    closeImpl(*timeout close_timeout) {
        timeout ct = close_timeout ?? 10s;
        {
            AutoLock al(lock);
            state = HttpClientConnectionState::Closed;
            ready_cond.broadcast();
            # Wait for any active driver to finish before aborting poll_op
            date end_time = now_us() + ct;
            while (driving) {
                date remaining = end_time - now_us();
                if (remaining <= 0us) {
                    log(LoggerLevel::ERROR, "close() timed out waiting for driver thread");
                    break;
                }
                ready_cond.wait(lock, min(remaining, 5s));
            }
        }
        try {
            poll_op.abort();
        } catch () {
            # Ignore abort errors during close
        }
    }

    #! Sends data on a stream (HTTP/2-specific streaming support)
    /** @param stream_id the stream ID to send data on
        @param data the data to send (can be string or binary)
        @param end_stream if True, signals end of stream

        @throw HTTP2-STREAM-ERROR if the stream ID is not found
        @throw HTTP2-STATE-ERROR if not in ready state
    */
    sendStreamData(int stream_id, data data, bool end_stream = False) {
        *object ctl;
        {
            AutoLock al(lock);
            if (state != HttpClientConnectionState::Ready) {
                throw "HTTP2-STATE-ERROR",
                    sprintf("cannot send data in state: %y", state);
            }
            last_activity = now_us();
            ctl = controller_ref;
        }

        log(LoggerLevel::DEBUG, "sending data on stream %d", stream_id);
        poll_op.sendStreamData(stream_id, data, end_stream);

        if (ctl) {
            ctl.wake();
        } else {
            poll_notifier.notify();
        }
    }

    #! Submits a request with streaming support (HTTP/2-specific)
    /** @param method HTTP method
        @param path request path
        @param headers request headers
        @param body request body (optional)
        @param callback response callback
        @param ctx optional context
        @param streaming if True, enable streaming mode

        @return the stream ID

        @note The capacity check is performed atomically inside
        poll_op.submitRequest() under stream_lock, so there is no TOCTOU gap.
    */
    int submitRequestStreaming(string method, string path, *hash<auto> headers, *data body,
            code callback, *hash<auto> ctx, bool streaming = False) {
        *object ctl;
        {
            AutoLock al(lock);
            if (state != HttpClientConnectionState::Ready) {
                throw "HTTPCLIENT-STATE-ERROR",
                    sprintf("connection not ready: state=%y", state);
            }
            last_activity = now_us();
            ctl = controller_ref;
        }

        log(LoggerLevel::DEBUG, "submitting request: %s %s (streaming=%y)", method, path, streaming);
        int stream_id = poll_op.submitRequest(method, path, headers, body, callback, ctx,
            max_concurrent_streams, streaming);

        if (ctl) {
            ctl.wake();
        } else {
            poll_notifier.notify();
        }

        return stream_id;
    }
}

} # namespace HttpClientIo
