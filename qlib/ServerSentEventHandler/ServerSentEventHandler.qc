# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ServerSentEventConnection.qc Qore handler definition for the HttpServer module for providing SSE connections

/*  ServerSentEventHandler.qc Copyright 2025 - 2026 Qore Technologies, s.r.o.

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

#! the ServerSentEventHandler namespace contains all the objects in the ServerSentEventHandler module
public namespace ServerSentEventHandler {
#! the main web socket handler class
/** connections are represented by object descended from
    @ref ServerSentEventHandler::ServerSentEventConnection "ServerSentEventConnection".
    @ref ServerSentEventHandler::ServerSentEventHandler::getConnectionImpl() returns a suitable object for each
    connection, this method can be overridden in subclasses to return a custom object for each connection if required.

    @par Sending ServerSentEvent Message to Clients
    To send a message to an HTTP client, call one of the following methods:
    - @ref ServerSentEventHandler::ServerSentEventHandler::sendOne() "ServerSentEventHandler::sendOne()": send a
      message to single connection given its connection ID
    - @ref ServerSentEventHandler::ServerSentEventHandler::sendAll() "ServerSentEventHandler::sendAll()": send a
      message to all connected clients

    ServerSentEvent connections are identified by their HTTP connection ID as passed in
    @ref ServerSentEventHandler::ServerSentEventHandler::handleRequest() "ServerSentEventHandler::handleRequest()" in
    the \c "cx.id" argument when the connection is first established.
*/
public class ServerSentEventHandler inherits HttpServer::AbstractHttpSocketHandler, Logger::LoggerWrapper {
    public {
        #! default socket send timeout
        const DefaultSocketSendTimeout = 30s;

        #! default heartbeat interval in seconds
        const DefaultHeartbeatInterval = 20;

        #! default heartbeat message; this is a short comment to be used as a heartbeat message
        const DefaultHeartbeatMsg = ":hb\n\n";

        #! known constructor options
        const Options = (
            "heartbeat",
            "heartbeat_msg",
            "logger",
            "data",
            "asyncapi",
            "asyncapi_channel",
            "validator",
            "max_queue_size",
        );

        #! Response header to SSE requests
        const SseResponseHeader = {
            "Cache-Control": "no-cache",
            "Connection": "Keep-Alive",
            "Content-Type": MimeTypeSse,
        };

        #! Data serialization support
        const DataSerializationSupport = {
            "json": \ServerSentEventHandler::serializeJson(),
            "none": \ServerSentEventHandler::serializeDefault(),
        };
    }

    private {
        #! connection read-write lock
        RWLock rwl();

        #! The value of the serialization option
        string serialization;

        #! Value serialization code
        code<string(auto)> serializer;

        #! connection hash
        hash<string, ServerSentEventConnection> ch;

        #! maximum per-connection outbound event queue size before the connection is dropped (0 = unlimited)
        int max_queue_size = 0;

        #! heartbeat interval in seconds
        int heartbeat = DefaultHeartbeatInterval;

        #! heartbeat message returned by getHeartbeatMessage()
        string heartbeat_msg = DefaultHeartbeatMsg;

        #! Optional default AsyncAPI validator assigned to new connections
        *AsyncApi::AsyncApi3Schema asyncapi_validator;

        #! Default AsyncAPI channel assigned to new connections
        *string asyncapi_channel;
    }

    private:internal {
    }

