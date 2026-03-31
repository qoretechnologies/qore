# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file HttpClientConnection.qc Abstract base for HTTP client connections

/** HttpClientConnection.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Abstract base class for HTTP client connections
/** This class provides the common interface and shared concrete methods for
    HTTP/1.1, HTTP/2, and HTTP/3 client connections.

    @par Thread Safety
    This class is thread-safe. All mutable state is protected by \c lock.

    @par Lock Ordering
    When both locks are needed, \c HttpClientConnection.lock must always be
    acquired before \c HttpClientPollOperation.stream_lock.  Never acquire
    in reverse order or a deadlock will result.
*/
public class HttpClientConnection inherits AbstractHttpPollConnection {
    private {
        #! URL info (host, port, scheme)
        hash<auto> url_info;

        #! Lock for Qore-level shared members (last_activity, controller_ref, pending_stream_count)
        /** @note Connection state (state, ready_cond) is in the C++ base class
            AbstractHttpPollConnection. This lock only protects Qore members.
        */
        Mutex lock();

        #! Maximum concurrent streams
        int max_concurrent_streams = DefaultMaxStreamsPerConnection;

        #! Connection ID
        int connection_id;

        #! Static counter for connection IDs (shared across protocols for globally unique IDs)
        static int next_connection_id = 0;

        #! Static lock for connection ID counter
        static Mutex connection_id_lock();

        #! Connect timeout
        timeout connect_timeout;

        #! Logger
        *LoggerInterface logger;

        #! Last activity timestamp
        date last_activity;

        #! Reference to AsyncSocketIoController for wake notifications
        *object controller_ref;

        #! Number of pending stream reservations (threads assigned but not yet submitted)
        int pending_stream_count = 0;

        #! Proxy connection info (if using a proxy)
        *hash<auto> proxy_info;

        #! Reference to the connection manager (for completion dispatch)
        *HttpClientConnectionManager manager_ref;

        #! Pool key in the connection manager
        *string pool_key;
    }

    #! Returns the protocol identifier: "h1", "h2", or "h3"
    abstract string getProtocol();

    #! Returns the poll operation for this connection
    abstract HttpClientPollOperation getPollOperation();

    #! Returns the underlying socket
    abstract Socket getSocket();

    #! Submits a request on this connection (protocol-specific)
    /** @param promise Promise to resolve with the response (pure C++ dispatch)
    */
    abstract int submitRequestImpl(string method, string path, *hash<auto> headers,
        *data body, Promise promise);

    #! Cancels a request (protocol-specific)
    abstract cancelRequestImpl(int stream_id);

    #! Closes the connection (protocol-specific)
    abstract closeImpl(*timeout close_timeout);

    #! Sets the connection manager context for completion dispatch
    /** @param mgr the connection manager that owns this connection
        @param key the pool key for this connection
        @note Called once during connection setup, before submission to the controller.
    */
    setManagerContext(HttpClientConnectionManager mgr, string key) {
        manager_ref = mgr;
        pool_key = key;
    }

    #! Called by the poll operation when onComplete fires
    /** Forwards to the connection manager's handleConnectionResult().
        @param result the poll result hash
        @note Called from a worker thread (dispatched by AsyncIoControllerPriv).
    */
    handleCompletion(hash<SocketPollResultInfo> result) {
        if (manager_ref) {
            manager_ref.handleConnectionResult(pool_key, self, result);
        }
    }

    #! Returns True if the connection can accept a new stream (advisory, no side effects)
    /** Allows both Ready and Connecting connections to accept streams, since
        the stream handle's request() method will wait for the connection to
        become ready before submitting the request.

        @note This is a pure query — it does not reserve a slot. Use
        \c tryReserveStream() for atomic check-and-reserve in connection
        selection paths where multiple threads compete.
    */
    bool canAcceptStream() {
        AutoLock al(lock);
        int cs = self.getConnectionState();
        return (cs == HttpClientConnectionState::Ready
                || cs == HttpClientConnectionState::Connecting)
            && (!max_concurrent_streams
                || (getPollOperation().getActiveStreamCount() + pending_stream_count)
                    < max_concurrent_streams);
    }

