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
        #! Async SSE poll operation for event delivery via the I/O controller
        *HttpServerAsyncIo::HttpSseStreamPollOperation poll_op;
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

    #! Sets the async poll operation for event delivery
    /** @param op the poll operation to use for sending SSE events via the async I/O controller
    */
    setAsyncPollOp(HttpServerAsyncIo::HttpSseStreamPollOperation op) {
        poll_op = op;
    }

    #! Returns the async poll operation, if set
    *HttpServerAsyncIo::HttpSseStreamPollOperation getAsyncPollOp() {
        return poll_op;
    }

    #! Pushes an already-encoded message for sending via the async I/O controller
    sendEncoded(string msg) {
        if (poll_op) {
            poll_op.queueRawString(msg);
        }
    }

    #! Encodes and queues an SSE message for sending
    send(hash<SseMessageInfo> msg) {
        sendEncoded(handler.encodeMessage(msg));
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

    #! Stops the connection
    stop() {
        stop_flag = True;
        if (poll_op) {
            poll_op.abort();
        }
    }
}
}