    #! Create the object optionally with the given @ref HttpServer::AbstractAuthenticator "AbstractAuthenticator"
    /** @param auth the authentication object to use to authenticate connections (see
        @ref HttpServer::AbstractAuthenticator "AbstractAuthenticator"); if no
        @ref HttpServer::AbstractAuthenticator "AbstractAuthenticator" object is passed, then by default no
        authentication will be required
        @param opts options for the object with the following optional keys:
        - \c heartbeat: a non-negative integer value giving seconds for the heartbeat interval for the server
          connection; \c 0 disables heartbeats (default: 20 seconds)
        - \c logger: a LoggerInterface object for logging
        - \c asyncapi: a string URL or file path to an AsyncAPI schema to load for automatic message validation
          on all new connections
        - \c validator: an @ref AsyncApi::AsyncApi3Schema "AsyncApi3Schema" object to use for automatic message
          validation on all new connections; this takes precedence over \c asyncapi
        - \c asyncapi_channel: the AsyncAPI channel name to use with \c asyncapi or \c validator; required when
          either validation option is configured

        @throw SERVERSENTEVENTHANDLER-OPTION-ERROR unknown or invalid option
    */
    constructor(*HttpServer::AbstractAuthenticator auth, *hash<auto> opts)
            : HttpServer::AbstractHttpSocketHandler(auth) {
        constructorInit(opts);
    }

    #! Create the object optionally with the given optiond
    constructor(hash<auto> opts) : HttpServer::AbstractHttpSocketHandler(auth) {
        constructorInit(opts);
    }

    #! returns the connection heartbeat interval as a number of seconds
    /** @return the connection heartbeat interval as a number of seconds
    */
    int getHeartbeat(ServerSentEventConnection conn) {
        return heartbeat;
    }

    #! sets the heartbeat interval as a number of seconds
    /** @param seconds the heartbeat interval as a number of seconds
    */
    setHeartbeat(softint seconds) {
        if (seconds < 0) {
            throw "SERVERSENTEVENTHANDLER-OPTION-ERROR", sprintf("option \"heartbeat\" value %y is not a "
                "non-negative integer giving the number of seconds in the heartbeat interval", seconds);
        }
        heartbeat = seconds;
        logDebug("Set heartbeat to %d second%s", heartbeat, heartbeat == 1 ? "" : "s");
    }

    #! sets the heartbeat interval as a number of seconds
    /** @param seconds the heartbeat interval as a number of seconds
    */
    setHeartbeat(date seconds) {
        int secs = seconds.durationSeconds();
        if (secs < 0) {
            throw "SERVERSENTEVENTHANDLER-OPTION-ERROR", sprintf("option \"heartbeat\" value %y is not a "
                "non-negative integer giving the number of seconds in the heartbeat interval", seconds);
        }
        heartbeat = secs;
        logDebug("Set heartbeat to %d second%s", heartbeat, heartbeat == 1 ? "" : "s");
    }

    #! sets the maximum per-connection outbound event queue size before a slow client is dropped (0 = unlimited)
    /** @param size the maximum number of queued events; \c 0 disables the limit

        @throw SERVERSENTEVENTHANDLER-OPTION-ERROR the value is negative

        @since %ServerSentEventHandler 1.2
    */
    setMaxQueueSize(softint size) {
        if (size < 0) {
            throw "SERVERSENTEVENTHANDLER-OPTION-ERROR", sprintf("max_queue_size value %y is not a non-negative "
                "integer", size);
        }
        max_queue_size = size;
    }

    #! returns the maximum per-connection outbound event queue size (0 = unlimited)
    /** @since %ServerSentEventHandler 1.2
    */
    int getMaxQueueSize() {
        return max_queue_size;
    }

