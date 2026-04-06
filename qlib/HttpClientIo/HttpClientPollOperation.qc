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
    HTTP/1.1, HTTP/2, and HTTP/3 client poll operations.

    The poll operation manages the lifecycle of a client connection:
    connecting, reading frames/packets, and dispatching responses via
    Promises (request/response) or Channels (streaming).

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

        #! Number of active streams
        int active_stream_count = 0;

        #! Lock for stream state access
        Mutex stream_lock();

        #! Error info if an error occurred
        *hash<auto> error_info;

        #! Counter for consecutive empty reads
        int empty_read_count = 0;

        #! Flag set when a response is dispatched during continuePoll()
        bool response_dispatched = False;

        #! Target URL info
        hash<auto> url_info;

        #! Connect timeout
        timeout connect_timeout;

        #! Logger
        *LoggerInterface logger;

        #! Reference to the owning connection (for ready/completion dispatch)
        *HttpClientConnection connection_ref;
    }

    #! Sets the owning connection reference
    /** @param conn the connection that owns this poll operation
        @note Must be called during construction, before any continuePoll().
    */
    setConnection(HttpClientConnection conn) {
        connection_ref = conn;
    }

    #! Called when the poll operation completes
    /** Dispatches to the connection's handleCompletion() for deterministic
        result delivery (no closures on the worker thread).
    */
    nothing onComplete(hash<SocketPollResultInfo> result) {
        if (connection_ref) {
            connection_ref.handleCompletion(result);
        }
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
    /** Subclasses override to reject Promises and close Channels.
        Default is a no-op (for H3 which uses C++ actions).
    */
    private notifyPendingStreams(hash<auto> err) {
    }

    #! Internal: notifies the connection that the poll operation is ready
    /** Calls connection_ref.onConnectionReady() directly — deterministic
        virtual dispatch, no closures.
    */
    private fireReadyCallback() {
        if (connection_ref) {
            try {
                connection_ref.onConnectionReady();
            } catch (hash<ExceptionInfo> ex) {
                log(LoggerLevel::ERROR, "onConnectionReady exception: %s: %s", ex.err, ex.desc);
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
