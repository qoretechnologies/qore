# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file Http3ClientConnection.qc HTTP/3 client connection with multiplexed streams over QUIC

/** Http3ClientConnection.qc Copyright 2026 Qore Technologies, s.r.o.

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
public namespace Http3ClientIo {

#! HTTP/3 client connection managing multiple concurrent streams over QUIC
/** This class wraps a Socket + Http3ClientPollOperation to provide a
    higher-level interface for HTTP/3 multiplexed client connections.

    A single QUIC connection can handle multiple concurrent streams, with
    responses dispatched to callbacks by stream ID. Unlike HTTP/2, streams
    are independent at the transport layer (no head-of-line blocking).

    @par Thread Safety
    This class is thread-safe. Stream submission uses the QuicSession's
    internal mutex for synchronization.
*/
public class Http3ClientConnection {
    private {
        #! URL info (host, port, scheme)
        hash<auto> url_info;

        #! Poll operation for this connection
        Http3ClientPollOperation poll_op;

        #! Connection state
        Http3ClientConnectionState state = Http3ClientConnectionState::Connecting;

        #! Connection lock for thread safety
        #! Lock ordering: lock → poll_op.stream_lock (never reverse)
        Mutex lock();

        #! Condition for waiting on connection ready
        Condition ready_cond();

        #! Maximum concurrent streams
        int max_concurrent_streams = DefaultMaxStreamsPerConnection;

        #! Connection ID
        int connection_id;

        #! Static counter for connection IDs
        static int next_connection_id = 0;

        #! Static lock for connection ID counter
        static Mutex connection_id_lock();

        #! Connect timeout
        timeout connect_timeout;

        #! Logger
        *Logger logger;

        #! Last activity timestamp
        date last_activity;

        #! Reference to AsyncSocketIoController for wake notifications
        *AsyncSocketIo::AsyncSocketIoController controller_ref;

        #! True when a thread is actively driving to ready
        bool driving = False;

        #! Notifier to wake blocked poll when new requests are submitted (sync mode)
        EventNotifier poll_notifier();
    }

    #! Creates a new HTTP/3 client connection
    /** @param url_info parsed URL info with host, port, scheme
        @param connect_timeout timeout for connection establishment
        @param logger optional logger for debug output
        @param max_streams maximum concurrent streams per connection
        @param ssl_verify_mode optional SSL certificate verification mode
        @param accept_all_certs if True, accept all server certificates (sets SSL_VERIFY_NONE)
    */
    constructor(hash<auto> url_info, timeout connect_timeout = DefaultConnectTimeout,
            *Logger logger, int max_streams = DefaultMaxStreamsPerConnection,
            *int ssl_verify_mode, bool accept_all_certs = False) {
        self.url_info = url_info;
        self.connect_timeout = connect_timeout;
        self.logger = logger;
        self.max_concurrent_streams = max_streams;
        self.last_activity = now_us();

        {
            AutoLock al(connection_id_lock);
            connection_id = ++next_connection_id;
        }

        log(LoggerLevel::DEBUG, "creating QUIC connection to %s:%d (max_streams=%d)",
            url_info.host, url_info.port, max_streams);

        poll_op = new Http3ClientPollOperation(url_info, connect_timeout, logger, ssl_verify_mode, accept_all_certs);
        poll_op.setReadyCallback(\onConnectionReady());
    }

    #! Copying is not supported (holds mutable state: sockets, mutexes, poll operations)
    private copy() {
        throw "COPY-ERROR", "Http3ClientConnection objects cannot be copied";
    }

    #! Internal: called by poll operation when connection becomes ready
    private onConnectionReady() {
        AutoLock al(lock);
        if (state == Http3ClientConnectionState::Connecting) {
            state = Http3ClientConnectionState::Ready;
            log(LoggerLevel::DEBUG, "connection ready (notified via callback)");
            ready_cond.broadcast();
        }
    }