    #! called by the HTTP server to handle incoming HTTP requests
    /** To accept a dedicated connection; make sure the return value hash's \c "code" key is 101 (i.e.
        \c "Switching Protocols") and the \c "close" key is not @ref False "False"

        @param cx call context hash; see @ref httpserverutil_request_context for details; note that \c id is the
        unique HTTP connection ID; this ID is also used to identify the HTTP client connection in
        @ref ServerSentEventHandler::ServerSentEventHandler::sendOne() "ServerSentEventHandler::sendOne()"
        @param hdr incoming header hash; all keys will be converted to lower-case, additionally the following keys
        will be present:
        - \c method: the HTTP method received (ie \c "GET", \c "POST", etc)
        - \c path: the HTTP path given in the request after URL decoding the resource path; the query string remains
          percent-encoded until parsed
        - \c http_version: the HTTP version number in the request (e.g. \c "1.1"; note that SSE
          requires HTTP/1.1 or later — HTTP/1.0 does not support the persistent connections needed
          for SSE streaming)
        @param b message body, if any

        @return a hash representing the response to the client as follows; to accept a dedicated connection; make sure
        the \c "code" is 101 (ie \c "Switching Protocols") or the \c Content-Type headers is set to
        \c text/event-stream, and the \c "close" key is not @ref False "False":
        - \c "code": the HTTP return code (see @ref HttpServer::HttpCodes) (101 \c "Switching Protocols" to accept the
          dedicated connection, in which case the start() method will be called)
        - \c "body": the message body to return in the response
        - \c "hdr": (optional) set this key to a hash of extra header information to be returned with the response

        ServerSentEvent connections are identified by their HTTP connection ID as passed in \c "cx.id"
    */
    hash<HttpResponseInfo> handleRequest(hash<auto> cx, hash<auto> hdr, *data b) {
        #logDebug("SSE request: cx: %Y", cx);
        #logDebug("SSE request: hdr: %Y", hdr);
        return <HttpResponseInfo>{
            "code": 200,
            "hdr": SseResponseHeader,
        };
    }

    #! get list of socket connection ids
    /**
    */
    *list<string> getConnectionIds() {
        rwl.readLock();
        on_exit rwl.readUnlock();
        return keys ch;
    }

    #! Removes a registered connection and runs connection-close hooks
    /** Called by async HTTP server teardown when the transport has detected that
        the peer closed the SSE stream.

        @param cid the SSE connection ID
    */
    deregisterConnection(softstring cid) {
        *ServerSentEventConnection conn;
        {
            rwl.writeLock();
            on_exit rwl.writeUnlock();
            conn = removeConnectionLocked(cid);
        }
        if (!conn) {
            return;
        }

        try {
            conn.connectionClosed();
        } catch (hash<ExceptionInfo> ex) {
            logError("Exception closing ServerSentEvent connection %y: %s", cid, get_exception_string(ex));
        }
        try {
            deregisterConnectionImpl(conn);
        } catch (hash<ExceptionInfo> ex) {
            logError("Exception deregistering ServerSentEvent connection %y: %s", cid, get_exception_string(ex));
        }
    }

    #! Legacy synchronous SSE handler entry point (no longer used)
    /** The HttpServer no longer uses synchronous dedicated handlers. SSE connections
        are managed by the async I/O controller via @ref setupAsyncSseConnection().

        @since ServerSentEventHandler 1.1 (throws; was synchronous in earlier versions)
    */
    bool startImpl(softstring lid, hash<auto> cx, hash<auto> hdr, Qore::Socket sock) {
        throw "SSE-SYNC-NOT-SUPPORTED", "synchronous SSE handling is no longer supported; "
            "use setupAsyncSseConnection() instead";
    }

    #! Sets up an async SSE connection for the given socket
    /** Creates the connection object, registers it, wires it to an HttpSseStreamPollOperation,
        and calls the registered/connectionRegisteredImpl hooks.

        @param sock the socket for the SSE connection
        @param cx call context hash
        @param hdr incoming request headers
        @param cid connection ID (typically cx.id)

        @return the poll operation for the async I/O controller to manage

        @since ServerSentEventHandler 1.1
    */
    HttpServerAsyncIo::HttpSseStreamPollOperation setupAsyncSseConnection(Socket sock, hash<auto> cx,
            hash<auto> hdr, string cid) {
        logDebug("ServerSentEventHandler::setupAsyncSseConnection() cid: %y", cid);

        # Create the poll operation for the async I/O controller
        HttpServerAsyncIo::HttpSseStreamPollOperation poll_op(sock);
        poll_op.setHeartbeat(heartbeat * 1000, heartbeat_msg);

        # Create and register the connection
        ServerSentEventConnection conn = createConnectionIntern(sock, cx, hdr, cid);
        conn.setAsyncPollOp(poll_op);

        registerAndInvokeHooks(conn, cid, hdr);

        return poll_op;
    }

