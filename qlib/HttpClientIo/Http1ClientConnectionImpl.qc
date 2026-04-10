# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file Http1ClientConnectionImpl.qc HTTP/1.1 client connection implementation

/** Http1ClientConnectionImpl.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! HTTP/1.1 client connection implementation
/** This class wraps a Socket + Http1ClientPollOperation to provide a
    higher-level interface for HTTP/1.1 client connections.

    HTTP/1.1 is a serial protocol — only one request can be in flight at a
    time on a given connection. The connection manager creates multiple
    connections for concurrent requests.

    @par Thread Safety
    This class is thread-safe. Stream submission uses the poll operation's
    internal mutex for synchronization.
*/
public class Http1ClientConnection inherits HttpClientConnection {
    private {
        #! Poll operation for this connection
        Http1ClientPollOperation poll_op;
        #! Cached socket reference — must return the same object on every call
        #! so the AsyncIoController's obj_to_sock_hash lookup works correctly
        Socket cached_socket;
    }

    #! Copying is not supported
    private copy() {
        throw "COPY-ERROR", "Http1ClientConnection objects cannot be copied";
    }

    #! Creates a new HTTP/1.1 client connection
    /** @param url_info parsed URL info with host, port, scheme
        @param connect_timeout timeout for connection establishment
        @param logger optional logger for debug output
        @param max_streams maximum concurrent streams (should be 1 for HTTP/1.1)
        @param ssl_verify_mode SSL verification mode (optional)
        @param accept_all_certs if True, accept all SSL certificates including self-signed
        @param proxy_info parsed HTTP proxy info (optional)
        @param client_cert X.509 client certificate for mutual TLS (optional)
        @param client_pk private key matching \a client_cert (optional)
    */
    constructor(hash<auto> url_info, timeout connect_timeout = DefaultConnectTimeout,
            *LoggerInterface logger, int max_streams = 1,
            *int ssl_verify_mode, bool accept_all_certs = False,
            *hash<auto> proxy_info,
            *SSLCertificate client_cert, *SSLPrivateKey client_pk) {
        self.url_info = url_info;
        self.connect_timeout = connect_timeout;
        self.logger = logger;
        self.proxy_info = proxy_info;
        # HTTP/1.1 is serial — only 1 concurrent stream is supported
        if (max_streams > 1) {
            throw "HTTP1-OPTION-ERROR",
                sprintf("HTTP/1.1 connections support only 1 concurrent stream, got %d", max_streams);
        }
        self.max_concurrent_streams = 1;
        self.last_activity = now_us();
        connection_id = allocateConnectionId();

        log(LoggerLevel::DEBUG, "creating HTTP/1.1 connection to %s:%d (ssl=%y proxy=%y mtls=%y)",
            url_info.host, url_info.port, url_info.protocol == "https",
            proxy_info ? sprintf("%s:%d", proxy_info.host, proxy_info.port) : NOTHING,
            client_cert ? True : False);

        poll_op = new Http1ClientPollOperation(url_info, connect_timeout, logger,
            ssl_verify_mode, accept_all_certs, proxy_info, self, client_cert, client_pk);
    }

    #! Returns the protocol identifier
    string getProtocol() {
        return "h1";
    }

    #! Returns the poll operation for this connection
    HttpClientPollOperation getPollOperation() {
        return poll_op;
    }

    #! Returns the underlying socket (cached for stable identity)
    Socket getSocket() {
        if (!cached_socket) {
            cached_socket = poll_op.getSocket();
        }
        return cached_socket;
    }

    #! Submits a request on this connection (protocol-specific)
    /** The capacity check is performed atomically inside poll_op.submitRequest()
        under the stream_lock.
    */
    int submitRequestImpl(string method, string path, *hash<auto> headers,
            *data body, Promise promise) {
        int sid = poll_op.submitRequest(method, path, headers, body, promise,
            max_concurrent_streams);
        # Wake the I/O controller so it processes the queued request
        *object ctl;
        {
            AutoLock al(lock);
            ctl = controller_ref;
        }
        if (ctl) {
            ctl.wakeSocket(getSocket());
        }
        return sid;
    }

    #! Submits a request using CompositeAction (Promise + EventNotifier) — HTTP/1.1
    int submitRequestWithNotifierImpl(string method, string path, *hash<auto> headers,
            *data body, Promise promise, EventNotifier notifier) {
        return poll_op.submitRequestWithNotifier(method, path, headers, body, promise,
            notifier, max_concurrent_streams);
    }

    #! Submits a streaming request on this connection
    /** Each response phase is pushed to the Channel:
        - Headers: hash with \c status_code and \c headers
        - Body data: hash with \c body (binary chunk)
        - End of stream: hash with \c end_stream: \c True

        @param method HTTP method
        @param path request path
        @param headers request headers (optional)
        @param body request body (optional)
        @param channel Channel for incremental response delivery
        @param streaming if \c True, enable streaming mode (incremental body delivery)

        @return the stream ID

        @see submitRequestImpl()
    */
    int submitRequestStreaming(string method, string path, *hash<auto> headers,
            *data body, Channel channel, bool streaming = True) {
        int sid = poll_op.submitRequestStreaming(method, path, headers, body, channel,
            max_concurrent_streams, streaming);
        # Wake the I/O controller so it processes the queued request
        *object ctl;
        {
            AutoLock al(lock);
            ctl = controller_ref;
        }
        if (ctl) {
            ctl.wakeSocket(getSocket());
        }
        return sid;
    }

    #! Cancels a request (protocol-specific)
    cancelRequestImpl(int stream_id) {
        poll_op.cancelRequest(stream_id);
    }

    #! Closes the connection (protocol-specific)
    closeImpl(*timeout close_timeout) {
        self.setClosed();
        try {
            poll_op.abort();
        } catch () {
            # Ignore abort errors during close
        }
    }
}

} # namespace HttpClientIo