    #! Returns the poll operation for this connection
    Http3ClientPollOperation getPollOperation() {
        return poll_op;
    }

    #! Returns the underlying socket
    Socket getSocket() {
        return poll_op.getSocket();
    }

    #! Sets the controller reference for wake notifications
    /** Also notifies the poll operation that an async controller drives I/O,
        so the STREAM_ID_BLOCKED retry loop does not call continuePoll() (which
        would violate the single-writer assumption).
    */
    setControllerRef(AsyncSocketIo::AsyncSocketIoController controller) {
        controller_ref = controller;
        poll_op.setControllerDriven();
    }

    #! Returns True if the connection is in controller mode
    bool isControllerMode() {
        return exists controller_ref;
    }

    #! Returns connection info
    hash<Http3ClientConnectionInfo> getConnectionInfo() {
        AutoLock al(lock);
        return <Http3ClientConnectionInfo>{
            "host": url_info.host,
            "port": url_info.port,
            "state": state,
            "active_streams": poll_op.getActiveStreamCount(),
            "max_concurrent_streams": max_concurrent_streams,
            "connection_id": connection_id,
        };
    }

    #! Returns True if the connection is ready for new requests
    bool isReady() {
        AutoLock al(lock);
        return isReadyLocked();
    }

    private bool isReadyLocked() {
        return state == Http3ClientConnectionState::Ready && poll_op.isReady();
    }

    #! Returns True if the connection is closed
    bool isClosed() {
        AutoLock al(lock);
        return state == Http3ClientConnectionState::Closed;
    }

    #! Returns True if the connection is draining
    bool isDraining() {
        AutoLock al(lock);
        return state == Http3ClientConnectionState::Draining;
    }

    #! Returns True if this connection can likely accept another stream
    /** @note This is a best-effort check; callers must handle HTTP3-CAPACITY-ERROR
        from submitRequest() if the capacity changes between check and submit.
    */
    bool canAcceptStream() {
        AutoLock al(lock);
        return state == Http3ClientConnectionState::Ready
            && poll_op.getActiveStreamCount() < max_concurrent_streams;
    }

