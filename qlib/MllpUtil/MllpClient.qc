# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file MllpClient.qc MLLP client support

/*  MllpClient.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace MllpUtil {
    #! Thread-safe MLLP client
    /** This class serializes access to the underlying socket with a Mutex, so
        concurrent callers cannot interleave frames on the connection.
    */
    public class MllpClient {
        private {
            Socket sock();
            Mutex m();
            string host = "localhost";
            int port = DefaultMllpPort;
            bool tls = False;
            timeout connect_timeout = DefaultTimeout;
            timeout io_timeout = DefaultTimeout;
            int max_frame_size = DefaultMaxFrameSize;
            string encoding = "UTF-8";
            *auto cert;
            *auto key;
        }

        #! Creates a client from an options hash
        /** Supported options:
            - \c url: \c mllp://host[:port] or \c mllps://host[:port]
            - \c host: host name or IP address; default \c "localhost"
            - \c port: TCP port; default \c 2575
            - \c tls: use TLS; implied by \c mllps://
            - \c cert: optional client certificate
            - \c key: optional client private key
            - \c connect_timeout: connect timeout; default 30 seconds
            - \c timeout: read/write timeout; default 30 seconds
            - \c max_frame_size: maximum inbound frame size in bytes
            - \c encoding: HL7 payload encoding; default \c "UTF-8"
            - \c no_connect: when True, do not connect in the constructor
        */
        constructor(*hash<auto> opts) {
            if (opts.url) {
                parseUrl(opts.url);
            }
            applyOptions(opts);
            if (!opts.no_connect) {
                connect();
            }
        }

        #! Creates a client from a URL
        /** @param url \c mllp://host[:port] or \c mllps://host[:port]
            @param do_connect if True, connect in the constructor
            @param opts optional client options
        */
        constructor(string url, bool do_connect = True, *hash<auto> opts = NOTHING) {
            parseUrl(url);
            applyOptions(opts);
            if (do_connect) {
                connect();
            }
        }

        #! Creates a client from a host and port
        /** @param host server host
            @param port server port
            @param opts optional client options
        */
        constructor(string host, int port, *hash<auto> opts = NOTHING) {
            setHostAndPort(host, port);
            applyOptions(opts);
            if (!opts.no_connect) {
                connect();
            }
        }

        #! Disconnects from the server when the object is deleted
        destructor() {
            disconnect();
        }

        #! Connects to the configured endpoint
        /** @throw MLLP-CONNECT-ERROR the client is already connected or connection setup fails
        */
        connect() {
            AutoLock al(m);
            if (sock.isOpen()) {
                throw "MLLP-CONNECT-ERROR", "MLLP client is already connected";
            }

            string target = sprintf("%s:%d", host, port);
            try {
                if (cert) {
                    sock.setCertificate(cert);
                }
                if (key) {
                    sock.setPrivateKey(key);
                }
                if (tls) {
                    sock.connectSSL(target, connect_timeout);
                } else {
                    sock.connect(target, connect_timeout);
                }
            } catch (hash<ExceptionInfo> ex) {
                throw "MLLP-CONNECT-ERROR", sprintf("could not connect to MLLP endpoint %s:%d: %s", host, port,
                    ex.desc);
            }
        }

        #! Disconnects from the endpoint if connected
        disconnect() {
            AutoLock al(m);
            disconnectIntern();
        }

        #! Returns True if the client socket is open
        bool isConnected() {
            AutoLock al(m);
            return sock.isOpen();
        }

        #! Sends one HL7 v2 message as an MLLP frame
        sendMessage(string message, *timeout to = NOTHING) {
            AutoLock al(m);
            checkConnected();
            mllp_send_message(sock, message, to ?? io_timeout, encoding);
        }

        #! Reads one HL7 v2 message from the server
        string receiveMessage(*timeout to = NOTHING) {
            AutoLock al(m);
            checkConnected();
            return mllp_read_message(sock, to ?? io_timeout, max_frame_size, encoding);
        }

        #! Sends one message and returns the raw response message
        string sendAndReceiveMessage(string message, *timeout to = NOTHING) {
            AutoLock al(m);
            checkConnected();
            timeout effective_timeout = to ?? io_timeout;
            mllp_send_message(sock, message, effective_timeout, encoding);
            return mllp_read_message(sock, effective_timeout, max_frame_size, encoding);
        }

        #! Sends one message and returns parsed ACK metadata
        hash<MllpAckInfo> sendAndReceiveAck(string message, *timeout to = NOTHING) {
            return mllp_parse_ack(sendAndReceiveMessage(message, to));
        }

        #! Returns the configured host
        string getHost() {
            return host;
        }

        #! Returns the configured port
        int getPort() {
            return port;
        }

        #! Returns True if TLS is enabled
        bool isTls() {
            return tls;
        }

        #! Returns the configured payload encoding
        string getEncoding() {
            return encoding;
        }

        private parseUrl(string url) {
            hash<UrlInfo> url_info = parse_url(url);
            if (url_info.protocol) {
                string protocol = url_info.protocol.lwr();
                if (protocol == "mllps") {
                    tls = True;
                } else if (protocol == "mllp") {
                    tls = False;
                } else {
                    throw "MLLP-URL-ERROR", sprintf("invalid MLLP URL protocol %y; expected \"mllp\" or \"mllps\"",
                        url_info.protocol);
                }
            }

            foreach string key in (("username", "password", "path", "query")) {
                if (url_info{key}) {
                    throw "MLLP-URL-ERROR", sprintf("MLLP URL component %s is not supported", key);
                }
            }

            if (!url_info.host) {
                throw "MLLP-URL-ERROR", "MLLP URL must include a host";
            }
            host = url_info.host;
            port = url_info.port ?? DefaultMllpPort;
            validatePort(port);
        }

        private applyOptions(*hash<auto> opts) {
            if (exists opts.host) {
                validateHost(opts.host);
                host = opts.host;
            }
            if (exists opts.port) {
                validatePort(opts.port);
                port = opts.port;
            }
            if (exists opts.tls) {
                tls = opts.tls;
            }
            if (opts.connect_timeout) {
                connect_timeout = opts.connect_timeout;
            }
            if (opts.timeout) {
                io_timeout = opts.timeout;
            }
            if (exists opts.max_frame_size) {
                validateMaxFrameSize(opts.max_frame_size);
                max_frame_size = opts.max_frame_size;
            }
            if (exists opts.encoding) {
                validateEncoding(opts.encoding);
                encoding = opts.encoding;
            }
            if (opts.cert) {
                cert = opts.cert;
            }
            if (opts.key) {
                key = opts.key;
            }
            validateHost(host);
        }

        private setHostAndPort(string host, int port) {
            validateHost(host);
            validatePort(port);
            self.host = host;
            self.port = port;
        }

        private checkConnected() {
            if (!sock.isOpen()) {
                throw "MLLP-NOT-CONNECTED", "MLLP client is not connected";
            }
        }

        private disconnectIntern() {
            if (sock.isOpen()) {
                try {
                    sock.close();
                } catch () {
                }
            }
        }

        private validateHost(string host) {
            if (host.empty()) {
                throw "MLLP-OPTION-ERROR", "MLLP host cannot be empty";
            }
        }

        private validateEncoding(string encoding) {
            if (encoding.empty()) {
                throw "MLLP-OPTION-ERROR", "MLLP payload encoding cannot be empty";
            }
        }

        private validatePort(int port) {
            if (port <= 0 || port > 65535) {
                throw "MLLP-OPTION-ERROR", sprintf("invalid MLLP port: %d", port);
            }
        }
    }
}
