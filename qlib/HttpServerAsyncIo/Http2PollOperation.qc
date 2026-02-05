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
public class Http2PollOperation inherits AbstractPollOperation {
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
    }

    #! Creates the poll operation
    /** @param sock the client socket with an established HTTP/2 connection
    */
    constructor(Socket sock) {
        @assert(sock, "socket cannot be null");
        @assert(sock.isOpen(), "socket must be open");
        self.sock = sock;
        # Start reading HTTP/2 request
        startReadRequest();
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
    auto getOutput() {
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
        @param body the body of the pushed resource

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

        The callback takes (string path, *hash<auto> headers) and returns the promised stream ID.
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
        if (state != STATE_SENT && state != STATE_REQUEST_READY) {
            throw "HTTP2-STATE-ERROR", sprintf("cannot read next request in state: %y", state);
        }
        remove request;
        startReadRequest();
    }

    #! Continues reading without creating a new C++ read operation
    /** Called after a request has been dispatched in multiplexing mode.  The same
        underlying C++ SocketHttp2ServerPollOperation continues reading frames.
        This avoids the overhead of creating a new operation and preserves the
        nghttp2 session state (including any already-received frames).
    */
    continueReading() {
        if (state != STATE_REQUEST_READY) {
            throw "HTTP2-STATE-ERROR", sprintf("cannot continue reading in state: %y", state);
        }
        remove request;
        empty_read_count = 0;
        state = STATE_READING;
    }

    #! Aborts the operation
    abort() {
        if (current_op) {
            current_op.abort();
        }
        state = STATE_CLOSED;
        try {
            sock.close();
        } catch () {}
    }

    #! Internal: starts the read request operation
    private startReadRequest() {
        current_op = sock.startPollReadHttp2Request();
        state = STATE_READING;
    }
}

} # namespace HttpServerAsyncIo
