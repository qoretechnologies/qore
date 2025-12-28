# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file HttpAcceptPollOperation.qc Composite poll operation for HTTP accept

/** HttpAcceptPollOperation.qc Copyright 2024 Qore Technologies, s.r.o.

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

    @since HttpServerAsyncIo 1.0
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

        #! Current poll operation being executed
        SocketPollOperation spop;

        #! Current state
        string state = STATE_ACCEPTING;

        #! The accepted client socket
        Socket client_sock;
    }

    #! Creates the poll operation
    /** @param listener the server socket to accept connections from
        @param ssl if True, perform SSL handshake after accept
        @param cert SSL certificate (SSLCertificate object or PEM data, required if ssl is True)
        @param key SSL private key (SSLPrivateKey object or PEM data, required if ssl is True)
        @param ssl_verify_flags SSL verification flags for client certificate verification (e.g. SSL_VERIFY_PEER)
    */
    constructor(Socket listener, bool ssl = False, auto cert = NOTHING, auto key = NOTHING,
            int ssl_verify_flags = SSL_VERIFY_NONE) {
        @assert(listener, "listener socket cannot be null");
        @assert(!ssl || (cert && key), "SSL mode requires certificate and key");
        self.listener = listener;
        self.ssl = ssl;
        self.cert = cert;
        self.key = key;
        self.ssl_verify_flags = ssl_verify_flags;
        # Start the accept poll operation
        spop = listener.startPollAccept();
        @assert(spop, "startPollAccept must return poll operation");
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
            auto raw_info = spop.continuePoll();

            # Check what we got back
            if (raw_info) {
                # It's poll info - return it
                return cast<hash<SocketPollInfo>>(raw_info);
            }

            # Sub-operation complete (returned NOTHING) - transition to next state
            switch (state) {
                case STATE_ACCEPTING: {
                    # Accept complete - get the client socket
                    client_sock = spop.getOutput();
                    @assert(client_sock, "accept must produce client socket");
                    @assert(client_sock.isOpen(), "accepted socket must be open");
                    # Set TCP_NODELAY to ensure responses are sent immediately
                    client_sock.setNoDelay(True);

                    if (ssl) {
                        # Check if SSL handshake was already done during accept
                        # This happens when the listener socket has SSL configured -
                        # startPollAccept() automatically upgrades the accepted socket to SSL
                        if (client_sock.isSecure()) {
                            # SSL already complete - done
                            state = STATE_COMPLETE;
                            return NOTHING;
                        }

                        # Transition to SSL handshake
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
                        spop = client_sock.startPollUpgradeServerToSSL();
                        @assert(spop, "startPollUpgradeServerToSSL must return poll operation");
                        # Continue loop to poll the new operation
                        continue;
                    } else {
                        # No SSL - accept complete, return NOTHING
                        state = STATE_COMPLETE;
                        return NOTHING;
                    }
                }

                case STATE_SSL_HANDSHAKE: {
                    # SSL handshake complete - done
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
    auto getOutput() {
        return {
            "sock": client_sock,
            "ssl": ssl,
        };
    }

    #! Returns True if this is an SSL connection
    bool isSsl() {
        return ssl;
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
