# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file Http3ClientConnectionImpl.qc HTTP/3 client connection implementation over QUIC

/** Http3ClientConnectionImpl.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! HTTP/3 client connection implementation over QUIC
/** This class wraps a Socket + Http3ClientPollOperation to provide a
    higher-level interface for HTTP/3 multiplexed client connections.

    A single QUIC connection can handle multiple concurrent streams, with
    responses dispatched to callbacks by stream ID. Unlike HTTP/2, streams
    are independent at the transport layer (no head-of-line blocking).

    @par Thread Safety
    This class is thread-safe. Stream submission uses the QuicSession's
    internal mutex for synchronization.
*/
public class Http3ClientConnection inherits HttpClientConnection {
    private {
        #! Poll operation for this connection
        Http3ClientPollOperation poll_op;
    }

    #! Creates a new HTTP/3 client connection
    /** @param url_info parsed URL info with host, port, scheme
        @param connect_timeout timeout for connection establishment
        @param logger optional logger for debug output
        @param max_streams maximum concurrent streams per connection
        @param ssl_verify_mode optional SSL certificate verification mode
        @param accept_all_certs if True, accept all server certificates
        @param client_cert optional client certificate for mutual TLS (mTLS)
        @param client_pk optional client private key for mutual TLS (mTLS)
    */
    constructor(hash<auto> url_info, timeout connect_timeout = DefaultConnectTimeout,
            *LoggerInterface logger, int max_streams = DefaultMaxStreamsPerConnection,
            *int ssl_verify_mode, bool accept_all_certs = False,
            *SSLCertificate client_cert, *SSLPrivateKey client_pk) {
        self.url_info = url_info;
        self.connect_timeout = connect_timeout;
        self.logger = logger;
        self.max_concurrent_streams = max_streams;
        self.last_activity = now_us();
        connection_id = allocateConnectionId();

        log(LoggerLevel::DEBUG, "creating QUIC connection to %s:%d (max_streams=%d)",
            url_info.host, url_info.port, max_streams);

        poll_op = new Http3ClientPollOperation(url_info, connect_timeout, logger, ssl_verify_mode,
            accept_all_certs, client_cert, client_pk, self);
    }

    #! Copying is not supported
    private copy() {
        throw "COPY-ERROR", "Http3ClientConnection objects cannot be copied";
    }

    #! Returns the protocol identifier
    string getProtocol() {
        return "h3";
    }

    #! Returns the poll operation for this connection
    HttpClientPollOperation getPollOperation() {
        return poll_op;
    }

    #! Returns the underlying socket
    Socket getSocket() {
        return poll_op.getSocket();
    }

    #! Returns the QUIC session ID (0 if not yet connected)
    int getSessionId() {
        return poll_op.getSessionId();
    }

    #! Returns True if the connection can accept a new stream (advisory, no side effects)
    /** Extends the base class check with the discovered QUIC stream limit and
        QUIC session health.
    */
    bool canAcceptStream() {
        AutoLock al(lock);
        int cs = self.getConnectionState();
        if (cs != HttpClientConnectionState::Ready
                && cs != HttpClientConnectionState::Connecting) {
            return False;
        }
        # Check if the underlying QUIC session is still alive
        if (!poll_op.isQuicSessionOpen()) {
            return False;
        }
        int total = poll_op.getActiveStreamCount() + pending_stream_count;
        if (max_concurrent_streams && total >= max_concurrent_streams) {
            return False;
        }
        int quic_limit = poll_op.getQuicStreamLimit();
        if (quic_limit && total >= quic_limit) {
            return False;
        }
        return True;
    }

    #! Atomically checks capacity and reserves a stream slot (QUIC-specific)
    /** Extends the base class with QUIC session health and stream limit checks.
    */
    bool tryReserveStream() {
        AutoLock al(lock);
        int cs = self.getConnectionState();
        if (cs != HttpClientConnectionState::Ready
                && cs != HttpClientConnectionState::Connecting) {
            return False;
        }
        if (!poll_op.isQuicSessionOpen()) {
            return False;
        }
        int total = poll_op.getActiveStreamCount() + pending_stream_count;
        if (max_concurrent_streams && total >= max_concurrent_streams) {
            return False;
        }
        int quic_limit = poll_op.getQuicStreamLimit();
        if (quic_limit && total >= quic_limit) {
            return False;
        }
        # Cap pending reservations on Connecting connections to limit
        # blast radius of a handshake failure
        if (cs == HttpClientConnectionState::Connecting
                && pending_stream_count >= DefaultMaxPendingPerConnection) {
            return False;
        }
        ++pending_stream_count;
        return True;
    }

    #! Submits a request on this connection (protocol-specific)
    /** The capacity check is performed atomically inside poll_op.submitRequest()
        under the stream_lock.
    */
    int submitRequestImpl(string method, string path, *hash<auto> headers,
            *data body, Promise promise) {
        return poll_op.submitRequest(method, path, headers, body, promise,
            max_concurrent_streams);
    }

    #! Submits a request using CompositeAction (Promise + EventNotifier) — HTTP/3
    int submitRequestWithNotifierImpl(string method, string path, *hash<auto> headers,
            *data body, Promise promise, EventNotifier notifier) {
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
        try {
            poll_op.abort();
        } catch () {
            # Ignore abort errors during close
        }
    }

