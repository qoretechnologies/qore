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
    HTTP/2 and HTTP/3 client connections.

    @par Thread Safety
    This class is thread-safe. All mutable state is protected by \c lock.

    @par Lock Ordering
    When both locks are needed, \c HttpClientConnection.lock must always be
    acquired before \c HttpClientPollOperation.stream_lock.  Never acquire
    in reverse order or a deadlock will result.
*/
public class HttpClientConnection {
    private {
        #! URL info (host, port, scheme)
        hash<auto> url_info;

        #! Connection state
        HttpClientConnectionState state = HttpClientConnectionState::Connecting;

        #! Connection lock for thread safety
        Mutex lock();

        #! Condition for waiting on connection ready
        Condition ready_cond();

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
        *Logger logger;

        #! Last activity timestamp
        date last_activity;

        #! Reference to AsyncSocketIoController for wake notifications
        *object controller_ref;

        #! True when a thread is actively driving to ready
        bool driving = False;

        #! Notifier to wake blocked poll when new requests are submitted (sync mode)
        EventNotifier poll_notifier();
    }

    #! Returns the protocol identifier: "h2" or "h3"
    abstract string getProtocol();

    #! Returns the poll operation for this connection
    abstract HttpClientPollOperation getPollOperation();

    #! Returns the underlying socket
    abstract Socket getSocket();

    #! Submits a request on this connection (protocol-specific)
    abstract int submitRequestImpl(string method, string path, *hash<auto> headers,
        *data body, code callback, *hash<auto> ctx);

    #! Cancels a request (protocol-specific)
    abstract cancelRequestImpl(int stream_id);

    #! Closes the connection (protocol-specific)
    abstract closeImpl(*timeout close_timeout);

    #! Internal: called by poll operation when connection becomes ready
    private onConnectionReady() {
        AutoLock al(lock);
        if (state == HttpClientConnectionState::Connecting) {
            state = HttpClientConnectionState::Ready;
            log(LoggerLevel::DEBUG, "connection ready (notified via callback)");
            ready_cond.broadcast();
        }
    }

    #! Returns True if the connection is ready for new requests
    bool isReady() {
        AutoLock al(lock);
        return isReadyLocked();
    }

    #! Internal: check ready state when lock is already held
    private bool isReadyLocked() {
        return state == HttpClientConnectionState::Ready && getPollOperation().isReady();
    }

    #! Returns True if the connection is closed
    bool isClosed() {
        AutoLock al(lock);
        return state == HttpClientConnectionState::Closed;
    }

    #! Returns True if the connection is draining (GOAWAY received)
    bool isDraining() {
        AutoLock al(lock);
        return state == HttpClientConnectionState::Draining;
    }

