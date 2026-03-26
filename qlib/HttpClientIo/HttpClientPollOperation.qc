# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file HttpClientPollOperation.qc Abstract base for HTTP client poll operations

/** HttpClientPollOperation.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Abstract base class for HTTP client poll operations
/** This class provides the common interface and shared concrete methods for
    HTTP/2 and HTTP/3 client poll operations.

    The poll operation manages the lifecycle of a multiplexed client connection:
    connecting, reading frames/packets, and dispatching responses to stream
    callbacks.

    @note The inherited AbstractPollOperation methods (continuePoll, abort, getGoal,
    goalReached, getState) are implemented by concrete subclasses directly.
*/
public class HttpClientPollOperation inherits AbstractPollOperation {
    public {
        #! Connecting state
        const STATE_CONNECTING = "connecting";
        #! Reading frames/packets state
        const STATE_READING = "reading";
        #! Waiting for socket readability
        const STATE_WAIT_READ = "wait_read";
        #! Connection closed
        const STATE_CLOSED = "closed";

        #! Maximum consecutive empty reads before closing
        const MAX_EMPTY_READS = 100;
    }

    private {
        #! The client socket
        Socket sock;

        #! Current state
        string state = STATE_CONNECTING;

        #! Current poll operation (connect, ssl, or multiplex)
        AbstractPollOperation current_op;

        #! Pending stream callbacks: stream_id -> callback
        hash<string, HttpResponseCallback> stream_callbacks;

        #! Pending stream contexts: stream_id -> context hash
        hash<string, hash<auto>> stream_ctxs;

        #! Number of active streams
        int active_stream_count = 0;

        #! Lock for stream callback access
        Mutex stream_lock();

        #! Error info if an error occurred
        *hash<auto> error_info;

        #! Counter for consecutive empty reads
        int empty_read_count = 0;

        #! Flag set when a stream callback is dispatched during continuePoll()
        bool callback_dispatched = False;

        #! Target URL info
        hash<auto> url_info;

        #! Connect timeout
        timeout connect_timeout;

        #! Logger
        *LoggerInterface logger;

        #! Callback invoked when connection becomes ready
        *code<nothing()> ready_callback;

        #! Completion handler called from onComplete()
        *code<nothing(hash<SocketPollResultInfo>)> completion_handler;
    }

    #! Returns True if the connection is closed
    bool isClosed() {
        string s = state;
        return s == STATE_CLOSED;
    }

    #! Returns True if the connection is ready for requests
    /** Subclasses can override isReadyImpl() to add additional readiness checks
        (e.g., h2c protocol confirmation for plain HTTP).
    */
    bool isReady() {
        string s = state;
        if (s != STATE_READING && s != STATE_WAIT_READ) {
            return False;
        }
        return isReadyImpl();
    }

    #! Hook for subclass-specific readiness checks
    /** Default returns True (no additional checks). Override to add protocol-specific
        readiness conditions (e.g., h2c confirmation for plain HTTP).
    */
    private bool isReadyImpl() {
        return True;
    }

    #! Returns True if a callback was dispatched during the last continuePoll() call, then clears the flag
    bool checkAndClearCallbackDispatched() {
        AutoLock al(stream_lock);
        if (callback_dispatched) {
            callback_dispatched = False;
            return True;
        }
        return False;
    }

    #! Sets a callback to be invoked when the connection becomes ready
    /** @note Must be called during construction, before any continuePoll()
        invocation.  Not safe to call concurrently with continuePoll().
    */
    setReadyCallback(code<nothing()> callback) {
        ready_callback = callback;
    }

    #! Sets a completion handler called from onComplete()
    setCompletionHandler(code<nothing(hash<SocketPollResultInfo>)> handler) {
        completion_handler = handler;
    }

    nothing onComplete(hash<SocketPollResultInfo> result) {
        if (completion_handler) {
            completion_handler(result);
        }
    }

    #! Returns True if an error occurred
    bool hasError() {
        return exists error_info;
    }

    #! Returns error information if an error occurred
    *hash<auto> getError() {
        return error_info;
    }

    #! Returns the socket
    Socket getSocket() {
        return sock;
    }

    #! Returns the number of active streams
    int getActiveStreamCount() {
        AutoLock al(stream_lock);
        return active_stream_count;
    }

    #! Continues reading without creating a new poll operation
    continueReading() {
        if (state != STATE_WAIT_READ) {
            throw "HTTPCLIENT-STATE-ERROR", sprintf("cannot continue reading in state: %y", state);
        }
        empty_read_count = 0;
        state = STATE_READING;
    }

    #! Cancels a pending request (subclasses must implement)
    abstract cancelRequest(int stream_id);

    #! Internal: sets error state and notifies pending streams
    private setError(string err, string desc) {
        error_info = {
            "err": err,
            "desc": desc,
        };
        state = STATE_CLOSED;
        log(LoggerLevel::ERROR, "connection error: %s: %s", err, desc);
        notifyPendingStreams(error_info);
    }

    #! Internal: notifies all pending streams of an error
    private notifyPendingStreams(hash<auto> err) {
        *hash<string, HttpResponseCallback> callbacks_copy;
        *hash<string, hash<auto>> ctxs_copy;
        {
            AutoLock al(stream_lock);
            callbacks_copy = stream_callbacks;
            ctxs_copy = stream_ctxs;
            remove stream_callbacks;
            remove stream_ctxs;
            active_stream_count = 0;
        }
        foreach string sid in (keys callbacks_copy) {
            HttpResponseCallback cb = callbacks_copy{sid};
            *hash<auto> stream_ctx = ctxs_copy{sid};
            try {
                cb({}, err + {"stream_id": sid.toInt(), "ctx": stream_ctx});
            } catch (hash<ExceptionInfo> ex) {
                log(LoggerLevel::ERROR, "stream error callback exception: %s: %s",
                    ex.err, ex.desc);
            }
        }
    }

    #! Internal: fires the ready callback if set
    private fireReadyCallback() {
        if (ready_callback) {
            try {
                ready_callback();
            } catch (hash<ExceptionInfo> ex) {
                log(LoggerLevel::ERROR, "ready callback exception: %s: %s", ex.err, ex.desc);
            }
        }
    }

    #! Internal: logging helper
    private log(int level, string fmt) {
        if (logger) {
            call_function_args(\logger.log(), (level, "HttpClientPollOperation: " + fmt) + argv);
        }
    }
}

} # namespace HttpClientIo
