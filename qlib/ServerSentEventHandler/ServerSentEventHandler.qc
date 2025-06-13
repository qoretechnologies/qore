# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ServerSentEventConnection.qc Qore handler definition for the HttpServer module for providing SSE connections

/*  ServerSentEventHandler.qc Copyright 2025 Qore Technologies, s.r.o.

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
        );

        #! Response header to SSE requests
        const SseResponseHeader = {
            "Cache-Control": "no-cache",
            "Connection": "Keep-Alive",
            "Content-Type": MimeTypeSse,
        };

        #! Data serialization support
        const DataSerializationSupport = {
%ifndef NoJson
            "json": \ServerSentEventHandler::serializeJson(),
%endif
            "none": \ServerSentEventHandler::serializeDefault(),
        };
    }

    private {
        #! connection read-write lock
        RWLock rwl();

        #! The value of the serialization option
        string serialization;

        #! Value serialization code
        code serializer;

        #! connection hash
        hash<string, ServerSentEventConnection> ch;

        #! heartbeat interval in seconds
        int heartbeat = DefaultHeartbeatInterval;

        #! heartbeat message returned by getHeartbeatMessage()
        string heartbeat_msg = DefaultHeartbeatMsg;
    }

    private:internal {
        #! Thread-local key for I/O check
        string tlk = get_random_string(50);
    }

    #! Create the object optionally with the given @ref HttpServer::AbstractAuthenticator "AbstractAuthenticator"
    /** @param auth the authentication object to use to authenticate connections (see
        @ref HttpServer::AbstractAuthenticator "AbstractAuthenticator"); if no
        @ref HttpServer::AbstractAuthenticator "AbstractAuthenticator" object is passed, then by default no
        authentication will be required
        @param opts options for the object with the following optional keys:
        - \c heartbeat: a positive integer value giving seconds for the heartbeat interval for the server connection
          (default: 20 seconds)
        - \c logger: a LoggerInterface object for logging

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
        if (seconds <= 0) {
            throw "SERVERSENTEVENTHANDLER-OPTION-ERROR", sprintf("option \"heartbeat\" value %y is not a positive "
                "integer giving the number of seconds in the heartbeat interval", seconds);
        }
        heartbeat = seconds;
        logDebug("Set heartbeat to %d second%s", heartbeat, heartbeat == 1 ? "" : "s");
    }

    #! sets the heartbeat interval as a number of seconds
    /** @param seconds the heartbeat interval as a number of seconds
    */
    setHeartbeat(date seconds) {
        int secs = seconds.durationSeconds();
        if (secs <= 0) {
            throw "SERVERSENTEVENTHANDLER-OPTION-ERROR", sprintf("option \"heartbeat\" value %y is not a positive "
                "integer giving the number of seconds in the heartbeat interval", seconds);
        }
        heartbeat = secs;
        logDebug("Set heartbeat to %d second%s", heartbeat, heartbeat == 1 ? "" : "s");
    }

    #! called by the HTTP server to handle incoming HTTP requests
    /** To accept a dedicated connection; make sure the return value hash's \c "code" key is 101 (i.e.
        \c "Switching Protocols") and the \c "close" key is not @ref False "False"

        @param cx call context hash; this hash will have the following keys:
        - \c socket: the bind address used to bind the listener (\c "socket-info" provides more detailed information)
        - \c socket-info: a hash of socket information for the listening socket (as returned by
          Qore::Socket::getSocketInfo())
        - \c peer-info: a hash of socket information for the remote socket (as returned by
          Qore::Socket::getPeerInfo())
        - \c url: a hash of broken-down URL information (as returned from parseURL())
        - \c id: the unique HTTP connection ID; this ID is also used to identify the HTTP client connection in
          @ref ServerSentEventHandler::ServerSentEventHandler::sendOne() "ServerSentEventHandler::sendOne()"
        - \c listener-id: the HTTP server listener ID (see HttpServer::HttpServer::getListenerInfo() )
        - \c user: the current RBAC username (if any)
        @param hdr incoming header hash; all keys will be converted to lower-case, additionally the following keys
        will be present:
        - \c method: the HTTP method received (ie \c "GET", \c "POST", etc)
        - \c path: the HTTP path given in the request, after processing by decode_url() (Qore function)
        - \c http_version: the HTTP version number in the request (either \c "1.0" or \c "1.1")
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
        #logDebug("SSE request: cx: %N", cx);
        #logDebug("SSE request: hdr: %N", hdr);
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

    #! Called from the HTTP server after the handleRequest() method indicates that a dedicated connection should be established
    /** This method should not return until the connection is closed or the stop() method is called

        @param lid the unique HTTP listener ID
        @param cx call context hash; this hash will have the following keys:
        - \c socket: the bind address used to bind the listener (\c "socket-info" provides more detailed information)
        - \c socket-info: a hash of socket information for the listening socket (as returned by
          Qore::Socket::getSocketInfo())
        - \c peer-info: a hash of socket information for the remote socket (as returned by
          Qore::Socket::getPeerInfo())
        - \c url: a hash of broken-down URL information (as returned from parseURL())
        - \c id: the unique HTTP connection ID
        - \c listener-id: the HTTP server listener ID (see HttpServer::HttpServer::getListenerInfo())
        - \c user: the current RBAC username (if any)
        @param hdr a hash of headers in the request
        @param sock the @ref Qore::Socket "Socket" object for the dedicated connection to the client
    */
    bool startImpl(softstring lid, hash<auto> cx, hash<auto> hdr, Qore::Socket sock) {
        logDebug("ServerSentEventHandler::startImpl() cid: %y from %s:%d", cx.id, cx."peer-info".hostname_desc,
            cx."peer-info".port);

        softstring cid = cx.id;

        # get new connection object
        ServerSentEventConnection conn = doNewConnection(sock, cx, hdr, cid);
        try {
            # issue #3225: handle exceptions in ServerSentEventHandler and remove the connection
            conn.registered();
        } catch (hash<ExceptionInfo> ex) {
            logError("Exception registering ServerSentEvent connection: %s", get_exception_string(ex));
            rwl.writeLock();
            on_exit rwl.writeUnlock();
            removeConnectionLocked(cid);
            rethrow;
        }
        connectionRegisteredImpl(conn);

        on_exit {
            #logDebug("ServerSentEventHandler::startImpl(): close connection: cid: %y from %s:%d", cx.id,
            #    cx."peer-info".hostname_desc, cx."peer-info".port);
            try {
                deregisterConnectionImpl(conn);
                conn.connectionClosed();
            } catch (hash<ExceptionInfo> ex) {
                logError("Exception deregistering ServerSentEvent connection: %s", get_exception_string(ex));
            }
            rwl.writeLock();
            on_exit rwl.writeUnlock();
            removeConnectionLocked(cid);
        }

        # issue #2887 timestamp of last traffic
        int timestamp = clock_getmillis() / 1000;

        while (True) {
            #logDebug("ServerSentEventHandler::startImpl() loop cid: %y", cx.id);
            if (stop || lsh{lid} || conn.stop_flag) {
                logDebug("ServerSentEventHandler::startImpl() cid: %y stopping; stop: %y lsh{%y}: %y, "
                    "conn.stop_flag: %y", cx.id, stop, lid, lsh{lid}, conn.stop_flag);
                break;
            }

            try {
                bool update;
                # send any and all messages on the queue
                *string str = conn.pollData();
                if (str.val()) {
                    sock.send2(str, DefaultSocketSendTimeout);
                    update = True;
                    flushIoIntern(conn, sock, False);
                }

                if (conn.stop_flag) {
                    continue;
                }
                int now = clock_getmillis() / 1000;
                if (!update) {
                    if ((now - timestamp) >= heartbeat) {
                        sendHeartbeatMsg(conn, sock);
                        timestamp = now;
                    }
                } else {
                    timestamp = now;
                }

                if (!sock.isOpen()) {
                    logInfo("SSE cid: %y remote end closed the connection", cx.id);
                    break;
                }
            } catch (hash<ExceptionInfo> ex) {
                # error in serversentevent handler server, log and close the connection
                string err = sprintf("%s: %s: %s; terminating connection thread", get_ex_pos(ex), ex.err, ex.desc);
                logError(err);
                break;
            }
        }
        #logDebug("ServerSentEventHandler::startImpl() exiting, cid: %y", cx.id);
        return False;
    }

    #! Flushes I/O in the connection queue
    /** @return True if any data was flushed, False if not
    */
    bool flushIo(ServerSentEventConnection conn, Socket sock) {
        return flushIoIntern(conn, sock, True);
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

%ifndef NoJson
    #! JSON value serialization
    static string serializeJson(auto v) {
        return make_json(v);
    }
%endif

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

    #! Called when a connection is established; the default implementation creates a ServerSentEventConnection object
    ServerSentEventConnection getConnectionImpl(Socket sock, hash<auto> cx, hash<auto> hdr, string cid) {
        return new ServerSentEventConnection(self, sock, cx, hdr, cid);
    }

    #! Called when a connection is established; registers the new connection internally
    ServerSentEventConnection doNewConnection(Socket sock, hash<auto> cx, hash<auto> hdr, string cid) {
        ServerSentEventConnection rv = getConnectionImpl(sock, cx, hdr, cid);

        # add connection object to hash
        {
            rwl.writeLock();
            on_exit rwl.writeUnlock();
            registerNewConnectionIntern(cid, rv);
        }

        if (hdr."last-event-id".val()) {
            replayEventsSince(rv, hdr."last-event-id");
        }

        return rv;
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
    /** @param d the data to send

        Messages are automatically encoded with @ref ServerSentEventUtil::ws_encode_message() before sending
    */
    sendAll(hash<SseMessageInfo> msg) {
        string str = encodeMessage(msg);

        rwl.readLock();
        on_exit rwl.readUnlock();

        # push the data on all connection queues
        map $1.sendEncoded(str), ch.iterator();
        logInfo("SSE sent to connections: %y", keys ch);
        #if (ch) logDebug("sending data to connections: %y", keys ch);
    }

    #! sends a message to the given connection ID
    /** ServerSentEvent connections are identified by their HTTP connection ID as passed in
        @ref ServerSentEventHandler::ServerSentEventHandler::handleRequest() "ServerSentEventHandler::handleRequest()"
        in the \c "cx.id" argument when the connection is first established.

        Messages are automatically encoded with @ref ServerSentEventUtil::ws_encode_message() before sending
    */
    sendOne(softstring id, hash<SseMessageInfo> msg) {
        string str = encodeMessage(msg);

        rwl.readLock();
        on_exit rwl.readUnlock();

        if (!exists ch{id})
            throw "SERVERSENTEVENT-CONNECTION-ERROR", sprintf("connection %y does not exist (valid connections: %y)",
                id, keys ch);
        #logDebug("sending data to connection: %y", id);
        ch{id}.sendEncoded(msg);
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
    private removeConnectionLocked(string cid) {
        remove ch{cid};
    }

    #! Send the default heartbeat message
    private sendHeartbeatMsg(ServerSentEventConnection conn, Socket sock) {
        sock.send2(getHeartbeatMessage(conn), DefaultSocketSendTimeout);
    }

    #! Resend all events since the given event
    private replayEventsSince(ServerSentEventConnection conn, auto id) {
        # override in child classes
    }

    #! Common constructor initialization
    private constructorInit(*hash<auto> opts) {
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
                    *code c = DataSerializationSupport{h.value};
                    if (!c) {
                        throw "SERVERSENTEVENTHANDLER-OPTION-ERROR", sprintf("option %y has type %y; expecting "
                            "\"LoggerInterface\"", h.key, h.value.fullType());
                    }
                    serialization = h.value;
                    serializer = c;
                    break;
                }
                default:
                    throw "SERVERSENTEVENTHANDLER-OPTION-ERROR", sprintf("option %y is unknown; known options: %y",
                        h.key, Options);
            }
        }
        if (!serializer) {
            serialization = DataSerializationSupport.firstKey();
            serializer = DataSerializationSupport.firstValue();
        }
    }

    private:internal bool flushIoIntern(ServerSentEventConnection conn, Socket sock, bool update_tld) {
        bool rv;
        while (!conn.stop_flag && exists (*string d = conn.pollDataImmediate())) {
            #printf("TID %d: ServerSentEventHandler::flushIo() sending msg %d bytes\n", gettid(), d.size());
            sock.send2(d, DefaultSocketSendTimeout);
            if (d && !rv) {
                rv = True;
                if (update_tld) {
                    save_thread_data(tlk, True);
                }
            }
        }
        return rv ?? False;
    }
}
}
