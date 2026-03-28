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

    The core poll state machine (continuePoll) runs in pure C++ via
    HttpAcceptPollOperationBase.  This Qore wrapper adds:
    - ALPN configuration on the listener before startPollAccept()
    - Completion context (setCompletionContext / onComplete) for the async
      I/O controller callback
*/
public class HttpAcceptPollOperation inherits HttpAcceptPollOperationBase {
    private {
        #! SSL certificate for the listener (if SSL) - can be SSLCertificate or data
        auto cert;

        #! SSL private key for the listener (if SSL) - can be SSLPrivateKey or data
        auto key;

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
            string http_version = "auto", bool do_ssl_handshake = True)
            : HttpAcceptPollOperationBase(listener,
                HttpAcceptPollOperation::startAcceptWithAlpn(listener, ssl, http_version),
                ssl, HttpAcceptPollOperation::ensureCert(cert), HttpAcceptPollOperation::ensureKey(key),
                ssl_verify_flags, ssl_accept_all_certs, http_version, do_ssl_handshake) {
        @assert(listener, "listener socket cannot be null");
        @assert(!ssl || (cert && key), "SSL mode requires certificate and key");
        # Store cert/key for onComplete callback (which needs them for handleAcceptResult)
        self.cert = cert;
        self.key = key;
    }

    #! Configures ALPN on the listener and starts the accept poll operation
    /** ALPN must be configured before startPollAccept() so that auto-SSL during
        accept negotiates the correct protocol.  This static helper ensures the
        ALPN call happens before the accept op is created.

        @param listener the server socket
        @param ssl if True, configure ALPN protocols
        @param http_version HTTP version mode
        @return the SocketPollOperation from listener.startPollAccept()
    */
    private static AbstractPollOperation startAcceptWithAlpn(Socket listener, bool ssl, string http_version) {
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
        return listener.startPollAccept();
    }

    #! Ensures the certificate is an SSLCertificate object
    /** Converts PEM string or DER binary data to SSLCertificate if needed.
        @param cert the certificate value (SSLCertificate, string, binary, or NOTHING)
        @return SSLCertificate object or NOTHING
    */
    private static *SSLCertificate ensureCert(auto cert) {
        if (!cert) {
            return NOTHING;
        }
        if (cert instanceof SSLCertificate) {
            return cert;
        }
        if (cert.typeCode() == NT_STRING) {
            return new SSLCertificate(cert);
        }
        if (cert.typeCode() == NT_BINARY) {
            return new SSLCertificate(cert);
        }
        throw "HTTPACCEPTPOLLOPERATION-ERROR", sprintf("invalid certificate type: %y", cert.type());
    }

    #! Ensures the key is an SSLPrivateKey object
    /** Converts PEM string or DER binary data to SSLPrivateKey if needed.
        @param key the key value (SSLPrivateKey, string, binary, or NOTHING)
        @return SSLPrivateKey object or NOTHING
    */
    private static *SSLPrivateKey ensureKey(auto key) {
        if (!key) {
            return NOTHING;
        }
        if (key instanceof SSLPrivateKey) {
            return key;
        }
        if (key.typeCode() == NT_STRING) {
            return new SSLPrivateKey(key);
        }
        if (key.typeCode() == NT_BINARY) {
            return new SSLPrivateKey(key);
        }
        throw "HTTPACCEPTPOLLOPERATION-ERROR", sprintf("invalid private key type: %y", key.type());
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
                isSsl(), cert, key, completion_http_version, completion_owner);
        }
    }
}

} # namespace HttpServerAsyncIo