    #! Sets up an async SSE connection on an HTTP/2 stream
    /** Creates the connection object, registers it, and wires it to an
        @ref HttpServerAsyncIo::Http2SseStream that writes DATA frames on the
        given stream.

        The caller is responsible for having already submitted the response
        headers via @ref Qore::Socket::submitHttp2StreamingResponseHeaders()
        before invoking this method (so that the SSE stream is open and
        headers don't get submitted twice).

        @param sock the socket the HTTP/2 session is running on
        @param cx call context hash
        @param hdr incoming request headers
        @param cid connection ID (typically cx.id + stream ID)
        @param stream_id the HTTP/2 stream ID reserved for this SSE response
        @param wake_callback closure that wakes the async I/O controller after
            each event is written (so the nghttp2 session flushes promptly)

        @return the SSE stream adapter wired to the connection

        @since %ServerSentEventHandler 1.2
    */
    HttpServerAsyncIo::Http2SseStream setupAsyncSseConnectionHttp2(Socket sock, hash<auto> cx,
            hash<auto> hdr, string cid, int stream_id, *code<nothing()> wake_callback) {
        logDebug("ServerSentEventHandler::setupAsyncSseConnectionHttp2() cid: %y stream: %d",
            cid, stream_id);

        HttpServerAsyncIo::Http2SseStream sse_stream(sock, stream_id, wake_callback, True);
        sse_stream.setHeartbeat(heartbeat * 1000, heartbeat_msg);

        ServerSentEventConnection conn = createConnectionIntern(sock, cx, hdr, cid);
        conn.setAsyncSseConnection(sse_stream);

        registerAndInvokeHooks(conn, cid, hdr);

        return sse_stream;
    }

    #! Sets up an async SSE connection on an HTTP/3 stream
    /** Creates the connection object, registers it, and wires it to an
        @ref HttpServerAsyncIo::Http3SseStream that writes DATA on the given
        QUIC stream.

        The caller is responsible for having already submitted the response
        headers via @ref Qore::Socket::submitQuicResponseStreaming() before
        invoking this method.

        @param sock the socket the QUIC session is running on
        @param cx call context hash
        @param hdr incoming request headers
        @param cid connection ID (typically cx.id + stream ID)
        @param session_id the QUIC session ID
        @param stream_id the HTTP/3 stream ID reserved for this SSE response
        @param wake_callback closure that wakes the async I/O controller after
            each event is written

        @return the SSE stream adapter wired to the connection

        @since %ServerSentEventHandler 1.2
    */
    HttpServerAsyncIo::Http3SseStream setupAsyncSseConnectionHttp3(Socket sock, hash<auto> cx,
            hash<auto> hdr, string cid, int session_id, int stream_id,
            *code<nothing()> wake_callback) {
        logDebug("ServerSentEventHandler::setupAsyncSseConnectionHttp3() cid: %y session: %d stream: %d",
            cid, session_id, stream_id);

        HttpServerAsyncIo::Http3SseStream sse_stream(sock, session_id, stream_id, wake_callback, True);
        sse_stream.setHeartbeat(heartbeat * 1000, heartbeat_msg);

        ServerSentEventConnection conn = createConnectionIntern(sock, cx, hdr, cid);
        conn.setAsyncSseConnection(sse_stream);

        registerAndInvokeHooks(conn, cid, hdr);

        return sse_stream;
    }

