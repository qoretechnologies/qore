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

    The core state machine (continuePoll) runs in pure C++ via the
    Http2PollOperationBase parent class for I/O thread efficiency.

    This Qore wrapper adds:
    - Stream queue registration for CONNECT handlers (registerStreamQueue delegates to C++ base)
    - Push promise support (pushResource, getPushResourceCallback)
    - Completion context (setCompletionContext, onComplete via HttpConnectionPollOperationBase)

    @note Stream queue drain logic runs in pure C++ on the I/O thread — no Qore interpreter
    calls.  The C++ base class (Http2PollOperationBase) handles drainStreamQueues() and
    clearStreamQueues() automatically.

*/
public class Http2PollOperation inherits Http2PollOperationBase, HttpConnectionPollOperationBase {
    public {
        #! State constants (match C++ Http2PollOperationPriv::H2ServerState)
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

    # Stream queue management is now handled entirely in C++ by the
    # Http2PollOperationBase parent class (registerStreamQueue, drainStreamQueues,
    # clearStreamQueues).

    private {
        #! Stream data listeners: stream_id string → listener object
        /** The listener's notifyIo() method is called from the controller's
            worker pool when the I/O thread drains data to the stream's Queue.
        */
        hash<string, object> stream_listeners;

        #! Pending data flags for streams where onStreamData fired before
        #! the listener was registered (race between I/O dispatch and handler setup)
        hash<string, bool> pending_stream_data;

        #! Lock protecting stream listener registration and dispatch
        Mutex stream_listener_mutex();
    }

    #! Registers a listener to be notified when stream data arrives
    /** @param stream_id the HTTP/2 stream ID
        @param listener an object with a notifyIo() method (e.g., WebSocketConnection)
    */
    registerStreamListener(int stream_id, object listener) {
        *bool has_pending;
        {
            AutoLock al(stream_listener_mutex);
            stream_listeners{stream_id} = listener;
            has_pending = remove pending_stream_data{stream_id};
        }
        # Replay missed notification if data arrived before registration
        if (has_pending) {
            listener.notifyIo();
        }
    }

    #! Called on a worker thread when stream data has been drained to a Queue
    /** Dispatched by the async I/O controller after the C++ drain pushes data.
        Wakes the handler thread for the given stream.
        @param stream_key stream identifier (string representation of stream_id)
    */
    nothing onStreamData(string stream_key) {
        int stream_id = stream_key.toInt();
        AutoLock al(stream_listener_mutex);
        *object listener = stream_listeners{stream_id};
        if (listener) {
            listener.notifyIo();
        } else {
            # Buffer the notification — the handler hasn't registered yet
            pending_stream_data{stream_id} = True;
        }
    }

    #! Creates the poll operation
    /** @param sock the client socket with an established HTTP/2 connection
        @param headers_only if True, dispatch requests as soon as headers arrive
        (before END_STREAM), enabling bidirectional streaming; the handler must read
        the body via Socket::readHttp2StreamDataBlock()
    */
    constructor(Socket sock, bool headers_only = False) :
            Http2PollOperationBase(sock,
                sock.startPollReadHttp2Request(headers_only ? {"headers_only": True} : NOTHING),
                headers_only) {
        @assert(sock, "socket cannot be null");
        @assert(sock.isOpen(), "socket must be open");
    }

    #! Explicit onComplete override for correct multiple inheritance resolution
    /** Without this, Qore resolves onComplete() to AbstractPollOperation::onComplete()
        along the Http2PollOperationBase -> SocketPollOperationBase -> AbstractPollOperation
        path, bypassing the HttpConnectionPollOperationBase override.
    */
    nothing onComplete(hash<SocketPollResultInfo> result) {
        HttpConnectionPollOperationBase::onComplete(result);
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
        string state = getH2State();
        if (state != STATE_REQUEST_READY && state != STATE_SENDING) {
            throw "HTTP2-STATE-ERROR", sprintf("cannot push resource in state: %y", state);
        }
        auto request = getOutput();
        if (!request || !request.stream_id) {
            throw "HTTP2-ERROR", "no stream_id available for push promise";
        }

        # Submit PUSH_PROMISE - returns the promised stream ID
        return getSocket().submitHttp2PushPromise(request.stream_id, path, headers ?? {});
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
        return getSocket().startPollSendHttp2Response(promised_stream_id, status_code, headers, body);
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
        string state = getH2State();
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
        auto request = getOutput();
        if (!request || !request.stream_id) {
            throw "HTTP2-ERROR", "no stream_id available for response";
        }

        # Create the send inner op via Qore Socket method, inject into C++ base
        auto send_op = getSocket().startPollSendHttp2Response(request.stream_id, status_code, headers, body);
        self.injectSendOp(send_op);
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
        auto request = getOutput();
        if (!request || !request.stream_id) {
            throw "HTTP2-ERROR", "no stream_id available for response";
        }

        auto send_op = getSocket().startPollSendHttp2StreamingResponse(
            request.stream_id, status_code, headers, body, chunk_size);
        self.injectSendOp(send_op);
    }

    #! Starts reading the next HTTP/2 request on this connection
    /** After sending a response or when goalReached indicated a request was ready but
        getOutput() returned NOTHING (e.g., after processing HTTP/2 control frames),
        call this to wait for the next request on the persistent HTTP/2 connection.
    */
    startReadNextRequest() {
        # Create a new read inner op via Qore Socket method, inject into C++ base
        auto read_op = getSocket().startPollReadHttp2Request(
            isHeadersOnly() ? {"headers_only": True} : NOTHING);
        self.injectReadOp(read_op);
    }

    #! Aborts the operation
    /** Calls the C++ base abort, then cleans up stream queues.
    */
    abort() {
        # C++ abort() calls clearStreamQueues() which pushes NOTHING sentinels
        # to all registered queues and notifies all EventNotifiers
        Http2PollOperationBase::abort();
        try {
            getSocket().close();
        } catch () {}
    }

    #! Registers a Queue for delivering data to a CONNECT stream handler
    /** Called by the I/O controller when dispatching an extended CONNECT handler.
        The Queue must be unlimited (created with no max size) to ensure non-blocking pushes.

        The optional EventNotifier is signaled on the I/O thread (pure C++) after data
        is pushed to the Queue, enabling efficient cross-thread wake-up without Qore
        interpreter calls.

        @param stream_id the HTTP/2 stream ID
        @param queue the unlimited Queue for data delivery
        @param notifier optional EventNotifier for cross-thread wake-up after data is
        drained to the Queue

        @note Must be called on the I/O controller thread before the handler starts reading.

        @since HttpServerAsyncIo 1.1
    */
    registerStreamQueue(int stream_id, Queue queue, *code<nothing()> notify) {
        # Delegate to the C++ base class — the notify closure is not used;
        # the Queue's internal condition variable handles handler wake-up.
        # The C++ drain pushes data to the Queue which wakes blocking get() calls.
        Http2PollOperationBase::registerStreamQueue(stream_id, queue);
    }

    # drainStreamQueues() is now implemented in pure C++ in the
    # Http2PollOperationBase parent class.  It is called automatically by
    # continueReadPoll() after each inner op continuePoll() cycle.
}

} # namespace HttpServerAsyncIo
