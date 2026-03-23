# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ServerSentEventConnection.qc Qore handler definition for the HttpServer module for providing SSE connections

/*  ServerSentEventConnection.qc Copyright 2025 - 2026 Qore Technologies, s.r.o.

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
#! this class represents a connection to an HTTP client
/** @par Sending ServerSentEvent Message to Clients
    To send a message to an HTTP client, call one of the following methods:
    - @ref ServerSentEventHandler::ServerSentEventConnection::send() "ServerSentEventConnection::send()": to send an
      unencoded message to the client
    - @ref ServerSentEventHandler::ServerSentEventConnection::sendEncoded() "ServerSentEventConnection::sendEncoded()":
      to send an already-encoded message to the client

    @note Reference: https://html.spec.whatwg.org/multipage/server-sent-events.html#parsing-an-event-stream
*/
public class ServerSentEventConnection {
    public {
        #! The parent handler object
        ServerSentEventHandler handler;

        #! Original request context
        hash<auto> cx;

        #! Original request header info
        hash<auto> hdr;

        #! The connection ID
        string cid;

        #! Stop connection flag
        bool stop_flag = False;

        # wait a maximum of 1/4 second for data
        const DefaultQueuePollingInterval = 250ms;
    }

    private {
        #! the @ref Qore::Thread::Channel "Channel" object stores messages to be sent to the server
        Qore::Thread::Channel channel(-1);

        #! The TID of the I/O thread
        int tid = gettid();
    }

    private:internal {
        #! The socket used by the server for this connection
        Socket sock;
    }

    #! the constructor is called when a new connection is made by an HTTP client
    /** Called from
        @ref ServerSentEventHandler::ServerSentEventHandler::getConnectionImpl() "ServerSentEventHandler::getConnectionImpl()", which
        can be overridden in subclasses to allow a subclass of this class to be used as the connection object
    */
    constructor(ServerSentEventHandler handler, Socket sock, hash<auto> cx, hash<auto> hdr, string cid) {
        self.handler := handler;
        self.cx = cx;
        self.hdr = hdr;
        self.cid = cid;
        self.sock = sock;
    }

    #! This method is called to poll for messages from the client to send from the server
    /** Called by the @ref ServerSentEventHandler::ServerSentEventHandler "ServerSentEventHandler"
    */
    *string pollData() {
        try {
            return channel.recv(DefaultQueuePollingInterval);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err != "CHANNEL-TIMEOUT") {
                rethrow;
            }
        }
    }

    #! This method is called to poll for messages to send to the client from the server
    /** Called by the @ref ServerSentEventHandler::ServerSentEventHandler "ServerSentEventHandler"
    */
    *string pollDataImmediate() {
        auto result = channel.tryRecv();
        return exists result ? result.value : NOTHING;
    }

    #! Pushes an already-encoded message on the connection's message channel
    sendEncoded(string msg) {
        try {
            channel.send(msg);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err != "CHANNEL-CLOSED") {
                rethrow;
            }
        }
    }

    #! Pushes an unencoded message on the connection's message channel
    /** The message will be encoded with @ref WebSocketUtil::ws_encode_message() before sending
    */
    send(hash<SseMessageInfo> msg) {
        try {
            channel.send(handler.encodeMessage(msg));
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err != "CHANNEL-CLOSED") {
                rethrow;
            }
        }
    }

    #! This method is called when the connection is closed
    /** Called by the @ref ServerSentEventHandler::ServerSentEventHandler "ServerSentEventHandler"

        The implementation in this class is empty; implement this method in a subclass to provide custom behavior
    */
    connectionClosed() {
    }

    #! Called when the connection has been registered in handler and accept sending messages
    /** Can be used to send initial welcome message, for example; the default implementation does nothing
    */
    registered() {
    }

    #! Flush all output data
    flush() {
        if (gettid() != tid) {
            while (!stop_flag && !channel.empty()) {
                usleep(DefaultQueuePollingInterval);
            }
        } else {
            handler.flushIo(self, sock);
        }
    }

    #! force closing socket related to connection
    stop() {
        stop_flag = True;
        # closing the channel wakes up the polling thread immediately
        channel.close();
    }
}
}