# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file Http2ClientConnection.qc HTTP/2 client connection with multiplexed streams

/** Http2ClientConnection.qc Copyright 2026 Qore Technologies, s.r.o.

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
public namespace Http2ClientIo {

#! HTTP/2 client connection managing multiple concurrent streams
/** This class wraps a Socket + Http2ClientPollOperation to provide a
    higher-level interface for HTTP/2 multiplexed client connections.

    A single connection can handle multiple concurrent streams, with
    responses dispatched to callbacks by stream ID.

    @par Thread Safety
    This class is thread-safe. Stream submission uses the nghttp2 session's
    internal mutex for synchronization.

    @par Usage with AsyncSocketIoController
    @code{.py}
# Create connection
Http2ClientConnection conn({"host": "example.com", "port": 443, "scheme": "https"});

# Register with controller
AsyncSocketIoController controller();
controller.registerPollOperation(conn.getPollOperation(), \conn.handlePollResult());

# Submit requests
int stream1 = conn.submitRequest("GET", "/api/v1/users", NOTHING, NOTHING,
    sub (hash<auto> response, *hash<auto> error) {
        printf("Users: %y\n", response.body);
    });

int stream2 = conn.submitRequest("GET", "/api/v1/orders", NOTHING, NOTHING,
    sub (hash<auto> response, *hash<auto> error) {
        printf("Orders: %y\n", response.body);
    });

# Run event loop
controller.run();
    @endcode

*/
public class Http2ClientConnection {
    private {
        #! URL info (host, port, scheme)
        hash<auto> url_info;

        #! Poll operation for this connection
        Http2ClientPollOperation poll_op;

        #! Connection state
        Http2ClientConnectionState state = Http2ClientConnectionState::Connecting;

        #! Connection lock for thread safety
        Mutex lock();

        #! Condition for waiting on connection ready
        Condition ready_cond();

        #! Maximum concurrent streams (from server SETTINGS)
        int max_concurrent_streams = DefaultMaxStreamsPerConnection;

        #! Connection ID
        int connection_id;

        #! Static counter for connection IDs
        static int next_connection_id = 0;

        #! Static lock for connection ID counter (protects across instances)
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

        #! Condition for waiting on responses in controller mode
        Condition response_cond();

        #! Map of stream_id (as string) -> response hash for sync waiting in controller mode
        hash<string, hash<auto>> pending_responses;
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

        {
            AutoLock al(connection_id_lock);
            connection_id = ++next_connection_id;
        }

        log(LoggerLevel::DEBUG, "creating connection to %s:%d (ssl=%y max_streams=%d)",
            url_info.host, url_info.port, url_info.protocol == "https", max_streams);

        # Create poll operation for this connection
        poll_op = new Http2ClientPollOperation(url_info, connect_timeout, logger,
            ssl_verify_mode, accept_all_certs);
    }

    #! Returns the poll operation for this connection
    /** Use this with AsyncSocketIoController to drive the connection.
    */
    Http2ClientPollOperation getPollOperation() {
        return poll_op;
    }

    #! Returns the underlying socket
    /** Use this with AsyncSocketIoController when submitting poll operations.
    */
    Socket getSocket() {
        return poll_op.getSocket();
    }

    #! Sets the controller reference for wake notifications
    /** When set, submitRequest() will call controller.wake() after queuing a
        request to ensure the I/O thread processes it promptly.

        @param controller the AsyncSocketIoController managing this connection
    */
    setControllerRef(object controller) {
        controller_ref = controller;
    }

    #! Returns True if the connection is in controller mode
    /** In controller mode, an AsyncSocketIoController drives all I/O operations
        and the synchronous request() method must wait on conditions rather than
        driving poll directly.
    */
    bool isControllerMode() {
        return exists controller_ref;
    }

    #! Notifies waiting threads that a response is ready
    /** Called by the controller callback when a response arrives.
        Stores the response in pending_responses and broadcasts on response_cond.

        @param stream_id the stream ID of the response
        @param response the response hash
    */
    notifyResponse(int stream_id, hash<auto> response) {
        AutoLock al(lock);
        pending_responses{string(stream_id)} = response;
        response_cond.broadcast();
    }

    #! Waits for a response in controller mode
    /** Used by synchronous callers when the controller drives I/O.

        @param stream_id the stream ID to wait for
        @param timeout_ms maximum time to wait

        @return the response hash, or NOTHING on timeout
    */
    *hash<auto> waitForResponse(int stream_id, timeout timeout_ms) {
        date end_time = now_us() + timeout_ms;
        string sid = string(stream_id);

        AutoLock al(lock);
        while (!pending_responses.hasKey(sid)) {
            date remaining = end_time - now_us();
            if (remaining <= 0us) {
                return NOTHING;
            }
            response_cond.wait(lock, remaining);
        }
        return remove pending_responses{sid};
    }