    #! Registers a stream for notifications and sets a listener
    /** @param stream_id the QUIC stream ID
        @param listener an object with a notifyIo() method
    */
    registerStreamNotifyAndListener(int stream_id, object listener) {
        poll_op.registerStreamNotifyAndListener(stream_id, listener);
    }

    #! Sends data on a streaming HTTP/3 request
    /** @param stream_id the stream ID to send data on
        @param data the data to send (can be NOTHING to send END_STREAM only)
        @param end_stream if True, signals end of stream

        @return 0 on success, 1 if buffer full (backpressure), -1 on error

        @throw HTTP3-STATE-ERROR if the connection is not ready
    */
    int sendStreamData(int stream_id, *data data, bool end_stream = False) {
        if (!self.isReady()) {
            throw "HTTP3-STATE-ERROR",
                sprintf("cannot send data in state: %y", self.getConnectionState());
        }
        *object ctl;
        {
            AutoLock al(lock);
            last_activity = now_us();
            ctl = controller_ref;
        }

        log(LoggerLevel::DEBUG, "sending data on stream %d", stream_id);
        int rv = poll_op.sendStreamData(stream_id, data, end_stream);

        if (ctl) {
            ctl.wakeSocket(getSocket());
        }

        return rv;
    }

    #! Submits a request with streaming support (HTTP/3-specific)
    /** @param method HTTP method
        @param path request path
        @param headers request headers
        @param body request body (optional)
        @param promise Promise to resolve with the response
        @param streaming if True, enable streaming mode (HEADERS without END_STREAM)

        @return the stream ID

        @note The capacity check is performed atomically inside
        poll_op.submitRequest() under stream_lock, so there is no TOCTOU gap.
    */
    int submitRequestStreaming(string method, string path, *hash<auto> headers, *data body,
            Promise promise, bool streaming = False) {
        *object ctl;
        if (!self.isReady()) {
            throw "HTTPCLIENT-STATE-ERROR",
                sprintf("connection not ready: state=%y", self.getConnectionState());
        }
        {
            AutoLock al(lock);
            last_activity = now_us();
            ctl = controller_ref;
        }

        log(LoggerLevel::DEBUG, "submitting request: %s %s (streaming=%y)", method, path, streaming);
        int stream_id = poll_op.submitRequest(method, path, headers, body, promise,
            max_concurrent_streams, streaming);

        if (ctl) {
            ctl.wakeSocket(getSocket());
        }

        return stream_id;
    }

    #! Submits a streaming request with Channel-based response delivery
    /** Each response chunk is pushed to the Channel via ChannelAction (pure C++).
        @param method HTTP method
        @param path request path
        @param headers request headers
        @param body request body (optional)
        @param channel Channel to push response chunks to
        @param streaming if True, enable bidirectional streaming mode
        @return the stream ID
    */
    int submitRequestStreamingWithChannel(string method, string path, *hash<auto> headers,
            *data body, Channel channel, bool streaming = False) {
        if (!self.isReady()) {
            throw "HTTPCLIENT-STATE-ERROR",
                sprintf("connection not ready: state=%y", self.getConnectionState());
        }
        *object ctl;
        {
            AutoLock al(lock);
            last_activity = now_us();
            ctl = controller_ref;
        }

        log(LoggerLevel::DEBUG, "submitting streaming request: %s %s (streaming=%y)", method, path, streaming);
        int stream_id = poll_op.submitRequestWithChannel(method, path, headers, body,
            channel, max_concurrent_streams, streaming);

        if (ctl) {
            ctl.wakeSocket(getSocket());
        }

        return stream_id;
    }

    #! Protocol-specific poll result hook, called after the base state update
    /** Checks for GOAWAY received from server after the base state update completes.
        @note Called without \c lock held.
    */
    private handlePollResultImpl() {
        if (poll_op.isGoawayReceived() && self.isReady()) {
            self.setDraining();
            log(LoggerLevel::DEBUG, "GOAWAY detected, transitioning to Draining");
        }
    }

    #! Initiates QUIC connection migration to a new network path
    /** Migration creates a new UDP socket and initiates QUIC path migration.
        The old fd is closed and the controller is woken so that its next
        forced poll cycle calls continuePoll(), which returns poll info
        referencing the Socket object (now pointing to the new fd), causing
        the EventLoop to re-register the new fd.

        @return True if migration was initiated successfully

        @throw HTTP3-STATE-ERROR if the connection is not in Ready state
    */
    bool migrate(*timeout migration_timeout) {
        if (!self.isReady()) {
            throw "HTTP3-STATE-ERROR",
                sprintf("cannot migrate: connection not ready (state=%y)", self.getConnectionState());
        }
        *object ctl;
        {
            AutoLock al(lock);
            last_activity = now_us();
            ctl = controller_ref;
        }

        log(LoggerLevel::DEBUG, "initiating QUIC connection migration");
        poll_op.migrate();
        log(LoggerLevel::DEBUG, "QUIC connection migration completed");

        # Wake the controller so it forces a continuePoll() cycle with the new fd
        if (ctl) {
            ctl.wakeSocket(getSocket());
        }

        return True;
    }

}

} # namespace HttpClientIo
