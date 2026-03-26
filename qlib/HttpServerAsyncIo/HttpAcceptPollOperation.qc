# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file HttpAcceptPollOperation.qc Composite poll operation for HTTP accept

/** HttpAcceptPollOperation.qc Copyright 2024 - 2026 Qore Technologies, s.r.o.

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

#! Poll operation for TCP accept and optional SSL handshake
/** This poll operation handles accepting a new TCP connection and optionally
    performing an SSL handshake:

    1. TCP Accept - wait for incoming connection
    2. SSL Handshake (optional) - for HTTPS listeners

    After the operation completes, the caller should handle HTTP header reading
    separately to allow the listener to accept new connections concurrently.

*/
public class HttpAcceptPollOperation inherits AbstractPollOperation {
    public {
        #! State constants
        const STATE_ACCEPTING = "accepting";
        const STATE_SSL_HANDSHAKE = "ssl_handshake";
        const STATE_COMPLETE = "complete";
    }

    private {
        #! The listener socket accepting connections
        Socket listener;

        #! Whether this is an SSL listener
        bool ssl;

        #! SSL certificate for the listener (if SSL) - can be SSLCertificate or data
        auto cert;

        #! SSL private key for the listener (if SSL) - can be SSLPrivateKey or data
        auto key;

        #! SSL verify flags for client certificate verification
        int ssl_verify_flags = SSL_VERIFY_NONE;

        #! Accept all client certificates (including self-signed)
        bool ssl_accept_all_certs = True;

        #! HTTP version mode ("auto", "1.0", "1.1", "2.0")
        string http_version = "auto";

        #! True to perform SSL handshake in this operation
        bool do_ssl_handshake = True;

        #! Current poll operation being executed
        SocketPollOperation spop;

        #! Current state
        string state = STATE_ACCEPTING;

        #! The accepted client socket
        Socket client_sock;

        #! The negotiated protocol after ALPN (e.g., "h2", "http/1.1")
        *string negotiated_protocol;

        #! True if the connection is using HTTP/2
        bool is_http2 = False;

        #! Controller for completion callback
        *HttpAsyncSocketIoController completion_controller;

        #! Listener socket for completion callback
        *Socket completion_listener;

        #! Target for completion callback
        *HttpAsyncTarget completion_target;

        #! HTTP version for completion callback
        *string completion_http_version;

        #! Owner identifier for completion callback
        *string completion_owner;
    }

    #! Creates the poll operation
    /** @param listener the server socket to accept connections from (Socket or any subclass like HttpListener)
        @param ssl if True, perform SSL handshake after accept
        @param cert SSL certificate (SSLCertificate object or PEM data, required if ssl is True)
        @param key SSL private key (SSLPrivateKey object or PEM data, required if ssl is True)
        @param ssl_verify_flags SSL verification flags for client certificate verification (e.g. SSL_VERIFY_PEER)
        @param ssl_accept_all_certs if True, accept all client certificates including self-signed ones
        @param http_version HTTP version mode: "auto" (default), "1.0", "1.1", "2.0", or "h2c"
                           - "h2c" enables HTTP/2 cleartext (prior knowledge) on non-SSL connections
        @param do_ssl_handshake if False, defer SSL handshake to the caller. When deferred, the caller
               must complete the handshake using Socket::upgradeServerToSSL() poll operation before
               proceeding with HTTP communication. This allows the EventLoop to handle the SSL handshake
               with proper socket monitoring.
    */
    constructor(Qore::Socket listener, bool ssl = False, auto cert = NOTHING, auto key = NOTHING,
            int ssl_verify_flags = SSL_VERIFY_NONE, bool ssl_accept_all_certs = True,
            string http_version = "auto", bool do_ssl_handshake = True) {
        @assert(listener, "listener socket cannot be null");
        @assert(!ssl || (cert && key), "SSL mode requires certificate and key");
        self.listener = listener;
        self.ssl = ssl;
        self.cert = cert;
        self.key = key;
        self.ssl_verify_flags = ssl_verify_flags;
        self.ssl_accept_all_certs = ssl_accept_all_certs;
        self.http_version = http_version;
        self.do_ssl_handshake = do_ssl_handshake;
        # Set ALPN protocols on the listener socket before accepting, so that SSL handshakes
        # performed automatically during startPollAccept() will negotiate the correct protocol
        if (ssl) {
            list<string> alpn;
            if (http_version == "auto") {
                alpn = ("h2", "http/1.1");
            } else if (http_version == "2.0" || http_version == "2") {
                alpn = ("h2",);
            } else {
                alpn = ("http/1.1",);
            }
            listener.setAlpnProtocols(alpn);
        }
        # Start the accept poll operation
        spop = listener.startPollAccept();
        @assert(spop, "startPollAccept must return poll operation");
        @debug {
            if (getenv("QORE_HTTP_ACCEPT_DEBUG")) {
                printf("%y TID %d: HttpAcceptPollOperation init: listener=%s ssl=%y http_version=%y "
                    "do_ssl_handshake=%y\n", now_us(), gettid(), socketId(listener), ssl, http_version,
                    do_ssl_handshake);
            }
        }
    }

