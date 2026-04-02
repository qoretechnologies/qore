# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file Http2ClientPollOperationImpl.qc HTTP/2 client poll operation implementation

/** Http2ClientPollOperationImpl.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! HTTP/2 poll operation implementation
/** This poll operation manages an HTTP/2 client connection lifecycle:

    1. Connect and negotiate HTTP/2 via ALPN
    2. Exchange connection preface (SETTINGS frames)
    3. Submit multiple concurrent requests
    4. Read HTTP/2 frames and dispatch responses by stream ID
    5. Handle GOAWAY for graceful shutdown

    The C++ base class (Http2ClientPollOperationBase) provides the
    SocketPollOperationBase fast path for the AsyncIoController, implementing
    the continuePoll() state machine entirely in C++.

    @par Response Delivery
    Responses are delivered via C++ completion actions (PromiseAction for
    request/response, ChannelAction for streaming) — no closures on the I/O thread.
*/
public class Http2ClientPollOperation inherits Http2ClientPollOperationBase, HttpClientPollOperation {
    private {
        #! Logger (kept for Qore-level logging)
        *LoggerInterface h2_logger;

        #! Stream data listeners: stream_id string -> listener object
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

    #! Creates the poll operation for a new connection
    /** @param url_info parsed URL info with host, port, scheme
        @param connect_timeout timeout for connection establishment
        @param logger optional logger for debug output
        @param ssl_verify_mode SSL verification mode (optional)
        @param accept_all_certs if True, accept all SSL certificates including self-signed
        @param proxy_info optional proxy configuration
    */
    constructor(hash<auto> url_info, timeout connect_timeout = DefaultConnectTimeout, *LoggerInterface logger,
            *int ssl_verify_mode, bool accept_all_certs = False,
            *hash<auto> proxy_info, HttpClientConnection connection) :
            Http2ClientPollOperationBase(
                Http2ClientPollOperation::createAndConfigureSocket(url_info, ssl_verify_mode, accept_all_certs),
                Http2ClientPollOperation::computeConnectTarget(url_info, proxy_info),
                url_info.host, url_info.port,
                url_info.protocol == "https",
                proxy_info && url_info.protocol == "https" ? True : False,
                connection) {
        h2_logger = logger;
        # Store connection in Qore data hash for DGC cycle detection
        setConnection(connection);
    }

    #! Prevent copying — shared socket/mutex/state would corrupt
    private copy() {
        throw "COPY-ERROR", "Http2ClientPollOperation cannot be copied";
    }

    #! Registers a listener to be notified when stream data arrives
    /** @param stream_id the HTTP/2 stream ID
        @param listener an object with a notifyIo() method
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

    #! Cancels a pending request
    /** @param stream_id the stream ID to cancel
    */
    cancelRequest(int stream_id) {
        if (!self.cancelStream(stream_id)) {
            log(LoggerLevel::DEBUG, "cancel ignored: stream %d not found (already completed)", stream_id);
            return;
        }
        # Cancel at the nghttp2 level
        Socket sock = self.getSocket();
        try {
            sock.cancelHttp2Stream(stream_id);
        } catch () {
        }
        log(LoggerLevel::DEBUG, "cancelled request: stream_id=%d", stream_id);
    }

    #! Internal: creates and configures a TCP socket for HTTP/2
    private static Socket createAndConfigureSocket(hash<auto> url_info, *int ssl_verify_mode,
            bool accept_all_certs) {
        Socket sock();
        bool ssl_required = url_info.protocol == "https";
        if (ssl_required) {
            if (exists ssl_verify_mode) {
                sock.setSslVerifyMode(ssl_verify_mode);
            }
            if (accept_all_certs) {
                sock.acceptAllCertificates(True);
            }
            sock.setAlpnProtocols(("h2",));
        }
        return sock;
    }

    #! Internal: computes the TCP connect target string
    private static string computeConnectTarget(hash<auto> url_info, *hash<auto> proxy_info) {
        if (proxy_info) {
            return sprintf("%s:%d", proxy_info.host, proxy_info.port);
        }
        return sprintf("%s:%d", url_info.host, url_info.port);
    }

    #! Internal: logging helper
    private log(int level, string fmt) {
        if (h2_logger) {
            call_function_args(\h2_logger.log(), (level, "Http2ClientPollOperation: " + fmt) + argv);
        }
    }
}

} # namespace HttpClientIo