    #! Returns the number of active streams
    int getActiveStreamCount() {
        return poll_op.getActiveStreamCount();
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

    #! Submits a new request on this connection
    /** The capacity check is performed atomically inside poll_op.submitRequest()
        under the stream_lock, eliminating the TOCTOU race between
        canAcceptStream() (optimistic) and the actual submission.
    */
    int submitRequest(string method, string path, *hash<auto> headers, *data body,
            code<auto(hash<auto>, *hash<auto>)> callback, *hash<auto> ctx) {
        {
            AutoLock al(lock);
            if (state != Http3ClientConnectionState::Ready) {
                throw "HTTP3-STATE-ERROR",
                    sprintf("connection not ready: state=%y", state);
            }
            last_activity = now_us();
        }

        log(LoggerLevel::DEBUG, "submitting request: %s %s", method, path);
        int stream_id = poll_op.submitRequest(method, path, headers, body, callback, ctx,
            max_concurrent_streams);

        if (controller_ref) {
            controller_ref.wake();
        } else {
            poll_notifier.notify();
        }

        return stream_id;
    }

    #! Cancels a pending request
    cancelRequest(int stream_id) {
        log(LoggerLevel::DEBUG, "cancelling request: stream_id=%d", stream_id);
        poll_op.cancelRequest(stream_id);
    }

    #! Handles poll result from AsyncSocketIoController
    handlePollResult() {
        AutoLock al(lock);
        last_activity = now_us();

        if (poll_op.isClosed()) {
            state = Http3ClientConnectionState::Closed;
            ready_cond.broadcast();
        } else if (poll_op.isReady() && state == Http3ClientConnectionState::Connecting) {
            state = Http3ClientConnectionState::Ready;
            ready_cond.broadcast();
        }
        # Check for GOAWAY received from server
        if (poll_op.isGoawayReceived() && state == Http3ClientConnectionState::Ready) {
            state = Http3ClientConnectionState::Draining;
            log(LoggerLevel::DEBUG, "GOAWAY detected, transitioning to Draining");
            ready_cond.broadcast();
        }
    }

    #! Waits for the connection to become ready
    bool waitForReady(timeout timeout_ms = 30s) {
        date end_time = now_us() + timeout_ms;

        AutoLock al(lock);
        while (state == Http3ClientConnectionState::Connecting) {
            date remaining = end_time - now_us();
            if (remaining <= 0us) {
                return False;
            }
            ready_cond.wait(lock, remaining);

            if (poll_op.isClosed() || poll_op.hasError()) {
                state = Http3ClientConnectionState::Closed;
                break;
            }
        }

        if (state == Http3ClientConnectionState::Closed) {
            *hash<auto> err = poll_op.getError();
            throw "HTTP3-STATE-ERROR",
                sprintf("connection closed: %s: %s", err.err ?? "UNKNOWN", err.desc ?? "unknown");
        }

        return state == Http3ClientConnectionState::Ready;
    }

    #! Drives the connection synchronously until ready or timeout
    bool driveToReady(timeout timeout_ms = 30s) {
        date end_time = now_us() + timeout_ms;

        {
            AutoLock al(lock);
            if (isReadyLocked()) {
                return True;
            }
            if (state == Http3ClientConnectionState::Closed) {
                *hash<auto> err = poll_op.getError();
                throw "HTTP3-CONNECT-ERROR",
                    sprintf("connection closed: %s: %s", err.err ?? "UNKNOWN", err.desc ?? "unknown");
            }
            while (driving) {
                date remaining = end_time - now_us();
                if (remaining <= 0us) {
                    return False;
                }
                ready_cond.wait(lock, remaining);
                if (isReadyLocked()) {
                    return True;
                }
                if (state == Http3ClientConnectionState::Closed) {
                    *hash<auto> err = poll_op.getError();
                    throw "HTTP3-CONNECT-ERROR",
                        sprintf("connection closed: %s: %s", err.err ?? "UNKNOWN", err.desc ?? "unknown");
                }
            }
            driving = True;
        }

        on_exit {
            AutoLock al(lock);
            driving = False;
            ready_cond.broadcast();
        }

        while (True) {
            # Check both predicates under a single lock acquisition to avoid TOCTOU
            {
                AutoLock al(lock);
                if (isReadyLocked() || state == Http3ClientConnectionState::Closed) {
                    break;
                }
            }
            date remaining = end_time - now_us();
            if (remaining <= 0us) {
                return False;
            }

            *hash<SocketPollInfo> poll_info = poll_op.continuePoll();
            if (!poll_info) {
                handlePollResult();
                if (poll_op.hasError()) {
                    *hash<auto> err = poll_op.getError();
                    throw "HTTP3-CONNECT-ERROR",
                        sprintf("%s: %s", err.err ?? "UNKNOWN", err.desc ?? "unknown");
                }
                continue;
            }

            timeout poll_timeout = min(remaining, 1s);
            Socket::poll(cast<list<hash<SocketPollInfo>>>((
                poll_info,
                <SocketPollInfo>{
                    "events": SOCK_POLLIN,
                    "socket": poll_notifier,
                },
            )), poll_timeout);
            poll_notifier.acknowledge();
            handlePollResult();
        }

        if (isClosed()) {
            *hash<auto> err = poll_op.getError();
            throw "HTTP3-CONNECT-ERROR",
                sprintf("connection closed: %s: %s", err.err ?? "UNKNOWN", err.desc ?? "unknown");
        }

        return isReady();
    }

    #! Drives the connection poll operation once
    bool drivePoll(timeout poll_timeout_ms = 1s) {
        {
            AutoLock al(lock);
            if (state == Http3ClientConnectionState::Closed) {
                return False;
            }
            if (driving) {
                ready_cond.wait(lock, poll_timeout_ms);
                return state != Http3ClientConnectionState::Closed
                    && !poll_op.hasError();
            }
            driving = True;
        }

        on_exit {
            AutoLock al(lock);
            driving = False;
            ready_cond.broadcast();
        }

        *hash<SocketPollInfo> poll_info = poll_op.continuePoll();
        if (!poll_info) {
            # continuePoll returned NOTHING: either goal reached or error.
            # Check for state changes under a single lock acquisition.
            AutoLock al(lock);
            last_activity = now_us();
            if (poll_op.isClosed()) {
                state = Http3ClientConnectionState::Closed;
                ready_cond.broadcast();
            } else if (poll_op.isReady() && state == Http3ClientConnectionState::Connecting) {
                state = Http3ClientConnectionState::Ready;
                ready_cond.broadcast();
            }
            # Check for GOAWAY received from server
            if (poll_op.isGoawayReceived() && state == Http3ClientConnectionState::Ready) {
                state = Http3ClientConnectionState::Draining;
                log(LoggerLevel::DEBUG, "GOAWAY detected in drivePoll, transitioning to Draining");
                ready_cond.broadcast();
            }
            return state != Http3ClientConnectionState::Closed && !poll_op.hasError();
        }

        if (poll_op.checkAndClearCallbackDispatched()) {
            # Callback was dispatched during continuePoll — response is ready.
            # Update activity timestamp without separate handlePollResult() call.
            AutoLock al(lock);
            last_activity = now_us();
            return True;
        }

        Socket::poll(cast<list<hash<SocketPollInfo>>>((
            poll_info,
            <SocketPollInfo>{
                "events": SOCK_POLLIN,
                "socket": poll_notifier,
            },
        )), poll_timeout_ms);
        poll_notifier.acknowledge();
        handlePollResult();
        return True;
    }

    #! Closes the connection
    /** @param close_timeout maximum time to wait for the driver thread to finish
        before proceeding with abort (default 10s)

        @par Thread Safety
        Coordinates with the driving mechanism: sets state to Closed
        and waits for any active driver thread to finish its
        continuePoll() call before calling abort().  This prevents
        the data race where abort() would modify poll_op state while
        continuePoll() is still running.

        In non-controller (sync) mode, the caller must ensure no
        concurrent submitRequest() is in progress (matching the
        existing single-writer contract for that mode).
    */
    close(timeout close_timeout = 10s) {
        log(LoggerLevel::DEBUG, "closing connection");
        {
            AutoLock al(lock);
            state = Http3ClientConnectionState::Closed;
            ready_cond.broadcast();
            # Wait for any active driver to finish before aborting poll_op.
            # After this loop: driving == False AND state == Closed, so no
            # thread can start a new drive cycle (all entry points check state).
            date end_time = now_us() + close_timeout;
            while (driving) {
                date remaining = end_time - now_us();
                if (remaining <= 0us) {
                    log(LoggerLevel::ERROR, "close() timed out waiting for driver thread");
                    break;
                }
                ready_cond.wait(lock, min(remaining, 5s));
            }
        }
        # Now safe to abort — no thread is driving continuePoll()
        try {
            poll_op.abort();
        } catch () {
            # Ignore abort errors during close
        }
    }

    #! Sets the connection to draining state
    setDraining() {
        AutoLock al(lock);
        if (state == Http3ClientConnectionState::Ready) {
            log(LoggerLevel::DEBUG, "connection draining");
            state = Http3ClientConnectionState::Draining;
        }
    }

    #! Internal: logging helper
    private log(int level, string fmt, ...) {
        if (logger) {
            call_function_args(\logger.log(), (level, sprintf("Http3ClientConnection[%d]: ", connection_id) + fmt) + argv);
        }
    }
}

} # namespace Http3ClientIo
