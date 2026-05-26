# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file MllpListener.qc MLLP listener support

/*  MllpListener.qc Copyright 2026 Qore Technologies, s.r.o.

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
    #! Synchronous TCP/TLS MLLP listener with deterministic shutdown
    public class MllpListener {
        private {
            Socket listener();
            Mutex m();
            Counter listener_done();
            Counter active_clients();
            hash<auto> clients = {};
            hash<auto> client_threads = {};
            int next_client_id = 0;
            string bind_target = "127.0.0.1:0";
            int backlog = 5;
            int port = 0;
            bool tls = False;
            bool running = False;
            bool stopping = False;
            timeout io_timeout = DefaultTimeout;
            int max_frame_size = DefaultMaxFrameSize;
            string encoding = "UTF-8";
            *auto cert;
            *auto key;
            *code<nothing(hash<ExceptionInfo>)> error_handler;
        }

        #! Creates an MLLP listener
        /** Supported options:
            - \c backlog: listener backlog; default 5
            - \c tls: use TLS; requires \c cert and \c key
            - \c cert: server certificate for TLS listeners
            - \c key: server private key for TLS listeners
            - \c timeout: per-message socket I/O timeout; default 30 seconds
            - \c max_frame_size: maximum inbound frame size in bytes
            - \c encoding: HL7 payload encoding; default \c "UTF-8"
            - \c error_handler: code receiving \c hash<ExceptionInfo> for background errors
        */
        constructor(string bind_target = "127.0.0.1:0", *hash<auto> opts = NOTHING) {
            if (bind_target.empty()) {
                throw "MLLP-OPTION-ERROR", "MLLP listener bind target cannot be empty";
            }
            self.bind_target = bind_target;
            applyOptions(opts);
        }

        #! Stops the listener when the object is deleted
        destructor() {
            try {
                stop();
            } catch () {
            }
        }

        #! Starts the listener in a background thread
        /** @param handler code called for each received HL7 v2 message; if it
            returns a string, the string is sent as an MLLP response frame

            @throw MLLP-LISTENER-ERROR the listener is already running or TLS
            setup is incomplete
        */
        start(code<*string(string)> handler) {
            AutoLock al(m);
            if (running) {
                throw "MLLP-LISTENER-ERROR", "MLLP listener is already running";
            }
            if (tls) {
                if (!cert || !key) {
                    throw "MLLP-LISTENER-ERROR", "TLS listeners require both cert and key options";
                }
                listener.setCertificate(cert);
                listener.setPrivateKey(key);
            }
            if (listener.bind(bind_target, True)) {
                throw "MLLP-LISTENER-ERROR", sprintf("could not bind MLLP listener to %s: %s", bind_target,
                    strerror());
            }
            if (listener.listen(backlog)) {
                listener.close();
                throw "MLLP-LISTENER-ERROR", sprintf("could not listen on MLLP target %s: %s", bind_target,
                    strerror());
            }
            port = listener.getSocketInfo().port;
            stopping = False;
            running = True;
            listener_done.inc();
            background acceptLoop(handler);
        }

        #! Stops the listener and waits for listener/client threads to finish
        /** @param to maximum time to wait for shutdown
        */
        stop(timeout to = DefaultCloseTimeout) {
            bool current_thread_is_client = False;
            {
                AutoLock al(m);
                if (!running && !listener.isOpen() && clients.empty()) {
                    return;
                }
                stopping = True;
                current_thread_is_client = exists client_threads{string(gettid())};
                closeSocketIntern(listener);
                foreach Socket client in (clients.values()) {
                    closeSocketIntern(client);
                }
            }

            if (listener_done.waitForZero(to)) {
                throw "MLLP-LISTENER-ERROR", "timed out waiting for MLLP listener thread to stop";
            }
            if (!current_thread_is_client && active_clients.waitForZero(to)) {
                throw "MLLP-LISTENER-ERROR", "timed out waiting for active MLLP client handlers to stop";
            }
        }

        #! Returns True if the listener is running
        bool isRunning() {
            AutoLock al(m);
            return running;
        }

        #! Returns the bound TCP port
        int getPort() {
            AutoLock al(m);
            return port;
        }

        #! Returns True if TLS is enabled
        bool isTls() {
            return tls;
        }

        private acceptLoop(code<*string(string)> handler) {
            on_exit {
                AutoLock al(m);
                closeSocketIntern(listener);
                running = False;
                listener_done.dec();
            }

            while (!isStopping()) {
                try {
                    Socket client = tls ? listener.acceptSSL() : listener.accept();
                    int client_id = addClient(client);
                    background serveClient(client_id, client, handler);
                } catch (hash<ExceptionInfo> ex) {
                    if (!isStopping()) {
                        reportError(ex);
                    }
                    break;
                }
            }
        }

        private serveClient(int client_id, Socket client, code<*string(string)> handler) {
            markClientThread();
            on_exit {
                unmarkClientThread();
                removeClient(client_id);
                closeSocketIntern(client);
            }

            while (!isStopping()) {
                string message;
                try {
                    message = mllp_read_message(client, io_timeout, max_frame_size, encoding);
                } catch (hash<ExceptionInfo> ex) {
                    if (!isStopping() && ex.err != "SOCKET-CLOSED") {
                        reportError(ex);
                    }
                    break;
                }

                try {
                    *string response = call_function(handler, message);
                    if (response) {
                        mllp_send_message(client, response, io_timeout, encoding);
                    }
                } catch (hash<ExceptionInfo> ex) {
                    if (!isStopping()) {
                        reportError(ex);
                    }
                    break;
                }
            }
        }

        private applyOptions(*hash<auto> opts) {
            if (exists opts.backlog) {
                if (opts.backlog <= 0) {
                    throw "MLLP-OPTION-ERROR", sprintf("listener backlog must be positive; got %d", opts.backlog);
                }
                backlog = opts.backlog;
            }
            if (exists opts.tls) {
                tls = opts.tls;
            }
            if (opts.timeout) {
                io_timeout = opts.timeout;
            }
            if (exists opts.max_frame_size) {
                validateMaxFrameSize(opts.max_frame_size);
                max_frame_size = opts.max_frame_size;
            }
            if (exists opts.encoding) {
                if (opts.encoding.empty()) {
                    throw "MLLP-OPTION-ERROR", "MLLP payload encoding cannot be empty";
                }
                encoding = opts.encoding;
            }
            if (opts.cert) {
                cert = opts.cert;
            }
            if (opts.key) {
                key = opts.key;
            }
            if (opts.error_handler) {
                error_handler = opts.error_handler;
            }
        }

        private bool isStopping() {
            AutoLock al(m);
            return stopping;
        }

        private int addClient(Socket client) {
            AutoLock al(m);
            int client_id = ++next_client_id;
            clients{string(client_id)} = client;
            active_clients.inc();
            return client_id;
        }

        private removeClient(int client_id) {
            AutoLock al(m);
            string key = string(client_id);
            if (exists clients{key}) {
                remove clients{key};
                active_clients.dec();
            }
        }

        private markClientThread() {
            AutoLock al(m);
            client_threads{string(gettid())} = True;
        }

        private unmarkClientThread() {
            AutoLock al(m);
            remove client_threads{string(gettid())};
        }

        private reportError(hash<ExceptionInfo> ex) {
            if (error_handler) {
                try {
                    call_function(error_handler, ex);
                } catch () {
                }
            }
        }

        private closeSocketIntern(Socket sock) {
            if (sock.isOpen()) {
                try {
                    sock.close();
                } catch () {
                }
            }
        }
    }
}
