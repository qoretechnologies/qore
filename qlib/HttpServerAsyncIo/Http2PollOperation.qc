# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file Http2PollOperation.qc Poll operation for HTTP/2 server connections

/** Http2PollOperation.qc Copyright 2025 - 2026 Qore Technologies, s.r.o.

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

#! Poll operation for HTTP/2 server connection handling
/** This poll operation manages an HTTP/2 connection lifecycle:

    1. Exchange connection preface (SETTINGS frames)
    2. Read HTTP/2 frames until a complete request is available
    3. Provide the request data to the handler
    4. Support sending responses via startSendResponse()

    This class wraps the low-level Socket HTTP/2 poll operations to provide
    a higher-level interface for the HttpServer async I/O controller.

*/
public class Http2PollOperation inherits HttpConnectionPollOperationBase {
    public {
        #! State constants
        const STATE_INIT = "init";
        const STATE_READING = "reading";
        const STATE_WAIT_READ = "wait_read";
        const STATE_REQUEST_READY = "request_ready";
        const STATE_SENDING = "sending";
        const STATE_SENT = "sent";
        const STATE_CLOSED = "closed";

        #! Maximum consecutive empty reads (goalReached with no request) before closing
        const MAX_EMPTY_READS = 100;
    }

    private {
        #! The client socket
        Socket sock;

        #! Current state
        string state = STATE_INIT;

        #! Current poll operation (read or send)
        AbstractPollOperation current_op;

        #! The HTTP/2 request when ready
        *hash<auto> request;

        #! Keep-alive flag (HTTP/2 connections are persistent by default)
        bool keep_alive = True;

        #! Error info if an error occurred
        *hash<auto> error_info;

        #! Counter for consecutive empty reads to prevent infinite poll loops
        int empty_read_count = 0;

        #! Whether to use headers-only mode for request reading
        bool headers_only = False;

        #! Set to True by continuePoll() when a request is stored; cleared by getOutput().
        #! Prevents handler threads from calling continueReading() between continuePoll()
        #! returning STATE_REQUEST_READY and the dispatch callback calling getOutput().
        bool request_pending = False;

        #! Mutex for thread safety between I/O thread (continuePoll) and handler
        #! thread (continueReading, startSendResponse, etc.)
        Mutex op_lock();

        #! Queues for delivering data to CONNECT stream handlers (stream_id string → Queue)
        /** Registered by the I/O controller when dispatching extended CONNECT handlers.
            Data is drained from the per-stream buffer to these Queues after each poll cycle.
            NOTHING is pushed as a sentinel when the stream closes.
        */
        hash<string, Queue> stream_queues;

        #! Optional notification callbacks per stream, called when data is drained to the Queue
        /** Keyed by stream_id string. Called on the I/O thread after data is pushed
            to the corresponding Queue, enabling event-driven wakeup of handler threads.

            @since HttpServerAsyncIo 1.1
        */
        hash<string, code<nothing()>> stream_notify_callbacks;
    }

    #! Creates the poll operation
    /** @param sock the client socket with an established HTTP/2 connection
        @param headers_only if True, dispatch requests as soon as headers arrive
        (before END_STREAM), enabling bidirectional streaming; the handler must read
        the body via Socket::readHttp2StreamDataBlock()
    */
    constructor(Socket sock, bool headers_only = False) {
        @assert(sock, "socket cannot be null");
        @assert(sock.isOpen(), "socket must be open");
        self.sock = sock;
        self.headers_only = headers_only;
        # Start reading HTTP/2 request
        startReadRequest(headers_only);
    }

    #! Returns the goal of the operation
    string getGoal() {
        return "http2_connection";
    }

    #! Returns the current state
    string getState() {
        return state;
    }

    #! Returns True when a request is ready to be processed or a send is complete
    /** The connection state (Http2 vs Http2Sending) distinguishes which completion occurred.
    */
    bool goalReached() {
        return state == STATE_REQUEST_READY || state == STATE_SENT;
    }