    #! Returns connection info
    hash<Http2ClientConnectionInfo> getConnectionInfo() {
        AutoLock al(lock);
        return <Http2ClientConnectionInfo>{
            "host": url_info.host,
            "port": url_info.port,
            "ssl": url_info.protocol == "https",
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

    #! Internal: check ready state when lock is already held
    private bool isReadyLocked() {
        return state == Http2ClientConnectionState::Ready && poll_op.isReady();
    }

    #! Returns True if the connection is closed
    bool isClosed() {
        AutoLock al(lock);
        return state == Http2ClientConnectionState::Closed;
    }

    #! Returns True if the connection is draining (GOAWAY received)
    bool isDraining() {
        AutoLock al(lock);
        return state == Http2ClientConnectionState::Draining;
    }

    #! Returns True if the connection can accept a new stream
    bool canAcceptStream() {
        AutoLock al(lock);
        return state == Http2ClientConnectionState::Ready
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
    /** @param method HTTP method (GET, POST, etc.)
        @param path request path
        @param headers request headers
        @param body request body (optional)
        @param callback callback to invoke when response is ready: sub(hash<auto> response, *hash<auto> error)
        @param context optional context to pass to callback

        @return the stream ID assigned to this request

        @throw HTTP2-STATE-ERROR if the connection is not ready for requests
        @throw HTTP2-CAPACITY-ERROR if the connection cannot accept more streams
    */
    int submitRequest(string method, string path, *hash<auto> headers, *data body,
            code callback, *hash<auto> ctx) {
        {
            AutoLock al(lock);
            if (state != Http2ClientConnectionState::Ready) {
                throw "HTTP2-STATE-ERROR",
                    sprintf("connection not ready: state=%y", state);
            }
            if (poll_op.getActiveStreamCount() >= max_concurrent_streams) {
                throw "HTTP2-CAPACITY-ERROR",
                    sprintf("connection at capacity: %d/%d streams",
                        poll_op.getActiveStreamCount(), max_concurrent_streams);
            }
            last_activity = now_us();
        }

        log(LoggerLevel::DEBUG, "submitting request: %s %s", method, path);
        int stream_id = poll_op.submitRequest(method, path, headers, body, callback, ctx);

        # Wake controller if in async mode to ensure the I/O thread processes the request
        if (controller_ref) {
            controller_ref.wake();
        }

        return stream_id;
    }

    #! Cancels a pending request
    /** @param stream_id the stream ID to cancel
    */
    cancelRequest(int stream_id) {
        log(LoggerLevel::DEBUG, "cancelling request: stream_id=%d", stream_id);
        poll_op.cancelRequest(stream_id);
    }

    #! Sends data on a stream
    /** @param stream_id the stream ID to send data on
        @param data the data to send (can be string or binary)
        @param end_stream if True, signals end of stream

        @throw HTTP2-STREAM-ERROR if the stream ID is not found
        @throw HTTP2-STATE-ERROR if not in ready state
    */
    sendStreamData(int stream_id, data data, bool end_stream = False) {
        {
            AutoLock al(lock);
            if (state != Http2ClientConnectionState::Ready) {
                throw "HTTP2-STATE-ERROR",
                    sprintf("cannot send data in state: %y", state);
            }
            last_activity = now_us();
        }

        log(LoggerLevel::DEBUG, "sending data on stream %d", stream_id);
        poll_op.sendStreamData(stream_id, data, end_stream);
    }

    #! Handles poll result from AsyncSocketIoController
    /** Call this when the controller indicates poll activity on this connection.
    */
    handlePollResult() {
        AutoLock al(lock);
        last_activity = now_us();

        # Update state based on poll operation state
        if (poll_op.isClosed()) {
            state = Http2ClientConnectionState::Closed;
        } else if (poll_op.isReady() && state == Http2ClientConnectionState::Connecting) {
            state = Http2ClientConnectionState::Ready;
            ready_cond.broadcast();
        }
    }

    #! Waits for the connection to become ready
    /** @param timeout_ms maximum time to wait in milliseconds

        @return True if the connection is ready, False on timeout

        @throw HTTP2-STATE-ERROR if the connection is closed or in error state

        @note In controller mode, this method periodically checks if the poll operation
        is ready, since the controller drives I/O and may not call handlePollResult()
        until the operation completes.
    */
    bool waitForReady(timeout timeout_ms = 30s) {
        date end_time = now_us() + timeout_ms;

        AutoLock al(lock);
        while (state == Http2ClientConnectionState::Connecting) {
            # Check if poll operation became ready (HTTP/2 handshake complete)
            # This is needed in controller mode where handlePollResult() is only
            # called when the poll operation completes, not during establishment
            if (poll_op.isReady()) {
                state = Http2ClientConnectionState::Ready;
                break;
            }
            if (poll_op.isClosed() || poll_op.hasError()) {
                state = Http2ClientConnectionState::Closed;
                break;
            }
            date remaining = end_time - now_us();
            if (remaining <= 0us) {
                return False;
            }
            # Use short wait intervals to check poll_op state more frequently
            ready_cond.wait(lock, min(remaining, 100ms));
        }

        if (state == Http2ClientConnectionState::Closed) {
            *hash<auto> err = poll_op.getError();
            throw "HTTP2-STATE-ERROR",
                sprintf("connection closed: %s: %s", err.err ?? "UNKNOWN", err.desc ?? "unknown");
        }

        return state == Http2ClientConnectionState::Ready;
    }

    #! Drives the connection synchronously until ready or timeout
    /** This method polls the connection in a loop until the HTTP/2 handshake
        completes and the connection is ready for requests.

        If another thread is already driving, this thread waits for it to finish.

        @param timeout_ms maximum time to wait

        @return True if the connection is ready, False on timeout

        @throw HTTP2-CONNECT-ERROR if the connection fails
    */
    bool driveToReady(timeout timeout_ms = 30s) {
        date end_time = now_us() + timeout_ms;

        # Check if already ready or if another thread is driving
        {
            AutoLock al(lock);
            if (isReadyLocked()) {
                return True;
            }
            if (state == Http2ClientConnectionState::Closed) {
                *hash<auto> err = poll_op.getError();
                throw "HTTP2-CONNECT-ERROR",
                    sprintf("connection closed: %s: %s", err.err ?? "UNKNOWN", err.desc ?? "unknown");
            }
            # If another thread is driving, wait for it
            while (driving) {
                date remaining = end_time - now_us();
                if (remaining <= 0us) {
                    return False;
                }
                ready_cond.wait(lock, remaining);
                # After waking, check if ready or closed
                if (isReadyLocked()) {
                    return True;
                }
                if (state == Http2ClientConnectionState::Closed) {
                    *hash<auto> err = poll_op.getError();
                    throw "HTTP2-CONNECT-ERROR",
                        sprintf("connection closed: %s: %s", err.err ?? "UNKNOWN", err.desc ?? "unknown");
                }
            }
            # We are the driving thread
            driving = True;
        }

        # Cleanup on any exit path
        on_exit {
            AutoLock al(lock);
            driving = False;
            ready_cond.broadcast();
        }

        while (!isReady() && !isClosed()) {
            date remaining = end_time - now_us();
            if (remaining <= 0us) {
                return False;
            }

            *hash<SocketPollInfo> poll_info = poll_op.continuePoll();
            if (!poll_info) {
                # Poll operation complete or no more work
                handlePollResult();
                if (poll_op.hasError()) {
                    *hash<auto> err = poll_op.getError();
                    throw "HTTP2-CONNECT-ERROR",
                        sprintf("%s: %s", err.err ?? "UNKNOWN", err.desc ?? "unknown");
                }
                continue;
            }

            # Wait for socket to be ready
            int poll_timeout = min(int(remaining), 1000);
            Socket::poll((poll_info,), poll_timeout);
            handlePollResult();
        }

        if (isClosed()) {
            *hash<auto> err = poll_op.getError();
            throw "HTTP2-CONNECT-ERROR",
                sprintf("connection closed: %s: %s", err.err ?? "UNKNOWN", err.desc ?? "unknown");
        }

        return isReady();
    }

    #! Drives the connection poll operation once
    /** @param poll_timeout_ms timeout for this poll iteration

        @return True if there is more work to do, False if poll is complete
    */
    bool drivePoll(timeout poll_timeout_ms = 1s) {
        *hash<SocketPollInfo> poll_info = poll_op.continuePoll();
        if (!poll_info) {
            handlePollResult();
            return !isClosed() && !poll_op.hasError();
        }

        Socket::poll((poll_info,), int(poll_timeout_ms));
        handlePollResult();
        return True;
    }

    #! Closes the connection
    close() {
        log(LoggerLevel::DEBUG, "closing connection");
        {
            AutoLock al(lock);
            state = Http2ClientConnectionState::Closed;
        }
        poll_op.abort();
    }

    #! Sets the connection to draining state (GOAWAY received)
    /** No new streams will be accepted, but existing streams continue.
    */
    setDraining() {
        AutoLock al(lock);
        if (state == Http2ClientConnectionState::Ready) {
            log(LoggerLevel::DEBUG, "connection draining");
            state = Http2ClientConnectionState::Draining;
        }
    }

    #! Internal: logging helper
    private log(int level, string fmt) {
        if (logger) {
            call_function_args(\logger.log(), (level, sprintf("Http2ClientConnection[%d]: ", connection_id) + fmt) + argv);
        }
    }
}

} # namespace Http2ClientIo
