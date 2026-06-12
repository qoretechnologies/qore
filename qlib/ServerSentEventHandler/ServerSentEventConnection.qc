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
        #! Async SSE connection used to deliver events via the I/O controller
        /** Protocol-neutral abstraction; concrete type depends on the HTTP
            version of the underlying stream:
            - HTTP/1.x: @ref HttpServerAsyncIo::Http1SseConnection wrapping an
              @ref HttpServerAsyncIo::HttpSseStreamPollOperation
            - HTTP/2:   @ref HttpServerAsyncIo::Http2SseStream
            - HTTP/3:   @ref HttpServerAsyncIo::Http3SseStream
        */
        *HttpServerAsyncIo::AbstractSseConnection sse_conn;

        #! Optional AsyncAPI validator
        *AsyncApi::AsyncApi3Schema asyncapi_validator;

        #! AsyncAPI channel name for validation
        *string asyncapi_channel;
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

    #! Sets the async SSE connection for event delivery (HTTP/1.x convenience wrapper)
    /** Wraps the H1-specific @ref HttpServerAsyncIo::HttpSseStreamPollOperation
        in an @ref HttpServerAsyncIo::Http1SseConnection adapter and stores it
        as the protocol-neutral SSE connection.

        @param op the HTTP/1.x poll operation to use for sending SSE events
    */
    setAsyncPollOp(HttpServerAsyncIo::HttpSseStreamPollOperation op) {
        sse_conn = new HttpServerAsyncIo::Http1SseConnection(op);
    }

    #! Returns the HTTP/1.x async poll operation, if this connection uses one
    /** @return the underlying poll operation for H1 SSE connections, or
            NOTHING if this connection is HTTP/2, HTTP/3, or not yet wired up
    */
    *HttpServerAsyncIo::HttpSseStreamPollOperation getAsyncPollOp() {
        if (sse_conn instanceof HttpServerAsyncIo::Http1SseConnection) {
            return cast<HttpServerAsyncIo::Http1SseConnection>(sse_conn).getPollOperation();
        }
        return NOTHING;
    }

    #! Sets the async SSE connection for event delivery
    /** Used for HTTP/2 and HTTP/3 SSE where the underlying transport is a
        multiplexed stream, not a dedicated HTTP/1.x poll operation.  For
        HTTP/1.x use @ref setAsyncPollOp() which wraps the poll operation in
        an @ref HttpServerAsyncIo::Http1SseConnection.

        @param conn the protocol-neutral SSE connection

        @since %ServerSentEventHandler 1.2
    */
    setAsyncSseConnection(HttpServerAsyncIo::AbstractSseConnection conn) {
        sse_conn = conn;
    }

    #! Returns the async SSE connection, if set
    *HttpServerAsyncIo::AbstractSseConnection getAsyncSseConnection() {
        return sse_conn;
    }

    #! Pushes an already-encoded message for sending via the async I/O controller
    sendEncoded(string msg) {
        if (sse_conn) {
            sse_conn.queueRawString(msg);
        }
    }

    #! Encodes and queues an SSE message for sending
    send(hash<SseMessageInfo> msg) {
        msg = validateOutgoingEvent(msg);
        sendEncoded(handler.encodeMessage(msg));
    }

    #! Sets an AsyncAPI validator for outbound SSE event data
    /** @param validator the AsyncAPI schema validator
        @param channel the channel name to use for validation
    */
    setAsyncApiValidator(AsyncApi::AsyncApi3Schema validator, string channel) {
        asyncapi_validator = validator;
        asyncapi_channel = channel;
    }

    #! Returns the current AsyncAPI validator, if any
    *AsyncApi::AsyncApi3Schema getAsyncApiValidator() {
        return asyncapi_validator;
    }

    #! Returns the current AsyncAPI channel name, if any
    *string getAsyncApiChannel() {
        return asyncapi_channel;
    }

    #! Clears the AsyncAPI validator
    clearAsyncApiValidator() {
        delete asyncapi_validator;
        delete asyncapi_channel;
    }

    #! Validates an outgoing SSE event payload against the AsyncAPI schema
    hash<SseMessageInfo> validateOutgoingEvent(hash<auto> event) {
        if (asyncapi_validator && asyncapi_channel) {
            auto payload = exists event.data ? event.data : NOTHING;
            hash<auto> headers = getAsyncApiHeaders(event);
            asyncapi_validator.validateOutgoingMessage(asyncapi_channel, \payload, \headers);
            if (exists event.data) {
                event.data = payload;
            }
        }
        return cast<hash<SseMessageInfo>>(event);
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
        if (sse_conn) {
            sse_conn.abort();
        }
    }

    private static hash<auto> getAsyncApiHeaders(hash<auto> event) {
        hash<auto> headers;
        foreach string key in (("event", "id", "retry", "comment")) {
            if (exists event{key}) {
                headers{key} = event{key};
            }
        }
        return headers;
    }
}
}
