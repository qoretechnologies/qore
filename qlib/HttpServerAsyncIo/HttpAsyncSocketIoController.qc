# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file HttpAsyncSocketIoController.qc HTTP-aware async I/O controller

/** HttpAsyncSocketIoController.qc Copyright 2024 - 2026 Qore Technologies, s.r.o.

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

#! HTTP connection state
public enum HttpConnectionState : string {
    #! Idle keep-alive connection
    Idle = "idle",
    #! Combined idle + header read (HTTP/1.x keep-alive optimization)
    KeepAlive = "keep_alive",
    #! Reading HTTP headers
    ReadHeader = "read_header",
    #! SSL/TLS handshake in progress
    SslHandshake = "ssl_handshake",
    #! Server-Sent Events streaming
    Sse = "sse",
    #! WebSocket bidirectional communication
    WebSocket = "websocket",
    #! HTTP/2 multiplexed connection
    Http2 = "http2",
    #! HTTP/2 sending response (async send via I/O thread)
    Http2Sending = "http2_sending",
    #! HTTP/2 WebSocket (RFC 8441)
    Http2WebSocket = "http2_websocket",
    #! Fused send response + idle wait + read next header (HTTP/1.x keep-alive optimization)
    SendAndReadHeader = "send_and_read_header",
    #! HTTP/3 multiplexed connection over QUIC
    Http3 = "http3",
    #! HTTP/3 sending response
    Http3Sending = "http3_sending",
    #! Sending HTTP/1.x InputStream response + idle wait + read next header
    SendStreamAndReadHeader = "send_stream_and_read_header",
    #! Sending HTTP/1.x InputStream response (no keep-alive, close after send)
    SendStreamResponse = "send_stream_response",
    #! Reading HTTP/1.x request body (Content-Length or chunked)
    ReadBody = "read_body",
}

#! HTTP poll operation type
public enum HttpPollType : string {
    #! Accept new connection
    Accept = "accept",
    #! SSL/TLS handshake
    Ssl = "ssl",
    #! Idle keep-alive monitoring
    Idle = "idle",
    #! Combined idle + header read (HTTP/1.x keep-alive)
    KeepAlive = "keep_alive",
    #! HTTP header reading
    Header = "header",
    #! Server-Sent Events
    Sse = "sse",
    #! WebSocket
    WebSocket = "websocket",
    #! HTTP/2
    Http2 = "http2",
    #! HTTP/2 WebSocket
    Http2WebSocket = "http2_websocket",
    #! HTTP/3
    Http3 = "http3",
}

#! HTTP version mode
public enum HttpVersionMode : string {
    #! Auto-negotiate (default)
    Auto = "auto",
    #! HTTP/1.0
    Http10 = "1.0",
    #! HTTP/1.1
    Http11 = "1.1",
    #! HTTP/2.0
    Http20 = "2.0",
    #! HTTP/2 cleartext
    H2c = "h2c",
    #! HTTP/3 over QUIC (UDP)
    H3 = "h3",
}

#! HTTP-aware async I/O controller for managing HTTP connections
/** This controller manages HTTP connections through their lifecycle:

    1. Accept new connections (with optional SSL)
    2. Read HTTP headers
    3. Dispatch to target callback
    4. Manage idle keep-alive connections
    5. Handle streaming protocols (SSE, WebSocket)

    All I/O polling is delegated to an @ref AsyncSocketIo::AsyncSocketIoController
    "AsyncSocketIoController" instance, using callback-based result delivery for
    efficient state machine transitions without a separate I/O thread.

    Multiple HttpServer instances can share a single controller for efficient
    resource usage across many listeners.

    The controller uses a callback pattern to notify the target about HTTP events.
    Implementations of @ref HttpAsyncTarget handle the actual request processing.

*/
public class HttpAsyncSocketIoController {
    public {
        #! Maximum concurrent accept operations per listener
        /** Multiple concurrent accept poll operations are supported on the
            same listener socket, reducing connection backlog during high
            connection rates.
        */
        const DefaultAcceptOps = 2;

        #! Timeout for synchronous poll loops (e.g., HTTP/2 response send on handler thread)
        const SyncPollTimeout = 30s;
    }

    private {
        #! Lock for atomic operations — RWLock allows concurrent read-only access
        RWLock m();

        #! Logger
        *LoggerInterface logger;

        #! Async I/O controller for all operations
        AsyncSocketIoController controller;

        #! Thread pool for request handler dispatch
        ThreadPool handler_pool;

        #! Handoff queue for handler-pool tasks
        Queue handler_task_queue();

        #! Thread pool configuration for recreation after stop()
        hash<auto> pool_opts;

        #! Shutdown flag; prevents handler tasks from submitting new operations during stop()
        bool stopping = False;

        #! True after stop() has been called; prevents new listeners from being added
        bool has_been_stopped = False;

        #! True if the handler pool has been stopped and needs recreation in start()
        bool pool_stopped = False;

        #! Registered listeners: listener unique hash -> listener info
        hash<string, hash<HttpListenerInfo>> listeners;

        #! QUIC HttpListener wrapper unique hash -> UDP listener unique hash
        hash<string, string> quic_listener_hash_by_http_listener;

        #! Active connections: socket unique hash -> connection info
        hash<string, hash<HttpActiveConnectionInfo>> connections;

        #! Listener accept operation counters
        hash<string, int> accept_counts;
        int accept_seq = 0;

        #! Queues for persistent HTTP connections keyed by socket/session unique hash
        /** When a handler opts into persistence for a connection, the handler
            pool thread enters a dedicated loop and registers a Queue here. Subsequent
            requests on the same connection are routed to the Queue instead of dispatching
            to a new pool thread, ensuring thread affinity for thread-local resources
            (e.g., database transactions).

        */
        Mutex persistent_queue_mutex();
        #! Persistent queues keyed by connection unique hash
        hash<string, hash<auto>> persistent_queues;
    }

    #! Creates the HTTP controller with an explicit I/O controller
    /** @param controller the shared async I/O controller to delegate polling to
        @param logger optional logger interface for debugging
        @param opts optional configuration options:
        - \c max_handler_threads: maximum number of handler threads (default 0 = unlimited)
        - \c min_idle_handler_threads: minimum idle handler threads (default 0)
        - \c max_idle_handler_threads: maximum idle handler threads (default 0)
    */
    constructor(AsyncSocketIoController controller, *LoggerInterface logger,
            hash<auto> opts = {}) {
        @assert(controller, "controller cannot be null");
        self.controller = controller;
        self.logger = logger;
        self.pool_opts = opts;
        self.handler_pool = new ThreadPool(
            opts.max_handler_threads ?? 0,
            opts.min_idle_handler_threads ?? 0,
            opts.max_idle_handler_threads ?? 0,
        );
    }

    #! Updates the logger used for HTTP-level async I/O diagnostics
    /** Call this when the application reconfigures its logging (e.g. after reading
        logger parameters from a database) so that ERROR messages from the HTTP/2
        handler pool are not silently dropped.

        @param new_logger the new logger to use (NOTHING disables logging)

        @since Qore 3.0.0
    */
    setLogger(*LoggerInterface new_logger) {
        logger = new_logger;
    }

    #! Destructor stops the controller
    destructor() {
        stop();
    }

    #! Starts the I/O controller
    start() {
        {
            AutoWriteLock al(m);
            has_been_stopped = False;
            # Recreate handler pool if it was stopped (stop() doesn't recreate to avoid
            # leaving worker threads running that would block process exit)
            if (pool_stopped) {
                handler_pool = new ThreadPool(
                    pool_opts.max_handler_threads ?? 0,
                    pool_opts.min_idle_handler_threads ?? 0,
                    pool_opts.max_idle_handler_threads ?? 0,
                );
                pool_stopped = False;
            }
        }
        if (!controller.running()) {
            controller.start();
        }
        @debug {
            if (getenv("QORE_HTTP_ACCEPT_TRACE")) {
                log(LoggerLevel::INFO, "HttpAsyncSocketIoController started (%d I/O threads)",
                    1);
            }
        }
    }

    #! Stops the HTTP controller
    /** Cancels all HTTP operations by their owners.
        Does NOT stop the shared controller — other users may still be using it.
        If the controller was created internally, it is stopped.
    */
    #! Pushes a \c close sentinel to every registered persistent-mux queue
    /** Called from both @ref stop() and @ref HttpServer::stopNoWait() so that
        dedicated persistent-handler threads blocked in @ref Queue::get() on the
        queue's idle TTL (default 75 s) exit promptly on shutdown.

        Without this, a \c stopNoWait() shutdown path (the one Qorus uses) would
        wait up to the full TTL for each dedicated handler before
        @ref HttpListener::stop()'s connection-thread WaitGroup drains — making
        server shutdown visibly sluggish even though all I/O is async.

        Safe to call multiple times.
    */
    signalPersistentQueuesToClose() {
        persistent_queue_mutex.lock();
        on_exit persistent_queue_mutex.unlock();
        foreach string key in (keys persistent_queues) {
            persistent_queues{key}.queue.push({"type": "close"});
        }
    }

    private logShutdownException(string action, hash<ExceptionInfo> ex) {
        try {
            log(LoggerLevel::ERROR, "Async HTTP shutdown %s failed: %s: %s", action, ex.err, ex.desc);
        } catch () {
        }
    }

    private dispatchOnError(*HttpAsyncTarget target, *Socket sock, hash<ExceptionInfo> ex,
            *HttpServer::HttpListenerInterface listener, *hash<auto> cx, *string uh, string phase) {
        string id = uh ?? "<unknown>";
        if (!exists target || !exists sock) {
            log(LoggerLevel::DEBUG, "Skipping onError callback for %s during %s: target=%y socket=%y (%s: %s)",
                id, phase, exists target, exists sock, ex.err, ex.desc);
            return;
        }
        try {
            call_object_method(target, "onError", sock, ex, listener, cx);
        } catch () {
            log(LoggerLevel::ERROR, "onError callback threw for %s during %s", id, phase);
        }
    }

    private bool wakeSocketIfPresent(*AbstractPollableIoObjectBase sock, *string uh, string phase) {
        string id = uh ?? "<unknown>";
        if (!exists sock) {
            log(LoggerLevel::DEBUG, "Skipping socket wake for %s during %s: no socket", id, phase);
            return False;
        }
        try {
            controller.wakeSocket(sock);
            return True;
        } catch () {
            log(LoggerLevel::DEBUG, "Socket wake failed for %s during %s", id, phase);
            return False;
        }
    }

    stop() {
        try {
            stopIntern();
        } catch (hash<ExceptionInfo> ex) {
            logShutdownException("stopping controller", ex);
        }
        # Reset stopping flag so the controller can be restarted
        try {
            AutoWriteLock al(m);
            stopping = False;
        } catch (hash<ExceptionInfo> ex) {
            logShutdownException("resetting shutdown flag", ex);
        }
        @debug {
            if (getenv("QORE_HTTP_ACCEPT_TRACE")) {
                log(LoggerLevel::INFO, "HttpAsyncSocketIoController stopped");
            }
        }
    }

    private stopIntern() {
        # Set stopping flag first — prevents handler tasks from submitting new operations
        # Copy keys under lock, then cancel without holding the lock
        # to avoid deadlock with controller callbacks that also acquire m
        *list<string> listener_keys;
        *list<Socket> conn_sockets;
        *list<string> conn_keys;
        *hash<string, hash<HttpListenerInfo>> detached_listeners;
        *hash<string, hash<HttpActiveConnectionInfo>> detached_connections;
        *hash<string, hash<HttpActiveConnectionInfo>> race_connections;
        # Snapshot stream_ops separately so we can call abort() on H2/H3 stream
        # poll operations whose handler-thread closure still holds a reference
        # (the controller's cancelByOwner / cancelAndClose below cancels only
        # ops that are currently in the controller cache; an op that has been
        # Phase3-REMOVE'd after a headers-only dispatch is not in the cache,
        # so without an explicit abort() its clearStreamQueues() never runs
        # until the handler closure releases its ref — which happens only
        # after the handler's body_queue.get() times out 30s later, blocking
        # handler_pool.stopWait() for that full duration).
        *list<AbstractPollOperation> stream_ops_to_abort;
        {
            AutoWriteLock al(m);
            # Guard against multiple stop() calls (e.g., explicit stop + destructor)
            if (pool_stopped) {
                return;
            }
            stopping = True;
            has_been_stopped = True;
            listener_keys = keys listeners;
            if (connections) {
                conn_keys = keys connections;
                conn_sockets = ();
                stream_ops_to_abort = ();
                foreach string uh in (conn_keys) {
                    hash<HttpActiveConnectionInfo> cinfo =
                        cast<hash<HttpActiveConnectionInfo>>(connections{uh});
                    if (cinfo.state != HttpConnectionState::Http3
                            && cinfo.state != HttpConnectionState::Http3Sending) {
                        push conn_sockets, cinfo.sock;
                    }
                    if (cinfo.stream_op) {
                        push stream_ops_to_abort, cinfo.stream_op;
                    }
                }
            }
            detached_listeners = listeners;
            detached_connections = connections;
            listeners = {};
            quic_listener_hash_by_http_listener = {};
            connections = {};
            accept_counts = {};
        }
        # Abort each stream_op so its clearStreamQueues() fires immediately,
        # pushing NOTHING sentinels onto any registered body_queue and
        # unblocking handler threads stuck in body_queue.get(timeout).
        # Wrapped in try/catch because abort() on an already-aborted op
        # raises HTTP2-STATE-ERROR / HTTP3-STATE-ERROR.
        if (stream_ops_to_abort) {
            foreach AbstractPollOperation sop in (stream_ops_to_abort) {
                try {
                    sop.abort();
                } catch (hash<ExceptionInfo> ex) {
                    logShutdownException("aborting stream operation", ex);
                }
            }
        }
        # Signal persistent queues to exit their dedicated loops
        try {
            signalPersistentQueuesToClose();
        } catch (hash<ExceptionInfo> ex) {
            logShutdownException("signalling persistent queues", ex);
        }
        foreach string lhash in (listener_keys) {
            try {
                controller.cancelByOwner(lhash);
            } catch (hash<ExceptionInfo> ex) {
                logShutdownException("cancelling listener owner " + lhash, ex);
            }
        }
        # Cancel connection operations by their owner key (= connection unique hash).
        # For QUIC/HTTP/3 connections the cache key and owner are the connection
        # unique hash (e.g., "HASH:quic:SESSION_ID"), NOT the raw socket hash, so
        # controller.cancel(sock) would miss them.  cancelByOwner(uh) matches the
        # owner field set by submitConnectionOpIntern().
        foreach string uh in (conn_keys) {
            try {
                controller.cancelByOwner(uh);
            } catch (hash<ExceptionInfo> ex) {
                logShutdownException("cancelling connection owner " + uh, ex);
            }
        }
        # Also cancel and close by socket for non-QUIC connections
        # (HTTP/1.x, HTTP/2) where the cache key is the socket unique hash.
        # HTTP/3 is intentionally skipped here because QUIC sessions share the
        # listener UDP socket; closing one logical connection would break the
        # other sessions on the listener.
        foreach Socket sock in (conn_sockets) {
            try {
                controller.cancelAndClose(sock);
            } catch (hash<ExceptionInfo> ex) {
                logShutdownException("cancelling and closing connection socket " + sock.uniqueHash(), ex);
            }
        }
        # Wait for all pending onComplete/abort callbacks to be processed before
        # stopping the handler pool. Cancelled WebSocket operations have their
        # onComplete dispatched asynchronously via QoreCallDispatcher. The handler
        # threads are blocked on done.waitForZero() until onComplete signals them.
        # Without this flush, handler_pool.stopWait() deadlocks.
        try {
            controller.flushCallbacks();
        } catch (hash<ExceptionInfo> ex) {
            logShutdownException("flushing cancellation callbacks", ex);
        }

        # Wait for in-flight handler tasks to complete; the stopping flag prevents
        # them from submitting new operations to the controller
        try {
            handler_pool.stopWait();
            pool_stopped = True;  # Mark for recreation in start()
        } catch (hash<ExceptionInfo> ex) {
            logShutdownException("stopping handler pool", ex);
        }
        # NOTE: We don't recreate the pool here anymore. The pool will be recreated
        # in start() if the controller is restarted. Recreating here would leave
        # a worker thread running that blocks process exit.
        if (detached_connections) {
            try {
                detached_connections = NOTHING;
            } catch (hash<ExceptionInfo> ex) {
                logShutdownException("destroying detached connections", ex);
            }
        }
        if (detached_listeners) {
            try {
                detached_listeners = NOTHING;
            } catch (hash<ExceptionInfo> ex) {
                logShutdownException("destroying detached listeners", ex);
            }
        }
        # Second pass: cancel any operations that handler tasks managed to submit
        # before observing the stopping flag (narrow race window)
        {
            AutoWriteLock al(m);
            if (connections) {
                conn_keys = keys connections;
                conn_sockets = ();
                foreach string uh in (conn_keys) {
                    hash<HttpActiveConnectionInfo> cinfo =
                        cast<hash<HttpActiveConnectionInfo>>(connections{uh});
                    if (cinfo.state != HttpConnectionState::Http3
                            && cinfo.state != HttpConnectionState::Http3Sending) {
                        push conn_sockets, cinfo.sock;
                    }
                }
                race_connections = connections;
                connections = {};
            } else {
                conn_keys = NOTHING;
                conn_sockets = NOTHING;
            }
        }
        foreach string uh in (conn_keys) {
            try {
                controller.cancelByOwner(uh);
            } catch (hash<ExceptionInfo> ex) {
                logShutdownException("cancelling late connection owner " + uh, ex);
            }
        }
        foreach Socket sock in (conn_sockets) {
            try {
                controller.cancelAndClose(sock);
            } catch (hash<ExceptionInfo> ex) {
                logShutdownException("cancelling and closing late connection socket " + sock.uniqueHash(), ex);
            }
        }
        try {
            controller.flushCallbacks();
        } catch (hash<ExceptionInfo> ex) {
            logShutdownException("flushing late cancellation callbacks", ex);
        }
        if (race_connections) {
            try {
                race_connections = NOTHING;
            } catch (hash<ExceptionInfo> ex) {
                logShutdownException("destroying late connections", ex);
            }
        }
    }

    #! Returns True if the controller is running
    bool running() {
        return !has_been_stopped && controller.running();
    }

    #! Waits for all I/O threads to stop
    waitStop() {
        controller.waitStop();
    }

    #! Submits a user-code body to the handler thread pool, wrapped in the target's
    #! per-dispatch context save/restore hooks
    /** All controller dispatches that may run user code (regular request handling,
        SSE disconnect notification, WebSocket close notification, WebSocket frame
        dispatch) MUST go through this method rather than calling
        @ref handler_pool.submit() directly.  This guarantees that
        @ref HttpAsyncTarget::saveDispatchContext() runs immediately before the
        user-code body and that @ref HttpAsyncTarget::restoreDispatchContext()
        runs immediately after, on the same handler-pool thread, so that
        thread reuse cannot leak per-dispatch state from one callback into
        the next.

        The default \c saveDispatchContext()/\c restoreDispatchContext() are
        no-ops; subclasses such as the HttpServer integration override them to
        capture and restore framework-specific thread-local data.

        The ThreadPool task itself is a no-argument method reference; the
        target/body pair is handed off through \c handler_task_queue.  This
        avoids submitting a wrapper closure that captures request-local values
        and can lose delayed dispatch state under heavy async scheduling.

        Exceptions raised by \a body are not caught here; callers may add their
        own try/catch inside \a body for dispatch-specific error handling.

        @param target the async target whose dispatch hooks should wrap the call
        @param body the user-code closure to run on a handler-pool thread
        @throw any exception raised by ThreadPool::submit() (e.g. when stopping)
    */
    submitToHandlerPool(HttpAsyncTarget target, code<nothing()> body) {
        handler_pool.submit(\runHandlerPoolTask());
        handler_task_queue.push({"target": target, "body": body});
    }

    private runHandlerPoolTask() {
        hash<auto> task;
        try {
            task = handler_task_queue.get(10s);
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::ERROR, "handler-pool task handoff failed: %s: %s",
                ex.err, ex.desc);
            return;
        }

        *HttpAsyncTarget target = task.target;
        *code<nothing()> body = task.body;
        if (!exists target || !exists body) {
            log(LoggerLevel::ERROR, "handler-pool task missing target/body: target=%y body=%y",
                exists target, exists body);
            return;
        }