    #! Registers a new connection and invokes replay/registered/registeredImpl hooks
    /** Internal helper shared by all three \c setupAsyncSseConnection*() variants.
    */
    private registerAndInvokeHooks(ServerSentEventConnection conn, string cid, hash<auto> hdr) {
        if (asyncapi_validator && asyncapi_channel && !conn.getAsyncApiValidator()) {
            conn.setAsyncApiValidator(asyncapi_validator, asyncapi_channel);
        }

        {
            rwl.writeLock();
            on_exit rwl.writeUnlock();
            registerNewConnectionIntern(cid, conn);
        }

        if (hdr."last-event-id".val()) {
            replayEventsSince(conn, hdr."last-event-id");
        }

        try {
            conn.registered();
        } catch (hash<ExceptionInfo> ex) {
            logError("Exception registering ServerSentEvent connection: %s", get_exception_string(ex));
            rwl.writeLock();
            on_exit rwl.writeUnlock();
            removeConnectionLocked(cid);
            rethrow;
        }
        connectionRegisteredImpl(conn);
    }

    #! Encode a message for sending
    string encodeMessage(hash<SseMessageInfo> msg) {
        string str = foldl $1 + $2, (
            map sprintf("%s: %s\n", $1.key == "comment" ? "" : $1.key,
                $1.key == "data" ? serializer($1.value) : $1.value),
                msg.pairIterator()
        );
        return (str ?? ":\n") + "\n";
    }

    #! JSON value serialization
    static string serializeJson(auto v) {
        return make_json(v);
    }

    #! Default value serialization
    static string serializeDefault(auto v) {
        string str;
        switch (v.typeCode()) {
            case NT_STRING:
                str = v;
                break;
            default:
                str = sprintf("%y", v);
        }
        str =~ s/\n/\\n/g;
        str =~ s/\r/\\r/g;
        return str;
    }

    #! Creates the connection object for a new SSE stream
    /** Internal seam called by all three \c setupAsyncSseConnection*() variants; the
        default implementation simply delegates to @ref getConnectionImpl().

        A handler that must produce a specific connection type for certain requests —
        one whose behavior the handler itself depends on, rather than one a subclass is
        free to replace — overrides this method instead of @ref getConnectionImpl(), so
        that a subclass customizing the documented factory hook cannot displace it.

        @param sock the socket for the SSE connection
        @param cx call context hash
        @param hdr incoming request headers
        @param cid connection ID

        @return the connection object to register
    */
    private ServerSentEventConnection createConnectionIntern(Socket sock, hash<auto> cx,
            hash<auto> hdr, string cid) {
        return getConnectionImpl(sock, cx, hdr, cid);
    }

    #! Called when a connection is established; the default implementation creates a ServerSentEventConnection object
    ServerSentEventConnection getConnectionImpl(Socket sock, hash<auto> cx, hash<auto> hdr, string cid) {
        return new ServerSentEventConnection(self, sock, cx, hdr, cid);
    }

    #! Called when the connection is registered; the default implementation does nothing
    connectionRegisteredImpl(ServerSentEventConnection conn) {
        # override in child classes
    }

    #! Called when the connection terminates; the default implementation does nothing
    deregisterConnectionImpl(ServerSentEventConnection conn) {
        # override in child classes
    }

    #! sends a message to all connected clients
    /** @param msg the data to send

        Messages are automatically encoded with @ref WebSocketUtil::ws_encode_message() before sending
    */
    sendAll(hash<SseMessageInfo> msg) {
        bool debug = isEnabledFor(LoggerLevel::DEBUG);
        list<ServerSentEventConnection> conns();
        list<string> ids();
        {
            rwl.readLock();
            on_exit rwl.readUnlock();
            foreach hash<auto> h in (ch.pairIterator()) {
                if (debug) {
                    ids += h.key;
                }
                conns += h.value;
            }
        }

        # push the data on all connection channels, dropping any client whose outbound queue is over the limit
        foreach ServerSentEventConnection conn in (conns) {
            if (dropIfOverflowing(conn)) {
                continue;
            }
            conn.send(msg);
        }
        if (debug) {
            logDebug("SSE sent to connections: %y", ids);
        }
    }