    #! Returns True if the connection can accept a new stream
    /** Allows both Ready and Connecting connections to accept streams, since
        the stream handle's request() method will wait for the connection to
        become ready before submitting the request.
    */
    bool canAcceptStream() {
        AutoLock al(lock);
        return (state == HttpClientConnectionState::Ready
                || state == HttpClientConnectionState::Connecting)
            && getPollOperation().getActiveStreamCount() < max_concurrent_streams;
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
            "state": state,
            "active_streams": getPollOperation().getActiveStreamCount(),
            "max_concurrent_streams": max_concurrent_streams,
            "connection_id": connection_id,
            "protocol": getProtocol(),
            "ssl": url_info.protocol == "https",
        };
    }

    #! Submits a new request on this connection
    /** Validates state and delegates to the protocol-specific submitRequestImpl().

        @note The capacity check is performed atomically inside the poll operation's
        submitRequest() under stream_lock, so there is no TOCTOU gap between the
        check and the actual stream creation.
    */
    int submitRequest(string method, string path, *hash<auto> headers, *data body,
            code callback, *hash<auto> ctx) {
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

        log(LoggerLevel::DEBUG, "submitting request: %s %s", method, path);
        int stream_id = submitRequestImpl(method, path, headers, body, callback, ctx);

        if (ctl) {
            ctl.wake();
        } else {
            poll_notifier.notify();
        }

        return stream_id;
    }

    #! Cancels a pending request
    cancelRequest(int stream_id) {
        log(LoggerLevel::DEBUG, "cancelling request: stream_id=%d", stream_id);
        cancelRequestImpl(stream_id);
    }

    #! Handles poll result from AsyncSocketIoController
    /** Updates state under lock, then calls handlePollResultImpl() for
        protocol-specific handling (e.g. GOAWAY check) while still holding the lock.
    */
    handlePollResult() {
        AutoLock al(lock);
        last_activity = now_us();

        if (getPollOperation().isClosed()) {
            state = HttpClientConnectionState::Closed;
            ready_cond.broadcast();
        } else if (getPollOperation().isReady() && state == HttpClientConnectionState::Connecting) {
            state = HttpClientConnectionState::Ready;
            ready_cond.broadcast();
        }

        # Protocol-specific hook called under lock — no gap for concurrent state changes
        handlePollResultImpl();
    }

    #! Protocol-specific poll result hook, called under lock
    /** Default no-op; override for protocol-specific handling (e.g. GOAWAY check).
        @note Called while \c lock is held — do not attempt to re-acquire it.
    */
    private handlePollResultImpl() {
    }

    #! Waits for the connection to become ready
    bool waitForReady(timeout timeout_ms = 30s) {
        date end_time = now_us() + timeout_ms;

        AutoLock al(lock);
        while (state == HttpClientConnectionState::Connecting) {
            date remaining = end_time - now_us();
            if (remaining <= 0us) {
                return False;
            }
            ready_cond.wait(lock, remaining);

            if (getPollOperation().isClosed() || getPollOperation().hasError()) {
                state = HttpClientConnectionState::Closed;
                break;
            }
        }

        if (state == HttpClientConnectionState::Closed) {
            *hash<auto> err = getPollOperation().getError();
            throw "HTTPCLIENT-STATE-ERROR",
                sprintf("connection closed: %s: %s", err.err ?? "UNKNOWN", err.desc ?? "unknown");
        }

        return state == HttpClientConnectionState::Ready;
    }

    #! Drives the connection synchronously until ready or timeout
    bool driveToReady(timeout timeout_ms = 30s) {
        date end_time = now_us() + timeout_ms;

        {
            AutoLock al(lock);
            if (isReadyLocked()) {
                return True;
            }
            if (state == HttpClientConnectionState::Closed) {
                *hash<auto> err = getPollOperation().getError();
                throw "HTTPCLIENT-CONNECT-ERROR",
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
                if (state == HttpClientConnectionState::Closed) {
                    *hash<auto> err = getPollOperation().getError();
                    throw "HTTPCLIENT-CONNECT-ERROR",
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
            {
                AutoLock al(lock);
                if (isReadyLocked() || state == HttpClientConnectionState::Closed) {
                    break;
                }
            }
            date remaining = end_time - now_us();
            if (remaining <= 0us) {
                return False;
            }

            *hash<SocketPollInfo> poll_info = getPollOperation().continuePoll();
            if (!poll_info) {
                handlePollResult();
                if (getPollOperation().hasError()) {
                    *hash<auto> err = getPollOperation().getError();
                    throw "HTTPCLIENT-CONNECT-ERROR",
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
            *hash<auto> err = getPollOperation().getError();
            throw "HTTPCLIENT-CONNECT-ERROR",
                sprintf("connection closed: %s: %s", err.err ?? "UNKNOWN", err.desc ?? "unknown");
        }

        return isReady();
    }

    #! Drives the connection poll operation once
    bool drivePoll(timeout poll_timeout_ms = 1s) {
        {
            AutoLock al(lock);
            if (state == HttpClientConnectionState::Closed) {
                return False;
            }
            if (driving) {
                ready_cond.wait(lock, poll_timeout_ms);
                return state != HttpClientConnectionState::Closed
                    && !getPollOperation().hasError();
            }
            driving = True;
        }

        on_exit {
            AutoLock al(lock);
            driving = False;
            ready_cond.broadcast();
        }

        *hash<SocketPollInfo> poll_info = getPollOperation().continuePoll();
        if (!poll_info) {
            handlePollResult();
            return !isClosed() && !getPollOperation().hasError();
        }

        if (getPollOperation().checkAndClearCallbackDispatched()) {
            handlePollResult();
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
        return !isClosed() && !getPollOperation().hasError();
    }

    #! Sets the connection to draining state (GOAWAY received)
    setDraining() {
        AutoLock al(lock);
        if (state == HttpClientConnectionState::Ready) {
            log(LoggerLevel::DEBUG, "connection draining");
            state = HttpClientConnectionState::Draining;
        }
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