        auto save = target.saveDispatchContext();
        on_exit target.restoreDispatchContext(save);
        body();
    }

    #! Registers a persistent queue for a connection
    /** Called by a handler pool thread when entering dedicated persistent mode.
        Subsequent requests on the same connection will be routed to the Queue
        instead of dispatching to a new pool thread.

        @param key the socket unique hash identifying the connection
        @param q the Queue to receive subsequent request hashes

    */
    registerPersistentQueue(string key, Queue q, *code<bool(hash<auto>)> filter) {
        persistent_queue_mutex.lock();
        on_exit persistent_queue_mutex.unlock();
        persistent_queues{key} = {"queue": q, "filter": filter};
    }

    #! Deregisters a persistent queue for a connection
    /** Called when the dedicated handler pool thread exits its persistent loop.

        @param key the socket unique hash identifying the connection

    */
    deregisterPersistentQueue(string key) {
        persistent_queue_mutex.lock();
        on_exit persistent_queue_mutex.unlock();
        remove persistent_queues{key};
    }

    #! Routes a request to a persistent queue if one is still registered
    /** @param key the socket unique hash identifying the connection
        @param hdr the request headers to test against the filter (if any)
        @param entry the queue entry to push if routing succeeds
        @param before_push optional internal callback run while routing is
            committed but before the entry is pushed
        @return True if the request was pushed to the dedicated thread, False otherwise
    */
    bool pushPersistentQueue(string key, *hash<auto> hdr, hash<auto> entry,
            *code<nothing()> before_push) {
        # Copy references under the lock, then release before invoking the filter
        # to avoid holding the mutex during a user-provided callback
        *code<bool(hash<auto>)> filter;
        *Queue queue;
        {
            persistent_queue_mutex.lock();
            on_exit persistent_queue_mutex.unlock();
            if (!persistent_queues{key}) {
                return False;
            }
            filter = persistent_queues{key}.filter;
            queue = persistent_queues{key}.queue;
        }
        # Invoke the filter outside the lock
        if (filter && !filter(hdr)) {
            return False;
        }

        persistent_queue_mutex.lock();
        on_exit persistent_queue_mutex.unlock();
        if (!persistent_queues{key} || persistent_queues{key}.queue !== queue) {
            return False;
        }
        if (before_push) {
            before_push();
        }
        queue.push(entry);
        return True;
    }

    private hash<auto> addPersistentQueueClosures(hash<auto> header_info) {
        header_info.register_persistent_queue = sub (string key, Queue q,
                *code<bool(hash<auto>)> filter) {
            self.registerPersistentQueue(key, q, filter);
        };
        header_info.deregister_persistent_queue = sub (string key) {
            self.deregisterPersistentQueue(key);
        };
        return header_info;
    }

    private bool routeRequestToPersistentQueue(string uh, hash<HttpConnectionInfo> conn_info) {
        if (!pushPersistentQueue(uh, conn_info.hdr, {
            "type": "request",
            "conn_info": conn_info,
        }, sub () {
            log(LoggerLevel::DEBUG, "HTTP/1.x request routed to persistent queue: %y %s %s",
                uh, conn_info.hdr.method, conn_info.hdr.path);
            AutoWriteLock al(m);
            remove connections{uh};
        })) {
            return False;
        }
        return True;
    }

    #! Signals persistent queues for a closed connection to exit
    /** Called when a connection closes so that dedicated handler pool
        threads blocked on Queue::get() are unblocked and can exit cleanly.

        Also callable from outside the framework (e.g. from
        @ref HttpServer::HttpListener::signalPersistentSessionClose())
        when a cooperative-abort path needs to terminate a specific
        persistent session immediately rather than wait for socket close
        or idle TTL.  Safe to call from any thread and idempotent: a
        no-op if no queue is registered under @c uh (e.g. the dedicated
        thread has already exited).

        @param uh the connection unique hash
        @param close_ack optional completion queue; when provided, the dedicated
            handler thread pushes a value only after its persistent-close
            callback and thread-local cleanup have completed

        @return @ref True when a close entry was queued, @ref False when no
            persistent queue was registered for @p uh
    */
    bool signalPersistentQueuesForConnection(string uh, *Queue close_ack) {
        persistent_queue_mutex.lock();
        on_exit persistent_queue_mutex.unlock();
        if (persistent_queues{uh}) {
            log(LoggerLevel::DEBUG, "signalling persistent queue for closed connection: %y", uh);
            persistent_queues{uh}.queue.push({
                "type": "close",
                "close_ack": close_ack,
            });
            return True;
        }
        return False;
    }

    #! Signals all persistent queues whose keys start with the given prefix
    /** HTTP/3 uses one shared UDP listener socket and one poll operation for
        multiple QUIC sessions.  The controller connection key is therefore
        listener-scoped, while persistent handler queues remain session-scoped
        (\c listener:quic:session).  When the shared H3 operation closes, every
        persistent queue under that listener has to be released.

        @param prefix the persistent queue key prefix to match
    */
    private signalPersistentQueuesForPrefix(string prefix) {
        persistent_queue_mutex.lock();
        on_exit persistent_queue_mutex.unlock();
        foreach string key in (keys persistent_queues) {
            if (key.startsWith(prefix)) {
                log(LoggerLevel::DEBUG, "signalling persistent queue for closed connection: %y", key);
                persistent_queues{key}.queue.push({"type": "close"});
            }
        }
    }

    #! Adds an HTTP listener to be managed
    /** @param listener the listener socket
        @param target the target to receive HTTP events
        @param ssl True if this is an HTTPS listener
        @param cert SSL certificate (SSLCertificate object or PEM/DER data, required if ssl)
        @param key SSL private key (SSLPrivateKey object or PEM/DER data, required if ssl)
        @param http_version HTTP version mode: "auto" (default), "1.0", "1.1", or "2.0"
        @param ssl_verify_flags SSL verify flags for client certificate verification
        @param ssl_accept_all_certs if True, accept all client certificates including self-signed
        @param get_remote_certs if True, capture remote (client) certificates
        @param max_request_body_size maximum request body size in bytes (0 = unlimited)
        @param http_listener optional HttpListenerInterface for handler dispatch and logging;
        when provided, this is passed to callbacks instead of the raw socket
    */
    addListener(Socket listener, HttpAsyncTarget target, bool ssl = False, auto cert = NOTHING,
            auto key = NOTHING, string http_version = HttpVersionMode::Auto,
            int ssl_verify_flags = SSL_VERIFY_NONE, bool ssl_accept_all_certs = True,
            bool get_remote_certs = False, int max_request_body_size = 0,
            *HttpServer::HttpListenerInterface http_listener) {
        @assert(listener, "listener socket cannot be null");
        @assert(target, "target cannot be null");
        @assert(!ssl || (cert && key), "SSL mode requires certificate and key");
        string lhash = listener.uniqueHash();

        {
            AutoWriteLock al(m);
            if (has_been_stopped) {
                throw "CONTROLLER-ADD-LISTENER-ERROR",
                    sprintf("cannot add listener %s: controller has been stopped", lhash);
            }
            @assert(!listeners{lhash}, "listener already registered: %s", lhash);
            listeners{lhash} = <HttpListenerInfo>{
                "listener": listener,
                "target": target,
                "ssl": ssl,
                "cert": cert,
                "key": key,
                "http_version": http_version,
                "ssl_verify_flags": ssl_verify_flags,
                "ssl_accept_all_certs": ssl_accept_all_certs,
                "get_remote_certs": get_remote_certs,
                "max_request_body_size": max_request_body_size,
                "http_listener": http_listener,
            };
        }

        # Submit N concurrent accept operations
        for (int i = 0; i < DefaultAcceptOps; ++i) {
            submitAcceptOp(listener, target, ssl, cert, key, http_version, lhash);
        }

        @debug(log(LoggerLevel::DEBUG, "addListener: registered listener %s with %d accept ops",
            lhash, DefaultAcceptOps));
        log(LoggerLevel::INFO, "Added listener %y (SSL: %y, HTTP version: %y)", lhash, ssl, http_version);
    }

    #! Adds a QUIC/HTTP/3 listener (UDP) to be managed
    /** @param listener the UDP listener socket (already bound via bindINET with SOCK_DGRAM)
        @param target the target to receive HTTP events
        @param cert SSL certificate for QUIC TLS handshake
        @param key SSL private key for QUIC TLS handshake

        @note QUIC uses UDP and handles TLS 1.3 as part of the QUIC handshake.
        The socket must already be bound to a UDP port before calling this method.
    */
    addQuicListener(Socket listener, HttpAsyncTarget target, auto cert, auto key,
            HttpServer::HttpListenerInterface http_listener,
            int ssl_verify_flags = SSL_VERIFY_NONE, bool ssl_accept_all_certs = True,
            bool get_remote_certs = False) {
        @assert(listener, "listener socket cannot be null");
        @assert(target, "target cannot be null");
        @assert(cert && key, "QUIC requires certificate and key for TLS 1.3");
        string lhash = listener.uniqueHash();

        # Set up SSL certificate and key BEFORE registering the listener,
        # so a failure here doesn't leave a stale entry in the map
        if (cert instanceof SSLCertificate) {
            listener.setCertificate(cert);
        }
        if (key instanceof SSLPrivateKey) {
            listener.setPrivateKey(key);
        }

        # Configure SSL verification mode for mTLS (mutual TLS) if requested
        if (get_remote_certs) {
            ssl_verify_flags |= SSL_VERIFY_PEER;
        }
        if (ssl_verify_flags != SSL_VERIFY_NONE) {
            listener.setSslVerifyMode(ssl_verify_flags);
        }
        listener.acceptAllCertificates(ssl_accept_all_certs);
        hash<auto> socket_info = listener.getSocketInfo(False);

        {
            AutoWriteLock al(m);
            if (has_been_stopped) {
                throw "CONTROLLER-ADD-LISTENER-ERROR",
                    sprintf("cannot add QUIC listener %s: controller has been stopped", lhash);
            }
            @assert(!listeners{lhash}, "listener already registered: %s", lhash);
            listeners{lhash} = <HttpListenerInfo>{
                "listener": listener,
                "target": target,
                "ssl": True,
                "cert": cert,
                "key": key,
                "http_version": HttpVersionMode::H3,
                "ssl_accept_all_certs": ssl_accept_all_certs,
                "ssl_verify_flags": ssl_verify_flags,
                "get_remote_certs": get_remote_certs,
                "http_listener": http_listener,
                "socket_info": socket_info,
            };
            quic_listener_hash_by_http_listener{http_listener.uniqueHash()} = lhash;
        }
        # Remove listener from map if submitQuicAcceptOp fails
        on_error {
            AutoWriteLock al(m);
            remove listeners{lhash};
            remove quic_listener_hash_by_http_listener{http_listener.uniqueHash()};
        }

        # Submit QUIC accept operation
        submitQuicAcceptOp(listener, target, lhash);

        log(LoggerLevel::INFO, "Added QUIC/HTTP/3 listener %y", lhash);
    }

    #! Removes a listener
    /** @param listener the listener to remove
    */
    removeListener(Socket listener) {
        string listener_hash = listener.uniqueHash();
        string lhash = listener_hash;
        string conn_listener_hash = listener_hash;
        *hash<HttpListenerInfo> linfo;
        {
            AutoReadLock al(m);
            linfo = listeners{lhash};
            if (!linfo && quic_listener_hash_by_http_listener.hasKey(listener_hash)) {
                lhash = quic_listener_hash_by_http_listener{listener_hash};
                linfo = listeners{lhash};
            }
            if (linfo && linfo.http_listener) {
                conn_listener_hash = linfo.http_listener.uniqueHash();
            }
        }

        @debug(log(LoggerLevel::DEBUG, "removeListener: removing listener %s", lhash));
        # Cancel accept operations (owned by the listener hash)
        controller.cancelByOwner(lhash);

        # Cancel all connection operations on this listener
        # Connection ops use the connection unique hash (uh) as owner,
        # not the listener hash, so cancelByOwner(lhash) doesn't reach them.
        # Match connections by comparing the dispatch listener object identity.
        list<string> conn_keys;
        {
            AutoReadLock al(m);
            foreach string uh in (keys connections) {
                *HttpServer::HttpListenerInterface li = connections{uh}.listener;
                if (li && li.uniqueHash() == conn_listener_hash) {
                    conn_keys += uh;
                }
            }
        }
        foreach string uh in (conn_keys) {
            try {
                controller.cancelByOwner(uh);
            } catch () {
                # ignore errors for already-cancelled ops
            }
        }

        if (linfo && linfo.http_version == HttpVersionMode::H3) {
            try {
                linfo.listener.close();
            } catch () {
                # ignore errors for already-closed listeners
            }
        }

        {
            AutoWriteLock al(m);
            if (quic_listener_hash_by_http_listener.hasKey(listener_hash)
                    && quic_listener_hash_by_http_listener{listener_hash} == lhash) {
                remove quic_listener_hash_by_http_listener{listener_hash};
            }
            if (linfo && linfo.http_listener) {
                remove quic_listener_hash_by_http_listener{linfo.http_listener.uniqueHash()};
            }
            remove listeners{lhash};
            remove accept_counts{lhash};
        }
        log(LoggerLevel::INFO, "Removed listener %y", lhash);
    }

    #! Updates the SSL certificate and private key for an already-registered listener
    /** Used by HttpServer::reloadCertificate(): swaps the cert/key references the
        controller hands to every subsequently-submitted accept poll operation, and
        also pushes the new cert/key onto the listener Socket itself (a no-op for TCP
        listeners — HttpListener::reloadCertificate has already done it via the
        Socket inheritance — but required for the QUIC paired listener whose udp_sock
        is owned by the controller and is the actual TLS endpoint for HTTP/3).

        In-flight TCP accept ops keep their current cert/key (each accept op owns its
        own ref); the next replenish picks up the new values via submitAcceptOp().

        @param listener the listener socket to update (must already be registered)
        @param cert the new SSL certificate (SSLCertificate or PEM/DER data)
        @param key the new SSL private key (SSLPrivateKey or PEM/DER data)

        @throw CONTROLLER-RELOAD-CERT-ERROR listener is not registered, was not
        registered with SSL, or controller has been stopped

        @since Qore 3.0.0
    */
    #! Updates the SSL certificate and private key for the QUIC paired listener
    #! whose async controller registration was made with the given
    #! HttpListenerInterface as the \c http_listener field.
    /** Used by HttpServer::reloadListenerCertificate() after the TCP-side cert
        reload, to keep the paired QUIC udp_sock in sync.  Without this push,
        every paired QUIC handshake keeps using the original cert/key set at
        addQuicListener() time — which stalls HTTP/3 handshakes for clients that
        follow the advertised Alt-Svc and triggers HTTPCLIENT-NEGOTIATE-TIMEOUT
        on the client side.

        The udp_sock that runs QUIC's TLS 1.3 handshake holds its mutex
        (`priv->m`) for long stretches during the packet-processing loop, so a
        live `setCertificate()` on it would serialize behind the QUIC server
        poll operation and block the caller for the entire `quic_drain_timeout`
        window.  Instead, this method:
          1. Cancels the in-flight Http3ServerPollOperation (which releases the
             udp_sock mutex)
          2. Updates the cert/key on the udp_sock and in the listeners map
          3. Re-submits a fresh accept op so QUIC keeps accepting with the new
             cert
        The TCP path uses the cheaper @ref reloadListenerCertificate variant —
        TCP accept ops only briefly touch the listener Socket, so a direct
        live-setCertificate works and there's no need to cancel/resubmit.

        @param http_listener the HttpListener wrapper attached to the desired
        QUIC controller entry at registration time
        @param cert the new SSL certificate
        @param key the new SSL private key

        @return @ref True if a matching entry was updated, @ref False otherwise

        @since Qore 3.0.0
    */
    bool reloadListenerCertificateByHttpListener(HttpServer::HttpListenerInterface http_listener,
            auto cert, auto key) {
        @assert(http_listener, "http_listener cannot be null");
        @assert(cert && key, "cert and key cannot be null");
        string target_lhash;
        *Http3ServerPollOperation target_h3op;
        {
            AutoWriteLock al(m);
            foreach string lhash in (keys listeners) {
                if (listeners{lhash}.http_listener === http_listener) {
                    listeners{lhash}.cert = cert;
                    listeners{lhash}.key = key;
                    target_lhash = lhash;
                    target_h3op = listeners{lhash}.h3op;
                    break;
                }
            }
        }
        if (!target_lhash) {
            return False;
        }
        # Stage the cert/key swap on the H3 server poll op.  The op picks it up
        # at the top of its next continuePoll() iteration on the I/O thread and
        # applies setCertificateAndPrivateKey + freeQuicServerSslCtx without
        # contending with itself for the udp_sock's mutex.  In-flight
        # QuicSessions hold their own SSL_CTX_up_ref and continue with the cert
        # they handshook against; new handshakes pick up the new material.
        if (target_h3op && cert instanceof SSLCertificate && key instanceof SSLPrivateKey) {
            target_h3op.reloadCertificate(cert, key);
            log(LoggerLevel::INFO, "Staged QUIC certificate reload for listener %y", target_lhash);
        } else {
            log(LoggerLevel::INFO, "Reloaded listeners-map certificate for listener %y "
                "(no live H3 op to swap — listener may not be active yet)", target_lhash);
        }
        return True;
    }

    reloadListenerCertificate(Socket listener, auto cert, auto key) {
        @assert(listener, "listener socket cannot be null");
        @assert(cert && key, "cert and key cannot be null");
        string lhash = listener.uniqueHash();
        AutoWriteLock al(m);
        if (has_been_stopped) {
            throw "CONTROLLER-RELOAD-CERT-ERROR",
                sprintf("cannot reload certificate for listener %s: controller has been stopped", lhash);
        }
        if (!listeners{lhash}) {
            throw "CONTROLLER-RELOAD-CERT-ERROR",
                sprintf("cannot reload certificate: listener %s is not registered", lhash);
        }
        if (!listeners{lhash}.ssl) {
            throw "CONTROLLER-RELOAD-CERT-ERROR",
                sprintf("cannot reload certificate: listener %s is not an SSL listener", lhash);
        }
        listeners{lhash}.cert = cert;
        listeners{lhash}.key = key;
        # Also push onto the listener socket itself.  For TCP listeners the
        # HttpListener Socket has already been updated by
        # HttpListener::reloadCertificate via setCertificateAndPrivateKey; this
        # call is then a redundant-but-cheap re-set.  For QUIC paired listeners
        # the socket here is the udp_sock that does TLS 1.3 inside the QUIC
        # handshake — without this push, every paired QUIC handshake keeps
        # using the original cert/key set at addQuicListener() time.
        if (cert instanceof SSLCertificate) {
            listener.setCertificate(cert);
        }
        if (key instanceof SSLPrivateKey) {
            listener.setPrivateKey(key);
        }
        log(LoggerLevel::INFO, "Reloaded certificate for listener %y", lhash);
    }

    #! Adds a socket back to idle pool for keep-alive
    /** @param sock the socket to monitor
        @param listener the listener object for callbacks
        @param cx connection context
        @param target the target for callbacks
        @param ttl keep-alive timeout
        @param reuse_op optional keep-alive poll operation to reuse
    */
    addIdleConnection(Socket sock, *HttpServer::HttpListenerInterface listener, hash<auto> cx, HttpAsyncTarget target,
            timeout ttl = DefaultIdleTimeout,
            *HttpKeepAlivePollOperation reuse_op) {
        # Guard against race: if the client closed the connection while the handler
        # thread was sending the response, don't re-register a closed socket
        if (!sock.isOpen()) {
            return;
        }
        string uh = sock.uniqueHash();
        hash<HttpActiveConnectionInfo> cinfo;
        {
            AutoWriteLock al(m);
            if (stopping) {
                try { sock.close(); } catch () {}
                return;
            }
            # Opt 5: reuse existing keep-alive operation to avoid EventLoop remove/add
            *HttpKeepAlivePollOperation ka_op = reuse_op;
            if (ka_op) {
                ka_op.resetForIdle();
            } else {
                ka_op = new HttpKeepAlivePollOperation(sock, ttl);
            }
            cinfo = connections{uh} = <HttpActiveConnectionInfo>{
                "sock": sock,
                "listener": listener,
                "cx": cx,
                "target": target,
                "state": HttpConnectionState::KeepAlive,
                "keep_alive_op": ka_op,
                "ttl": ttl,
                "ssl": cx.ssl ?? False,
            };
        }

        submitConnectionOp(uh, cinfo);
        log(LoggerLevel::DEBUG, "Added idle connection %y", uh);
    }

    #! Registers an HTTP/1.1 WebSocket connection with the async I/O controller
    /** The 101 Switching Protocols response must already have been sent before
        calling this method. The socket transitions directly to WebSocket frame I/O.

        @param sock the WebSocket socket (after upgrade handshake)
        @param poll_op the WebSocket poll operation to drive
        @param target the target for callbacks
        @param listener the listener interface for logging
        @param cx connection context
    */
    registerH1WebSocketConnection(Socket sock, HttpWebSocketPollOperation poll_op,
            HttpAsyncTarget target, *HttpServer::HttpListenerInterface listener, hash<auto> cx) {
        string uh = sock.uniqueHash();
        hash<HttpActiveConnectionInfo> cinfo;
        {
            AutoWriteLock al(m);
            if (stopping) {
                # Signal the poll op as completed so the handler thread
                # doesn't block forever in done.waitForZero()
                poll_op.onComplete(<SocketPollResultInfo>{});
                return;
            }
            cinfo = connections{uh} = <HttpActiveConnectionInfo>{
                "sock": sock,
                "listener": listener,
                "cx": cx,
                "target": target,
                "state": HttpConnectionState::WebSocket,
                "stream_op": poll_op,
                "ssl": cx.ssl ?? False,
            };
        }
        submitConnectionOp(uh, cinfo);
        controller.wakeSocket(sock);
    }

    #! Deregisters an HTTP/1.1 WebSocket connection from the async I/O controller
    /** @param sock the WebSocket socket to deregister
    */
    deregisterH1WebSocketConnection(Socket sock) {
        string uh = sock.uniqueHash();
        {
            AutoWriteLock al(m);
            remove connections{uh};
        }
        try {
            controller.cancelAndClose(sock);
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::DEBUG, "Error cancelling WebSocket connection %y: %s: %s", uh, ex.err, ex.desc);
        }
    }

    #! Gets controller statistics
    hash<auto> getStats() {
        AutoReadLock al(m);
        return {
            "listeners": listeners.size(),
            "connections": connections.size(),
            "running": !has_been_stopped && controller.running(),
        };
    }

    #! Returns the total number of operations in all I/O controller caches (useful for testing cleanup)
    /** This method returns the sum of cache sizes across all I/O threads. A non-zero value
        after connections are closed indicates a cache leak (poll operations not properly cancelled).
    */
    int getTotalCacheSize() {
        return controller.getCacheSize();
    }

    #! Submits an accept operation to the controller
    private submitAcceptOp(Socket listener, HttpAsyncTarget target, bool ssl,
            auto cert, auto key, string http_version, string owner) {
        int seq;
        int ssl_verify_flags = SSL_VERIFY_NONE;
        bool ssl_accept_all_certs = True;
        int cnt_before;
        int cnt_after;
        {
            AutoWriteLock al(m);
            seq = ++accept_seq;
            cnt_before = accept_counts{owner} ?? 0;
            accept_counts{owner} = cnt_before + 1;
            cnt_after = accept_counts{owner};
            # Get SSL params from listener info for consistent handshake behavior
            # (refresh cert/key on every submit so reloadListenerCertificate is honoured
            # without having to cancel in-flight ops or re-add the listener)
            if (listeners{owner}) {
                ssl_verify_flags = listeners{owner}.ssl_verify_flags;
                ssl_accept_all_certs = listeners{owner}.ssl_accept_all_certs;
                if (ssl) {
                    cert = listeners{owner}.cert;
                    key = listeners{owner}.key;
                }
            }
        }
        on_error {
            AutoWriteLock al(m);
            int cnt = accept_counts{owner} ?? 0;
            if (cnt > 0) {
                accept_counts{owner} = cnt - 1;
            }
        }
        string op_key = sprintf("accept:%s:%d", owner, seq);

        # Get the effective HTTP version - respect global HTTP/2 mode at runtime
        string eff_http_version = getEffectiveHttpVersion(http_version);

        HttpAcceptPollOperation op(listener, ssl, cert, key, ssl_verify_flags,
            ssl_accept_all_certs, eff_http_version, False);

        @debug {
            if (getenv("QORE_HTTP_ACCEPT_TRACE")) {
                log(LoggerLevel::INFO,
                    "submitAcceptOp: key=%s listener=%s ssl=%y http_version=%s",
                    op_key, owner, ssl, eff_http_version);
            }
        }

        op.setCompletionContext(self, listener, target, http_version, owner);
        controller.submit(<SocketPollOperationInfo>{
            "sock": listener,
            "spop": op,
            "key": op_key,
            "thread_key": listener.uniqueHash(),
            "owner": owner,
            "to": -1s,  # no timeout for accepts
        });
    }

    #! Handles the result of a completed accept poll operation
    /** Called by the onComplete callback of @ref HttpAcceptPollOperation when a new client
        connection has been accepted (or the accept failed). This method:
        -# Decrements the outstanding accept count for the listener
        -# If the operation was canceled (shutdown in progress), returns immediately
        -# Replenishes the accept operation pool so the listener stays ready for new connections
        -# On accept error, logs the error and returns
        -# On success, retrieves the accepted socket and delegates to @ref handleAcceptComplete()
           for SSL handshake, HTTP/2 negotiation, and connection tracking

        @param result the poll result containing the accepted socket or error information
        @param listener the listener socket that accepted the connection
        @param target the async target for dispatching HTTP events
        @param ssl whether the listener requires SSL/TLS
        @param cert the SSL certificate for the listener (if SSL)
        @param key the SSL private key for the listener (if SSL)
        @param http_version the HTTP version for the listener (e.g. \c "1.1", \c "2")
        @param owner the listener owner key used for tracking accept counts

        @note This method runs on an async callback dispatcher worker and must not block.
    */
    handleAcceptResult(hash<SocketPollResultInfo> result, Socket listener,
            HttpAsyncTarget target, bool ssl, auto cert, auto key,
            string http_version, string owner) {
        # Decrement accept count
        int cnt_before;
        int cnt_after;
        {
            AutoWriteLock al(m);
            cnt_before = accept_counts{owner} ?? 0;
            if (cnt_before > 0) {
                accept_counts{owner} = cnt_before - 1;
            }
            cnt_after = accept_counts{owner} ?? 0;
        }

        if (result.canceled) {
            return;
        }

        # Replenish: submit a new accept operation (outside the lock to avoid deadlock,
        # since submitAcceptOp() also acquires m)
        bool do_replenish = False;
        bool has_listener;
        int cnt_check;
        {
            AutoWriteLock al(m);
            has_listener = exists listeners{owner};
            cnt_check = accept_counts{owner} ?? 0;
            if (has_listener) {
                if (cnt_check < DefaultAcceptOps) {
                    do_replenish = True;
                }
            }
        }

        if (do_replenish) {
            try {
                submitAcceptOp(listener, target, ssl, cert, key, http_version, owner);
            } catch (hash<ExceptionInfo> ex) {
                log(LoggerLevel::ERROR, "FAILED to submit accept op for owner=%s: %s: %s",
                    owner, ex.err, ex.desc);
            }
        } else {
            log(LoggerLevel::WARN, "NOT REPLENISHING accept for owner=%s (has_listener=%y cnt=%d)",
                owner, has_listener, cnt_check);
        }

        if (result.ex) {
            log(LoggerLevel::WARN, "Accept error: %s: %s", result.ex.err, result.ex.desc);
            return;
        }

        hash<auto> output = result.spop.getOutput();
        # Use stored listener info to preserve all configuration (SSL verify flags, etc.)
        *hash<HttpListenerInfo> linfo;
        {
            AutoReadLock al(m);
            linfo = listeners{owner};
        }
        if (!linfo) {
            # Listener was removed; use the parameters we have
            linfo = <HttpListenerInfo>{
                "listener": listener,
                "target": target,
                "ssl": ssl,
                "cert": cert,
                "key": key,
                "http_version": http_version,
            };
        }
        handleAcceptComplete(linfo, output);
    }

    private hash<auto> getUnknownConnectionContext() {
        # Keep the outer hash wide enough to add non-hash values such as peer-info-future.
        return cast<hash>({
            "peer": {"address": "unknown", "address_desc": "unknown", "port": 0},
            "socket": {"address": "unknown", "address_desc": "unknown", "port": 0},
        });
    }

    private *Future startPeerInfoFuture(hash<auto> peer_info) {
        *string address = peer_info.address;
        if (!address || address == "unknown" || address == "quic") {
            return NOTHING;
        }

        int family = peer_info.family ?? (address =~ /:/ ? AF_INET6 : AF_INET);
        if (family != AF_INET && family != AF_INET6) {
            return NOTHING;
        }

        try {
            # Keep the typed local; direct soft return can otherwise lose the Future value.
            *Future f = getnameinfo_async(address, family, peer_info.port ?? 0);
            return f;
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::DEBUG, "Could not start async reverse DNS for %s: %s: %s",
                peer_info.address_desc ?? address, ex.err, ex.desc);
        }

        return NOTHING;
    }

    private hash<auto> getAcceptConnectionContext(hash<auto> output, string uh, string phase) {
        *hash<auto> peer_info = output.peer_info;
        *hash<auto> socket_info = output.socket_info;
        if (!peer_info || !socket_info) {
            log(LoggerLevel::DEBUG, "No numeric peer info in accept output for %s connection %s", phase, uh);
            return getUnknownConnectionContext();
        }

        hash<auto> cx = cast<hash>({
            "peer": peer_info,
            "socket": socket_info,
        });

        *Future peer_info_future = startPeerInfoFuture(peer_info);
        if (exists peer_info_future) {
            cx{"peer-info-future"} = peer_info_future;
        }

        return cx;
    }

    private hash<auto> getActiveConnectionContext(string uh) {
        AutoReadLock al(m);
        *hash<HttpActiveConnectionInfo> cinfo = connections{uh};
        return cinfo ? cinfo.cx : getUnknownConnectionContext();
    }

    #! Handles accept completion
    private handleAcceptComplete(hash<HttpListenerInfo> linfo, hash<auto> output) {
        Socket sock = output.sock;
        string uh = sock.uniqueHash();
        # Ensure the accepted socket is closed on any unexpected exception
        on_error try { sock.close(); } catch () {}

        # Check stopping flag — reject new connections during shutdown
        {
            AutoReadLock al(m);
            if (stopping) {
                try { sock.close(); } catch () {}
                return;
            }
        }

        # Set body size limits on accepted socket before HTTP/2 session setup
        if (linfo.max_request_body_size > 0) {
            sock.setMaxChunkedBodySize(linfo.max_request_body_size);
            sock.setHttp2MaxRequestBodySize(linfo.max_request_body_size);
        }

        log(LoggerLevel::DEBUG, "Accept complete: %y (SSL: %y, HTTP/2: %y)", uh, output.ssl, output.is_http2);
        @debug {
            if (getenv("QORE_HTTP_ACCEPT_TRACE")) {
                log(LoggerLevel::INFO,
                    "Accept complete: sock=%s ssl=%y secure=%y http2=%y",
                    uh, output.ssl, output.secure ?? False, output.is_http2);
            }
        }

        if (output.ssl && !(output.secure ?? False)) {
            hash<auto> cx = getAcceptConnectionContext(output, uh, "SSL");

            if (linfo.cert) {
                sock.setCertificate(linfo.cert);
            }
            if (linfo.key) {
                sock.setPrivateKey(linfo.key);
            }
            sock.setSslVerifyMode(linfo.ssl_verify_flags);
            sock.acceptAllCertificates(linfo.ssl_accept_all_certs);
            if (linfo.get_remote_certs) {
                sock.captureRemoteCertificates();
            }

            list<string> alpn;
            if (output.http_version == HttpVersionMode::Auto) {
                alpn = ("h2", "http/1.1");
            } else if (output.http_version == HttpVersionMode::Http20 || output.http_version == "2") {
                alpn = ("h2",);
            } else {
                alpn = ("http/1.1",);
            }
            sock.setAlpnProtocols(alpn);

            SocketPollOperation ssl_op = sock.startPollUpgradeServerToSSL();
            @debug {
                if (getenv("QORE_HTTP_ACCEPT_TRACE")) {
                    log(LoggerLevel::INFO, "SSL handshake start: sock=%s alpn=%y", uh, alpn);
                }
            }

            # Prepare connection info outside lock
            hash<HttpActiveConnectionInfo> new_cinfo = <HttpActiveConnectionInfo>{
                "sock": sock,
                "listener": linfo.http_listener,
                "cx": cx,
                "target": linfo.target,
                "state": HttpConnectionState::SslHandshake,
                "ssl_op": ssl_op,
                "http_version": output.http_version,
                "is_http2": False,
                "ssl": False,
            };
            hash<HttpActiveConnectionInfo> cinfo;
            {
                AutoWriteLock al(m);
                cinfo = connections{uh} = new_cinfo;
            }

            # Submit SSL handshake operation
            submitSslOp(sock, uh, ssl_op, linfo, output.http_version);
            return;
        }

        # Check if this is an HTTP/2 connection
        if (output.is_http2) {
            hash<auto> cx = getAcceptConnectionContext(output, uh, "h2c");

            Http2PollOperation h2op(sock, True);

            hash<HttpActiveConnectionInfo> new_cinfo = <HttpActiveConnectionInfo>{
                "sock": sock,
                "listener": linfo.http_listener,
                "cx": cx,
                "target": linfo.target,
                "state": HttpConnectionState::Http2,
                "stream_op": h2op,
                "is_http2": True,
                "ssl": output.secure ?? False,
            };
            hash<HttpActiveConnectionInfo> cinfo;
            {
                AutoWriteLock al(m);
                cinfo = connections{uh} = new_cinfo;
            }

            log(LoggerLevel::DEBUG, "Starting HTTP/2 connection handling for %y", uh);
            submitConnectionOp(uh, cinfo);
            return;
        }

        # HTTP/1.x path (no SSL)
        hash<auto> cx = getAcceptConnectionContext(output, uh, "HTTP/1.x");

        hash<HttpActiveConnectionInfo> new_cinfo = <HttpActiveConnectionInfo>{
            "sock": sock,
            "listener": linfo.http_listener,
            "cx": cx,
            "target": linfo.target,
            "state": HttpConnectionState::ReadHeader,
            "http_version": output.http_version,
            "is_http2": False,
            "ssl": output.secure ?? False,
        };
        hash<HttpActiveConnectionInfo> cinfo;
        {
            AutoWriteLock al(m);
            cinfo = connections{uh} = new_cinfo;
        }

        log(LoggerLevel::DEBUG, "Starting HTTP/1.x header reading for %y", uh);
        submitConnectionOp(uh, cinfo);
    }

    #! Submits a QUIC accept operation on a UDP listener
    private submitQuicAcceptOp(Socket listener, HttpAsyncTarget target, string owner) {
        int seq;
        {
            AutoWriteLock al(m);
            seq = ++accept_seq;
        }
        string op_key = sprintf("quic-accept:%s:%d", owner, seq);

        # quic_headers_only defaults to True for parity with the H2 server
        # (HttpServerAsyncIo always-on at :1257/1949) and to match enterprise
        # expectations: dispatch the handler on HEADERS so wantsBodyStreaming()
        # handlers receive incremental body reads, large uploads don't buffer
        # the full body in memory before dispatch, and max_request_body_size
        # rejects mid-stream rather than after a full buffer-up.
        # The Phase 1 (commit a66b7d9ee, shutdownStreamReads) and Phase 3
        # (commit af07f1235, closeStreamQueues) shutdown-race fixes are the
        # prerequisites for this flip — without them an in-flight headers-only
        # handler waiting on body data would block HttpServer.stop() for the
        # full body-read timeout in the future graceful-drain or per-stream-
        # cancel scenarios that keep the session alive across an H3 op cancel.
        Http3ServerPollOperation h3op(listener, pool_opts.quic_drain_timeout
            ?? Http3ServerPollOperation::DefaultDrainTimeout,
            pool_opts.quic_headers_only ?? True,
            pool_opts.max_request_body_size ?? 0);

        log(LoggerLevel::DEBUG, "submitQuicAcceptOp: key=%s listener=%s", op_key, owner);

        h3op.setCompletionContext(self, listener, target, owner);
        # Record the live h3op on the listener entry so reloadCertificate can
        # reach it without scanning the controller's ops cache.
        {
            AutoWriteLock al(m);
            if (listeners{owner}) {
                listeners{owner}.h3op = h3op;
            }
        }
        controller.submit(<SocketPollOperationInfo>{
            "sock": listener,
            "spop": h3op,
            "key": op_key,
            "thread_key": listener.uniqueHash(),
            "owner": owner,
            "to": -1s,  # no timeout for QUIC accept
        });
    }

    #! Handles QUIC accept result
    handleQuicAcceptResult(hash<SocketPollResultInfo> result, Socket listener,
            HttpAsyncTarget target, string owner, Http3ServerPollOperation h3op) {
        if (result.canceled) {
            return;
        }

        if (result.ex) {
            log(LoggerLevel::ERROR, "QUIC accept error on %s: %s: %s", owner,
                result.ex.err, result.ex.desc);
            # Replenish accept op
            {
                AutoReadLock al(m);
                if (stopping || !listeners{owner}) {
                    return;
                }
            }
            submitQuicAcceptOp(listener, target, owner);
            return;
        }

        if (h3op.hasError()) {
            log(LoggerLevel::ERROR, "QUIC accept poll operation error on %s: %y", owner,
                h3op.getError());
            {
                AutoReadLock al(m);
                if (stopping || !listeners{owner}) {
                    return;
                }
            }
            submitQuicAcceptOp(listener, target, owner);
            return;
        }

        if (!h3op.goalReached()) {
            # Not yet ready — resubmit
            {
                AutoReadLock al(m);
                if (stopping || !listeners{owner}) {
                    return;
                }
            }
            submitQuicAcceptOp(listener, target, owner);
            return;
        }

        # HTTP/3 request is ready — dispatch it
        *hash<auto> request = h3op.getOutput();
        if (!request) {
            # No request yet, continue reading
            h3op.continueReading();
            int seq;
            {
                AutoWriteLock al(m);
                seq = ++accept_seq;
            }
            h3op.setCompletionContext(self, listener, target, owner);
            controller.submit(<SocketPollOperationInfo>{
                "sock": listener,
                "spop": h3op,
                "key": sprintf("quic-read:%s:%d", owner, seq),
                "thread_key": listener.uniqueHash(),
                "owner": owner,
                "to": -1s,
            });
            return;
        }

        # NOTE: Unlike TCP, QUIC uses a single UDP socket for all connections.
        # We do NOT replenish with a new accept op here. The same h3op is
        # re-submitted via handleHttp3Request() → submitConnectionOp() and
        # continues handling the socket. A new accept op would conflict on
        # the non-blocking guard flag.

        # Get listener info
        hash<HttpListenerInfo> linfo;
        {
            AutoReadLock al(m);
            if (!listeners{owner}) {
                return;
            }
            linfo = listeners{owner};
        }

        handleHttp3Request(linfo, h3op, request, listener, target, owner);
    }

    #! Handles an HTTP/3 request from a QUIC connection
    private handleHttp3Request(hash<HttpListenerInfo> linfo, Http3ServerPollOperation h3op,
            hash<auto> request, Socket listener, HttpAsyncTarget target, string old_uh) {
        int session_id = request.session_id;
        int stream_id = request.stream_id;

        log(LoggerLevel::DEBUG, "HTTP/3 request ready: session=%d stream=%d %s %s",
            session_id, stream_id, request.method, request.path);

        # NOTE: h3op is already in STATE_READING — getOutput() atomically
        # transitions from STATE_REQUEST_READY to STATE_READING when it
        # extracts the request.  No separate continueReading() call needed.

        # Submit h3op back to I/O controller for continued reading.
        # Use a socket-level key: there is exactly one h3op per QUIC listener
        # socket, so one connections{} entry suffices.  Handler threads receive
        # their own cinfo copy (Qore hashes are value types), so the shared
        # entry only needs to reflect the h3op's current poll state.
        string base_uh = listener.uniqueHash();
        # The HTTP/3 poll operation is listener-scoped: one shared UDP socket
        # processes packets for all QUIC sessions.  Keep the controller key
        # stable per listener so a later session cannot remove or replace the
        # poll operation for an earlier long-lived stream such as SSE.
        string uh = sprintf("%s:quic", base_uh);
        # Persistent handler queues are still session-scoped so requests from
        # one QUIC client session cannot be routed to another session's
        # dedicated handler thread.
        string persistent_uh = sprintf("%s:%d", uh, session_id);
        # NOTE: cx here is intentionally minimal (just "peer" and "socket").
        # The onHttpRequest handler rebuilds the full context — including
        # "peer-info", "id", "ssl", "listener-id", etc. — via
        # HttpListener::getContextTemplate(info.cx.peer) before calling
        # processNativeRequest().  Error paths in this controller use
        # submitQuicResponse() directly, bypassing cx entirely.
        # Keep the outer hash wide enough to add peer-info-future below.
        hash<auto> cx = cast<hash>({
            "peer": {
                "address": request.peer_address ?? "quic",
                "address_desc": request.peer_address ?? "quic",
                "port": request.peer_port ?? 0,
                "family": (request.peer_address ?? "") =~ /:/ ? AF_INET6 : AF_INET,
                "familystr": (request.peer_address ?? "") =~ /:/ ? "ipv6" : "ipv4",
            },
            "socket": linfo.socket_info ?? {
                "address": "quic",
                "address_desc": "quic",
                "port": 0,
                "family": AF_INET,
                "familystr": "ipv4",
            },
        });
        hash<HttpActiveConnectionInfo> new_cinfo = <HttpActiveConnectionInfo>{
            "sock": listener,
            "listener": linfo.http_listener,
            "cx": cx,
            "target": target,
            "state": HttpConnectionState::Http3,
            "stream_op": h3op,
            "is_http2": False,
            "ssl": True,
        };
        *Future peer_info_future = startPeerInfoFuture(new_cinfo.cx.peer);
        if (exists peer_info_future) {
            new_cinfo.cx{"peer-info-future"} = peer_info_future;
        }

        {
            AutoWriteLock al(m);
            # First call: old_uh is the listener key (no-op remove); subsequent
            # calls: old_uh == uh (socket-level key is stable across requests)
            if (old_uh != uh) {
                remove connections{old_uh};
            }
            connections{uh} = new_cinfo;
        }
        submitConnectionOp(uh, new_cinfo);

        # Check for RFC 9220 extended CONNECT over HTTP/3 (CONNECT with :protocol)
        bool is_extended_connect = request.method == "CONNECT"
            && request.headers.":protocol".val();
        bool is_websocket_connect = is_extended_connect
            && request.headers.":protocol" == "websocket";
        if (is_extended_connect) {
            log(LoggerLevel::DEBUG, "HTTP/3 extended CONNECT (%s): session=%d stream=%d %s",
                request.headers.":protocol", session_id, stream_id, request.path);
        }

        # Build header hash (same format as HTTP/2)
        hash<auto> hdr = request.headers ?? {};
        hdr.method = request.method;
        hdr.path = request.path;
        hdr.":authority" = request.authority;
        hdr.":scheme" = request.scheme ?? "https";
        hdr.host = request.authority;
        hdr.http_version = "HTTP/3";

        # Parse accept-encoding header
        *list<string> accept_encoding;
        if (hdr."accept-encoding") {
            accept_encoding = map trim($1.split(";")[0]),
                hdr."accept-encoding".split(",");
        }

        # Detect headers-only dispatch
        bool body_streaming = request.hdr && !request.headers_end_stream;

        # For extended CONNECT: create unlimited Queue for async data delivery.
        # WebSocket CONNECT uses the C++ WebSocketStreamFrameState path —
        # the Queue is populated with one entry per complete WebSocket
        # message (a typed hash), not per raw transport chunk.  This avoids
        # O(N²) Qore-side binary concatenation for large WS messages.
        # Other extended-CONNECT tunnels (e.g. QUIC datagram tunnels) keep
        # the raw-byte Queue contract.
        *Queue stream_data_queue;
        bool stream_uses_frame_state = False;
        if (is_extended_connect) {
            stream_data_queue = new Queue();
            if (is_websocket_connect) {
                listener.registerQuicConnectStreamFrameState(session_id, stream_id,
                    stream_data_queue);
                stream_uses_frame_state = True;
            } else {
                listener.registerQuicConnectStreamQueue(session_id, stream_id,
                    stream_data_queue);
            }
            # Register the Queue with the H3 poll operation for dispatch tracking
            h3op.registerStreamQueue(session_id, stream_id, stream_data_queue);
        }

        # Create send-and-wake wrapper for handler threads (HTTP/3 version).
        # Same interface as the HTTP/2 wrapper — handlers use this uniformly
        # regardless of protocol.  Uses CV-based waitForQuicStreamDrain()
        # for efficient backpressure instead of polling.
        code send_stream_data = sub (*binary data, bool end_stream = False) {
            int retries = 0;
            while (listener.submitQuicStreamData(
                    session_id, stream_id, data, end_stream) == 1) {
                if (++retries > 200) {
                    throw "HTTP3-FLOW-CONTROL", sprintf("stream %d: buffer "
                        "not draining after %d retries", stream_id, retries);
                }
                controller.wakeSocket(listener);
                # CV-based wait: wakes immediately when I/O thread drains
                # the buffer, falls back to 50ms timeout otherwise
                int drain_rv = listener.waitForQuicStreamDrain(
                    session_id, stream_id, 50ms);
                if (drain_rv == -1) {
                    throw "HTTP3-STREAM-ERROR", sprintf("stream %d: stream "
                        "removed while waiting for drain", stream_id);
                }
            }
            controller.wakeSocket(listener);
        };

        # Convert to HttpConnectionInfo (same dispatch chain as HTTP/2)
        hash<HttpConnectionInfo> conn_info = <HttpConnectionInfo>{
            "sock": listener,
            "hdr": hdr,
            "header_info": {
                "http3": True,
                "session_id": session_id,
                "stream_id": stream_id,
                "is_extended_connect": is_extended_connect,
                "is_websocket_connect": is_websocket_connect,
                "accept-encoding": accept_encoding,
                "body": body_streaming ? NOTHING : request.body,
                "body_streaming": body_streaming,
                "headers_end_stream": request.headers_end_stream ?? False,
                "stream_data_queue": stream_data_queue,
                "stream_uses_frame_state": stream_uses_frame_state,
                "send_stream_data": send_stream_data,
                "io_wake": sub () { controller.wakeSocket(listener); },
                "register_stream_listener": sub (object listener) {
                    h3op.registerStreamListener(session_id, stream_id, listener);
                },
                "register_persistent_queue": sub (string key, Queue q,
                        *code<bool(hash<auto>)> filter) {
                    self.registerPersistentQueue(key, q, filter);
                    # Bind the queue to the QUIC session so libqore delivers a
                    # close signal on peer stream reset / session close, releasing
                    # the dedicated handler thread even though disconnect() does
                    # not close a multiplexed connection (issue #5339).
                    h3op.registerPersistentSessionQueue(session_id, q);
                },
                "deregister_persistent_queue": sub (string key) {
                    self.deregisterPersistentQueue(key);
                    h3op.deregisterPersistentSessionQueue(session_id);
                },
                # NOTE: resume_body_streaming is NOTHING for HTTP/3 because
                # the h3op is already resumed during dispatch (getOutput()
                # atomically transitions to STATE_READING).  Calling
                # resumeMultiplexedReading() from a handler thread races with
                # the I/O thread's dispatch of subsequent requests.
                "resume_body_streaming": NOTHING,
            },
            "listener": linfo.http_listener,
            "cx": new_cinfo.cx,
            "keep_alive": True,
            "ssl": True,  # QUIC always uses TLS
        };

        # Check for persistent queue — if a dedicated thread is waiting for this
        # connection, route the request to it instead of the pool
        *code resume_reading;
        if (body_streaming) {
            resume_reading = sub () {
                resumeMultiplexedReading(h3op, uh, HttpConnectionState::Http3);
            };
        }
        if (pushPersistentQueue(persistent_uh, hdr, {
            "type": "request",
            "conn_info": conn_info,
            "stream_id": stream_id,
            "body_streaming": body_streaming,
            "resume_reading": resume_reading,
        }, sub () {
            log(LoggerLevel::DEBUG, "HTTP/3 request routed to persistent queue: %y "
                "session=%d stream=%d %s %s",
                persistent_uh, session_id, stream_id, request.method, request.path);
        })) {
            return;
        }

        # The HTTP server does not interpret any application-specific
        # persistent-session control header.  A request that arrives for a
        # persistent session whose dedicated loop has already deregistered
        # its queue simply falls through to normal handler-pool dispatch;
        # the handler owns the persistent-session protocol and is
        # responsible for responding appropriately (idempotently) to such a
        # request.  The dedicated loop now stays bound for the connection's
        # lifetime for handlers that request it
        # (keepPersistentConnectionDedicated()), so an end-of-session
        # request reaches the live loop and is recognised generically via
        # AbstractHttpRequestHandler::isPersistentSessionEndRequest().

        # Give the target a protocol-independent admission point before the shared handler queue.
        try {
            *hash<HttpRequestResultInfo> prepared_result = target.prepareHttpRequest(conn_info);
            if (prepared_result) {
                on_exit if (request.hdr) {
                    try {
                        listener.cleanupQuicStream(session_id, stream_id);
                    } catch () {}
                }
                handleHttp3MultiplexedResult(uh, new_cinfo, session_id, stream_id,
                    prepared_result);
                return;
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::ERROR, "Error preparing HTTP/3 request session=%d stream=%d: %s: %s",
                session_id, stream_id, ex.err, ex.desc);
            try {
                listener.submitQuicResponse(session_id, stream_id, 500,
                    {"content-type": "text/plain"}, "Internal Server Error");
            } catch () {}
            wakeSocketIfPresent(listener, uh, "HTTP/3 preparation error response");
            if (request.hdr) {
                try {
                    listener.cleanupQuicStream(session_id, stream_id);
                } catch () {}
            }
            return;
        }

        # Dispatch to handler thread pool
        # NOTE: for body_streaming requests, HttpServer calls resume_body_streaming (in
        # header-info) as an *early* resume right after the framework finishes reading the
        # body.  This keeps the QUIC session alive (PING, SETTINGS, other streams) during
        # long-running handlers.  The resumeMultiplexedReading() calls below are a *safety
        # net* that guarantee the h3op is resumed even when the early resume was not called:
        #   - wantsBodyStreaming() handlers that read the body themselves
        #   - error paths during framework body reading (timeout, max_body, etc.)
        # resumeMultiplexedReading() silently handles double-resume (catches STATE-ERROR).
        try {
            submitToHandlerPool(target, sub () {
                # Clean up dispatched stream state after handler completes
                # (applies to all headers-only dispatched streams, not just body_streaming)
                # NOTE: on_exit must be at closure scope (not inside if-block), otherwise
                # the cleanup fires immediately when the if-block exits
                on_exit if (request.hdr) {
                    try {
                        listener.cleanupQuicStream(session_id, stream_id);
                    } catch () {}
                }
                try {
                    hash<HttpRequestResultInfo> result = target.onHttpRequest(conn_info);
                    # NOTE: no resumeMultiplexedReading() needed here — the h3op
                    # was already resumed during dispatch (getOutput() atomically
                    # transitions to STATE_READING + submitConnectionOp() resubmits
                    # it to the I/O controller).  Calling resumeMultiplexedReading()
                    # from this handler thread races with the I/O thread's dispatch
                    # of subsequent requests, causing lost requests and 30s timeouts.
                    handleHttp3MultiplexedResult(uh, new_cinfo, session_id, stream_id, result);
                } catch (hash<ExceptionInfo> ex) {
                    log(LoggerLevel::ERROR, "Error handling HTTP/3 request session=%d stream=%d: %s: %s",
                        session_id, stream_id, ex.err, ex.desc);
                    try {
                        listener.submitQuicResponse(session_id, stream_id, 500,
                            {"content-type": "text/plain"},
                            "Internal Server Error");
                    } catch (hash<ExceptionInfo> ex2) {
                        log(LoggerLevel::DEBUG, "failed to send 500 response for session=%d "
                            "stream=%d: %s: %s", session_id, stream_id, ex2.err, ex2.desc);
                    }
                    wakeSocketIfPresent(listener, uh, "HTTP/3 handler error response");
                }
            });
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::ERROR, "Failed to submit HTTP/3 handler for session=%d stream=%d: %s: %s",
                session_id, stream_id, ex.err, ex.desc);
            try {
                target.cancelPreparedHttpRequest(conn_info);
            } catch (hash<ExceptionInfo> cancel_ex) {
                log(LoggerLevel::ERROR, "Failed to cancel HTTP/3 request preparation for session=%d "
                    "stream=%d: %s: %s", session_id, stream_id, cancel_ex.err,
                    cancel_ex.desc);
            }
            try {
                listener.submitQuicResponse(session_id, stream_id, 503,
                    {"content-type": "text/plain"},
                    "Service Unavailable");
            } catch (hash<ExceptionInfo> ex2) {
                log(LoggerLevel::DEBUG, "failed to send 503 response for session=%d "
                    "stream=%d: %s: %s", session_id, stream_id, ex2.err, ex2.desc);
            }
            wakeSocketIfPresent(listener, uh, "HTTP/3 submit failure response");
            # Clean up dispatched stream state for headers-only dispatch
            if (request.hdr) {
                try {
                    listener.cleanupQuicStream(session_id, stream_id);
                } catch () {}
            }
        }
    }

    #! Handles HTTP/3 multiplexed response from handler thread
    /** Uses submitQuicResponse() for the multiplexed path because
        continueReading() (called during dispatch) clears the h3op's
        request context.  The Http3Sending state is reserved for the
        sequential path where startSendResponse() is called before
        continueReading().

        Thread safety: submitQuicResponse() acquires the socket lock
        (priv->m) internally, so it is safe to call from handler
        threads while the I/O thread polls the same socket via
        continuePoll().
    */
    private handleHttp3MultiplexedResult(string uh, hash<HttpActiveConnectionInfo> cinfo,
            int session_id, int stream_id, hash<HttpRequestResultInfo> result) {
        if (result.close) {
            # For HTTP/3, don't close the entire connection — HTTP/3 is multiplexed,
            # so closing the connection would kill all active streams.  Instead, send
            # the error response on this stream and let the connection continue.
            log(LoggerLevel::DEBUG, "HTTP/3 handler requested close for session %d stream %d on %y "
                "(stream-only; connection stays open)", session_id, stream_id, uh);
            if (result.code) {
                try {
                    cinfo.sock.submitQuicResponse(session_id, stream_id, result.code, result.hdr,
                        result.body);
                } catch (hash<ExceptionInfo> ex) {
                    log(LoggerLevel::DEBUG, "HTTP/3 close response session=%d stream=%d failed: "
                        "%s: %s", session_id, stream_id, ex.err, ex.desc);
                }
            }
            controller.wakeSocket(cinfo.sock);
            return;
        }

        if (result.dedicated) {
            log(LoggerLevel::DEBUG, "HTTP/3 session %d stream %d on %y handed to dedicated handler",
                session_id, stream_id, uh);
            return;
        }

        if (result.code && !result.streaming) {
            try {
                # Convert send_callback to InputStream for HTTP/3 (send_callback is
                # HTTP/1.x chunked-only; HTTP/3 uses QUIC DATA frames via InputStream)
                if (result.send_callback && !result.input_stream) {
                    auto cb = result.send_callback;
                    result.input_stream = new HttpServer::CallbackInputStream(
                        auto sub() { return cb(); }, cinfo.sock);
                    remove result.send_callback;
                    remove result.hdr."Transfer-Encoding";
                }

                # InputStream response body: try the fast path
                # (submit*StreamingResponseWithStream), which delegates to
                # the C++ vtable's isIoThreadSafe() check — that gate is the
                # authoritative one (Qore subclass overrides of
                # isIoThreadSafe() do NOT propagate to the C++ vtable).  If
                # the gate rejects (rv=1), the stream is truly unsafe for
                # the I/O thread — spawn a StreamResponseProducer to drain
                # it on a worker thread without materialization.
                if (result.input_stream) {
                    hash<auto> resp_hdr = result.hdr ?? {};
                    remove resp_hdr."Transfer-Encoding";
                    int rv = cinfo.sock.submitQuicStreamingResponseWithStream(
                        session_id, stream_id, result.code, resp_hdr, result.input_stream);
                    if (!rv) {
                        # Memory-backed stream — I/O thread drives it.
                        controller.wakeSocket(cinfo.sock);
                    } else {
                        # C++ gate rejected: non-I/O-thread-safe stream.
                        # Spawn a producer worker to push DATA frames.
                        cinfo.sock.submitQuicResponseStreaming(session_id, stream_id,
                            result.code, resp_hdr);
                        result.input_stream.unassignThread();
                        code<nothing()> wake_fn = sub () {
                            controller.wakeSocket(cinfo.sock);
                        };
                        HttpServer::StreamResponseProducer producer(cinfo.sock,
                            stream_id, session_id, result.input_stream, wake_fn);
                        producer.start();
                        controller.wakeSocket(cinfo.sock);
                    }
                } else {
                    cinfo.sock.submitQuicResponse(session_id, stream_id, result.code,
                        result.hdr, result.body);
                    wakeSocketIfPresent(cinfo.sock, uh, "HTTP/2 handler error response");
                }
            } catch (hash<ExceptionInfo> ex) {
                log(LoggerLevel::ERROR,
                    "Error submitting HTTP/3 response for session=%d stream=%d on %y: %s: %s",
                    session_id, stream_id, uh, ex.err, ex.desc);
            }
            # connections{uh} stays: it's the h3op's socket-level entry, not
            # per-request.  Cleaned up on h3op close/error or result.close.
            return;
        }

        if (result.streaming) {
            log(LoggerLevel::DEBUG, "HTTP/3 streaming response for session=%d stream=%d on %y (type=%y)",
                session_id, stream_id, uh, result.stream_type);
            # The streaming handler manages the stream lifecycle directly
            # (SSE events sent via submitQuicStreamData).
            # h3op stays on the I/O thread handling other streams.
            # Wake the controller in case the handler already queued data.
            controller.wakeSocket(cinfo.sock);
            return;
        }

        # No response code — the handler already managed the response lifecycle
        # (e.g., persistent handler loop completed, WebSocket/SSE upgrade consumed the
        # response).  Just wake the controller and return; do NOT send a spurious 500
        # on the already-completed stream, which would corrupt the HTTP/3 session.
        if (!result.code) {
            log(LoggerLevel::DEBUG, "HTTP/3 session %d stream %d on %y: handler already sent "
                "response (no code in result)", session_id, stream_id, uh);
            controller.wakeSocket(cinfo.sock);
            return;
        }
    }

    #! Submits an SSL handshake operation
    private submitSslOp(Socket sock, string uh, AbstractPollOperation ssl_op,
            hash<HttpListenerInfo> linfo, string http_version) {
        {
            AutoWriteLock al(m);
            if (stopping) {
                remove connections{uh};
                try { sock.close(); } catch () {}
                return;
            }
        }
        string owner = linfo.listener.uniqueHash();
        try {
            auto wrapped_op = new DelegatingPollOperation(ssl_op,
                sub (hash<SocketPollResultInfo> result) {
                    handleSslResult(result, uh, linfo, http_version);
                });
            controller.submit(<SocketPollOperationInfo>{
                "sock": sock,
                "spop": wrapped_op,
                "owner": owner,
                "to": AsyncSocketIoController::DefaultIoOperationTimeout,
            });
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::ERROR, "Failed to submit SSL op for %s: %s: %s", uh, ex.err, ex.desc);
            {
                AutoWriteLock al(m);
                remove connections{uh};
            }
            try { sock.close(); } catch () {}
        }
    }

    #! Handles SSL handshake result
    private handleSslResult(hash<SocketPollResultInfo> result, string uh,
            hash<HttpListenerInfo> linfo, string http_version) {
        if (result.canceled) {
            {
                AutoWriteLock al(m);
                remove connections{uh};
            }
            return;
        }

        if (result.ex) {
            log(LoggerLevel::ERROR, "SSL handshake error for %s: %s: %s",
                uh, result.ex.err, result.ex.desc);
            *hash<HttpActiveConnectionInfo> cinfo;
            {
                AutoWriteLock al(m);
                cinfo = remove connections{uh};
            }
            if (cinfo) {
                dispatchOnError(cinfo.target, result.sock, result.ex, cinfo.listener, cinfo.cx, uh,
                    "SSL error");
            }
            try { cast<Socket>(result.sock).close(); } catch () {}
            return;
        }

        # SSL handshake complete - socket is now stable and validated
        Socket sock = result.sock;
        hash<auto> cx = getActiveConnectionContext(uh);
        hash<auto> peer_info = cx.peer;
        hash<auto> socket_info = cx.socket;

        hash<auto> ssl_output = result.spop.getOutput();
        *string negotiated = ssl_output.negotiated_protocol;
        bool is_http2 = ssl_output.is_http2;

        if ((http_version == HttpVersionMode::Http20 || http_version == "2") && !is_http2) {
            log(LoggerLevel::WARN, "HTTP/2 required but client negotiated: %y", negotiated ?? "none");
            try { sock.close(); } catch () {}
            {
                AutoWriteLock al(m);
                remove connections{uh};
            }
            return;
        }

        @debug {
            if (getenv("QORE_HTTP_ACCEPT_TRACE")) {
                log(LoggerLevel::INFO,
                    "SSL handshake complete: sock=%s negotiated=%y http2=%y",
                    uh, negotiated ?? "none", is_http2);
            }
        }

        try {
            hash<HttpActiveConnectionInfo> cinfo;
            if (is_http2) {
                Http2PollOperation h2op(sock, True);
                {
                    AutoWriteLock al(m);
                    # Check if connection still exists (may have been removed during shutdown)
                    if (!connections.hasKey(uh)) {
                        try { sock.close(); } catch () {}
                        return;
                    }
                    # Cast to typed hash to update fields and assign back
                    hash<HttpActiveConnectionInfo> conn_info = cast<hash<HttpActiveConnectionInfo>>(connections{uh});
                    conn_info.state = HttpConnectionState::Http2;
                    conn_info.stream_op = h2op;
                    conn_info.is_http2 = True;
                    conn_info.ssl = True;
                    # Update cx with real peer info now that socket is stable
                    conn_info.cx.peer = peer_info;
                    conn_info.cx.socket = socket_info;
                    if (exists cx{"peer-info-future"}) {
                        conn_info.cx{"peer-info-future"} = cx{"peer-info-future"};
                    } else {
                        remove conn_info.cx{"peer-info-future"};
                    }
                    cinfo = connections{uh} = conn_info;
                }
            } else {
                {
                    AutoWriteLock al(m);
                    # Check if connection still exists (may have been removed during shutdown)
                    if (!connections.hasKey(uh)) {
                        try { sock.close(); } catch () {}
                        return;
                    }
                    # Cast to typed hash to update fields and assign back
                    hash<HttpActiveConnectionInfo> conn_info = cast<hash<HttpActiveConnectionInfo>>(connections{uh});
                    conn_info.state = HttpConnectionState::ReadHeader;
                    conn_info.is_http2 = False;
                    conn_info.ssl = True;
                    # Update cx with real peer info now that socket is stable
                    conn_info.cx.peer = peer_info;
                    conn_info.cx.socket = socket_info;
                    if (exists cx{"peer-info-future"}) {
                        conn_info.cx{"peer-info-future"} = cx{"peer-info-future"};
                    } else {
                        remove conn_info.cx{"peer-info-future"};
                    }
                    cinfo = connections{uh} = conn_info;
                }
            }

            submitConnectionOp(uh, cinfo);
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::ERROR, "SSL post-handshake setup failed for %s: %s: %s", uh, ex.err, ex.desc);
            {
                AutoWriteLock al(m);
                remove connections{uh};
            }
            try { cast<Socket>(result.sock).close(); } catch () {}
        }
    }

    #! Resumes multiplexed reading after a handler completes body_streaming
    /** Calls continueReading() on the stream op, updates connection state, and resubmits
        the connection for I/O polling.  Silently catches exceptions if the op was already
        resumed (e.g. by resume_body_streaming).

        @param stream_op the Http2PollOperation or Http3ServerPollOperation to resume
        @param uh the unique connection handle
        @param state the connection state to set (HttpConnectionState::Http2 or Http3)
    */
    private resumeMultiplexedReading(AbstractPollOperation stream_op, string uh,
            string state) {
        try {
            call_object_method(stream_op, "continueReading");
        } catch (hash<ExceptionInfo> ex) {
            *string op_state;
            try {
                op_state = call_object_method(stream_op, "getState");
            } catch () {
            }

            if (op_state == "wait_read") {
                # The op has no request to consume, but may also no longer be
                # registered with the controller after the previous callback.
                # Re-submit it so queued response frames are flushed.
            } else {
                # Already in STATE_READING (getOutput() transitioned atomically)
                # or request pending dispatch — nothing to do here
                return;
            }
        }
        hash<HttpActiveConnectionInfo> resumed_cinfo;
        {
            AutoWriteLock al(m);
            if (connections.hasKey(uh)) {
                hash<HttpActiveConnectionInfo> conn_info_upd =
                    cast<hash<HttpActiveConnectionInfo>>(connections{uh});
                conn_info_upd.stream_op = stream_op;
                conn_info_upd.state = state;
                resumed_cinfo = connections{uh} = conn_info_upd;
            }
        }
        if (resumed_cinfo) {
            submitConnectionOp(uh, resumed_cinfo);
            controller.wakeSocket(resumed_cinfo.sock);
        }
    }

    #! Submits a connection operation, skipping the stopping check (caller already verified)
    private submitConnectionOpUnlocked(string uh, hash<HttpActiveConnectionInfo> cinfo) {
        submitConnectionOpIntern(uh, cinfo);
    }

    #! Submits a connection operation based on connection state
    /** Checks the stopping flag before submitting; handler tasks may call this after stop() begins.
    */
    private submitConnectionOp(string uh, hash<HttpActiveConnectionInfo> cinfo) {
        # Check shutdown flag — handler tasks may call this after stop() begins
        {
            AutoWriteLock al(m);
            if (stopping) {
                remove connections{uh};
                # Do not close shared UDP socket for HTTP/3 connections
                if (cinfo.state != HttpConnectionState::Http3
                        && cinfo.state != HttpConnectionState::Http3Sending) {
                    try { cinfo.sock.close(); } catch () {}
                }
                # Signal the stream_op as completed so any handler thread
                # waiting in done.waitForZero() can unblock.
                # setCompletionContext() has not been called yet (only called
                # inside submitConnectionOpIntern), so onComplete() will NOT
                # trigger handleConnectionResult() — it just signals done.
                if (cinfo.stream_op) {
                    cinfo.stream_op.onComplete(<SocketPollResultInfo>{});
                }
                return;
            }
        }
        submitConnectionOpIntern(uh, cinfo);
    }

    #! Shared implementation for submitConnectionOp / submitConnectionOpUnlocked
    private submitConnectionOpIntern(string uh, hash<HttpActiveConnectionInfo> cinfo) {
        AbstractPollOperation op;
        date timeout = AsyncSocketIoController::DefaultIoOperationTimeout;
        string owner = uh;

        @debug {
            if (getenv("QORE_HTTP_ACCEPT_TRACE")) {
                log(LoggerLevel::INFO, "submitConnectionOpIntern: uh=%s state=%y", uh, cinfo.state);
            }
        }

        switch (cinfo.state) {
            case HttpConnectionState::Idle: {
                op = new HttpIdlePollOperation(cinfo.sock, cinfo.ttl);
                timeout = milliseconds(cinfo.ttl);
                break;
            }
            case HttpConnectionState::KeepAlive: {
                # TTL covers idle wait; safety margin covers header-read phase
                op = cinfo.keep_alive_op;
                timeout = milliseconds(cinfo.ttl) + AsyncSocketIoController::DefaultIoOperationTimeout;
                break;
            }
            case HttpConnectionState::ReadHeader: {
                op = cinfo.sock.startPollReadHttpHeader();
                break;
            }
            case HttpConnectionState::Http2: {
                if (!cinfo.stream_op) {
                    log(LoggerLevel::ERROR, "HTTP/2 connection %s has no stream_op", uh);
                    return;
                }
                op = cinfo.stream_op;
                timeout = -1s;  # no timeout for HTTP/2 connections
                break;
            }
            case HttpConnectionState::Http2Sending: {
                if (!cinfo.stream_op) {
                    return;
                }
                op = cinfo.stream_op;
                timeout = AsyncSocketIoController::DefaultIoOperationTimeout;
                break;
            }
            case HttpConnectionState::Sse: {
                if (!cinfo.stream_op) return;
                op = cinfo.stream_op;
                # SSE queue wakeups are delivered by HttpSseStreamPollOperation's
                # EventNotifier; no periodic polling timeout is needed.
                timeout = -1s;
                break;
            }
            case HttpConnectionState::WebSocket: {
                if (!cinfo.stream_op) return;
                op = cinfo.stream_op;
                timeout = -1s;
                break;
            }
            case HttpConnectionState::Http2WebSocket: {
                if (!cinfo.stream_op) return;
                op = cinfo.stream_op;
                timeout = -1s;
                break;
            }
            case HttpConnectionState::Http3: {
                if (!cinfo.stream_op) {
                    log(LoggerLevel::ERROR, "HTTP/3 connection %s has no stream_op", uh);
                    return;
                }
                op = cinfo.stream_op;
                timeout = -1s;  # no timeout for HTTP/3 connections
                break;
            }
            case HttpConnectionState::Http3Sending: {
                if (!cinfo.stream_op) {
                    return;
                }
                op = cinfo.stream_op;
                timeout = AsyncSocketIoController::DefaultIoOperationTimeout;
                break;
            }
            case HttpConnectionState::SendAndReadHeader: {
                if (!cinfo.stream_op) return;
                op = cinfo.stream_op;
                # TTL covers idle wait after send; safety margin covers send + header-read phases
                timeout = milliseconds(cinfo.ttl) + AsyncSocketIoController::DefaultIoOperationTimeout;
                break;
            }
            case HttpConnectionState::SendStreamAndReadHeader: {
                if (!cinfo.stream_op) return;
                op = cinfo.stream_op;
                # TTL covers idle wait after stream send; safety margin covers send + header-read phases
                timeout = milliseconds(cinfo.ttl) + AsyncSocketIoController::DefaultIoOperationTimeout;
                break;
            }
            case HttpConnectionState::SendStreamResponse: {
                if (!cinfo.stream_op) return;
                op = cinfo.stream_op;
                timeout = AsyncSocketIoController::DefaultIoOperationTimeout;
                break;
            }
            case HttpConnectionState::ReadBody: {
                if (!cinfo.stream_op) return;
                op = cinfo.stream_op;
                timeout = AsyncSocketIoController::DefaultIoOperationTimeout;
                break;
            }
            default:
                log(LoggerLevel::ERROR, "Unknown connection state %y for %s", cinfo.state, uh);
                return;
        }

        try {
            # Set completion context on operations that support it (our base class);
            # wrap C++ operations (startPollReadHttpHeader) in DelegatingPollOperation
            AbstractPollOperation submit_op;
            if (op instanceof HttpConnectionPollOperationBase) {
                cast<HttpConnectionPollOperationBase>(op).setCompletionContext(self, uh, cinfo.state,
                    cinfo.target);
                submit_op = op;
            } else if (op instanceof Http3ServerPollOperation) {
                # Http3ServerPollOperation manages its own completion via
                # setCompletionContext/onComplete — no DelegatingPollOperation
                # wrapper needed.  Using a wrapper would break result.spop
                # (handleConnectionResult would receive the wrapper, not the
                # h3op, and the cast to Http3ServerPollOperation would fail).
                cast<Http3ServerPollOperation>(op).setConnectionCompletionContext(
                    self, uh, cinfo.state);
                submit_op = op;
            } else {
                submit_op = new DelegatingPollOperation(op,
                    sub (hash<SocketPollResultInfo> result) {
                        handleConnectionResult(result, uh, cinfo.state);
                    });
            }
            hash<SocketPollOperationInfo> submit_info = <SocketPollOperationInfo>{
                "sock": cinfo.sock,
                "spop": submit_op,
                "owner": owner,
                "to": timeout,
                # Force onComplete dispatch for operations with HttpConnectionPollOperationBase
                # or Http3ServerPollOperation — their onComplete overrides may not be detected
                # by findMethod() due to C++/Qore multiple inheritance resolution order
                "has_on_complete": True,
            };
            controller.submit(submit_info, True);  # replace=True since connection key may be reused
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::ERROR, "Failed to submit connection op for %s (state=%y): %s: %s",
                uh, cinfo.state, ex.err, ex.desc);
            {
                AutoWriteLock al(m);
                remove connections{uh};
            }
            # Do not close shared UDP socket for HTTP/3 connections
            if (cinfo.state != HttpConnectionState::Http3
                    && cinfo.state != HttpConnectionState::Http3Sending) {
                try { cinfo.sock.close(); } catch () {}
            }
        }
    }

    #! Handles the result of a completed connection poll operation
    /** Called by the onComplete callback of connection-phase poll operations (SSL handshake,
        header read, idle wait, keep-alive, WebSocket, SSE, HTTP/2, etc.) when the operation
        completes, times out, or encounters an error. This method:
        -# If the operation was canceled, closes the socket and removes the connection
        -# If the connection has been removed (e.g. during shutdown), returns immediately
        -# On error, handles state-specific error recovery (e.g. SSE timeout resubmission)
           and delegates to @ref handleConnectionError() for fatal errors
        -# On success, dispatches to the appropriate state handler based on \a orig_state:
           - \c Idle: checks for timeout, data, or close via @ref handleIdleTimeout(),
             @ref handleIdleDataReady(), @ref handleConnectionClosed()
           - \c KeepAlive: checks for timeout, headers ready, or close
           - \c ReadHeader / \c SendAndReadHeader: processes completed headers
           - \c SslHandshake: starts header reading after successful handshake
           - \c Sse / \c WebSocket / \c Http2 / \c Http3: delegates to streaming handlers

        @param result the poll result containing completion status or error information
        @param uh the unique hash identifying the connection socket
        @param orig_state the @ref HttpConnectionState that was active when the operation
            was submitted; used to dispatch to the correct handler

        @note This method runs on an async callback dispatcher worker and must not block.
    */
    handleConnectionResult(hash<SocketPollResultInfo> result, string uh,
            string orig_state) {
        if (!result) {
            log(LoggerLevel::ERROR, "handleConnectionResult: null result for %s state=%y", uh,
                orig_state);
            return;
        }
        if (result.canceled) {
            log(LoggerLevel::DEBUG, "connection op canceled: %s state=%y", uh, orig_state);
            # Close the socket so the remote peer detects the disconnection
            *hash<HttpActiveConnectionInfo> cancel_cinfo;
            {
                AutoWriteLock al(m);
                cancel_cinfo = remove connections{uh};
            }
            if (cancel_cinfo && cancel_cinfo.sock) {
                try {
                    cancel_cinfo.sock.close();
                } catch () {
                    # ignore close errors
                }
            }
            return;
        }

        *hash<HttpActiveConnectionInfo> cinfo;
        {
            AutoReadLock al(m);
            cinfo = connections{uh};
        }

        if (!cinfo) {
            log(LoggerLevel::DEBUG, "connection gone for %s during %y callback", uh, orig_state);
            return;
        }

        if (result.ex) {
            # Handle error — but for SSE, timeouts are expected (periodic polling)
            if (orig_state == HttpConnectionState::Sse
                    && (result.ex.err == "SOCKET-POLL-TIMEOUT"
                        || result.ex.err == "SOCKET-TIMEOUT")) {
                # Resubmit for continued polling
                submitConnectionOp(uh, cinfo);
                return;
            }
            log(LoggerLevel::DEBUG, "handleConnectionResult: error for %s state=%y: %s: %s",
                uh, orig_state, result.ex.err, result.ex.desc);
            handleConnectionError(uh, cinfo, orig_state, result.ex);
            return;
        }

        # Dispatch based on the state that was active when the operation was submitted
        switch (orig_state) {
            case HttpConnectionState::Idle: {
                HttpIdlePollOperation op = result.spop;
                if (op.timedOut()) {
                    handleIdleTimeout(uh, cinfo);
                } else if (op.hasData()) {
                    handleIdleDataReady(uh, cinfo);
                } else {
                    handleConnectionClosed(uh, cinfo);
                }
                break;
            }
            case HttpConnectionState::KeepAlive: {
                # Opt 1: combined idle + header poll operation
                HttpKeepAlivePollOperation op = result.spop;
                if (op.timedOut()) {
                    handleIdleTimeout(uh, cinfo);
                } else if (op.headersReady()) {
                    # Headers fully read — dispatch directly without round-trip
                    hash<auto> output = op.getOutput();
                    handleHeaderComplete(uh, cinfo, output);
                } else {
                    handleConnectionClosed(uh, cinfo);
                }
                break;
            }
            case HttpConnectionState::ReadHeader: {
                hash<auto> output = result.spop.getOutput();
                handleHeaderComplete(uh, cinfo, output);
                break;
            }
            case HttpConnectionState::Sse: {
                HttpSseStreamPollOperation op = result.spop;
                if (op.isDisconnected()) {
                    handleSseDisconnect(uh, cinfo, op);
                } else {
                    # Re-submit for continued polling (picks up queued events)
                    submitConnectionOp(uh, cinfo);
                }
                break;
            }
            case HttpConnectionState::WebSocket: {
                HttpWebSocketPollOperation op = result.spop;
                if (op.goalReached()) {
                    handleWebSocketClose(uh, cinfo, op);
                }
                break;
            }
            case HttpConnectionState::Http2: {
                Http2PollOperation h2op = result.spop;
                if (h2op.goalReached()) {
                    handleHttp2RequestReady(uh, cinfo, h2op);
                } else if (h2op.isClosed() || h2op.hasError()) {
                    handleHttp2ConnectionClosed(uh, cinfo, h2op);
                }
                break;
            }
            case HttpConnectionState::Http2Sending: {
                Http2PollOperation h2op = result.spop;
                if (h2op.goalReached()) {
                    handleHttp2SendComplete(uh, cinfo, h2op);
                } else if (h2op.isClosed() || h2op.hasError()) {
                    handleHttp2ConnectionClosed(uh, cinfo, h2op);
                }
                break;
            }
            case HttpConnectionState::Http2WebSocket: {
                Http2WebSocketStream ws_stream = cast<Http2WebSocketStream>(result.spop);
                if (!ws_stream.isOpen()) {
                    handleHttp2WebSocketClose(uh, cinfo, ws_stream);
                }
                break;
            }
            case HttpConnectionState::Http3: {
                Http3ServerPollOperation h3op = result.spop;
                if (h3op.goalReached()) {
                    *hash<auto> request = h3op.getOutput();
                    if (request) {
                        # Get listener info by direct key lookup — for HTTP/3,
                        # cinfo.sock IS the shared UDP listener socket
                        hash<HttpListenerInfo> linfo;
                        {
                            AutoReadLock al(m);
                            string lkey = cinfo.sock.uniqueHash();
                            linfo = listeners{lkey};
                        }
                        if (linfo) {
                            handleHttp3Request(linfo, h3op, request, cinfo.sock, cinfo.target, uh);
                        }
                    } else {
                        # No request, continue reading
                        h3op.continueReading();
                        hash<HttpActiveConnectionInfo> new_cinfo;
                        {
                            AutoWriteLock al(m);
                            if (!connections.hasKey(uh)) {
                                return;
                            }
                            hash<HttpActiveConnectionInfo> conn_info = cast<hash<HttpActiveConnectionInfo>>(connections{uh});
                            conn_info.stream_op = h3op;
                            conn_info.state = HttpConnectionState::Http3;
                            new_cinfo = connections{uh} = conn_info;
                        }
                        submitConnectionOp(uh, new_cinfo);
                    }
                } else if (h3op.isClosed() || h3op.hasError()) {
                    log(LoggerLevel::DEBUG, "HTTP/3 connection closed: %y", uh);
                    # Notify stream listeners (e.g., WebSocketStreamProcessor) so
                    # handler threads blocked in stream_done.waitForZero() wake up
                    h3op.notifyStreamListenersClosed();
                    signalPersistentQueuesForPrefix(uh + ":");
                    {
                        AutoWriteLock al(m);
                        remove connections{uh};
                    }
                }
                break;
            }
            case HttpConnectionState::Http3Sending: {
                Http3ServerPollOperation h3op = result.spop;
                if (h3op.goalReached()) {
                    # Send complete, go back to reading
                    h3op.startReadNextRequest();
                    hash<HttpActiveConnectionInfo> new_cinfo;
                    {
                        AutoWriteLock al(m);
                        if (!connections.hasKey(uh)) {
                            return;
                        }
                        hash<HttpActiveConnectionInfo> conn_info = cast<hash<HttpActiveConnectionInfo>>(connections{uh});
                        conn_info.stream_op = h3op;
                        conn_info.state = HttpConnectionState::Http3;
                        new_cinfo = connections{uh} = conn_info;
                    }
                    submitConnectionOp(uh, new_cinfo);
                } else if (h3op.isClosed() || h3op.hasError()) {
                    log(LoggerLevel::DEBUG, "HTTP/3 send error: %y", uh);
                    {
                        AutoWriteLock al(m);
                        remove connections{uh};
                    }
                }
                break;
            }
            case HttpConnectionState::SendAndReadHeader: {
                # Fused send+idle+read header operation
                AbstractPollOperation fused_op = result.spop;
                string state = fused_op.getState();
                if (state == "header-complete") {
                    # Headers fully read — dispatch to handler
                    hash<auto> output = fused_op.getOutput();
                    handleHeaderComplete(uh, cinfo, output);
                } else if (state == "timeout") {
                    # Idle timeout expired
                    handleIdleTimeout(uh, cinfo);
                } else {
                    # Error or unexpected state — close connection
                    handleConnectionClosed(uh, cinfo);
                }
                break;
            }
            case HttpConnectionState::SendStreamAndReadHeader: {
                # Fused InputStream stream send + idle + read header operation
                AbstractPollOperation fused_op = result.spop;
                string state = fused_op.getState();
                if (state == "stream-complete") {
                    # Headers fully read — dispatch to handler
                    hash<auto> output = fused_op.getOutput();
                    handleHeaderComplete(uh, cinfo, output);
                } else if (state == "timeout") {
                    # Idle timeout expired
                    handleIdleTimeout(uh, cinfo);
                } else {
                    # Error or unexpected state — close connection
                    handleConnectionClosed(uh, cinfo);
                }
                break;
            }
            case HttpConnectionState::SendStreamResponse: {
                # Async response send completed
                AbstractPollOperation send_op = result.spop;
                string state = send_op.getState();
                if (state == "stream-complete" || state == "sent") {
                    # Check if there's a pending streaming operation to start
                    # (SSE/WS headers were sent; now start the streaming op)
                    if (cinfo.pending_stream_op) {
                        hash<HttpActiveConnectionInfo> new_cinfo;
                        {
                            AutoWriteLock al(m);
                            hash<HttpActiveConnectionInfo> upd =
                                cast<hash<HttpActiveConnectionInfo>>(connections{uh});
                            upd.state = cinfo.pending_stream_type;
                            upd.stream_op = cinfo.pending_stream_op;
                            remove upd.pending_stream_type;
                            remove upd.pending_stream_op;
                            new_cinfo = connections{uh} = upd;
                        }
                        submitConnectionOp(uh, new_cinfo);
                        # Wake the socket to flush any data queued during
                        # header send (SSE events, WS frames)
                        wakeSocketIfPresent(cinfo.sock, uh, "HTTP/2 submit failure resume");
                    } else {
                        # No pending streaming — close connection
                        try { cinfo.sock.close(); } catch () {}
                        {
                            AutoWriteLock al(m);
                            remove connections{uh};
                        }
                    }
                } else {
                    # Error or unexpected state — close connection
                    handleConnectionClosed(uh, cinfo);
                }
                break;
            }
            case HttpConnectionState::ReadBody: {
                # Async H1 body read completed
                AbstractPollOperation body_op = result.spop;
                string state = body_op.getState();
                if (state == "done") {
                    # Body read complete — dispatch to handler with full request
                    *binary body_data = body_op.getOutput();
                    handleBodyComplete(uh, cinfo, body_data);
                } else {
                    # Error reading body — close connection
                    handleConnectionClosed(uh, cinfo);
                }
                break;
            }
            default: {
                log(LoggerLevel::ERROR, "Unhandled connection state %y for %y", orig_state, uh);
                handleConnectionClosed(uh, cinfo);
                break;
            }
        }
    }

    #! Handles connection errors
    private handleConnectionError(string uh, hash<HttpActiveConnectionInfo> cinfo,
            string state, hash<ExceptionInfo> ex) {
        @debug {
            if (getenv("QORE_ASYNCIO_TRACE")) {
                log(LoggerLevel::INFO,
                    "connection error: state=%y conn=%y err=%s desc=%s",
                    state, uh, ex.err, ex.desc);
            }
        }

        if ((state == HttpConnectionState::Idle || state == HttpConnectionState::KeepAlive
                || state == HttpConnectionState::ReadHeader
                || state == HttpConnectionState::SendAndReadHeader
                || state == HttpConnectionState::Http2Sending
                || state == HttpConnectionState::Http3
                || state == HttpConnectionState::Http3Sending)
            && ex.err == "SOCKET-HTTP-ERROR"
            && ex.desc =~ /remote end closed connection/) {
            log(LoggerLevel::DEBUG, "Connection closed by peer during read: %y", uh);
            handleConnectionClosed(uh, cinfo);
            return;
        }

        {
            AutoWriteLock al(m);
            remove connections{uh};
        }

        dispatchOnError(cinfo.target, cinfo.sock, ex, cinfo.listener, cinfo.cx, uh, "connection error");
        # Do not close shared UDP socket for HTTP/3 connections
        if (state != HttpConnectionState::Http3 && state != HttpConnectionState::Http3Sending) {
            try { cinfo.sock.close(); } catch () {}
        }
    }

    #! Handles header read completion
    private handleHeaderComplete(string uh, hash<HttpActiveConnectionInfo> cinfo, hash<auto> output) {
        string dispatch_uh = uh;
        Socket dispatch_sock = cinfo.sock;
        HttpAsyncTarget dispatch_target = cinfo.target;
        *HttpServer::HttpListenerInterface dispatch_listener = cinfo.listener;
        hash<auto> dispatch_cx = cinfo.cx;
        bool dispatch_ssl = cinfo.ssl;
        string dispatch_state = cinfo.state;
        timeout dispatch_ttl = cinfo.ttl;
        *HttpKeepAlivePollOperation dispatch_keep_alive_op = cinfo.keep_alive_op;
        hash<HttpActiveConnectionInfo> dispatch_cinfo = <HttpActiveConnectionInfo>{
            "sock": dispatch_sock,
            "listener": dispatch_listener,
            "cx": dispatch_cx,
            "target": dispatch_target,
            "state": dispatch_state,
            "ttl": dispatch_ttl,
            "keep_alive_op": dispatch_keep_alive_op,
            "ssl": dispatch_ssl,
        };

        output.info = addPersistentQueueClosures(output.info);
        hash<HttpConnectionInfo> conn_info = <HttpConnectionInfo>{
            "sock": dispatch_sock,
            "hdr": output.hdr,
            "header_info": output.info,
            "listener": dispatch_listener,
            "cx": dispatch_cx,
            "keep_alive": True,
            "ssl": dispatch_ssl,
        };

        log(LoggerLevel::DEBUG, "Header complete for keep-alive: %y", dispatch_uh);

        # Check if H1 request body should be read by I/O controller
        # Only pre-read Content-Length bodies — NOT chunked. Chunked encoding
        # is designed for streaming; handlers (AbstractStreamRequest) read
        # chunked data incrementally via callbacks. Pre-reading would consume
        # the data before the handler can process it.
        # Expect: 100-continue requests fall through to the handler (which sends
        # 100-continue and reads the body synchronously) to avoid a round-trip.
        if (!output.info.http2 && !output.info.http3) {
            auto content_length = output.hdr."content-length";
            bool has_expect = (output.hdr.expect =~ /100-continue/i);
            bool has_body = (content_length > 0);

            if (has_body && !has_expect) {
                try {
                    softint cl = content_length;
                    bool conn_close = (output.hdr.connection =~ /close/i);
                    AbstractPollOperation body_op = dispatch_sock.startPollReadHttpBody(cl, False,
                        conn_close);
                    # Stay in controller tracking — socket is managed during body read
                    hash<HttpActiveConnectionInfo> new_cinfo;
                    {
                        AutoWriteLock al(m);
                        hash<HttpActiveConnectionInfo> upd =
                            cast<hash<HttpActiveConnectionInfo>>(connections{dispatch_uh});
                        upd.state = HttpConnectionState::ReadBody;
                        upd.stream_op = body_op;
                        upd.pending_hdr = output.hdr;
                        upd.pending_header_info = output.info;
                        new_cinfo = connections{dispatch_uh} = upd;
                    }
                    submitConnectionOp(dispatch_uh, new_cinfo);
                    return;
                } catch (hash<ExceptionInfo> ex) {
                    log(LoggerLevel::ERROR, "Error starting body read for %y: %s: %s",
                        dispatch_uh, ex.err, ex.desc);
                    # Fall through to normal dispatch — handler will read body
                }
            }
        }

        if (routeRequestToPersistentQueue(dispatch_uh, conn_info)) {
            return;
        }

        # Remove from our tracking
        {
            AutoWriteLock al(m);
            remove connections{dispatch_uh};
        }

        # Try lightweight inline path first (no body, handler opts in via canHandleInline).
        # This runs directly on the call_dispatcher worker — no handler_pool thread hop.
        # The inline path skips context setup, auth, and logging for maximum throughput.
        try {
            *hash<HttpRequestResultInfo> inline_result = dispatch_target.tryInlineRequest(conn_info);
            if (inline_result) {
                handleRequestResult(dispatch_sock, dispatch_cinfo, dispatch_cx,
                    inline_result, dispatch_target);
                return;
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::ERROR, "Error handling inline request: %s: %s", ex.err, ex.desc);
            dispatchOnError(dispatch_target, dispatch_sock, ex, dispatch_listener, dispatch_cx,
                dispatch_uh, "inline request");
            if (dispatch_state != HttpConnectionState::Http3
                    && dispatch_state != HttpConnectionState::Http3Sending) {
                try { dispatch_sock.close(); } catch () {}
            }
            return;
        }

        # Give the target a protocol-independent admission point before the shared handler queue.
        try {
            *hash<HttpRequestResultInfo> prepared_result =
                dispatch_target.prepareHttpRequest(conn_info);
            if (prepared_result) {
                handleRequestResult(dispatch_sock, dispatch_cinfo, dispatch_cx,
                    prepared_result, dispatch_target);
                return;
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::ERROR, "Error preparing request for %y: %s: %s",
                dispatch_uh, ex.err, ex.desc);
            dispatchOnError(dispatch_target, dispatch_sock, ex, dispatch_listener,
                dispatch_cx, dispatch_uh, "request preparation");
            if (dispatch_state != HttpConnectionState::Http3
                    && dispatch_state != HttpConnectionState::Http3Sending) {
                try { dispatch_sock.close(); } catch () {}
            }
            return;
        }

        # The inline and preparation callbacks returned NOTHING — dispatch to the handler_pool.
        # onHttpRequest / processNativeRequest may read the request body or
        # block while waiting for controller-backed socket I/O, so it must run
        # on a handler thread, not the call_dispatcher worker.
        code<nothing()> request_handler = sub () {
            try {
                hash<HttpRequestResultInfo> result = dispatch_target.onHttpRequest(conn_info);
                handleRequestResult(dispatch_sock, dispatch_cinfo, dispatch_cx, result,
                    dispatch_target);
            } catch (hash<ExceptionInfo> ex) {
                log(LoggerLevel::ERROR, "Error handling keep-alive request for %y: %s: %s",
                    dispatch_uh, ex.err, ex.desc);
                dispatchOnError(dispatch_target, dispatch_sock, ex, dispatch_listener, dispatch_cx,
                    dispatch_uh, "keep-alive request");
                if (dispatch_state != HttpConnectionState::Http3
                        && dispatch_state != HttpConnectionState::Http3Sending) {
                    try { dispatch_sock.close(); } catch () {}
                }
            }
        };

        try {
            submitToHandlerPool(dispatch_target, request_handler);
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::ERROR, "Failed to submit handler for %y: %s: %s",
                dispatch_uh, ex.err, ex.desc);
            try {
                dispatch_target.cancelPreparedHttpRequest(conn_info);
            } catch (hash<ExceptionInfo> cancel_ex) {
                log(LoggerLevel::ERROR, "Failed to cancel request preparation for %y: %s: %s",
                    dispatch_uh, cancel_ex.err, cancel_ex.desc);
            }
            if (dispatch_state != HttpConnectionState::Http3
                    && dispatch_state != HttpConnectionState::Http3Sending) {
                try { dispatch_sock.close(); } catch () {}
            }
        }
    }

    #! Handles completion of H1 async body reading
    /** Dispatches the request to the handler with the pre-read body stored in
        \c cx."header-info".body (same convention as H2/H3 paths).

        @param uh unique hash of the socket
        @param cinfo the connection info (must have pending_hdr/pending_header_info set)
        @param body_data the body data read by the I/O controller (may be NOTHING for empty body)
    */
    private handleBodyComplete(string uh, hash<HttpActiveConnectionInfo> cinfo, *binary body_data) {
        string dispatch_uh = uh;
        Socket dispatch_sock = cinfo.sock;
        HttpAsyncTarget dispatch_target = cinfo.target;
        *HttpServer::HttpListenerInterface dispatch_listener = cinfo.listener;
        hash<auto> dispatch_cx = cinfo.cx;
        bool dispatch_ssl = cinfo.ssl;
        string dispatch_state = cinfo.state;
        timeout dispatch_ttl = cinfo.ttl;
        *HttpKeepAlivePollOperation dispatch_keep_alive_op = cinfo.keep_alive_op;
        hash<HttpActiveConnectionInfo> dispatch_cinfo = <HttpActiveConnectionInfo>{
            "sock": dispatch_sock,
            "listener": dispatch_listener,
            "cx": dispatch_cx,
            "target": dispatch_target,
            "state": dispatch_state,
            "ttl": dispatch_ttl,
            "keep_alive_op": dispatch_keep_alive_op,
            "ssl": dispatch_ssl,
        };

        # Build the connection info from saved headers (same as handleHeaderComplete)
        hash<HttpConnectionInfo> conn_info = <HttpConnectionInfo>{
            "sock": dispatch_sock,
            "hdr": cinfo.pending_hdr,
            "header_info": cinfo.pending_header_info,
            "listener": dispatch_listener,
            "cx": dispatch_cx,
            "keep_alive": True,
            "ssl": dispatch_ssl,
        };

        # Store the pre-read body for the handler (same convention as H2/H3)
        if (body_data) {
            conn_info.header_info.body = body_data;
        }

        log(LoggerLevel::DEBUG, "Body read complete for %y: %d bytes", uh,
            body_data ? body_data.size() : 0);

        if (routeRequestToPersistentQueue(dispatch_uh, conn_info)) {
            return;
        }

        # Remove from our tracking — handler thread owns the socket now
        {
            AutoWriteLock al(m);
            remove connections{dispatch_uh};
        }

        # Give the target the same pre-dispatch admission point as a bodyless request.
        try {
            *hash<HttpRequestResultInfo> prepared_result =
                dispatch_target.prepareHttpRequest(conn_info);
            if (prepared_result) {
                handleRequestResult(dispatch_sock, dispatch_cinfo, dispatch_cx,
                    prepared_result, dispatch_target);
                return;
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::ERROR, "Error preparing request after body read for %y: %s: %s",
                dispatch_uh, ex.err, ex.desc);
            dispatchOnError(dispatch_target, dispatch_sock, ex, dispatch_listener,
                dispatch_cx, dispatch_uh, "request preparation after body read");
            try { dispatch_sock.close(); } catch () {}
            return;
        }

        # Dispatch to handler pool (same as handleHeaderComplete)
        code<nothing()> request_handler = sub () {
            try {
                hash<HttpRequestResultInfo> result = dispatch_target.onHttpRequest(conn_info);
                handleRequestResult(dispatch_sock, dispatch_cinfo, dispatch_cx, result,
                    dispatch_target);
            } catch (hash<ExceptionInfo> ex) {
                log(LoggerLevel::ERROR, "Error handling request after body read for %y: %s: %s",
                    dispatch_uh, ex.err, ex.desc);
                dispatchOnError(dispatch_target, dispatch_sock, ex, dispatch_listener, dispatch_cx,
                    dispatch_uh, "request after body read");
                try { dispatch_sock.close(); } catch () {}
            }
        };

        try {
            submitToHandlerPool(dispatch_target, request_handler);
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::ERROR, "Failed to submit handler after body read for %y: %s: %s",
                dispatch_uh, ex.err, ex.desc);
            try {
                dispatch_target.cancelPreparedHttpRequest(conn_info);
            } catch (hash<ExceptionInfo> cancel_ex) {
                log(LoggerLevel::ERROR, "Failed to cancel request preparation after body read for %y: %s: %s",
                    dispatch_uh, cancel_ex.err, cancel_ex.desc);
            }
            try { dispatch_sock.close(); } catch () {}
        }
    }

    #! Handles the result of request processing
    # @param linfo hash<HttpActiveConnectionInfo> — typed as auto to avoid circular
    #   dependency on HttpActiveConnectionInfo (defined in this module); callers always
    #   pass cinfo which is hash<HttpActiveConnectionInfo>
    private handleRequestResult(Socket sock, auto linfo, hash<auto> cx,
            hash<HttpRequestResultInfo> result, HttpAsyncTarget target) {
        string uh = sock.uniqueHash();

        # NOTE: send_callback responses are NOT converted to InputStream here.
        # send_callback is a streaming producer (may block waiting for data)
        # that must run on the handler thread.  For H1, the handler thread
        # owns the socket exclusively and sendHTTPResponseWithCallback streams
        # correctly.  For H2/H3 multiplexed paths, the conversion happens in
        # handleHttp2MultiplexedResult / handleHttp3MultiplexedResult.

        # Non-I/O-thread-safe InputStream response body: sync-stream it on
        # this handler pool thread via sendHTTPResponseWithCallback.  H1.x
        # is one-connection-per-handler while in-flight, so the async I/O
        # controller doesn't touch this socket until we hand it back via
        # addIdleConnection() (keep-alive) or close it.  read() runs on
        # this handler pool worker — never on the async I/O thread —
        # preserving the architectural rule.
        #
        # This replaces the previous "materialize on handler thread, then
        # submit I/O-thread streaming poll op" pattern, which buffered the
        # whole body in memory.  True streaming with bounded memory.
        if (result.input_stream && !result.input_stream.isIoThreadSafe()) {
            string status_msg = result.status_message ?? "Unknown";
            timeout send_timeout = result.send_timeout ?? 30s;
            InputStream is = result.input_stream;
            try {
                is.reassignThread();
                sock.sendHTTPResponseWithCallback(
                    auto sub () { return is.read(65536); },
                    result.code, status_msg, "1.1", result.hdr ?? {}, send_timeout);
            } catch (hash<ExceptionInfo> ex) {
                log(LoggerLevel::ERROR,
                    "HTTP/1.x stream send error for %y: %s: %s", uh, ex.err, ex.desc);
                try { is.unassignThread(); } catch () {}
                try { sock.close(); } catch () {}
                return;
            }
            try { is.unassignThread(); } catch () {}
            remove result.input_stream;

            # Response fully sent.  Either keep-alive or close.
            if (result.close || !result.return_to_idle || !sock.isOpen()) {
                try { sock.shutdown(); sock.close(); } catch () {}
                return;
            }
            timeout ttl = result.ttl ?? DefaultIdleTimeout;
            addIdleConnection(sock, linfo.listener, cx, target, ttl);
            return;
        }

        # Opt #3a: InputStream + keep-alive → fused stream send + idle + read header
        # Only use fused I/O-thread path for I/O-thread-safe InputStreams (C++ native);
        # Qore InputStreams (CallbackInputStream etc.) converted to body/send_callback above
        if (result.code && !result.streaming && result.input_stream && !result.send_callback
                && !result.close && result.return_to_idle && result.hdr."Content-Length"
                && result.input_stream.isIoThreadSafe()) {
            try {
                string status_msg = result.status_message ?? "Unknown";
                timeout ttl = result.ttl ?? DefaultIdleTimeout;
                AbstractPollOperation fused_op =
                    sock.startPollSendHttpResponseWithStreamAndReadHeader(
                        result.code, status_msg, "1.1", result.hdr ?? {},
                        result.input_stream, int(result.hdr."Content-Length"), ttl);
                hash<HttpActiveConnectionInfo> new_cinfo = <HttpActiveConnectionInfo>{
                    "sock": sock,
                    "listener": linfo.listener,
                    "cx": cx,
                    "target": target,
                    "state": HttpConnectionState::SendStreamAndReadHeader,
                    "stream_op": fused_op,
                    "ttl": ttl,
                    "ssl": linfo.ssl,
                };
                hash<HttpActiveConnectionInfo> cinfo;
                {
                    AutoWriteLock al(m);
                    cinfo = connections{uh} = new_cinfo;
                }
                submitConnectionOp(uh, cinfo);
            } catch (hash<ExceptionInfo> ex) {
                log(LoggerLevel::ERROR, "Error starting fused stream send+read for %y: %s: %s",
                    uh, ex.err, ex.desc);
                try { sock.close(); } catch () {}
            }
            return;
        }

        # Opt #3b: InputStream + close → non-fused stream send (I/O-thread-safe only)
        if (result.code && !result.streaming && result.input_stream && !result.send_callback
                && result.close && result.input_stream.isIoThreadSafe()) {
            try {
                string status_msg = result.status_message ?? "Unknown";
                AbstractPollOperation send_op;
                if (result.hdr."Content-Length") {
                    send_op = sock.startPollSendHttpResponseWithStream(
                        result.code, status_msg, "1.1", result.hdr ?? {},
                        result.input_stream, int(result.hdr."Content-Length"));
                } else {
                    # Chunked stream + close
                    send_op = sock.startPollSendHttpChunkedResponseWithStream(
                        result.code, status_msg, "1.1", result.hdr ?? {},
                        result.input_stream);
                }
                hash<HttpActiveConnectionInfo> new_cinfo = <HttpActiveConnectionInfo>{
                    "sock": sock,
                    "listener": linfo.listener,
                    "cx": cx,
                    "target": target,
                    "state": HttpConnectionState::SendStreamResponse,
                    "stream_op": send_op,
                    "ssl": linfo.ssl,
                };
                hash<HttpActiveConnectionInfo> cinfo;
                {
                    AutoWriteLock al(m);
                    cinfo = connections{uh} = new_cinfo;
                }
                submitConnectionOp(uh, cinfo);
            } catch (hash<ExceptionInfo> ex) {
                log(LoggerLevel::ERROR, "Error starting stream send for %y: %s: %s",
                    uh, ex.err, ex.desc);
                try { sock.close(); } catch () {}
            }
            return;
        }

        # Opt #3: Use fused send+idle+read operation when possible
        # This bypasses the blocking sendHTTPResponse() on the handler thread and
        # combines send + idle wait + header read into a single non-blocking operation
        # on the I/O thread.
        # Requirements: has response, not streaming, not using InputStream or send_callback,
        # returning to idle
        if (result.code && !result.streaming && !result.input_stream && !result.send_callback
                && !result.close && result.return_to_idle) {
            try {
                string status_msg = result.status_message ?? "Unknown";
                timeout ttl = result.ttl ?? DefaultIdleTimeout;
                AbstractPollOperation fused_op = sock.startPollSendHttpResponseAndReadHeader(
                    result.code, status_msg, "1.1", result.hdr ?? {}, result.body, ttl);
                hash<HttpActiveConnectionInfo> new_cinfo = <HttpActiveConnectionInfo>{
                    "sock": sock,
                    "listener": linfo.listener,
                    "cx": cx,
                    "target": target,
                    "state": HttpConnectionState::SendAndReadHeader,
                    "stream_op": fused_op,
                    "ttl": ttl,
                    "ssl": linfo.ssl,
                };
                hash<HttpActiveConnectionInfo> cinfo;
                {
                    AutoWriteLock al(m);
                    cinfo = connections{uh} = new_cinfo;
                }
                submitConnectionOp(uh, cinfo);
            } catch (hash<ExceptionInfo> ex) {
                log(LoggerLevel::ERROR, "Error starting fused send+read for %y: %s: %s",
                    uh, ex.err, ex.desc);
                try { sock.close(); } catch () {}
            }
            return;
        }

        # Opt #3d: InputStream + chunked + keep-alive → fused chunked stream send + idle + read header
        # (I/O-thread-safe only; non-I/O-safe InputStreams already converted above)
        if (result.code && !result.streaming && result.input_stream && !result.send_callback
                && !result.close && result.return_to_idle && result.chunked
                && result.input_stream.isIoThreadSafe()) {
            try {
                string status_msg = result.status_message ?? "Unknown";
                timeout ttl = result.ttl ?? DefaultIdleTimeout;
                AbstractPollOperation fused_op =
                    sock.startPollSendHttpChunkedResponseAndReadHeader(
                        result.code, status_msg, "1.1", result.hdr ?? {},
                        result.input_stream, ttl);
                hash<HttpActiveConnectionInfo> new_cinfo = <HttpActiveConnectionInfo>{
                    "sock": sock,
                    "listener": linfo.listener,
                    "cx": cx,
                    "target": target,
                    "state": HttpConnectionState::SendStreamAndReadHeader,
                    "stream_op": fused_op,
                    "ttl": ttl,
                    "ssl": linfo.ssl,
                };
                hash<HttpActiveConnectionInfo> cinfo;
                {
                    AutoWriteLock al(m);
                    cinfo = connections{uh} = new_cinfo;
                }
                submitConnectionOp(uh, cinfo);
            } catch (hash<ExceptionInfo> ex) {
                log(LoggerLevel::ERROR, "Error starting chunked fused send+read for %y: %s: %s",
                    uh, ex.err, ex.desc);
                try { sock.close(); } catch () {}
            }
            return;
        }

        # Send HTTP/1.x response if response data is present and not streaming
        if (result.code && !result.streaming) {
            try {
                string status_msg = result.status_message ?? "Unknown";
                if (result.send_callback) {
                    # Drain the send_callback on the handler thread (the
                    # callback is user code that must run here), then submit
                    # the materialized body via controller-backed async I/O.
                    # This keeps Qore callback execution off the I/O thread
                    # and preserves the H1 connection state-machine paths
                    # below for close and keep-alive handling.
                    binary buf;
                    while (True) {
                        auto chunk = result.send_callback();
                        if (!exists chunk) {
                            break;
                        }
                        buf += binary(chunk);
                    }
                    result.body = buf.size() > 0 ? buf : NOTHING;
                    remove result.send_callback;
                    remove result.hdr."Transfer-Encoding";

                    # For keep-alive, use fused send+read (same as Opt #3)
                    if (result.return_to_idle && !result.close) {
                        timeout ttl = result.ttl ?? DefaultIdleTimeout;
                        AbstractPollOperation fused_op =
                            sock.startPollSendHttpResponseAndReadHeader(
                                result.code, status_msg, "1.1", result.hdr ?? {},
                                result.body, ttl);
                        hash<HttpActiveConnectionInfo> new_cinfo = <HttpActiveConnectionInfo>{
                            "sock": sock,
                            "listener": linfo.listener,
                            "cx": cx,
                            "target": target,
                            "state": HttpConnectionState::SendAndReadHeader,
                            "stream_op": fused_op,
                            "ttl": ttl,
                            "ssl": linfo.ssl,
                        };
                        hash<HttpActiveConnectionInfo> cinfo;
                        {
                            AutoWriteLock al(m);
                            cinfo = connections{uh} = new_cinfo;
                        }
                        submitConnectionOp(uh, cinfo);
                        return;
                    }
                }
                {
                    # Use async poll operation for all other H1 responses
                    AbstractPollOperation send_op = sock.startPollSendHttpResponse(
                        result.code, status_msg, "1.1", result.hdr ?? {},
                        result.body);
                    hash<HttpActiveConnectionInfo> new_cinfo = <HttpActiveConnectionInfo>{
                        "sock": sock,
                        "listener": linfo.listener,
                        "cx": cx,
                        "target": target,
                        "state": HttpConnectionState::SendStreamResponse,
                        "stream_op": send_op,
                        "ssl": linfo.ssl,
                    };
                    hash<HttpActiveConnectionInfo> cinfo;
                    {
                        AutoWriteLock al(m);
                        cinfo = connections{uh} = new_cinfo;
                    }
                    submitConnectionOp(uh, cinfo);
                    return;
                }
            } catch (hash<ExceptionInfo> ex) {
                log(LoggerLevel::ERROR, "Error sending HTTP/1.x response for %y: %s: %s",
                    uh, ex.err, ex.desc);
                try { sock.close(); } catch () {}
                return;
            }
        }

        if (result.close) {
            try { sock.close(); } catch () {}
            return;
        }

        if (result.streaming) {
            # For H1 SSE/WS: send HTTP response headers via async poll operation
            # before entering streaming mode
            string stream_state = result.stream_type;
            if (result.code) {
                try {
                    string status_msg = result.status_message ?? HttpServer::HttpCodes{result.code} ?? "OK";
                    AbstractPollOperation send_op = sock.startPollSendHttpResponse(
                        result.code, status_msg, "1.1", result.hdr ?? {});
                    # Store the streaming op info — handleConnectionResult will
                    # transition to streaming state after the header send completes
                    hash<HttpActiveConnectionInfo> cinfo;
                    {
                        AutoWriteLock al(m);
                        cinfo = connections{uh} = <HttpActiveConnectionInfo>{
                            "sock": sock,
                            "listener": linfo.listener,
                            "cx": cx,
                            "target": target,
                            "state": HttpConnectionState::SendStreamResponse,
                            "stream_op": send_op,
                            "ttl": result.ttl ?? DefaultIdleTimeout,
                            # Store pending streaming info for transition after header send
                            "pending_stream_type": stream_state,
                            "pending_stream_op": result.stream_operation,
                            "ssl": linfo.ssl,
                        };
                    }
                    submitConnectionOp(uh, cinfo);
                } catch (hash<ExceptionInfo> ex) {
                    log(LoggerLevel::ERROR, "Error sending SSE response for %y: %s: %s",
                        uh, ex.err, ex.desc);
                    try { sock.close(); } catch () {}
                }
            } else {
                hash<HttpActiveConnectionInfo> cinfo;
                {
                    AutoWriteLock al(m);
                    cinfo = connections{uh} = <HttpActiveConnectionInfo>{
                        "sock": sock,
                        "listener": linfo.listener,
                        "cx": cx,
                        "target": target,
                        "state": stream_state,
                        "stream_op": result.stream_operation,
                        "ttl": result.ttl ?? DefaultIdleTimeout,
                        "ssl": linfo.ssl,
                    };
                }
                submitConnectionOp(uh, cinfo);
            }
            return;
        }

        if (result.return_to_idle) {
            # Opt 5: reuse the keep-alive operation for persistent socket registration
            *HttpKeepAlivePollOperation reuse_op = linfo.keep_alive_op;
            addIdleConnection(sock, linfo.listener, cx, target,
                result.ttl ?? DefaultIdleTimeout, reuse_op);
        }
    }

    #! Handles idle timeout
    private handleIdleTimeout(string uh, hash<HttpActiveConnectionInfo> cinfo) {
        log(LoggerLevel::DEBUG, "Idle timeout: %y", uh);

        {
            AutoWriteLock al(m);
            remove connections{uh};
        }

        try {
            cinfo.target.onIdleTimeout(cinfo.sock, cinfo.listener, cinfo.cx);
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::ERROR, "onIdleTimeout callback threw for %s: %s: %s", uh, ex.err, ex.desc);
        }
    }

    #! Handles data ready on idle connection
    private handleIdleDataReady(string uh, hash<HttpActiveConnectionInfo> cinfo) {
        log(LoggerLevel::DEBUG, "Data ready on idle: %y", uh);

        # Opt 4: merge state update + stopping check into a single lock scope
        {
            AutoWriteLock al(m);
            if (stopping || !connections.hasKey(uh)) {
                remove connections{uh};
                try { cinfo.sock.close(); } catch () {}
                return;
            }
            # Cast to typed hash to update fields and assign back
            hash<HttpActiveConnectionInfo> conn_info = cast<hash<HttpActiveConnectionInfo>>(connections{uh});
            conn_info.state = HttpConnectionState::ReadHeader;
            cinfo = connections{uh} = conn_info;
        }

        submitConnectionOpUnlocked(uh, cinfo);
    }

    #! Handles connection closed
    private handleConnectionClosed(string uh, hash<HttpActiveConnectionInfo> cinfo) {
        log(LoggerLevel::INFO, "Connection closed: %y", uh);

        signalPersistentQueuesForConnection(uh);

        {
            AutoWriteLock al(m);
            remove connections{uh};
        }

        # Do NOT cancel the shared UDP socket for HTTP/3 connections — the
        # socket is shared across all QUIC sessions and cancelling it would
        # break other sessions' I/O.  HTTP/3 cleanup is handled by removing
        # the connection entry; the poll operation continues for other sessions.
        if (cinfo.state == HttpConnectionState::Http3
                || cinfo.state == HttpConnectionState::Http3Sending) {
            return;
        }

        # Cancel the poll operation from the I/O controller cache to release socket reference
        try {
            controller.cancel(cinfo.sock);
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::DEBUG, "Error cancelling operation for %y: %s: %s", uh, ex.err, ex.desc);
        }
    }

    #! Handles SSE disconnect
    /** Controller bookkeeping (state removal, socket close) runs synchronously on
        the calling thread; the user-visible \c onSseDisconnect callback is hopped
        onto a handler-pool thread via @ref submitToHandlerPool() so that user
        code never runs on the AsyncSocketIo callback dispatcher worker (which
        would risk deadlocks and bypass per-dispatch context setup).
    */
    private handleSseDisconnect(string uh, hash<HttpActiveConnectionInfo> cinfo,
            HttpSseStreamPollOperation op) {
        log(LoggerLevel::INFO, "SSE disconnect: %y", uh);

        {
            AutoWriteLock al(m);
            remove connections{uh};
        }

        # Close through the controller after the SSE poll result has returned.
        # This avoids re-entering synchronous close/cancel from the poll
        # operation's own in-flight continuePoll() worker.
        try {
            cinfo.sock.close();
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::DEBUG, "Error closing SSE socket for %y: %s: %s", uh, ex.err, ex.desc);
        }

        try {
            *code<nothing()> stream_close_callback = op.getCloseCallback();
            submitToHandlerPool(cinfo.target, sub () {
                if (stream_close_callback) {
                    try {
                        stream_close_callback();
                    } catch (hash<ExceptionInfo> ex) {
                        log(LoggerLevel::ERROR, "SSE close callback threw for %s: %s: %s",
                            uh, ex.err, ex.desc);
                    }
                }
                try {
                    cinfo.target.onSseDisconnect(cinfo.sock, cinfo.listener, cinfo.cx);
                } catch (hash<ExceptionInfo> ex) {
                    log(LoggerLevel::ERROR, "onSseDisconnect callback threw for %s: %s: %s",
                        uh, ex.err, ex.desc);
                }
            });
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::ERROR, "Failed to submit onSseDisconnect for %y: %s: %s",
                uh, ex.err, ex.desc);
        }
    }

    #! Handles WebSocket close
    /** Controller bookkeeping (state removal, poll-operation cancel) runs
        synchronously on the calling thread; the user-visible \c onWebSocketClose
        callback is hopped onto a handler-pool thread via
        @ref submitToHandlerPool() so that user code never runs on the
        AsyncSocketIo callback dispatcher worker (which would risk deadlocks
        and bypass per-dispatch context setup).  The close code/reason are
        captured before the submission so they remain available even if \c op
        is recycled before the handler runs.
    */
    private handleWebSocketClose(string uh, hash<HttpActiveConnectionInfo> cinfo,
            HttpWebSocketPollOperation op) {
        int close_code = op.getCloseCode();
        string close_reason = op.getCloseReason();
        log(LoggerLevel::INFO, "WebSocket close: %y (code: %d)", uh, close_code);

        {
            AutoWriteLock al(m);
            remove connections{uh};
        }

        # The terminal onComplete() that brought us here has already removed
        # the poll op from the async I/O controller cache and released its
        # socket reference.  Do not send a second cancel here: during teardown
        # it can race with controller autostop and leave the completion worker
        # waiting on a cancel that has no useful work left to do.

        try {
            submitToHandlerPool(cinfo.target, sub () {
                try {
                    cinfo.target.onWebSocketClose(cinfo.sock, close_code, close_reason,
                        cinfo.listener, cinfo.cx);
                } catch (hash<ExceptionInfo> ex) {
                    log(LoggerLevel::ERROR, "onWebSocketClose callback threw for %s: %s: %s",
                        uh, ex.err, ex.desc);
                }
            });
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::ERROR, "Failed to submit onWebSocketClose for %y: %s: %s",
                uh, ex.err, ex.desc);
        }
    }

    #! Handles HTTP/2 request ready
    /** All HTTP/2 connections use the multiplexed path: h2op stays on the I/O thread
        at all times.  When a request completes:
        1. The I/O thread extracts the request data and immediately re-submits h2op
           for continued reading (via continueReading() + submitConnectionOp).
        2. The request is dispatched to a handler_pool worker thread.
        3. The handler thread processes the request and submits a response via
           submitHttp2Response() (for regular requests) or manages the stream
           directly (for WebSocket/SSE/streaming).
        4. The I/O thread drives h2op (reading) and any pending sends simultaneously
           via event-union polling (POLLIN|POLLOUT).

        This allows true HTTP/2 multiplexing: multiple requests, WebSocket streams,
        SSE streams, and streaming responses can all coexist on a single connection.
    */
    private handleHttp2RequestReady(string uh, hash<HttpActiveConnectionInfo> cinfo,
            Http2PollOperation h2op) {
        *hash<auto> request = h2op.getOutput();
        if (!request) {
            # Could be a protocol-level rejection (RST_STREAM for invalid CONNECT, etc.)
            # or an empty read — continue reading instead of closing
            h2op.continueReading();
            hash<HttpActiveConnectionInfo> new_cinfo;
            {
                AutoWriteLock al(m);
                # Check if connection still exists
                if (!connections.hasKey(uh)) {
                    try { cinfo.sock.close(); } catch () {}
                    return;
                }
                # Cast to typed hash to update fields and assign back
                hash<HttpActiveConnectionInfo> conn_info = cast<hash<HttpActiveConnectionInfo>>(connections{uh});
                conn_info.stream_op = h2op;
                conn_info.state = HttpConnectionState::Http2;
                new_cinfo = connections{uh} = conn_info;
            }
            submitConnectionOp(uh, new_cinfo);
            return;
        }

        int stream_id = request.stream_id;

        # Check for RFC 8441 extended CONNECT over HTTP/2 (CONNECT with :protocol)
        bool is_extended_connect = request.method == "CONNECT" &&
            request.headers.":protocol".val();
        bool is_websocket_connect = is_extended_connect &&
            request.headers.":protocol" == "websocket";

        if (is_extended_connect) {
            log(LoggerLevel::DEBUG, "HTTP/2 extended CONNECT (%s): %y stream=%d %s",
                request.headers.":protocol", uh, stream_id, request.path);
            handleHttp2ExtendedConnect(uh, cinfo, h2op, request, stream_id,
                is_websocket_connect);
            return;
        }

        # Multiplexed path: h2op stays on the I/O thread, handler sends response via
        # submitHttp2Response() — supports true HTTP/2 multiplexing with all response types
        log(LoggerLevel::DEBUG, "HTTP/2 request ready: %y stream=%d %s %s",
            uh, stream_id, request.method, request.path);

        # Build header hash
        hash<auto> hdr = request.headers;
        hdr.method = request.method;
        hdr.path = request.path;
        hdr.":authority" = request.authority;
        hdr.":scheme" = request.scheme;
        hdr.host = request.authority;
        hdr.http_version = "HTTP/2";

        # Parse accept-encoding header for compression support
        *list<string> accept_encoding;
        if (hdr."accept-encoding") {
            accept_encoding = map trim($1.split(";")[0]),
                hdr."accept-encoding".split(",");
        }

        # In headers-only mode, check if the HEADERS frame itself had END_STREAM.
        # If not, body data is expected and the handler thread gets exclusive socket access.
        # NOTE: We use headers_end_stream (set by C++) rather than isHttp2StreamComplete()
        # because DATA+END_STREAM can arrive in the same receiveData() call as HEADERS,
        # making isHttp2StreamComplete() return True even when body data exists.
        bool body_streaming = request.hdr.toBool()
            && !request.headers_end_stream.toBool();

        if (!body_streaming) {
            # Normal path: resume h2op immediately for multiplexing.
            # The handler thread sends responses via submitHttp2Response().
            #
            # Race condition: a previous handler's handleHttp2MultiplexedResult may
            # have already called resumeMultiplexedReading() → continueReading() on
            # this h2op, transitioning it from REQUEST_READY → READING and re-submitting
            # it to the controller.  In that case, our continueReading() throws because
            # the h2op is already in READING state.  This is benign — the h2op is already
            # active, so we skip re-registration and proceed to dispatch the handler.
            try {
                h2op.continueReading();
                hash<HttpActiveConnectionInfo> new_cinfo;
                {
                    AutoWriteLock al(m);
                    if (!connections.hasKey(uh)) {
                        try { cinfo.sock.close(); } catch () {}
                        return;
                    }
                    hash<HttpActiveConnectionInfo> conn_info_upd =
                        cast<hash<HttpActiveConnectionInfo>>(connections{uh});
                    conn_info_upd.stream_op = h2op;
                    conn_info_upd.state = HttpConnectionState::Http2;
                    new_cinfo = connections{uh} = conn_info_upd;
                }
                submitConnectionOp(uh, new_cinfo);
            } catch () {
                # Already in READING state (resumed by a concurrent handler's
                # resumeMultiplexedReading) — h2op is already on the controller,
                # proceed to dispatch the request handler below
            }
        }

        string dispatch_uh = uh;
        int dispatch_stream_id = stream_id;
        Socket dispatch_sock = cinfo.sock;
        HttpAsyncTarget dispatch_target = cinfo.target;
        *HttpServer::HttpListenerInterface dispatch_listener = cinfo.listener;
        hash<auto> dispatch_cx = cinfo.cx;
        bool dispatch_ssl = cinfo.ssl;
        Http2PollOperation dispatch_h2op = h2op;
        hash<HttpActiveConnectionInfo> dispatch_cinfo = <HttpActiveConnectionInfo>{
            "sock": dispatch_sock,
            "listener": dispatch_listener,
            "cx": dispatch_cx,
            "target": dispatch_target,
            "state": HttpConnectionState::Http2,
            "stream_op": dispatch_h2op,
            "is_http2": True,
            "ssl": dispatch_ssl,
        };

        # Convert HTTP/2 request to HttpConnectionInfo format
        hash<HttpConnectionInfo> conn_info = <HttpConnectionInfo>{
            "sock": dispatch_sock,
            "hdr": hdr,
            "header_info": {
                "http2": True,
                "stream_id": dispatch_stream_id,
                "accept-encoding": accept_encoding,
                "body": body_streaming ? NOTHING : request.body,
                "body_streaming": body_streaming,
                "headers_end_stream": request.headers_end_stream ?? False,
                "io_wake": sub () { controller.wakeSocket(dispatch_sock); },
                "register_persistent_queue": sub (string key, Queue q,
                        *code<bool(hash<auto>)> filter) {
                    self.registerPersistentQueue(key, q, filter);
                    # Bind the queue to the H2 connection so libqore delivers a
                    # close signal on peer stream reset, releasing the dedicated
                    # handler thread even though disconnect() does not close a
                    # multiplexed connection (issue #5339).
                    dispatch_h2op.registerPersistentSessionQueue(q);
                },
                "deregister_persistent_queue": sub (string key) {
                    self.deregisterPersistentQueue(key);
                    dispatch_h2op.deregisterPersistentSessionQueue();
                },
                "resume_body_streaming": body_streaming ? sub () {
                    resumeMultiplexedReading(dispatch_h2op, dispatch_uh,
                        HttpConnectionState::Http2);
                } : NOTHING,
                # Async-body opt-in: handlers that want inbound DATA frames
                # delivered via a Qore Queue (instead of the handler-thread
                # sync Socket::readHttp2StreamDataBlock() path) call this
                # closure with an unlimited (max=-1) Queue.  The I/O thread
                # drains previously-buffered frames into the Queue at
                # registration time and pushes subsequent frames as they
                # arrive; a NOTHING sentinel marks END_STREAM.  Used by
                # GrpcServerAsync for client-streaming / bidi and to drain
                # the unary request body without the sync helper.
                "register_body_queue": body_streaming ? sub (Queue q) {
                    dispatch_h2op.registerStreamQueue(dispatch_stream_id, q);
                    resumeMultiplexedReading(dispatch_h2op, dispatch_uh,
                        HttpConnectionState::Http2);
                } : NOTHING,
            },
            "listener": dispatch_listener,
            "cx": dispatch_cx,
            "keep_alive": True,
            "ssl": dispatch_ssl,
        };

        # Check for persistent queue — if a dedicated thread is waiting for this
        # connection and the request matches the session filter, route the request
        # to the dedicated thread instead of the pool
        *code resume_reading;
        if (body_streaming) {
            resume_reading = sub () {
                resumeMultiplexedReading(dispatch_h2op, dispatch_uh, HttpConnectionState::Http2);
            };
        }
        if (pushPersistentQueue(dispatch_uh, hdr, {
            "type": "request",
            "conn_info": conn_info,
            "stream_id": dispatch_stream_id,
            "body_streaming": body_streaming,
            "resume_reading": resume_reading,
        }, sub () {
            log(LoggerLevel::DEBUG, "HTTP/2 request routed to persistent queue: %y stream=%d %s %s",
                dispatch_uh, dispatch_stream_id, request.method, request.path);
        })) {
            return;
        }

        # The HTTP server does not interpret any application-specific
        # persistent-session control header.  A request that arrives for a
        # persistent session whose dedicated loop has already deregistered
        # its queue simply falls through to normal handler-pool dispatch;
        # the handler owns the persistent-session protocol and is
        # responsible for responding appropriately (idempotently) to such a
        # request.  The dedicated loop now stays bound for the connection's
        # lifetime for handlers that request it
        # (keepPersistentConnectionDedicated()), so an end-of-session
        # request reaches the live loop and is recognised generically via
        # AbstractHttpRequestHandler::isPersistentSessionEndRequest().

        # Give the target a protocol-independent admission point before the shared handler queue.
        try {
            *hash<HttpRequestResultInfo> prepared_result =
                dispatch_target.prepareHttpRequest(conn_info);
            if (prepared_result) {
                if (body_streaming) {
                    resumeMultiplexedReading(dispatch_h2op, dispatch_uh,
                        HttpConnectionState::Http2);
                }
                handleHttp2MultiplexedResult(dispatch_uh, dispatch_cinfo,
                    dispatch_stream_id, prepared_result);
                return;
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::ERROR, "Error preparing HTTP/2 request stream=%d on %y: %s: %s",
                dispatch_stream_id, dispatch_uh, ex.err, ex.desc);
            if (body_streaming) {
                resumeMultiplexedReading(dispatch_h2op, dispatch_uh,
                    HttpConnectionState::Http2);
            }
            try {
                dispatch_sock.submitHttp2Response(dispatch_stream_id, 500,
                    {"content-type": "text/plain"}, "Internal Server Error");
            } catch () {}
            wakeSocketIfPresent(dispatch_sock, dispatch_uh,
                "HTTP/2 preparation error response");
            return;
        }

        # Dispatch to handler thread pool
        # NOTE: for body_streaming requests, HttpServer calls resume_body_streaming (in
        # header-info) as an *early* resume right after the framework finishes reading the
        # body.  This keeps the connection alive (PING, SETTINGS, WINDOW_UPDATE) during
        # long-running handlers.  The resumeMultiplexedReading() calls below are a *safety
        # net* that guarantee the h2op is resumed even when the early resume was not called:
        #   - wantsBodyStreaming() handlers that read the body themselves
        #   - error paths during framework body reading (timeout, max_body, etc.)
        # resumeMultiplexedReading() silently handles double-resume (catches STATE-ERROR).
        try {
            submitToHandlerPool(dispatch_target, sub () {
                try {
                    hash<HttpRequestResultInfo> result = dispatch_target.onHttpRequest(conn_info);
                    if (body_streaming) {
                        resumeMultiplexedReading(dispatch_h2op, dispatch_uh,
                            HttpConnectionState::Http2);
                    }
                    handleHttp2MultiplexedResult(dispatch_uh, dispatch_cinfo,
                        dispatch_stream_id, result);
                } catch (hash<ExceptionInfo> ex) {
                    log(LoggerLevel::ERROR, "Error handling HTTP/2 request stream=%d on %y: %s: %s",
                        dispatch_stream_id, dispatch_uh, ex.err, ex.desc);
                    if (body_streaming) {
                        resumeMultiplexedReading(dispatch_h2op, dispatch_uh,
                            HttpConnectionState::Http2);
                    }
                    try {
                        dispatch_sock.submitHttp2Response(dispatch_stream_id, 500,
                            {"content-type": "text/plain"},
                            "Internal Server Error");
                    } catch (hash<ExceptionInfo> ex2) {
                        log(LoggerLevel::DEBUG, "submitHttp2Response 500 stream=%d failed: %s: %s",
                            dispatch_stream_id, ex2.err, ex2.desc);
                    }
                    wakeSocketIfPresent(dispatch_sock, dispatch_uh,
                        "HTTP/2 multiplexed handler error response");
                }
            });
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::ERROR, "Failed to submit HTTP/2 multiplexed handler for stream=%d "
                "on %y: %s: %s", dispatch_stream_id, dispatch_uh, ex.err, ex.desc);
            try {
                dispatch_target.cancelPreparedHttpRequest(conn_info);
            } catch (hash<ExceptionInfo> cancel_ex) {
                log(LoggerLevel::ERROR, "Failed to cancel HTTP/2 request preparation for stream=%d "
                    "on %y: %s: %s", dispatch_stream_id, dispatch_uh,
                    cancel_ex.err, cancel_ex.desc);
            }
            if (body_streaming) {
                # Resume h2op — it was not resumed before submit in body_streaming mode
                try {
                    dispatch_h2op.continueReading();
                    hash<HttpActiveConnectionInfo> resumed_cinfo;
                    {
                        AutoWriteLock al(m);
                        if (connections.hasKey(dispatch_uh)) {
                            hash<HttpActiveConnectionInfo> conn_info_upd =
                                cast<hash<HttpActiveConnectionInfo>>(connections{dispatch_uh});
                            conn_info_upd.stream_op = dispatch_h2op;
                            conn_info_upd.state = HttpConnectionState::Http2;
                            resumed_cinfo = connections{dispatch_uh} = conn_info_upd;
                        }
                    }
                    if (resumed_cinfo) {
                        submitConnectionOp(dispatch_uh, resumed_cinfo);
                        wakeSocketIfPresent(resumed_cinfo.sock, dispatch_uh,
                            "HTTP/2 multiplexed submit failure resume");
                    }
                } catch () {}
            }
            # Send error response for this stream
            try {
                dispatch_sock.submitHttp2Response(dispatch_stream_id, 503,
                    {"content-type": "text/plain"},
                    "Service Unavailable");
            } catch () {}
            wakeSocketIfPresent(dispatch_sock, dispatch_uh, "HTTP/2 submit failure response");
        }
    }

    #! Handles extended CONNECT over HTTP/2 (RFC 8441) in multiplexed mode
    /** h2op stays on the I/O thread; the extended CONNECT request is dispatched
        to the handler pool like any other request.  The handler returns a
        \c dedicated result, and the socket handler manages the stream directly
        via submitConnectResponse() and sendStreamData().

        A Queue is created internally (unlimited size) and registered with h2op
        so that the I/O controller drains per-stream DATA into the Queue after
        each poll cycle.  The handler reads from the Queue instead of doing
        direct socket I/O, keeping the async I/O path clean and non-blocking.
    */
    private handleHttp2ExtendedConnect(string uh, hash<HttpActiveConnectionInfo> cinfo,
            Http2PollOperation h2op, hash<auto> request, int stream_id,
            bool is_websocket_connect = True) {
        log(LoggerLevel::DEBUG, "HTTP/2 extended CONNECT (%s, multiplexed): %y stream=%d %s",
            request.headers.":protocol", uh, stream_id, request.path);

        # Create unlimited Queue for async data delivery to the handler.
        # WebSocket CONNECT takes the C++ WebSocketStreamFrameState fast
        # path — the Queue is populated with one entry per complete
        # WebSocket message (a typed hash), not per raw transport chunk.
        # This eliminates O(N²) Qore-side binary concatenation for large
        # WS messages (mirrors the H3 path).  Non-WS extended-CONNECT
        # tunnels keep the raw-byte Queue contract.
        Queue stream_data_queue();
        bool stream_uses_frame_state = False;
        if (is_websocket_connect) {
            h2op.registerStreamFrameState(stream_id, stream_data_queue);
            stream_uses_frame_state = True;
        } else {
            h2op.registerStreamQueue(stream_id, stream_data_queue);
        }

        string dispatch_uh = uh;
        int dispatch_stream_id = stream_id;
        Socket dispatch_sock = cinfo.sock;
        HttpAsyncTarget dispatch_target = cinfo.target;
        *HttpServer::HttpListenerInterface dispatch_listener = cinfo.listener;
        hash<auto> dispatch_cx = cinfo.cx;
        bool dispatch_ssl = cinfo.ssl;
        Http2PollOperation dispatch_h2op = h2op;
        hash<HttpActiveConnectionInfo> dispatch_cinfo = <HttpActiveConnectionInfo>{
            "sock": dispatch_sock,
            "listener": dispatch_listener,
            "cx": dispatch_cx,
            "target": dispatch_target,
            "state": HttpConnectionState::Http2,
            "stream_op": dispatch_h2op,
            "is_http2": True,
            "ssl": dispatch_ssl,
        };

        # Closure for handlers to register a stream data listener.
        # The listener's notifyIo() method is called from the controller's
        # worker pool when the I/O thread drains data to the Queue.
        code<nothing(object)> register_stream_listener = sub (object listener) {
            dispatch_h2op.registerStreamListener(dispatch_stream_id, listener);
        };

        # Continue reading on h2op immediately — the CONNECT stream coexists
        # with other streams on the same connection
        dispatch_h2op.continueReading();
        hash<HttpActiveConnectionInfo> new_cinfo;
        {
            AutoWriteLock al(m);
            # Check if connection still exists
            if (!connections.hasKey(dispatch_uh)) {
                try { dispatch_sock.close(); } catch () {}
                return;
            }
            # Cast to typed hash to update fields and assign back
            hash<HttpActiveConnectionInfo> conn_info = cast<hash<HttpActiveConnectionInfo>>(connections{dispatch_uh});
            conn_info.stream_op = dispatch_h2op;
            conn_info.state = HttpConnectionState::Http2;
            new_cinfo = connections{dispatch_uh} = conn_info;
        }
        submitConnectionOp(dispatch_uh, new_cinfo);

        # Build header hash
        hash<auto> hdr = request.headers;
        hdr.method = request.method;
        hdr.path = request.path;
        hdr.":authority" = request.authority;
        hdr.":scheme" = request.scheme;
        hdr.host = request.authority;
        hdr.http_version = "HTTP/2";

        # Parse accept-encoding header for compression support
        *list<string> accept_encoding;
        if (hdr."accept-encoding") {
            accept_encoding = map trim($1.split(";")[0]),
                hdr."accept-encoding".split(",");
        }

        # Create send-and-wake wrapper for handler threads.
        # Queues data in nghttp2 with transparent backpressure handling,
        # then wakes the I/O controller to flush.  Handlers use this
        # instead of calling sendHttp2StreamData() + io_wake() separately.
        code send_stream_data = sub (*binary data, bool end_stream = False) {
            int retries = 0;
            while (True) {
                try {
                    dispatch_sock.sendHttp2StreamData(dispatch_stream_id, data, end_stream);
                    break;
                } catch (hash<ExceptionInfo> ex) {
                    if (ex.err == "HTTP2-FLOW-CONTROL") {
                        if (++retries > 200) {
                            throw "HTTP2-FLOW-CONTROL", sprintf("stream %d: buffer "
                                "not draining after %d retries", dispatch_stream_id, retries);
                        }
                        # Buffer full — wake I/O thread to drain, then wait
                        # deterministically for the CV signal
                        controller.wakeSocket(dispatch_sock);
                        if (dispatch_sock.waitForHttp2StreamDrain(dispatch_stream_id, 10s)) {
                            throw "HTTP2-STREAM-ERROR", sprintf("stream %d: drain "
                                "timed out or stream removed", dispatch_stream_id);
                        }
                        continue;
                    }
                    rethrow;
                }
            }
            controller.wakeSocket(dispatch_sock);
        };

        hash<HttpConnectionInfo> conn_info = <HttpConnectionInfo>{
            "sock": dispatch_sock,
            "hdr": hdr,
            "header_info": {
                "http2": True,
                "stream_id": dispatch_stream_id,
                "is_extended_connect": True,
                "is_websocket_connect": is_websocket_connect,
                "accept-encoding": accept_encoding,
                "body": request.body,
                "stream_data_queue": stream_data_queue,
                "stream_uses_frame_state": stream_uses_frame_state,
                "register_stream_listener": register_stream_listener,
                "send_stream_data": send_stream_data,
                "io_wake": sub () { controller.wakeSocket(dispatch_sock); },
            },
            "listener": dispatch_listener,
            "cx": dispatch_cx,
            "keep_alive": True,
            "ssl": dispatch_ssl,
        };

        # Apply the same pre-dispatch admission contract to extended CONNECT requests.
        try {
            *hash<HttpRequestResultInfo> prepared_result =
                dispatch_target.prepareHttpRequest(conn_info);
            if (prepared_result) {
                handleHttp2MultiplexedResult(dispatch_uh, dispatch_cinfo,
                    dispatch_stream_id, prepared_result);
                return;
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::ERROR, "Error preparing HTTP/2 extended CONNECT stream=%d: %s: %s",
                dispatch_stream_id, ex.err, ex.desc);
            try {
                dispatch_sock.submitHttp2Response(dispatch_stream_id, 500,
                    {"content-type": "text/plain"}, "Internal Server Error");
            } catch () {}
            wakeSocketIfPresent(dispatch_sock, dispatch_uh,
                "HTTP/2 extended CONNECT preparation error response");
            return;
        }

        # Dispatch to handler thread pool
        try {
            submitToHandlerPool(dispatch_target, sub () {
                try {
                    hash<HttpRequestResultInfo> result = dispatch_target.onHttpRequest(conn_info);
                    handleHttp2MultiplexedResult(dispatch_uh, dispatch_cinfo,
                        dispatch_stream_id, result);
                } catch (hash<ExceptionInfo> ex) {
                    log(LoggerLevel::ERROR, "Error handling HTTP/2 extended CONNECT stream=%d: "
                        "%s: %s", dispatch_stream_id, ex.err, ex.desc);
                    # NOTE: do NOT call cinfo.target.onError() here — that closes the shared
                    # connection socket which breaks all other streams.  In HTTP/2 multiplexed
                    # mode, a handler error affects only this stream; send a 500 response.
                    try {
                        dispatch_sock.submitHttp2Response(dispatch_stream_id, 500,
                            {"content-type": "text/plain"},
                            "Internal Server Error");
                    } catch () {}
                    wakeSocketIfPresent(dispatch_sock, dispatch_uh,
                        "HTTP/2 extended CONNECT handler error response");
                }
            });
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::ERROR, "Failed to submit HTTP/2 extended CONNECT handler for stream=%d "
                "on %y: %s: %s", dispatch_stream_id, dispatch_uh, ex.err, ex.desc);
            try {
                dispatch_target.cancelPreparedHttpRequest(conn_info);
            } catch (hash<ExceptionInfo> cancel_ex) {
                log(LoggerLevel::ERROR, "Failed to cancel HTTP/2 extended CONNECT preparation for stream=%d "
                    "on %y: %s: %s", dispatch_stream_id, dispatch_uh,
                    cancel_ex.err, cancel_ex.desc);
            }
            # Send error for this stream only — other streams continue
            try {
                dispatch_sock.submitHttp2Response(dispatch_stream_id, 503,
                    {"content-type": "text/plain"},
                    "Service Unavailable");
            } catch () {}
            wakeSocketIfPresent(dispatch_sock, dispatch_uh,
                "HTTP/2 extended CONNECT submit failure response");
        }
    }

    #! Handles the result of an HTTP/2 request in multiplexing mode
    /** Called on a handler_pool worker thread.  h2op remains on the I/O thread
        reading frames.  Responses are sent via separate send operations submitted
        to the I/O controller, allowing multiple concurrent requests per connection.

        Supports all response types:
        - Regular responses (with or without body)
        - InputStream responses (read on handler thread, submitted to nghttp2)
        - Streaming responses (SSE, WebSocket) via dedicated handlers
    */
    private handleHttp2MultiplexedResult(string uh, hash<HttpActiveConnectionInfo> cinfo,
            int stream_id, hash<HttpRequestResultInfo> result) {
        if (result.close) {
            # For HTTP/2, don't close the entire connection — HTTP/2 is multiplexed,
            # so closing the connection would kill all active streams.  Instead, send
            # the error response on this stream and let the connection continue.
            log(LoggerLevel::DEBUG, "HTTP/2 handler requested close for stream %d on %y "
                "(stream-only; connection stays open)", stream_id, uh);
            if (result.code) {
                try {
                    cinfo.sock.submitHttp2Response(stream_id, result.code, result.hdr,
                        result.body);
                } catch (hash<ExceptionInfo> ex) {
                    log(LoggerLevel::DEBUG, "HTTP/2 close response stream=%d failed: %s: %s",
                        stream_id, ex.err, ex.desc);
                }
            }
            controller.wakeSocket(cinfo.sock);
            return;
        }

        if (result.dedicated) {
            # Connection was handed to a dedicated handler (WebSocket/SSE).
            # The handler manages the stream directly via sendStreamData() /
            # submitConnectResponse() etc.  h2op is still on the I/O thread
            # handling other streams, so do NOT remove the connection or close.
            log(LoggerLevel::DEBUG, "HTTP/2 stream %d on %y handed to dedicated handler", stream_id, uh);
            controller.wakeSocket(cinfo.sock);
            return;
        }

        # Submit HTTP/2 response directly to the nghttp2 session.
        # The read operation (h2op) on the I/O thread handles the actual socket write
        # via sendPendingData() in its continuePoll().  This avoids creating a separate
        # send operation which would conflict with h2op's non-blocking mode management.
        if (result.code && !result.streaming) {
            try {
                # Convert send_callback to InputStream for HTTP/2 (send_callback is
                # HTTP/1.x chunked-only; HTTP/2 uses DATA frames via InputStream)
                if (result.send_callback && !result.input_stream) {
                    auto cb = result.send_callback;
                    result.input_stream = new HttpServer::CallbackInputStream(
                        auto sub() { return cb(); }, cinfo.sock);
                    remove result.send_callback;
                    remove result.hdr."Transfer-Encoding";
                }

                # InputStream response body: try the fast path; if the
                # C++ gate rejects (non-I/O-thread-safe), spawn a producer.
                # See the H3 branch above for detail on why the gate
                # (not the Qore-level isIoThreadSafe()) is authoritative.
                if (result.input_stream) {
                    hash<auto> resp_hdr = result.hdr ?? {};
                    remove resp_hdr."Transfer-Encoding";
                    int rv = cinfo.sock.submitHttp2StreamingResponseWithStream(
                        stream_id, result.code, resp_hdr, result.input_stream);
                    if (!rv) {
                        # Memory-backed stream — I/O thread drives it.
                        log(LoggerLevel::DEBUG, "HTTP/2 stream %d: submitting "
                            "InputStream response (CL=%y)",
                            stream_id, result.hdr."Content-Length");
                        controller.wakeSocket(cinfo.sock);
                        return;
                    }
                    # C++ gate rejected: spawn a producer worker.
                    cinfo.sock.submitHttp2StreamingResponseHeaders(stream_id,
                        result.code, resp_hdr);
                    result.input_stream.unassignThread();
                    code<nothing()> wake_fn = sub () {
                        controller.wakeSocket(cinfo.sock);
                    };
                    HttpServer::StreamResponseProducer producer(cinfo.sock,
                        stream_id, NOTHING, result.input_stream, wake_fn);
                    producer.start();
                    controller.wakeSocket(cinfo.sock);
                    return;
                }

                # In-memory response body
                *data body = result.body;

                # Submit response to nghttp2 session (thread-safe)
                cinfo.sock.submitHttp2Response(stream_id, result.code, result.hdr, body);

                # Re-submit the H2 poll operation to the controller so the I/O
                # thread can flush the response frames non-blockingly via
                # continuePoll() → sendPendingData(0).
                #
                # We must NOT call flushHttp2(-1) here — that does a blocking
                # send (sendPendingDataBlocking) while holding Http2Session::m.
                # If the socket is non-blocking (set by the I/O controller) and
                # the send buffer is full, sendIntern blocks in poll(-1) while
                # holding the mutex.  The I/O thread needs that same mutex to
                # process incoming ACKs that would unblock the send — deadlock.
                resumeMultiplexedReading(cinfo.stream_op, uh,
                    HttpConnectionState::Http2);
                controller.wakeSocket(cinfo.sock);
            } catch (hash<ExceptionInfo> ex) {
                log(LoggerLevel::ERROR,
                    "Error submitting HTTP/2 response for stream=%d on %y: %s: %s",
                    stream_id, uh, ex.err, ex.desc);
                # Send RST_STREAM so the client gets an error instead of waiting forever
                try {
                    cinfo.sock.resetHttp2Stream(stream_id);
                    controller.wakeSocket(cinfo.sock);
                } catch () {}
            }
            return;
        }

        # Streaming responses: the handler has set up a streaming operation
        # (SSE or WebSocket) that manages the stream directly.  The dedicated
        # handler sends data via sendStreamData() / submitConnectResponse() etc.
        # h2op continues on the I/O thread handling other streams.
        if (result.streaming) {
            log(LoggerLevel::DEBUG, "HTTP/2 streaming response for stream=%d on %y (type=%y)",
                stream_id, uh, result.stream_type);
            # The streaming handler manages the stream lifecycle directly.
            # h2op stays on the I/O thread reading other streams.
            # Wake the I/O controller in case the handler already queued data.
            controller.wakeSocket(cinfo.sock);
            return;
        }

        # No response code — the handler already managed the response lifecycle
        # (e.g., persistent handler loop completed, WebSocket/SSE upgrade consumed the
        # response).  Just wake the controller and return; do NOT send a spurious 500
        # on the already-completed stream, which would corrupt the HTTP/2 session.
        if (!result.code) {
            log(LoggerLevel::DEBUG, "HTTP/2 stream %d on %y: handler already sent response "
                "(no code in result)", stream_id, uh);
            controller.wakeSocket(cinfo.sock);
            return;
        }

        # Fallback: code is set but streaming is also set without a stream type
        # This shouldn't happen but handle it gracefully
        try {
            cinfo.sock.submitHttp2Response(stream_id, result.code, result.hdr, result.body);
            controller.wakeSocket(cinfo.sock);
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::ERROR,
                "Error submitting HTTP/2 fallback response for stream=%d on %y: %s: %s",
                stream_id, uh, ex.err, ex.desc);
        }
    }

    #! Handles HTTP/2 connection closed
    private handleHttp2ConnectionClosed(string uh, hash<HttpActiveConnectionInfo> cinfo,
            Http2PollOperation h2op) {
        *hash<auto> error = h2op.getError();
        if (error) {
            log(LoggerLevel::DEBUG, "HTTP/2 connection closed with error: %y (%s: %s)",
                uh, error.err, error.desc);
            dispatchOnError(cinfo.target, cinfo.sock, cast<hash<ExceptionInfo>>(error),
                cinfo.listener, cinfo.cx, uh, "HTTP/2 connection close");
        } else {
            log(LoggerLevel::DEBUG, "HTTP/2 connection closed: %y", uh);
        }

        signalPersistentQueuesForConnection(uh);

        # Notify all registered stream listeners (e.g., WebSocketStreamProcessor)
        # that the connection is dead. Without this, handler threads block forever
        # in stream_done.waitForZero() because no onStreamData() fires for a
        # closed connection.
        h2op.notifyStreamListenersClosed();

        {
            AutoWriteLock al(m);
            remove connections{uh};
        }

        # Cancel the poll operation from the I/O controller cache to release socket reference
        try {
            controller.cancel(cinfo.sock);
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::DEBUG, "Error cancelling HTTP/2 operation for %y: %s: %s", uh, ex.err, ex.desc);
        }
    }

    #! Handles HTTP/2 async send completion (called on I/O thread)
    /** After the I/O thread drives h2op through SENDING -> SENT, this method
        handles the keep-alive/close transition — the same logic that was
        previously done synchronously on the handler thread.
    */
    private handleHttp2SendComplete(string uh, hash<HttpActiveConnectionInfo> cinfo,
            Http2PollOperation h2op) {
        if (h2op.shouldKeepAlive()) {
            log(LoggerLevel::DEBUG, "HTTP/2 async send complete, waiting for next request: %y", uh);
            h2op.startReadNextRequest();
            hash<HttpActiveConnectionInfo> new_cinfo;
            {
                AutoWriteLock al(m);
                # Check if connection still exists
                if (!connections.hasKey(uh)) {
                    try { cinfo.sock.close(); } catch () {}
                    return;
                }
                # Cast to typed hash to update fields and assign back
                hash<HttpActiveConnectionInfo> conn_info = cast<hash<HttpActiveConnectionInfo>>(connections{uh});
                conn_info.stream_op = h2op;
                conn_info.state = HttpConnectionState::Http2;
                new_cinfo = connections{uh} = conn_info;
            }
            submitConnectionOp(uh, new_cinfo);
        } else {
            log(LoggerLevel::DEBUG, "HTTP/2 async send complete, closing: %y", uh);
            {
                AutoWriteLock al(m);
                remove connections{uh};
            }
            try { cinfo.sock.close(); } catch () {}
        }
    }

    #! Handles HTTP/2 WebSocket close
    private handleHttp2WebSocketClose(string uh, hash<HttpActiveConnectionInfo> cinfo,
            Http2WebSocketStream ws_stream) {
        log(LoggerLevel::DEBUG, "HTTP/2 WebSocket close: %y stream=%d (code: %d)",
            uh, ws_stream.getStreamId(), ws_stream.getCloseCode());

        {
            AutoWriteLock al(m);
            remove connections{uh};
        }

        # Cancel the poll operation from the I/O controller cache to release socket reference
        try {
            controller.cancel(cinfo.sock);
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::DEBUG, "Error cancelling HTTP/2 WebSocket operation for %y: %s: %s", uh, ex.err, ex.desc);
        }

        try {
            cinfo.target.onWebSocketClose(cinfo.sock, ws_stream.getCloseCode(),
                ws_stream.getCloseReason(), cinfo.listener, cinfo.cx);
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::ERROR, "onWebSocketClose callback threw for %s: %s: %s", uh, ex.err, ex.desc);
        }
    }

    #! Logs a message
    private log(int level, string fmt) {
        if (logger) {
            # use "%s" wrapper so the already-formatted message is treated as a
            # literal — re-passing it as Logger::log()'s format string would
            # re-interpret any embedded "%XY" (e.g. URL-encoded "%2F") and
            # collapse pure "%s" results to empty output
            logger.log(level, "%s", vsprintf(fmt, argv));
        }
    }

    #! Returns the effective HTTP version respecting global HTTP/2 mode
    private string getEffectiveHttpVersion(string configured_version) {
        string global_mode = get_global_http2_mode();
        if (global_mode == "disabled"
            && (configured_version == HttpVersionMode::Auto
                || configured_version == HttpVersionMode::Http20
                || configured_version == "2")) {
            return HttpVersionMode::Http11;
        }
        return configured_version;
    }
}

} # namespace HttpServerAsyncIo

