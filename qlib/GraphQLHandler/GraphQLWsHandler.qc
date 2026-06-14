# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file GraphQLWsHandler.qc GraphQL subscriptions over WebSocket (graphql-transport-ws protocol)

/*  GraphQLHandler Copyright (C) 2026 Qore Technologies, s.r.o.

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

public namespace GraphQLHandler {
#! A WebSocket handler exposing GraphQL operations (including subscriptions) over the
#! <a href="https://github.com/enisdenjo/graphql-ws">graphql-transport-ws</a> protocol
/** Attach to an @ref httpserverintro "HttpServer" like any other WebSocket handler. Each client
    connection speaks the graphql-transport-ws message protocol:
    - the client sends \c connection_init; the server replies \c connection_ack
    - the client sends \c subscribe with an \c id and a \c payload (\c query / \c variables /
      \c operationName); the server replies with one or more \c next messages and a final
      \c complete (or an \c error message before the stream begins)
    - query and mutation operations execute once (a single \c next then \c complete); a
      subscription operation streams a \c next per event from its event source
    - the client sends \c complete with an \c id to stop a subscription; \c ping is answered with
      \c pong

    A subscription field's resolver must return a @ref Qore::AbstractIterator (the event source);
    the engine executes the subscription's selection set against each yielded event.

    @note A subscription stops when its event source is exhausted, or after its next event once the
    client sends \c complete or disconnects; an event source intended to be cancellable should be
    finite or support interruption.

    @par Example:
    @code{.py}
GraphQLExecutor executor(schema, resolvers);
GraphQLWsHandler ws(executor);
HttpServer server(<HttpServerOptionInfo>{});
server.setHandler("graphql-ws", "/graphql", NOTHING, ws, ("Sec-WebSocket-Protocol",), False);
    @endcode
*/
public class GraphQLWsHandler inherits WebSocketHandler::WebSocketHandler {
    private {
        #! the execution engine
        GraphQLExecutor executor;
    }

    #! creates the handler for the given execution engine
    /** @param n_executor the GraphQL execution engine
        @param auth an optional authenticator for the handler
    */
    constructor(GraphQLExecutor n_executor, *HttpServer::AbstractAuthenticator auth)
            : WebSocketHandler::WebSocketHandler(auth) {
        executor = n_executor;
    }

    #! returns a GraphQL WebSocket connection object for a new client connection
    WebSocketHandler::WebSocketConnection getConnectionImpl(hash<auto> cx, hash<auto> hdr, string cid) {
        return new GraphQLWsConnection(self, executor, cx);
    }
}

#! A single graphql-transport-ws client connection
public class GraphQLWsConnection inherits WebSocketHandler::WebSocketConnection {
    private {
        #! the execution engine
        GraphQLExecutor executor;
        #! the resolver context value (the HTTP call context)
        auto gctx;
        #! active subscription ids -> running flag
        hash<string, bool> active;
        #! serializes access to @ref active
        Qore::Thread::Mutex sub_lock();
    }

    #! creates the connection
    constructor(GraphQLWsHandler handler, GraphQLExecutor n_executor, auto n_ctx)
            : WebSocketHandler::WebSocketConnection(handler) {
        executor = n_executor;
        gctx = n_ctx;
    }

    #! handles an incoming text protocol message
    gotMessage(string msg) {
        hash<auto> m;
        try {
            m = parse_json(msg);
        } catch (hash<ExceptionInfo> ex) {
            sendMsg({"type": "error", "payload": ({"message": "invalid JSON message"},)});
            return;
        }
        switch (m.type) {
            case "connection_init":
                sendMsg({"type": "connection_ack"});
                break;
            case "ping":
                sendMsg({"type": "pong"});
                break;
            case "pong":
                break;
            case "subscribe":
                handleSubscribe(m.id, m.payload);
                break;
            case "complete":
                stopSubscription(m.id);
                break;
        }
    }

    #! handles an incoming binary protocol message (decoded as UTF-8 text)
    gotMessage(binary msg) {
        gotMessage(msg.toString());
    }

    #! starts a query/mutation (one-shot) or subscription (streamed) operation
    private handleSubscribe(string id, hash<auto> payload) {
        *string query = payload.query;
        if (!query.val()) {
            sendMsg({"type": "error", "id": id, "payload": ({"message": "no query provided"},)});
            return;
        }
        *string opType = executor.getOperationType(query, payload.operationName);
        if (opType == "subscription") {
            hash<auto> subr = executor.executeSubscription(query, payload.variables,
                payload.operationName, gctx);
            if (exists subr.errors) {
                sendMsg({"type": "error", "id": id, "payload": subr.errors});
                return;
            }
            {
                AutoLock al(sub_lock);
                active{id} = True;
            }
            # the event source is created and consumed in the stream thread (iterators are
            # thread-bound)
            background streamLoop(id, subr.subscription);
        } else {
            # query or mutation: execute once, then complete
            hash<auto> result = executor.execute(query, payload.variables, payload.operationName, gctx);
            sendMsg({"type": "next", "id": id, "payload": result});
            sendMsg({"type": "complete", "id": id});
        }
    }

    #! iterates a subscription's event source, sending a \c next per event then \c complete
    /** the event source is obtained here (in this thread) because Qore iterators are thread-bound
    */
    private streamLoop(string id, hash<auto> subscription) {
        try {
            AbstractIterator it = executor.getSubscriptionStream(subscription);
            while (isActive(id) && it.next()) {
                hash<auto> ev = executor.resolveSubscriptionEvent(subscription, it.getValue());
                sendMsg({"type": "next", "id": id, "payload": ev});
            }
            if (isActive(id)) {
                sendMsg({"type": "complete", "id": id});
            }
        } catch (hash<ExceptionInfo> ex) {
            sendMsg({"type": "error", "id": id, "payload": ({"message": ex.desc},)});
        }
        AutoLock al(sub_lock);
        remove active{id};
    }

    #! returns @ref True if the given subscription is still active
    private bool isActive(string id) {
        AutoLock al(sub_lock);
        return active{id} ?? False;
    }

    #! marks a subscription as stopped (its stream thread exits after its current/next event)
    private stopSubscription(string id) {
        AutoLock al(sub_lock);
        if (exists active{id}) {
            active{id} = False;
        }
    }

    #! the connection has closed: stop all active subscriptions
    connectionClosed() {
        AutoLock al(sub_lock);
        foreach string id in (keys active) {
            active{id} = False;
        }
    }

    #! sends a protocol message as JSON text to the client
    private sendMsg(hash<auto> m) {
        try {
            send(make_json(m));
        } catch (hash<ExceptionInfo> ex) {
            # the connection may have closed underneath a streaming thread; ignore send failures
        }
    }
}
}