    #! Atomically checks capacity and reserves a stream slot
    /** Returns True if a reservation was made, False if the connection is at
        capacity or not in a usable state.

        @note The caller MUST call \c releaseStreamReservation() when the
        stream is submitted (converted to an active stream) or abandoned.

        @par Thread Safety
        The check and reservation happen under \c lock in one critical section,
        eliminating the TOCTOU gap that exists with separate
        \c canAcceptStream() + manual reservation.
    */
    bool tryReserveStream() {
        AutoLock al(lock);
        int cs = self.getConnectionState();
        if (cs != HttpClientConnectionState::Ready
                && cs != HttpClientConnectionState::Connecting) {
            return False;
        }
        int total = getPollOperation().getActiveStreamCount() + pending_stream_count;
        if (max_concurrent_streams && total >= max_concurrent_streams) {
            return False;
        }
        ++pending_stream_count;
        return True;
    }

    #! Releases a pending stream reservation
    /** Called when a reserved stream is submitted (reservation converts to
        active stream) or when the request is abandoned before submission.
    */
    releaseStreamReservation() {
        AutoLock al(lock);
        if (pending_stream_count > 0) {
            --pending_stream_count;
        }
    }

    #! Returns the number of active streams
    int getActiveStreamCount() {
        return getPollOperation().getActiveStreamCount();
    }

    #! Returns the connection ID
    int getConnectionId() {
        return connection_id;
    }

    #! Returns the last activity timestamp
    date getLastActivity() {
        AutoLock al(lock);
        return last_activity;
    }

    #! Returns True if the connection is in controller mode
    bool isControllerMode() {
        return exists controller_ref;
    }

    #! Sets the controller reference for wake notifications
    setControllerRef(object controller) {
        controller_ref = controller;
    }

    #! Returns connection info
    hash<HttpClientConnectionInfo> getConnectionInfo() {
        AutoLock al(lock);
        return <HttpClientConnectionInfo>{
            "host": url_info.host,
            "port": url_info.port,
            "state": cast<HttpClientConnectionState>(self.getConnectionState()),
            "active_streams": getPollOperation().getActiveStreamCount(),
            "max_concurrent_streams": max_concurrent_streams,
            "connection_id": connection_id,
            "protocol": getProtocol(),
            "ssl": url_info.protocol == "https",
        };
    }

    #! Submits a new request on this connection
    /** Validates state and delegates to the protocol-specific submitRequestImpl().
        The result is delivered via the returned Future (pure C++ Promise resolution
        on the I/O thread — no user code executes on the I/O thread).

        @param method HTTP method (e.g., \c "GET", \c "POST")
        @param path request path (e.g., \c "/api/users")
        @param headers request headers (optional)
        @param body request body (optional)
        @param suppress_wake if True, suppresses the I/O controller wake after
        submitting the request.

        @return a hash with "stream_id" (int) and "future" (Future) keys

        @throw HTTPCLIENT-STATE-ERROR if the connection is not ready
    */
    hash<auto> submitRequest(string method, string path, *hash<auto> headers, *data body,
            bool suppress_wake = False) {
        *object ctl;
        {
            AutoLock al(lock);
            if (!self.isReady()) {
                throw "HTTPCLIENT-STATE-ERROR",
                    sprintf("connection not ready: state=%y", self.getConnectionState());
            }
            last_activity = now_us();
            ctl = controller_ref;
        }

        log(LoggerLevel::DEBUG, "submitting request: %s %s", method, path);
        Promise p();
        Future f = p.getFuture();
        int stream_id;
        try {
            stream_id = submitRequestImpl(method, path, headers, body, p);
        } catch (hash<ExceptionInfo> ex) {
            # Mark connection Closed only for connection-fatal errors so the
            # pool evicts it immediately; transient errors leave it Ready
            if (HttpClientIo::HttpClientConnectionManager::isConnectionErrorStatic(ex.err)) {
                self.setClosed();
            }
            rethrow;
        }
        @assert(stream_id >= 0);

        # Release the pending reservation AFTER successful submit —
        # active_stream_count was incremented inside submitRequestImpl().
        # If submitRequestImpl() throws, the caller releases the reservation.
        {
            AutoLock al(lock);
            if (pending_stream_count > 0) {
                --pending_stream_count;
            }
        }

        if (!suppress_wake && ctl) {
            ctl.wakeSocket(getSocket());
        }

        # Return the Promise alongside the Future so the caller keeps it alive
        # until the Future is consumed. The Promise must not be destroyed before
        # the I/O thread resolves it.
        return {
            "stream_id": stream_id,
            "future": f,
            "promise": p,
        };
    }