    private string socketId(Socket s) {
        return sprintf("%s %s", s.className(), s.uniqueHash()[0..8]);
    }

    #! Returns the goal of the operation
    string getGoal() {
        return "http_accept";
    }

    #! Returns the current state
    string getState() {
        return state;
    }

    #! Returns True when the operation is complete
    bool goalReached() {
        return state == STATE_COMPLETE;
    }

    #! Continues the poll operation
    /** @return poll info for the next poll() call, or NOTHING if complete
    */
    *hash<SocketPollInfo> continuePoll() {
        while (True) {
            # Drive the current sub-operation
            @debug {
                if (getenv("QORE_ACCEPT_POLL_DEBUG")) {
                    printf("%y TID %d: HttpAcceptPollOperation::continuePoll BEFORE: state=%y listener=%s "
                        "spop_state=%y\n", now_us(), gettid(), state, socketId(listener), spop.getState());
                }
            }
            auto raw_info = spop.continuePoll();
            @debug {
                if (getenv("QORE_ACCEPT_POLL_DEBUG")) {
                    printf("%y TID %d: HttpAcceptPollOperation::continuePoll AFTER: state=%y listener=%s "
                        "spop_state=%y raw_info=%y\n", now_us(), gettid(), state, socketId(listener),
                        spop.getState(), raw_info ? sprintf("events=%d socket=%s", raw_info.events,
                            raw_info.socket ? socketId(raw_info.socket) : "null") : "NOTHING");
                }
            }

            # Check what we got back
            if (raw_info) {
                # It's poll info - return it
                return cast<hash<SocketPollInfo>>(raw_info);
            }

            # Sub-operation complete (returned NOTHING) - transition to next state
            switch (state) {
                case STATE_ACCEPTING: {
                    # Accept complete - get the client socket
                    # Note: getOutput() may return NOTHING if the listener was closed during accept
                    *Socket sock = spop.getOutput();
                    if (!sock) {
                        @debug {
                            if (getenv("QORE_HTTP_ACCEPT_DEBUG")) {
                                printf("%y TID %d: HttpAcceptPollOperation accept: no socket returned; "
                                    "listener=%s\n", now_us(), gettid(), socketId(listener));
                            }
                        }
                        # Listener closed - operation aborted
                        state = STATE_COMPLETE;
                        return NOTHING;
                    }
                    client_sock = sock;
                    @assert(client_sock.isOpen(), "accepted socket must be open");
                    # Set TCP_NODELAY to ensure responses are sent immediately
                    client_sock.setNoDelay(True);
                    @debug {
                        if (getenv("QORE_HTTP_ACCEPT_TRACE")) {
                            printf("%y TID %d: HttpAcceptPollOperation accept: listener=%s client=%s ssl=%y\n",
                                now_us(), gettid(), socketId(listener), socketId(client_sock), ssl);
                        }
                    }
                    if (ssl) {
                        @debug {
                            if (getenv("QORE_HTTP_ACCEPT_DEBUG")) {
                                printf("%y TID %d: HttpAcceptPollOperation accept: client=%s secure=%y\n",
                                    now_us(), gettid(), socketId(client_sock), client_sock.isSecure());
                            }
                        }
                        # Check if SSL handshake was already done during accept
                        # This happens when the listener socket has SSL configured -
                        # startPollAccept() automatically upgrades the accepted socket to SSL
                        if (client_sock.isSecure()) {
                            negotiated_protocol = client_sock.getAlpnProtocol();
                            is_http2 = (negotiated_protocol == "h2");

                            # If HTTP/2 is required but not negotiated, reject the connection
                            if ((http_version == "2.0" || http_version == "2") && !is_http2) {
                                client_sock.close();
                                throw "HTTP2-REQUIRED-ERROR",
                                    sprintf("HTTP/2 required but client negotiated: %y",
                                        negotiated_protocol ?? "none");
                            }

                            # SSL already complete - done
                            state = STATE_COMPLETE;
                            return NOTHING;
                        }

                        if (!do_ssl_handshake) {
                            # Defer SSL handshake to the caller
                            state = STATE_COMPLETE;
                            return NOTHING;
                        }

                        # Transition to SSL handshake
                        @debug {
                            if (getenv("QORE_HTTP_ACCEPT_DEBUG")) {
                                printf("%y TID %d: HttpAcceptPollOperation ssl: starting handshake for %s\n",
                                    now_us(), gettid(), socketId(client_sock));
                            }
                        }
                        state = STATE_SSL_HANDSHAKE;
                        if (cert) {
                            client_sock.setCertificate(cert);
                        }
                        if (key) {
                            client_sock.setPrivateKey(key);
                        }
                        # Set SSL verify mode and enable certificate capture for client certs
                        if (ssl_verify_flags != SSL_VERIFY_NONE) {
                            client_sock.setSslVerifyMode(ssl_verify_flags);
                            client_sock.captureRemoteCertificates();
                        }
                        # Configure client certificate acceptance behavior
                        client_sock.acceptAllCertificates(ssl_accept_all_certs);
                        # Set ALPN protocols based on http_version mode
                        list<string> alpn;
                        if (http_version == "auto") {
                            # Prefer HTTP/2, fall back to HTTP/1.1
                            alpn = ("h2", "http/1.1");
                        } else if (http_version == "2.0" || http_version == "2") {
                            # HTTP/2 only
                            alpn = ("h2",);
                        } else {
                            # HTTP/1.x only - advertise http/1.1
                            alpn = ("http/1.1",);
                        }
                        client_sock.setAlpnProtocols(alpn);
                        spop = client_sock.startPollUpgradeServerToSSL();
                        @assert(spop, "startPollUpgradeServerToSSL must return poll operation");
                        # Continue loop to poll the new operation
                        continue;
                    } else {
                        # No SSL - check for h2c (HTTP/2 cleartext) mode
                        if (http_version == "h2c") {
                            # h2c direct mode - start HTTP/2 with prior knowledge
                            is_http2 = True;
                            negotiated_protocol = "h2c";
                        }
                        # Accept complete, return NOTHING
                        state = STATE_COMPLETE;
                        return NOTHING;
                    }
                }

                case STATE_SSL_HANDSHAKE: {
                    # SSL handshake complete - detect negotiated protocol
                    negotiated_protocol = client_sock.getAlpnProtocol();
                    is_http2 = (negotiated_protocol == "h2");
                    @debug {
                        if (getenv("QORE_HTTP_ACCEPT_DEBUG")) {
                            printf("%y TID %d: HttpAcceptPollOperation ssl: complete client=%s alpn=%y "
                                "http2=%y\n", now_us(), gettid(), socketId(client_sock), negotiated_protocol,
                                is_http2);
                        }
                        if (getenv("QORE_HTTP_ACCEPT_TRACE")) {
                            printf("%y TID %d: HttpAcceptPollOperation ssl: complete client=%s alpn=%y "
                                "http2=%y\n", now_us(), gettid(), socketId(client_sock), negotiated_protocol,
                                is_http2);
                        }
                    }

                    # If HTTP/2 is required but not negotiated, reject the connection
                    if ((http_version == "2.0" || http_version == "2") && !is_http2) {
                        client_sock.close();
                        throw "HTTP2-REQUIRED-ERROR", sprintf("HTTP/2 required but client negotiated: %y",
                            negotiated_protocol ?? "none");
                    }

                    state = STATE_COMPLETE;
                    return NOTHING;
                }
            }

            # Should not reach here
            @assert(False, "unexpected state: %y", state);
            break;
        }

        return NOTHING;
    }

