# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file Http1ClientPollOperationImpl.qc HTTP/1.1 client poll operation implementation

/** Http1ClientPollOperationImpl.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! HTTP/1.1 poll operation implementation
/** This poll operation manages an HTTP/1.1 client connection lifecycle:

    1. Connect (optionally upgrading to SSL/TLS)
    2. Send a serialized HTTP/1.1 request
    3. Read and parse the response header
    4. Read the response body (Content-Length, chunked, or connection-close)
    5. Resolve the response Promise (or push to Channel for streaming) and return to idle

    The C++ base class (Http1ClientPollOperationBase) provides the
    SocketPollOperationBase fast path for the AsyncIoController, implementing
    the continuePoll() state machine entirely in C++.

    @par Response Delivery
    Responses are delivered via C++ completion actions (PromiseAction for
    request/response, ChannelAction for streaming) — no closures on the I/O thread.
*/
public class Http1ClientPollOperation inherits Http1ClientPollOperationBase, HttpClientPollOperation {
    private {
        #! Logger (kept for Qore-level logging)
        *LoggerInterface h1_logger;
    }

    #! Creates the poll operation for a new HTTP/1.1 connection
    /** @param url_info parsed URL info with host, port, scheme
        @param connect_timeout timeout for connection establishment
        @param logger optional logger for debug output
        @param ssl_verify_mode SSL verification mode (optional)
        @param accept_all_certs if True, accept all SSL certificates including self-signed
        @param proxy_info optional proxy configuration
        @param connection the owning HttpClientConnection
        @param client_cert X.509 client certificate for mutual TLS (optional)
        @param client_pk private key matching \a client_cert (optional)
    */
    constructor(hash<auto> url_info, timeout connect_timeout = DefaultConnectTimeout,
            *LoggerInterface logger, *int ssl_verify_mode, bool accept_all_certs = False,
            *hash<auto> proxy_info, HttpClientConnection connection,
            *SSLCertificate client_cert, *SSLPrivateKey client_pk) :
            Http1ClientPollOperationBase(
                Http1ClientPollOperation::createAndConfigureSocket(url_info, ssl_verify_mode,
                    accept_all_certs, client_cert, client_pk),
                Http1ClientPollOperation::computeConnectTarget(url_info, proxy_info),
                url_info.host, url_info.port,
                url_info.protocol == "https",
                proxy_info && url_info.protocol == "https" ? True : False,
                proxy_info && url_info.protocol != "https" ? True : False,
                connection) {
        h1_logger = logger;
        # Store connection in Qore data hash for DGC cycle detection
        setConnection(connection);
    }

    #! Prevent copying — shared socket/mutex/state would corrupt
    private copy() {
        throw "COPY-ERROR", "Http1ClientPollOperation cannot be copied";
    }

    #! Cancels a pending request
    /** @param stream_id the stream ID to cancel
    */
    cancelRequest(int stream_id) {
        if (!self.cancelStream(stream_id)) {
            log(LoggerLevel::DEBUG, "cancel ignored: stream %d not found (already completed)",
                stream_id);
            return;
        }
        log(LoggerLevel::DEBUG, "cancelled request: stream_id=%d", stream_id);
    }

    #! Internal: creates and configures a TCP socket for HTTP/1.1
    /** No ALPN is set — HTTP/1.1 is the default protocol.
    */
    private static Socket createAndConfigureSocket(hash<auto> url_info, *int ssl_verify_mode,
            bool accept_all_certs, *SSLCertificate client_cert, *SSLPrivateKey client_pk) {
        Socket sock();
        bool ssl_required = url_info.protocol == "https";
        if (ssl_required) {
            if (exists ssl_verify_mode) {
                sock.setSslVerifyMode(ssl_verify_mode);
            }
            if (accept_all_certs) {
                sock.acceptAllCertificates(True);
            }
            if (client_cert) {
                sock.setCertificate(client_cert);
            }
            if (client_pk) {
                sock.setPrivateKey(client_pk);
            }
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
        if (h1_logger) {
            call_function_args(\h1_logger.log(), (level, "Http1ClientPollOperation: " + fmt) + argv);
        }
    }
}

} # namespace HttpClientIo
