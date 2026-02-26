# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file Http3ServerPollOperation.qc Poll operation for HTTP/3 server connections

/** Http3ServerPollOperation.qc Copyright 2026 Qore Technologies, s.r.o.

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
public namespace HttpServerAsyncIo {

#! Poll operation for HTTP/3 server connection handling over QUIC
/** This poll operation manages an HTTP/3 server connection lifecycle:

    1. Accept a QUIC connection (including TLS 1.3 handshake)
    2. Read QUIC packets and process HTTP/3 frames
    3. Provide complete HTTP/3 requests to the handler
    4. Support sending responses via startSendResponse()

    Unlike HTTP/2 which builds on a TCP connection with a separate SSL handshake,
    QUIC combines transport and encryption in a single handshake. This class wraps
    the low-level Socket QUIC poll operations to provide a higher-level interface
    for the HttpServer async I/O controller.

    @note QUIC uses UDP, so this operation works with a UDP socket bound via
    @ref Qore::Socket::bindINET() "Socket::bindINET()" with SOCK_DGRAM.
*/
public class Http3ServerPollOperation inherits AbstractPollOperation {
    public {
        #! State constants
        const STATE_INIT = "init";
        const STATE_READING = "reading";
        const STATE_WAIT_READ = "wait_read";
        const STATE_REQUEST_READY = "request_ready";
        const STATE_SENDING = "sending";
        const STATE_SENT = "sent";
        const STATE_CLOSED = "closed";

        #! Maximum consecutive empty reads before closing
        const MAX_EMPTY_READS = 100;
    }

    private {
        #! The UDP socket
        Socket sock;

        #! Current state
        string state = STATE_INIT;

        #! Current poll operation (the C++ SocketQuicServerPollOperation)
        AbstractPollOperation current_op;

        #! The HTTP/3 request when ready
        *hash<auto> request;

        #! Keep-alive flag (QUIC connections are persistent)
        bool keep_alive = True;

        #! Error info if an error occurred
        *hash<auto> error_info;

        #! Counter for consecutive empty reads
        int empty_read_count = 0;

        #! Mutex for thread safety between I/O thread (continuePoll) and handler
        #! thread (startSendResponse)
        Mutex op_lock();
    }

    #! Creates the poll operation
    /** @param sock the UDP socket bound and ready for QUIC connections
    */
    constructor(Socket sock) {
        if (!sock) {
            throw "HTTP3-ERROR", "socket cannot be null";
        }
        if (!sock.isOpen()) {
            throw "HTTP3-ERROR", "socket must be open";
        }
        self.sock = sock;
        # Start accepting QUIC connection and reading HTTP/3 request
        startReadRequest();
    }

    #! Copying is not supported (holds mutable state: Socket, Mutex, poll operations)
    private copy() {
        throw "COPY-ERROR", "Http3ServerPollOperation objects cannot be copied";
    }

    #! Returns the goal of the operation
    string getGoal() {
        return "http3_connection";
    }

    #! Returns the current state
    string getState() {
        return state;
    }

    #! Returns True when a request is ready or a send is complete
    bool goalReached() {
        AutoLock al(op_lock);
        return state == STATE_REQUEST_READY || state == STATE_SENT;
    }

    #! Returns True if the connection is closed
    bool isClosed() {
        AutoLock al(op_lock);
        return state == STATE_CLOSED;
    }

    #! Returns True if an error occurred
    bool hasError() {
        AutoLock al(op_lock);
        return exists error_info;
    }

    #! Returns error information if an error occurred
    *hash<auto> getError() {
        AutoLock al(op_lock);
        return error_info;
    }

    #! Continues the poll operation
    /** @note Thread safety: acquires op_lock to prevent concurrent modification
        by handler threads calling startSendResponse().
    */
    *hash<SocketPollInfo> continuePoll() {
        AutoLock al(op_lock);
        while (True) {
            switch (state) {
                case STATE_INIT:
                case STATE_READING: {
                    if (!current_op) {
                        state = STATE_CLOSED;
                        return NOTHING;
                    }
                    try {
                        auto poll_info = current_op.continuePoll();

                        # QUIC is a long-running multiplexed operation:
                        # continuePoll() always returns poll info (never NOTHING).
                        # Check for available output after each continuePoll() call.
                        request = current_op.getOutput();
                        if (request) {
                            empty_read_count = 0;
                            state = STATE_REQUEST_READY;
                            return NOTHING;
                        }

                        if (poll_info) {
                            return poll_info;
                        }

                        # continuePoll() returned NOTHING but no output available
                        ++empty_read_count;
                        if (empty_read_count > MAX_EMPTY_READS) {
                            error_info = {
                                "err": "HTTP3-EMPTY-READ",
                                "desc": sprintf("max consecutive empty reads (%d) exceeded",
                                    MAX_EMPTY_READS),
                            };
                            state = STATE_CLOSED;
                            return NOTHING;
                        }
                        state = STATE_WAIT_READ;
                        return <SocketPollInfo>{
                            "events": SOCK_POLLIN,
                            "socket": sock,
                        };
                    } catch (hash<ExceptionInfo> ex) {
                        error_info = {
                            "err": ex.err,
                            "desc": ex.desc,
                        };
                        state = STATE_CLOSED;
                        return NOTHING;
                    }
                }

                case STATE_WAIT_READ: {
                    # Socket has new data available; resume reading on existing op
                    state = STATE_READING;
                    continue;
                }

                case STATE_SENDING: {
                    if (!current_op) {
                        state = STATE_CLOSED;
                        return NOTHING;
                    }
                    try {
                        auto poll_info = current_op.continuePoll();

                        # Check if the send completed by examining the C++ op state.
                        # For send operations, goalReached() should work correctly
                        # since they are finite (not long-running multiplexed).
                        if (current_op.goalReached()) {
                            state = STATE_SENT;
                            return NOTHING;
                        }

                        if (poll_info) {
                            return poll_info;
                        }

                        # continuePoll returned NOTHING but goal not reached
                        state = STATE_CLOSED;
                        return NOTHING;
                    } catch (hash<ExceptionInfo> ex) {
                        error_info = {
                            "err": ex.err,
                            "desc": ex.desc,
                        };
                        state = STATE_CLOSED;
                        return NOTHING;
                    }
                }

                case STATE_REQUEST_READY:
                case STATE_SENT:
                case STATE_CLOSED:
                    return NOTHING;
            }
        }
    }