    #! Returns the accepted Socket
    Socket getSocket() {
        if (state != STATE_COMPLETE) {
            throw "OPERATION-NOT-COMPLETE", sprintf("Cannot get socket: state is %y", state);
        }
        return client_sock;
    }

    #! Returns the output of the operation
    /** The returned hash contains:
        - \c sock: the accepted Socket
        - \c ssl: True if this is an SSL connection
        - \c is_http2: True if HTTP/2 was negotiated via ALPN
        - \c negotiated_protocol: the ALPN negotiated protocol (e.g., "h2", "http/1.1"), or NOTHING
        - \c http_version: the configured HTTP version mode
    */
    auto getOutput() {
        return {
            "sock": client_sock,
            "ssl": ssl,
            "is_http2": is_http2,
            "negotiated_protocol": negotiated_protocol,
            "http_version": http_version,
        };
    }

    #! Returns True if this is an SSL connection
    bool isSsl() {
        return ssl;
    }

    #! Returns True if HTTP/2 was negotiated
    bool isHttp2() {
        return is_http2;
    }

    #! Returns the negotiated ALPN protocol
    *string getNegotiatedProtocol() {
        return negotiated_protocol;
    }

    #! Sets the completion context so that onComplete() can dispatch the result
    /** @param controller the async I/O controller
        @param listener the server listener socket
        @param target the async target
        @param http_version the HTTP version string
        @param owner the owner identifier
    */
    nothing setCompletionContext(HttpAsyncSocketIoController controller, Socket listener,
            HttpAsyncTarget target, string http_version, string owner) {
        completion_controller = controller;
        completion_listener = listener;
        completion_target = target;
        completion_http_version = http_version;
        completion_owner = owner;
    }

    #! Called when the operation completes; dispatches result to the controller
    nothing onComplete(hash<SocketPollResultInfo> result) {
        if (completion_controller) {
            completion_controller.handleAcceptResult(result, completion_listener, completion_target,
                ssl, cert, key, completion_http_version, completion_owner);
        }
    }

    #! Aborts the operation
    abort() {
        if (spop) {
            spop.abort();
        }
        if (client_sock) {
            client_sock.close();
        }
    }
}

} # namespace HttpServerAsyncIo