    #! Drops a connection whose outbound queue has exceeded \c max_queue_size (a slow or non-reading client)
    /** Enforces the per-connection outbound backpressure limit: a client that does not read fast enough would
        otherwise grow the server's outbound queue without bound.  Returns @ref True if the connection was dropped
        (the caller must then skip sending to it).  The stream is aborted; the async I/O controller tears the
        connection down and deregisters it.

        @since %ServerSentEventHandler 1.2
    */
    private bool dropIfOverflowing(ServerSentEventConnection conn) {
        if (!max_queue_size || conn.getQueueSize() < max_queue_size) {
            return False;
        }
        logWarn("SSE connection %y outbound queue reached the limit (%d events); dropping the slow client to "
            "protect server memory", conn.cid, max_queue_size);
        try {
            # abort the stream (closes the client connection) and deregister it (removes it from the connection map
            # and runs the close hooks) so the slow client cannot continue to consume server memory
            conn.stop();
        } catch (hash<ExceptionInfo> ex) {
            logError("error stopping overflowing SSE connection %y: %s", conn.cid, get_exception_string(ex));
        }
        # NOTE: must NOT be called while holding the connection lock (deregisterConnection() takes the write lock)
        deregisterConnection(conn.cid);
        return True;
    }

    #! sends a message to the given connection ID
    /** ServerSentEvent connections are identified by their HTTP connection ID as passed in
        @ref ServerSentEventHandler::ServerSentEventHandler::handleRequest() "ServerSentEventHandler::handleRequest()"
        in the \c "cx.id" argument when the connection is first established.

        Messages are automatically encoded with @ref WebSocketUtil::ws_encode_message() before sending
    */
    sendOne(softstring id, hash<SseMessageInfo> msg) {
        ServerSentEventConnection conn;
        {
            rwl.readLock();
            on_exit rwl.readUnlock();

            if (!exists ch{id})
                throw "SERVERSENTEVENT-CONNECTION-ERROR", sprintf("connection %y does not exist (valid "
                    "connections: %y)", id, keys ch);
            conn = ch{id};
        }
        #logDebug("sending data to connection: %y", id);
        # drop the client instead of sending if its outbound queue is over the limit (done outside the connection
        # lock because the drop path takes the write lock)
        if (dropIfOverflowing(conn)) {
            return;
        }
        conn.send(msg);
    }

    #! stop given connection ID
    stopOne(softstring id) {
        rwl.readLock();
        on_exit rwl.readUnlock();

        if (!exists ch{id})
            throw "SERVERSENTEVENT-CONNECTION-ERROR", sprintf("connection %y does not exist (valid connections: %y)",
                id, keys ch);
        #logDebug("stopping connection: %y", id);
        ch{id}.stop();
    }

    #! Log to the logger
    logInfo(string fmt, ...) {
        logArgs(LoggerLevel::INFO, fmt, argv);
    }

    #! Log to the logger
    logError(string fmt, ...) {
        logArgs(LoggerLevel::ERROR, fmt, argv);
    }

    #! Log to the logger
    logWarn(string fmt, ...) {
        logArgs(LoggerLevel::WARN, fmt, argv);
    }

    #! Log to the logger
    logDebug(string fmt, ...) {
        logArgs(LoggerLevel::DEBUG, fmt, argv);
    }

    #! Returns the heartbeat message to send
    private string getHeartbeatMessage(ServerSentEventConnection conn) {
        return heartbeat_msg;
    }

    #! Called while holding the write lock to register the new connection
    private registerNewConnectionIntern(string cid, ServerSentEventConnection conn) {
        ch{cid} = conn;
    }

    #! Called when a connection is removed from the connection map
    private *ServerSentEventConnection removeConnectionLocked(string cid) {
        return remove ch{cid};
    }

    #! Resend all events since the given event
    private replayEventsSince(ServerSentEventConnection conn, auto id) {
        # override in child classes
    }