    #! Returns the HTTP/3 request when ready
    /** The returned hash contains:
        - \c session_id: the QUIC session ID (needed for sending response)
        - \c method: the HTTP method (GET, POST, etc.)
        - \c path: the request path
        - \c stream_id: the HTTP/3 stream ID (needed for sending response)
        - \c headers: hash of request headers
        - \c body: binary request body (if present)
        - \c authority: the :authority pseudo-header value (if present)
        - \c scheme: the :scheme pseudo-header value (if present)
    */
    #! Returns the completed request (if any)
    /** @note No lock needed: the poll framework serializes continuePoll() (which writes
        \c request under op_lock) and getOutput() — they never run concurrently.
        continuePoll() returns NOTHING when a request is ready, then the framework
        calls getOutput() on the same thread.
    */
    *hash<auto> getOutput() {
        return request;
    }

    #! Returns the socket
    Socket getSocket() {
        return sock;
    }

    #! Returns True if the connection should be kept alive
    bool shouldKeepAlive() {
        AutoLock al(op_lock);
        return keep_alive && state != STATE_CLOSED && !error_info;
    }

    #! Starts sending an HTTP/3 response
    /** @param status_code the HTTP status code
        @param headers response headers
        @param body response body (optional)

        @note This must be called after the request is ready (goalReached() returns True)
    */
    startSendResponse(int status_code, *hash<auto> headers, *data body) {
        AutoLock al(op_lock);
        if (state != STATE_REQUEST_READY) {
            throw "HTTP3-STATE-ERROR", sprintf("cannot send response in state: %y "
                "(HTTP/3 allows only one response per request stream)", state);
        }
        if (!request || !request.stream_id) {
            throw "HTTP3-ERROR", "no stream_id available for response";
        }
        if (!request.session_id) {
            throw "HTTP3-ERROR", "no session_id available for response";
        }

        # Use the Socket QUIC send response operation
        current_op = sock.startPollQuicSendResponse(request.session_id, request.stream_id,
            status_code, headers, body);
        state = STATE_SENDING;
    }

    #! Starts reading the next HTTP/3 request on this connection
    /** After sending a response, call this to wait for the next request
        on the persistent QUIC connection.

        @note This creates a new C++ read poll operation because after sending a
        response, the current_op is the send-response op, not the read op.  A
        new SocketQuicServerPollOperation must be created for reading.  For
        multiplexed request handling without send/read transitions, use
        continueReading() instead.
    */
    startReadNextRequest() {
        AutoLock al(op_lock);
        if (state != STATE_SENT && state != STATE_REQUEST_READY) {
            throw "HTTP3-STATE-ERROR", sprintf("cannot read next request in state: %y", state);
        }
        remove request;
        startReadRequest();
    }

    #! Continues reading without creating a new C++ read operation
    /** Called after a request has been dispatched in multiplexing mode. The same
        underlying C++ SocketQuicServerPollOperation continues reading packets.
        This avoids the overhead of creating a new operation and preserves the
        QUIC/HTTP/3 session state.
    */
    continueReading() {
        AutoLock al(op_lock);
        if (state != STATE_REQUEST_READY) {
            throw "HTTP3-STATE-ERROR", sprintf("cannot continue reading in state: %y", state);
        }
        remove request;
        empty_read_count = 0;
        state = STATE_READING;
    }

    #! Gracefully shuts down the QUIC connection
    /** @note Full HTTP/3 GOAWAY handling requires nghttp3_conn_shutdown() which needs
        C++ API additions; for now, this performs a clean close without sending GOAWAY.
    */
    shutdown() {
        # NOTE: full HTTP/3 GOAWAY handling requires nghttp3_conn_shutdown()
        # which needs C++ API additions; for now delegates to abort()
        abort();
    }

    #! Aborts the operation
    /** @note Does NOT close the UDP socket — it is shared across all QUIC sessions
        on this listener.  The socket lifetime is managed by the HttpServer listener,
        not by individual poll operations.
    */
    abort() {
        AutoLock al(op_lock);
        state = STATE_CLOSED;
        if (current_op) {
            try { current_op.abort(); } catch () {}
        }
    }

    #! Internal: starts the read request operation
    /** @note startPollQuicAccept() creates a new C++ SocketQuicServerPollOperation
        that copies existing sessions from the socket's authoritative session map.
        Despite the "Accept" name, this handles both new QUIC connections AND
        reading next requests on existing sessions — it is the correct call for
        both initial accept and subsequent request reads.
    */
    private startReadRequest() {
        current_op = sock.startPollQuicAccept();
        state = STATE_READING;
    }
}

} # namespace HttpServerAsyncIo