    #! Returns True if the connection is closed
    bool isClosed() {
        return state == STATE_CLOSED;
    }

    #! Returns True if an error occurred
    bool hasError() {
        return exists error_info;
    }

    #! Returns error information if an error occurred
    *hash<auto> getError() {
        return error_info;
    }

    #! Continues the poll operation
    /** @return poll info for the next poll() call, or NOTHING if complete
    */
    *hash<SocketPollInfo> continuePoll() {
        AutoLock al(op_lock);
        while (True) {
            @debug {
                if (getenv("QORE_H2OP_TRACE")) {
                    printf("Http2PollOperation::continuePoll() state=%s sock=%s\n",
                        state, sock.uniqueHash()[0..8]);
                }
            }

            switch (state) {
                case STATE_INIT:
                case STATE_READING: {
                    if (!current_op) {
                        @debug {
                            if (getenv("QORE_H2OP_TRACE")) {
                                printf("Http2PollOperation: no current_op, closing\n");
                            }
                        }
                        state = STATE_CLOSED;
                        return NOTHING;
                    }
                    try {
                        auto poll_info = current_op.continuePoll();
                        # Drain data for registered CONNECT streams after each poll cycle
                        if (stream_queues) {
                            drainStreamQueues();
                        }
                        @debug {
                            if (getenv("QORE_H2OP_TRACE")) {
                                printf("Http2PollOperation: current_op.continuePoll() returned poll_info=%y goal=%y\n",
                                    poll_info != NOTHING, current_op.goalReached());
                            }
                        }
                        if (poll_info) {
                            return poll_info;
                        }
                        # Read complete
                        if (current_op.goalReached()) {
                            request = current_op.getOutput();
                            if (request) {
                                empty_read_count = 0;
                                request_pending = True;
                                state = STATE_REQUEST_READY;

                                return NOTHING;
                            }
                            # goalReached but no request: HTTP/2 control frames were processed
                            # (WINDOW_UPDATE, PING, SETTINGS ACK, etc.) without a new request.
                            # Wait for new socket data before retrying.
                            ++empty_read_count;
                            if (empty_read_count > MAX_EMPTY_READS) {
                                error_info = {
                                    "err": "HTTP2-EMPTY-READ",
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
                        } else {
                            # Operation failed
                            state = STATE_CLOSED;
                            return NOTHING;
                        }
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
                    # Socket has new data available; start a fresh read operation

                    startReadRequest();
                    # Use continue to restart the while loop (avoids potential
                    # fall-through to STATE_SENDING in Qore's switch statement)
                    continue;
                }

                case STATE_SENDING: {
                    if (!current_op) {
                        state = STATE_CLOSED;
                        return NOTHING;
                    }
                    try {
                        auto poll_info = current_op.continuePoll();
                        if (poll_info) {
                            return poll_info;
                        }
                        # Send complete
                        if (current_op.goalReached()) {
                            state = STATE_SENT;
                            return NOTHING;
                        } else {
                            # Operation failed
                            state = STATE_CLOSED;
                            return NOTHING;
                        }
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
        return NOTHING;
    }

    #! Returns the HTTP/2 request when ready
    /** The returned hash contains:
        - \c method: the HTTP method (GET, POST, etc.)
        - \c path: the request path
        - \c stream_id: the HTTP/2 stream ID (needed for sending response)
        - \c headers: hash of request headers
        - \c body: binary request body (if present)
        - \c authority: the :authority pseudo-header value (if present)
        - \c scheme: the :scheme pseudo-header value (if present)
    */
    #! Returns the completed request (if any)
    /** @note Acquires op_lock to synchronize with continueReading() called from
        handler threads.  Clears request_pending so that concurrent
        continueReading() calls correctly throw instead of clearing a request
        that is being dispatched.

        Unlike Http3ServerPollOperation::getOutput(), this does NOT atomically
        transition to STATE_READING because HTTP/2 body_streaming defers the
        resume: the h2op is NOT submitted during dispatch — the handler's
        resume_body_streaming callback submits it later via continueReading().
    */
    auto getOutput() {
        AutoLock al(op_lock);
        # Clear the pending flag — getOutput() is the dispatch callback's
        # entry point, so the request is now being processed (or was never set).
        request_pending = False;
        return request;
    }

    #! Returns the socket
    Socket getSocket() {
        return sock;
    }

    #! Returns True if the connection should be kept alive
    bool shouldKeepAlive() {
        return keep_alive && state != STATE_CLOSED && !error_info;
    }

    #! Pushes a resource to the client using HTTP/2 Server Push (PUSH_PROMISE)
    /** @param path the path of the resource to push
        @param headers response headers for the pushed resource

        @return the promised stream ID on success, -1 if push failed or not supported

        @note This must be called after the request is ready (goalReached() returns True)
        and before or during response sending. The pushed resource is sent as a separate
        stream, allowing the client to receive it before or alongside the main response.

        @par Example:
        @code{.py}
# Push CSS and JS files when serving an HTML page
int css_stream = h2op.pushResource("/style.css", {"content-type": "text/css"}, css_data);
int js_stream = h2op.pushResource("/app.js", {"content-type": "application/javascript"}, js_data);

# Send the main HTML response
h2op.startSendResponse(200, {"content-type": "text/html"}, html_body);
        @endcode

    */
    int pushResource(string path, *hash<auto> headers) {
        AutoLock al(op_lock);
        if (state != STATE_REQUEST_READY && state != STATE_SENDING) {
            throw "HTTP2-STATE-ERROR", sprintf("cannot push resource in state: %y", state);
        }
        if (!request || !request.stream_id) {
            throw "HTTP2-ERROR", "no stream_id available for push promise";
        }

        # Submit PUSH_PROMISE - returns the promised stream ID
        # The caller should then use startPollSendHttp2Response() to send the pushed content
        return sock.submitHttp2PushPromise(request.stream_id, path, headers ?? {});
    }

    #! Creates a poll operation to send a pushed response
    /** @param promised_stream_id the stream ID returned by pushResource()
        @param status_code the HTTP status code (typically 200)
        @param headers response headers for the pushed resource
        @param body the body of the pushed resource

        @return a poll operation for sending the pushed response

        @par Example:
        @code{.py}
# Push a CSS file
int css_stream = h2op.pushResource("/style.css", {"content-type": "text/css"});
if (css_stream > 0) {
    AbstractPollOperation push_op = h2op.startSendPushedResponse(css_stream, 200,
        {"content-type": "text/css"}, css_data);
    # Drive push_op alongside main response...
}
        @endcode

    */
    AbstractPollOperation startSendPushedResponse(int promised_stream_id, int status_code,
            *hash<auto> headers, *data body) {
        return sock.startPollSendHttp2Response(promised_stream_id, status_code, headers, body);
    }

    #! Returns a callback for pushing resources (for use in handler context)
    /** @return a closure that can be called to push resources

        The callback takes (string path, *hash&lt;auto&gt; headers) and returns the promised stream ID.
        Use startSendPushedResponse() to send the actual content on the returned stream ID.

        @par Example:
        @code{.py}
# In HttpServer handler context
code push = h2op.getPushResourceCallback();
if (push) {
    int stream_id = push("/style.css", {"content-type": "text/css"});
    if (stream_id > 0) {
        # Send the pushed content
        h2op.startSendPushedResponse(stream_id, 200, {"content-type": "text/css"}, css_data);
    }
}
        @endcode

    */
    *code getPushResourceCallback() {
        AutoLock al(op_lock);
        if (state != STATE_REQUEST_READY && state != STATE_SENDING) {
            return NOTHING;
        }
        return int sub (string path, *hash<auto> headers) {
            return pushResource(path, headers);
        };
    }

    #! Starts sending an HTTP/2 response
    /** @param status_code the HTTP status code
        @param headers response headers
        @param body response body (optional)

        @note This must be called after the request is ready (goalReached() returns True)
    */
    startSendResponse(int status_code, *hash<auto> headers, *data body) {
        AutoLock al(op_lock);
        if (state != STATE_REQUEST_READY && state != STATE_SENT) {
            throw "HTTP2-STATE-ERROR", sprintf("cannot send response in state: %y", state);
        }
        if (!request || !request.stream_id) {
            throw "HTTP2-ERROR", "no stream_id available for response";
        }

        # Use the Socket HTTP/2 send response operation
        current_op = sock.startPollSendHttp2Response(request.stream_id, status_code, headers, body);
        state = STATE_SENDING;
    }

    #! Starts sending an HTTP/2 streaming response from an InputStream
    /** @param status_code the HTTP status code
        @param headers response headers
        @param body the InputStream to read the response body from
        @param chunk_size the size of chunks to read from the stream (default: 16384)

        @note This must be called after the request is ready (goalReached() returns True).
        The InputStream body is read incrementally and sent as HTTP/2 DATA frames without
        buffering the entire body in memory.

    */
    startSendResponse(int status_code, *hash<auto> headers, InputStream body, *int chunk_size) {
        AutoLock al(op_lock);
        if (state != STATE_REQUEST_READY && state != STATE_SENT) {
            throw "HTTP2-STATE-ERROR", sprintf("cannot send response in state: %y", state);
        }
        if (!request || !request.stream_id) {
            throw "HTTP2-ERROR", "no stream_id available for response";
        }

        current_op = sock.startPollSendHttp2StreamingResponse(
            request.stream_id, status_code, headers, body, chunk_size);
        state = STATE_SENDING;
    }

    #! Starts reading the next HTTP/2 request on this connection
    /** After sending a response or when goalReached indicated a request was ready but
        getOutput() returned NOTHING (e.g., after processing HTTP/2 control frames),
        call this to wait for the next request on the persistent HTTP/2 connection.
    */
    startReadNextRequest() {
        AutoLock al(op_lock);
        if (state != STATE_SENT && state != STATE_REQUEST_READY) {
            throw "HTTP2-STATE-ERROR", sprintf("cannot read next request in state: %y", state);
        }
        remove request;
        startReadRequest(headers_only);
    }

    #! Continues reading without creating a new C++ read operation
    /** Called after a request has been dispatched in multiplexing mode.  The same
        underlying C++ SocketHttp2ServerPollOperation continues reading frames.
        This avoids the overhead of creating a new operation and preserves the
        nghttp2 session state (including any already-received frames).

        @note Thread safety: rejects the call if state is STATE_REQUEST_READY but
        the request hasn't been consumed via getOutput() yet.  This prevents a
        handler thread (calling resumeMultiplexedReading after body streaming)
        from racing with the I/O controller dispatch.
    */
    continueReading() {
        AutoLock al(op_lock);
        if (state == STATE_REQUEST_READY && request_pending) {
            # A request was found by continuePoll() but not yet consumed by
            # getOutput().  A handler thread (via resumeMultiplexedReading) is
            # racing with the I/O controller's dispatch callback.
            throw "HTTP2-STATE-ERROR", "cannot continue reading: request pending dispatch";
        }
        if (state != STATE_REQUEST_READY) {
            throw "HTTP2-STATE-ERROR", sprintf("cannot continue reading in state: %y", state);
        }
        remove request;
        request_pending = False;
        empty_read_count = 0;
        state = STATE_READING;
    }

    #! Aborts the operation
    abort() {
        AutoLock al(op_lock);
        if (current_op) {
            current_op.abort();
        }
        state = STATE_CLOSED;
        # Push NOTHING sentinels to all registered stream queues so handler threads
        # waiting on data wake up and see the stream is closed
        foreach string sid in (keys stream_queues) {
            try {
                stream_queues{sid}.push(NOTHING);
            } catch () {
                # Ignore errors pushing to the queue
            }
        }
        # Call all stream notify callbacks so handler threads are woken
        foreach string sid in (keys stream_notify_callbacks) {
            try {
                stream_notify_callbacks{sid}();
            } catch () {
                # Ignore errors in notify callbacks
            }
        }
        # Clear registrations
        remove stream_queues;
        remove stream_notify_callbacks;
        try {
            sock.close();
        } catch () {}
    }

    #! Registers a Queue for delivering data to a CONNECT stream handler
    /** Called by the I/O controller when dispatching an extended CONNECT handler.
        The Queue must be unlimited (created with no max size) to ensure non-blocking pushes.

        @param stream_id the HTTP/2 stream ID
        @param queue the unlimited Queue for data delivery
        @param notify optional callback called on the I/O thread after data is pushed to the
        Queue; use this for event-driven wakeup of handler threads (e.g., signaling a
        Condition variable)

        @note Must be called on the I/O controller thread before the handler starts reading.

        @since HttpServerAsyncIo 1.1 added @a notify parameter
    */
    registerStreamQueue(int stream_id, Queue queue, *code<nothing()> notify) {
        AutoLock al(op_lock);
        stream_queues{stream_id} = queue;
        if (notify) {
            stream_notify_callbacks{stream_id} = notify;
        }
    }

    #! Internal: starts the read request operation
    private startReadRequest(*bool headers_only) {
        current_op = sock.startPollReadHttp2Request(headers_only ? {"headers_only": True} : NOTHING);
        state = STATE_READING;
    }

    #! Internal: drains buffered data for registered CONNECT streams into their Queues
    /** Called after each continuePoll() cycle on the I/O controller thread.
        Reads all available data from each registered stream's per-stream buffer
        and pushes it to the corresponding Queue. When a stream is closed
        (RST_STREAM, END_STREAM, or GOAWAY), a NOTHING sentinel is pushed
        and the Queue registration is removed.
    */
    private drainStreamQueues() {
        list<string> completed;
        foreach string sid in (keys stream_queues) {
            int stream_id = int(sid);
            Queue queue = stream_queues{sid};
            # Drain all available data from the per-stream buffer
            int drain_count = 0;
            int drain_bytes = 0;
            while (True) {
                *binary data = sock.readHttp2StreamData(stream_id);
                if (!data) {
                    break;
                }
                drain_bytes += data.size();
                ++drain_count;
                queue.push(data);
            }
            @debug {
                if (drain_count && getenv("QORE_BIDI_TRACE")) {
                    printf("drainStreamQueues: stream=%d drained %d chunks (%d bytes) qsize=%d\n",
                        stream_id, drain_count, drain_bytes, queue.size());
                }
            }
            # Notify handler thread that data is available
            if (drain_count && stream_notify_callbacks{sid}) {
                stream_notify_callbacks{sid}();
            }
            # Check if the remote peer has sent END_STREAM (client done sending)
            # or if the stream is fully closed (RST_STREAM, GOAWAY)
            if (sock.isHttp2StreamRemoteClosed(stream_id)
                    || sock.isHttp2StreamClosed(stream_id)) {
                @debug {
                    if (getenv("QORE_BIDI_TRACE")) {
                        printf("drainStreamQueues: stream=%d CLOSED, pushing sentinel\n",
                            stream_id);
                    }
                }
                queue.push(NOTHING);
                # Notify on close too so the handler wakes up
                if (stream_notify_callbacks{sid}) {
                    stream_notify_callbacks{sid}();
                }
                completed += sid;
            }
        }
        # Remove completed streams outside the iteration loop
        map (remove stream_queues{$1}, remove stream_notify_callbacks{$1}), completed;
    }
}

} # namespace HttpServerAsyncIo
