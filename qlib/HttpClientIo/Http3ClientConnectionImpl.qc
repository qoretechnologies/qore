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
            accept_all_certs, client_cert, client_pk);
        poll_op.setReadyCallback(\onConnectionReady());
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

    #! Sets the controller reference for wake notifications
    /** Also notifies the poll operation that an async controller drives I/O,
        so the STREAM_ID_BLOCKED retry loop does not call continuePoll().
    */
    setControllerRef(object controller) {
        HttpClientConnection::setControllerRef(controller);
        poll_op.setControllerDriven();
    }

    #! Submits a request on this connection (protocol-specific)
    /** The capacity check is performed atomically inside poll_op.submitRequest()
        under the stream_lock.
    */
    int submitRequestImpl(string method, string path, *hash<auto> headers,
            *data body, HttpResponseCallback callback, *hash<auto> ctx) {
        return poll_op.submitRequest(method, path, headers, body, callback, ctx,
            max_concurrent_streams);
    }

    #! Cancels a request (protocol-specific)
    cancelRequestImpl(int stream_id) {
        poll_op.cancelRequest(stream_id);
    }

    #! Closes the connection (protocol-specific)
    closeImpl(*timeout close_timeout) {
        {
            AutoLock al(lock);
            state = HttpClientConnectionState::Closed;
            ready_cond.broadcast();
        }
        try {
            poll_op.abort();
        } catch () {
            # Ignore abort errors during close
        }
    }

    #! Protocol-specific poll result hook, called under lock
    /** Checks for GOAWAY received from server atomically with the base state update.
        @note Called while \c lock is held — do not re-acquire.
    */
    private handlePollResultImpl() {
        if (poll_op.isGoawayReceived() && state == HttpClientConnectionState::Ready) {
            state = HttpClientConnectionState::Draining;
            log(LoggerLevel::DEBUG, "GOAWAY detected, transitioning to Draining");
            ready_cond.broadcast();
        }
    }

    #! Initiates QUIC connection migration to a new network path
    /** @return True if migration was initiated successfully

        @throw HTTP3-STATE-ERROR if the connection is not in Ready state
    */
    bool migrate(*timeout migration_timeout) {
        {
            AutoLock al(lock);
            if (state != HttpClientConnectionState::Ready) {
                throw "HTTP3-STATE-ERROR",
                    sprintf("cannot migrate: connection not ready (state=%y)", state);
            }
            last_activity = now_us();
        }

        log(LoggerLevel::DEBUG, "initiating QUIC connection migration");
        poll_op.migrate();
        log(LoggerLevel::DEBUG, "QUIC connection migration completed");
        return True;
    }

}

} # namespace HttpClientIo