    #! Common constructor initialization
    private constructorInit(*hash<auto> opts) {
        *string asyncapi_url;
        foreach hash<auto> h in (opts.pairIterator()) {
            switch (h.key) {
                case "heartbeat": {
                    if (!h.value.intp()) {
                        throw "SERVERSENTEVENTHANDLER-OPTION-ERROR", sprintf("option %y value %y cannot be converted "
                            "to an integer giving the number of seconds in the heartbeat interval", h.key, h.value);
                    }
                    setHeartbeat(h.value);
                    break;
                }
                case "logger": {
                    if (!(h.value instanceof LoggerInterface)) {
                        throw "SERVERSENTEVENTHANDLER-OPTION-ERROR", sprintf("option %y has type %y; expecting "
                            "\"LoggerInterface\"", h.key, h.value.fullType());
                    }
                    logger = h.value;
                    break;
                }
                case "data": {
                    *code<string(auto)> c = DataSerializationSupport{h.value};
                    if (!c) {
                        throw "SERVERSENTEVENTHANDLER-OPTION-ERROR", sprintf("option %y has type %y; expecting "
                            "\"LoggerInterface\"", h.key, h.value.fullType());
                    }
                    serialization = h.value;
                    serializer = c;
                    break;
                }
                case "asyncapi": {
                    if (h.value.typeCode() != NT_STRING || !h.value.val()) {
                        throw "SERVERSENTEVENTHANDLER-OPTION-ERROR", sprintf("option %y has type %y and value %y; "
                            "expecting a non-empty string URL or file path", h.key, h.value.fullType(), h.value);
                    }
                    asyncapi_url = h.value;
                    break;
                }
                case "validator": {
                    if (!(h.value instanceof AsyncApi::AsyncApi3Schema)) {
                        throw "SERVERSENTEVENTHANDLER-OPTION-ERROR", sprintf("option %y has type %y; expecting "
                            "\"AsyncApi::AsyncApi3Schema\"", h.key, h.value.fullType());
                    }
                    asyncapi_validator = h.value;
                    break;
                }
                case "asyncapi_channel": {
                    if (h.value.typeCode() != NT_STRING || !h.value.val()) {
                        throw "SERVERSENTEVENTHANDLER-OPTION-ERROR", sprintf("option %y has type %y and value %y; "
                            "expecting a non-empty string channel name", h.key, h.value.fullType(), h.value);
                    }
                    asyncapi_channel = h.value;
                    break;
                }
                case "max_queue_size": {
                    if (!h.value.intp() || h.value.toInt() < 0) {
                        throw "SERVERSENTEVENTHANDLER-OPTION-ERROR", sprintf("option %y value %y is not a "
                            "non-negative integer giving the maximum per-connection outbound event queue size "
                            "(0 = unlimited)", h.key, h.value);
                    }
                    max_queue_size = h.value.toInt();
                    break;
                }
                default:
                    throw "SERVERSENTEVENTHANDLER-OPTION-ERROR", sprintf("option %y is unknown; known options: %y",
                        h.key, Options);
            }
        }
        if (!asyncapi_validator && asyncapi_url) {
            asyncapi_validator = AsyncApi::AsyncApi3Loader::fromUrl(asyncapi_url);
        }
        if (asyncapi_validator && !asyncapi_channel) {
            throw "SERVERSENTEVENTHANDLER-OPTION-ERROR", "option \"asyncapi_channel\" is required when "
                "\"asyncapi\" or \"validator\" is configured";
        }
        if (!asyncapi_validator && asyncapi_channel) {
            throw "SERVERSENTEVENTHANDLER-OPTION-ERROR", "option \"asyncapi_channel\" requires \"asyncapi\" or "
                "\"validator\"";
        }
        if (!serializer) {
            serialization = DataSerializationSupport.firstKey();
            serializer = DataSerializationSupport.firstValue();
        }
    }

}

}