    #! Cancels a pending request
    cancelRequest(int stream_id) {
        log(LoggerLevel::DEBUG, "cancelling request: stream_id=%d", stream_id);
        cancelRequestImpl(stream_id);
    }

    #! Wakes the I/O controller or poll notifier to flush pending writes
    /** Call this after queuing data outside of the normal request path
        (e.g., QUIC datagrams via submitQuicDatagram()) so the poll loop
        triggers a write cycle promptly.
    */
    notifyPendingData() {
        *object ctl;
        {
            AutoLock al(lock);
            ctl = controller_ref;
        }
        if (ctl) {
            ctl.wakeSocket(getSocket());
        }
    }

    #! Handles poll result from AsyncSocketIoController
    /** Updates state under lock, then calls handlePollResultImpl() for
        protocol-specific handling (e.g. GOAWAY check) while still holding the lock.
    */
    handlePollResult() {
        {
            AutoLock al(lock);
            last_activity = now_us();
        }

        if (getPollOperation().isClosed()) {
            self.setClosed();
        }

        # Protocol-specific hook (not called under Qore lock — C++ base handles state)
        handlePollResultImpl();
    }

    #! Protocol-specific poll result hook, called under lock
    /** Default no-op; override for protocol-specific handling (e.g. GOAWAY check).
        @note Called while \c lock is held — do not attempt to re-acquire it.
    */
    private handlePollResultImpl() {
    }

    #! Waits for the connection to become ready
    /** Delegates to the C++ base class waitForReady() for the condition wait,
        then checks for errors (the C++ base returns False for both timeout
        and closed-with-error).
    */
    bool waitForReady(timeout timeout_ms = 30s) {
        log(LoggerLevel::DEBUG, "waitForReady() timeout=%y", timeout_ms);
        bool ready = AbstractHttpPollConnection::waitForReady(timeout_ms);

        if (self.isClosed()) {
            *hash<auto> err = getPollOperation().getError();
            throw "HTTPCLIENT-CONNECT-ERROR",
                sprintf("connection closed: %s: %s", err.err ?? "UNKNOWN", err.desc ?? "unknown");
        }

        return ready;
    }

    #! Sets the connection to draining state (GOAWAY received)
    setDraining() {
        log(LoggerLevel::DEBUG, "connection draining");
        AbstractHttpPollConnection::setDraining();
    }

    #! Closes the connection
    close(*timeout close_timeout) {
        log(LoggerLevel::DEBUG, "closing connection");
        closeImpl(close_timeout);
    }

    #! Initiates connection migration (HTTP/3 only; default no-op for HTTP/2)
    bool migrate(*timeout migration_timeout) {
        return False;
    }

    #! Internal: allocates a unique connection ID
    private int allocateConnectionId() {
        AutoLock al(connection_id_lock);
        return ++next_connection_id;
    }

    #! Internal: logging helper
    private log(int level, string fmt) {
        if (logger) {
            call_function_args(\logger.log(),
                (level, sprintf("HttpClientConnection[%d/%s]: ", connection_id, getProtocol()) + fmt) + argv);
        }
    }
}

} # namespace HttpClientIo
