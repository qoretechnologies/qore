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
    }

    #! Creates a new HTTP/2 client connection
    /** @param url_info parsed URL info with host, port, scheme
        @param connect_timeout timeout for connection establishment
        @param logger optional logger for debug output
    */
    constructor(hash<auto> url_info, timeout connect_timeout = DefaultConnectTimeout,
            *Logger logger) {
        self.url_info = url_info;
        self.connect_timeout = connect_timeout;
        self.logger = logger;
        self.last_activity = now_us();

        {
            AutoLock al(connection_id_lock);
            connection_id = ++next_connection_id;
        }

        log(LoggerLevel::DEBUG, "creating connection to %s:%d (ssl=%y)",
            url_info.host, url_info.port, url_info.protocol == "https");

        # Create poll operation for this connection
        poll_op = new Http2ClientPollOperation(url_info, connect_timeout, logger);
    }

    #! Returns the poll operation for this connection
    /** Use this with AsyncSocketIoController to drive the connection.
    */
    Http2ClientPollOperation getPollOperation() {
        return poll_op;
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
        return poll_op.submitRequest(method, path, headers, body, callback, ctx);
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
    */
    bool waitForReady(timeout timeout_ms = 30s) {
        date end_time = now_us() + timeout_ms;

        AutoLock al(lock);
        while (state == Http2ClientConnectionState::Connecting) {
            date remaining = end_time - now_us();
            if (remaining <= 0us) {
                return False;
            }
            ready_cond.wait(lock, remaining);
        }

        if (state == Http2ClientConnectionState::Closed) {
            *hash<auto> err = poll_op.getError();
            throw "HTTP2-STATE-ERROR",
                sprintf("connection closed: %s: %s", err.err ?? "UNKNOWN", err.desc ?? "unknown");
        }

        return state == Http2ClientConnectionState::Ready;
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
            logger.log(level, sprintf("Http2ClientConnection[%d]: ", connection_id) + fmt, argv);
        }
    }
}

} # namespace Http2ClientIo
