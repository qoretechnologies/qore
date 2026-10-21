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
        @param ssl_ca_file a PEM file with the CA certificates to verify the server with instead of the default CAs
        @param ssl_ca_path a directory with the CA certificates to verify the server with instead of the default CAs
        @param address_family the address family to connect in; if not set, the family of the first address of the
            host is used, unless it is in \a avoid_families
        @param avoid_families address families not to connect in if the host also has an address in another family

        Creating the connection runs nothing that can block, so it can also be created on an async I/O thread: the
        host name is resolved and the UDP socket is bound by the connection's poll operation once it is submitted to
        the I/O controller (see @ref getAddressFamily())
    */
    constructor(hash<auto> url_info, timeout connect_timeout = DefaultConnectTimeout,
            *LoggerInterface logger, int max_streams = DefaultMaxStreamsPerConnection,
            *int ssl_verify_mode, bool accept_all_certs = False,
            *SSLCertificate client_cert, *SSLPrivateKey client_pk, *string ssl_ca_file, *string ssl_ca_path,
            *int address_family, *list<int> avoid_families)
            : HttpClientConnection(url_info, connect_timeout, logger, max_streams) {

        log(LoggerLevel::DEBUG, "creating QUIC connection to %s:%d (max_streams=%d)",
            url_info.host, url_info.port, max_streams);

        poll_op = new Http3ClientPollOperation(url_info, connect_timeout, logger, ssl_verify_mode,
            accept_all_certs, client_cert, client_pk, self, ssl_ca_file, ssl_ca_path, address_family,
            avoid_families);
    }

    #! Copying is not supported
    private copy() {
        throw "COPY-ERROR", "Http3ClientConnection objects cannot be copied";
    }

    #! Returns the address family of the connection (@ref Qore::AF_INET or @ref Qore::AF_INET6)
    /** Does not block

        @return the address family the connection is made in, or @ref nothing if the host name has not been resolved
        yet
    */
    *int getAddressFamily() {
        return poll_op.getAddressFamily();
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
        int total = poll_op.getActiveStreamCount() + pending_stream_count;
        if (max_concurrent_streams && total >= max_concurrent_streams) {
            return False;
        }
        if (cs == HttpClientConnectionState::Connecting) {
            return pending_stream_count < DefaultMaxPendingPerConnection;
        }
        # Check if the underlying QUIC session is still alive after the
        # handshake has completed. During CONNECTING the session may not yet
        # be open, but reserved stream handles will wait for readiness before
        # submitting any request bytes.
        if (!poll_op.isQuicSessionOpen()) {
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
        int total = poll_op.getActiveStreamCount() + pending_stream_count;
        if (max_concurrent_streams && total >= max_concurrent_streams) {
            return False;
        }
        if (cs == HttpClientConnectionState::Connecting) {
            # Cap pending reservations on Connecting connections to limit
            # blast radius of a handshake failure.
            if (pending_stream_count >= DefaultMaxPendingPerConnection) {
                return False;
            }
        } else {
            if (!poll_op.isQuicSessionOpen()) {
                return False;
            }
            int quic_limit = poll_op.getQuicStreamLimit();
            if (quic_limit && total >= quic_limit) {
                return False;
            }
        }
        ++pending_stream_count;
        return True;
    }

    #! Wakes the async I/O controller to flush pending QUIC writes
    /** Queued QUIC packets are produced and sent by the controller's I/O
        thread; this method only wakes the controller.

    */
    flushPendingWrites() {
        notifyPendingData();
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

    #! Submits a request using CompositeAction (Promise + EventNotifier) — HTTP/3
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
        }
        try {
            self.getSocket().close();
        } catch () {
            # Ignore close errors (socket may already be closed)
        }
    }

    #! Registers a stream for notifications and sets a listener
    /** @param stream_id the QUIC stream ID
        @param listener an object with a notifyIo() method
    */
    registerStreamNotifyAndListener(int stream_id, object listener) {
        poll_op.registerStreamNotifyAndListener(stream_id, listener);
    }

    #! Installs a WebSocket frame-state decoder on an existing CONNECT stream
    /** @param stream_id the QUIC stream ID
        @param msg_queue Queue to receive typed frame hashes
        @param initial_body optional body bytes already consumed before
        installing the frame state
    */
    installFrameState(int stream_id, Queue msg_queue, *data initial_body) {
        poll_op.installFrameState(stream_id, msg_queue, initial_body);
    }

    #! Installs an SSE parser on an existing streaming request
    /** @param stream_id the QUIC stream ID
        @param queue Queue to receive parsed SSE events
        @param notifier optional EventNotifier signaled after queue updates
        @param initial_body optional body bytes already consumed before
        installing the parser
    */
    installSseState(int stream_id, Queue queue, *EventNotifier notifier,
            *data initial_body) {
        poll_op.installSseState(stream_id, queue, notifier, initial_body);
    }

    #! Releases the records that the session of the connection keeps for a stream that the caller is done with
    /** Called by the stream handle when it is destroyed; see
        @ref Qore::Http3ClientPollOperationBase::releaseStream() "Http3ClientPollOperationBase::releaseStream()"

        @param stream_id the ID of the stream

        @since HttpClientIo 1.0
    */
    releaseStream(int stream_id) {
        poll_op.releaseStream(stream_id);
    }

    #! Sends trailers that end the request body of a streaming request
    /** @param stream_id the HTTP/3 stream ID
        @param trailers the trailer fields

        @throw HTTP3-STATE-ERROR if the connection is neither ready nor draining
        @throw HTTP3-CONNECTION-CLOSED if the QUIC session is closed
        @throw QUIC-ERROR if the trailers cannot be sent

        @since HttpClientIo 1.0
    */
    sendTrailers(int stream_id, hash<string, string> trailers) {
        # a draining connection completes the streams in progress
        if (!self.canSendOnStreams()) {
            throw "HTTP3-STATE-ERROR",
                sprintf("cannot send trailers in state: %y", self.getConnectionState());
        }
        Socket sock = poll_op.getSocket();
        if (sock.isQuicSessionClosed()) {
            self.setClosed();
            throw "HTTP3-CONNECTION-CLOSED",
                "QUIC session closed; connection evicted from pool";
        }
        *AsyncSocketIoController ctl;
        {
            AutoLock al(lock);
            touchActivityLocked();
            ctl = controller_ref;
        }
        log(LoggerLevel::DEBUG, "sending trailers on stream %d", stream_id);
        sock.sendQuicClientTrailers(stream_id, trailers);
        if (ctl) {
            ctl.wakeSocket(sock: sock);
        }
    }

    #! Sends data on a streaming HTTP/3 request
    /** @param stream_id the stream ID to send data on
        @param data the data to send (can be NOTHING to send END_STREAM only)
        @param end_stream if True, signals end of stream

        @return 0 on success, -1 on error

        @throw HTTP3-STATE-ERROR if the connection is neither ready nor draining (a draining connection completes the
        streams in progress)
        @throw HTTP3-FLOW-CONTROL if the stream buffer does not drain
        @throw QUIC-STREAM-CLOSED if the server stopped reading the stream or closed it without an error and
        \a data is not empty or \a end_stream is not set; an empty send with \a end_stream set on such a stream is
        a no-op
        @throw QUIC-STREAM-RESET if the server reset the stream with an error
    */
    int sendStreamData(int stream_id, *data data, bool end_stream = False) {
        # a draining connection completes the streams in progress
        if (!self.canSendOnStreams()) {
            throw "HTTP3-STATE-ERROR",
                sprintf("cannot send data in state: %y", self.getConnectionState());
        }
        Socket sock = poll_op.getSocket();
        # Check if the underlying QUIC session is dead before sending
        if (sock.isQuicSessionClosed()) {
            self.setClosed();
            throw "HTTP3-CONNECTION-CLOSED",
                "QUIC session closed; connection evicted from pool";
        }
        *AsyncSocketIoController ctl;
        {
            AutoLock al(lock);
            touchActivityLocked();
            ctl = controller_ref;
        }

        log(LoggerLevel::DEBUG, "sending data on stream %d", stream_id);
        int retries = 0;
        while (True) {
            int rv = poll_op.sendStreamData(stream_id, data, end_stream);
            if (ctl) {
                ctl.wakeSocket(sock: sock);
            }
            if (rv != 1) {
                return rv;
            }
            if (++retries > 200) {
                throw "HTTP3-FLOW-CONTROL", sprintf("stream %d: buffer not "
                    "draining after %d retries", stream_id, retries);
            }
            int drain_rv = sock.waitForQuicClientStreamDrain(stream_id, 50ms);
            if (drain_rv == -1) {
                throw "HTTP3-STREAM-ERROR", sprintf("stream %d: stream "
                    "removed while waiting for drain", stream_id);
            }
        }
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
        @throw HTTPCLIENT-REQUEST-BODY-ERROR a GET request specifies a body or enables request-body streaming
    */
    int submitRequestStreaming(string method, string path, *hash<auto> headers, *data body,
            Promise promise, bool streaming = False) {
        checkRequestBody(method, headers, body, streaming);
        *AsyncSocketIoController ctl;
        if (!self.isReady()) {
            throw "HTTPCLIENT-STATE-ERROR",
                sprintf("connection not ready: state=%y", self.getConnectionState());
        }
        {
            AutoLock al(lock);
            touchActivityLocked();
            ctl = controller_ref;
        }

        log(LoggerLevel::DEBUG, "submitting request: %s %s (streaming=%y)", method, path, streaming);
        int stream_id = poll_op.submitRequest(method, path, headers, body, promise,
            max_concurrent_streams, streaming);

        if (ctl) {
            ctl.wakeSocket(sock: getSocket());
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
        @param notifier optional EventNotifier signaled after channel updates
        @return the stream ID
        @param sse_install_eligible permits installing the SSE parser for a qualifying response
        @throw HTTPCLIENT-REQUEST-BODY-ERROR a GET request specifies a body or enables request-body streaming
    */
    int submitRequestStreamingWithChannel(string method, string path, *hash<auto> headers,
            *data body, Channel channel, bool streaming = False,
            *EventNotifier notifier, bool sse_install_eligible = False) {
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

        log(LoggerLevel::DEBUG, "submitting streaming request: %s %s (streaming=%y)", method, path, streaming);
        int stream_id = poll_op.submitRequestWithChannel(method, path, headers, body,
            channel, max_concurrent_streams, streaming, notifier,
            sse_install_eligible);

        if (ctl) {
            ctl.wakeSocket(sock: getSocket());
        }

        return stream_id;
    }

    #! Submits an SSE request with Queue-based parsed event delivery
    /** Like submitRequestStreamingWithChannel() but creates an SseAction that parses
        SSE events on the I/O thread and pushes parsed SseMessageInfo hashes to a Queue.

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
        int stream_id = poll_op.submitRequestWithQueue(method, path, headers, body,
            queue, max_concurrent_streams, notifier);

        if (ctl) {
            ctl.wakeSocket(sock: getSocket());
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

    #! Migrates the QUIC connection to a new network path (RFC 9000 section 9)
    /** The connection gets a new UDP socket with a new local port, and the QUIC layer moves the connection to the
        new path: it validates the path with the server in the background while requests continue on it.  The
        migration is made by the I/O thread that drives the connection; this method returns once the I/O thread
        has made it, and raises its error if it failed.  Requests in progress continue on the new path.

        Each call makes one migration; concurrent calls are made one after the other, in order.

        Each migration uses a spare connection ID that the server provided, and retires the ID of the path it
        leaves.  When the server has no spare connection ID for a new path, the migration waits until the server
        provides one: the I/O thread tries it again each time packets arrive, as the server sends a new ID in a
        \c NEW_CONNECTION_ID frame.  A server replaces a retired ID only after a while: one probe timeout (PTO, about
        the round-trip time of the connection plus the peer's acknowledgment delay) after it learned of the
        retirement, and an ngtcp2-based server, as the Qore server is, keeps at most 8 connection IDs including the
        retired ones, so a client can make at most 7 migrations in a row before a migration waits for the server.

        @par Example:
        @code{.py}
int old_port = conn.getSocket().getSocketInfo().port;
try {
    conn.migrate(5s);
    printf("migrated from local port %d to %d\n", old_port, conn.getSocket().getSocketInfo().port);
} catch (hash<ExceptionInfo> ex) {
    if (ex.err == "QUIC-MIGRATION-ERROR" || ex.err == "HTTP3-MIGRATION-TIMEOUT") {
        # the connection stays on its previous path; for example, the server provided no spare connection ID
        # within the timeout
        printf("not migrated: %s\n", ex.desc);
    } else {
        rethrow;
    }
}
        @endcode

        @param migration_timeout the maximum time to wait for the migration to start: for the I/O thread to start
        it, and for a spare connection ID if the server has none for a new path; when it expires, the migration is
        withdrawn and the connection is not migrated.  A migration that has started is always waited for; it makes
        no blocking calls.  If no value is given, the connection's connect timeout is used; a value <= 0 means no
        timeout.

        @return @ref True: the connection was migrated

        @throw HTTP3-STATE-ERROR the connection is not ready
        @throw QUIC-MIGRATION-ERROR the QUIC layer refused the migration (for example, the handshake is not
        confirmed yet, or the server disabled active migration), or the new socket could not be created; the
        connection stays on its previous path
        @throw SOCKET-CLOSED the socket of the connection was closed during the migration
        @throw HTTP3-MIGRATION-TIMEOUT the migration did not start within \a migration_timeout: the server provided
        no spare connection ID in time (the description says so), or the I/O thread did not start it; the
        connection was not migrated
        @throw HTTP3-CONNECTION-CLOSED the connection closed before it was migrated
        @throw SOCKET-SYNC-ON-IO-THREAD-ERROR called on an async I/O thread, which drives the connections and so
        could never see the result

        @see @ref HttpClientConnectionManager::migrateAll()
    */
    bool migrate(*timeout migration_timeout) {
        if (!self.isReady()) {
            throw "HTTP3-STATE-ERROR",
                sprintf("cannot migrate: connection not ready (state=%y)", self.getConnectionState());
        }
        *AsyncSocketIoController ctl;
        {
            AutoLock al(lock);
            touchActivityLocked();
            ctl = controller_ref;
        }

        Socket sock = getSocket();
        int old_port = sock.getSocketInfo().port;
        log(LoggerLevel::DEBUG, "initiating QUIC connection migration from local port %d", old_port);
        int id = poll_op.migrate();

        # the I/O thread runs the migration in its next read cycle
        if (ctl) {
            ctl.wakeSocket(sock: sock);
        }

        poll_op.waitForMigration(id, migration_timeout ?? (connect_timeout > 0 ? connect_timeout : -1));
        log(LoggerLevel::DEBUG, "QUIC connection migrated from local port %d to %d", old_port,
            sock.getSocketInfo().port);
        return True;
    }

}

} # namespace HttpClientIo