#! @cond nodoc
namespace Priv {

#! Internal hashdecl for listener info
hashdecl HttpListenerInfo {
    #! The listener socket
    Socket listener;

    #! Target for callbacks
    HttpAsyncTarget target;

    #! True for SSL listeners
    bool ssl = False;

    #! SSL certificate (SSLCertificate object or PEM/DER data)
    auto cert;

    #! SSL private key (SSLPrivateKey object or PEM/DER data)
    auto key;

    #! HTTP version mode
    string http_version = HttpVersionMode::Auto;

    #! SSL verify flags for client certificate verification
    int ssl_verify_flags = SSL_VERIFY_NONE;

    #! Accept all certificates (including self-signed)
    bool ssl_accept_all_certs = True;

    #! True to capture remote (client) certificates
    bool get_remote_certs = False;

    #! Maximum request body size in bytes (0 = unlimited)
    int max_request_body_size = 0;

    #! HTTP listener object for handler dispatch (used for QUIC/HTTP/3 listeners)
    *HttpServer::HttpListenerInterface http_listener;

    #! Numeric local socket info captured when the listener is registered
    *hash<auto> socket_info;

    #! The Http3ServerPollOperation accepting QUIC connections (QUIC listeners only)
    /** Used by reloadListenerCertificateByHttpListener() to stage a TLS-material
        swap that the H3 op applies on the I/O thread without contending for the
        udp_sock's mutex.
    */
    *Http3ServerPollOperation h3op;
}

#! Internal hashdecl for active connection info
hashdecl HttpActiveConnectionInfo {
    #! The socket
    Socket sock;

    #! The listener interface for handler dispatch and logging
    *HttpServer::HttpListenerInterface listener;

    #! Connection context
    hash<auto> cx;

    #! Target for callbacks
    HttpAsyncTarget target;

    #! Connection state
    string state = HttpConnectionState::Idle;

    #! SSL handshake poll operation (for "ssl_handshake" state)
    *AbstractPollOperation ssl_op;

    #! Stream operation (for SSE/WebSocket/HTTP2)
    *AbstractPollOperation stream_op;

    #! Keep-alive poll operation (for KeepAlive state, HTTP/1.x optimization)
    *HttpKeepAlivePollOperation keep_alive_op;

    #! Keep-alive timeout
    timeout ttl = DefaultIdleTimeout;

    #! HTTP version mode
    string http_version = HttpVersionMode::Auto;

    #! True if this is an HTTP/2 connection
    bool is_http2 = False;

    #! True if SSL/TLS is active for this connection
    bool ssl = False;

    #! Pending streaming state type (set during header send before SSE/WS transition)
    *string pending_stream_type;

    #! Pending streaming operation (set during header send before SSE/WS transition)
    *AbstractPollOperation pending_stream_op;

    #! Parsed headers (saved during ReadBody state for dispatch after body is read)
    *hash<auto> pending_hdr;

    #! Header parsing info (saved during ReadBody state for dispatch after body is read)
    *hash<auto> pending_header_info;

}

} # namespace Priv
#! @endcond
