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
    the engine executes the subscription's selection set against each yielded event. For a
    long-lived source, return a @ref GraphQLHandler::GraphQLEventQueue "GraphQLEventQueue" (or any
    @ref GraphQLHandler::GraphQLCancellableSource "GraphQLCancellableSource"): the handler then
    \c cancel()s it when the client sends \c complete or disconnects, unblocking a stream waiting
    for an event so it exits promptly.

    @note The handler enforces the protocol lifecycle (\c connection_init must precede any other
    message; a duplicate \c connection_init or a duplicate subscription \c id closes the connection)
    and a per-connection concurrent-subscription cap (the \c max_subscriptions option). A
    subscription stops when its event source is exhausted, or when the client sends \c complete or
    disconnects.

    @par Example:
    @code{.py}
GraphQLExecutor executor(schema, resolvers);
GraphQLWsHandler ws(executor);
HttpServer server(<HttpServerOptionInfo>{});
server.setHandler("graphql-ws", "/graphql", NOTHING, ws, ("Sec-WebSocket-Protocol",), False);
    @endcode
*/
public class GraphQLWsHandler inherits WebSocketHandler::WebSocketHandler {
    public {
        #! the default maximum number of concurrent subscriptions per connection
        const DefaultMaxSubscriptions = 100;
    }

    private {
        #! the execution engine
        GraphQLExecutor executor;
        #! the maximum number of concurrent subscriptions allowed per connection
        int max_subscriptions = DefaultMaxSubscriptions;
    }

    #! creates the handler for the given execution engine
    /** @param n_executor the GraphQL execution engine
        @param auth an optional authenticator for the handler
        @param opts optional options:
        - \c max_subscriptions (int): the maximum number of concurrent subscriptions allowed per
          connection (default @ref DefaultMaxSubscriptions); additional \c subscribe requests are
          rejected with an \c error message until a subscription completes
    */
    constructor(GraphQLExecutor n_executor, *HttpServer::AbstractAuthenticator auth, *hash<auto> opts)
            : WebSocketHandler::WebSocketHandler(auth) {
        executor = n_executor;
        if (exists opts.max_subscriptions) {
            if (opts.max_subscriptions < 1) {
                throw "GRAPHQL-WS-OPTION-ERROR", sprintf("max_subscriptions must be >= 1 (got %y)",
                    opts.max_subscriptions);
            }
            max_subscriptions = opts.max_subscriptions;
        }
    }

    #! returns a GraphQL WebSocket connection object for a new client connection
    WebSocketHandler::WebSocketConnection getConnectionImpl(hash<auto> cx, hash<auto> hdr, string cid) {
        return new GraphQLWsConnection(self, executor, cx, max_subscriptions);
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
        #! active subscription ids -> their event source (for cross-thread cancellation)
        hash<string, object> sources;
        #! @ref True once the client has sent \c connection_init and received \c connection_ack
        bool initialized = False;
        #! the maximum number of concurrent subscriptions allowed on this connection
        int max_subscriptions;
        #! serializes access to @ref active, @ref sources, and @ref initialized
        Qore::Thread::Mutex sub_lock();
    }

    #! creates the connection
    constructor(GraphQLWsHandler handler, GraphQLExecutor n_executor, auto n_ctx, int n_max_subscriptions)
            : WebSocketHandler::WebSocketConnection(handler) {
        executor = n_executor;
        gctx = n_ctx;
        max_subscriptions = n_max_subscriptions;
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
        # per graphql-transport-ws, the only message allowed before connection_init is
        # connection_init itself; anything else is a protocol violation and the connection is closed
        if (!isInitialized() && m.type != "connection_init") {
            sendMsg({"type": "error",
                "payload": ({"message": "connection_init must be sent before any other message"},)});
            stop();
            return;
        }
        switch (m.type) {
            case "connection_init":
                handleConnectionInit();
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

    #! handles \c connection_init: replies with \c connection_ack, or closes on a duplicate
    private handleConnectionInit() {
        {
            AutoLock al(sub_lock);
            if (initialized) {
                # graphql-transport-ws: too many initialisation requests -> close the connection
                sendMsg({"type": "error",
                    "payload": ({"message": "too many initialisation requests"},)});
                stop();
                return;
            }
            initialized = True;
        }
        sendMsg({"type": "connection_ack"});
    }

    #! returns @ref True if the connection has completed the connection_init handshake
    private bool isInitialized() {
        AutoLock al(sub_lock);
        return initialized;
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
            {
                AutoLock al(sub_lock);
                # graphql-transport-ws: a subscribe with an id already in use closes the connection
                if (exists active{id}) {
                    sendMsg({"type": "error", "id": id,
                        "payload": ({"message": sprintf("subscriber for id %y already exists", id)},)});
                    stop();
                    return;
                }
                # enforce the per-connection concurrent-subscription cap
                if (active.size() >= max_subscriptions) {
                    sendMsg({"type": "error", "id": id, "payload": ({"message":
                        sprintf("the per-connection subscription limit (%d) has been reached",
                            max_subscriptions)},)});
                    return;
                }
            }
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
    /** the event source is obtained here (in this thread) because Qore iterators are thread-bound;
        a @ref GraphQLHandler::GraphQLCancellableSource "GraphQLCancellableSource" is registered so
        @ref stopSubscription() / @ref connectionClosed() can unblock a source waiting for an event
    */
    private streamLoop(string id, hash<auto> subscription) {
        AbstractIterator it;
        try {
            it = executor.getSubscriptionStream(subscription);
        } catch (hash<ExceptionInfo> ex) {
            sendMsg({"type": "error", "id": id, "payload": ({"message": ex.desc},)});
            AutoLock al(sub_lock);
            remove active{id};
            return;
        }
        # register a cancellable source so a blocked next() can be interrupted from another thread;
        # if the subscription was already stopped before registration, cancel immediately
        if (it instanceof GraphQLCancellableSource) {
            AutoLock al(sub_lock);
            if (active{id}) {
                sources{id} = it;
            } else {
                cast<GraphQLCancellableSource>(it).cancel();
            }
        }
        try {
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
        remove sources{id};
    }

    #! returns @ref True if the given subscription is still active
    private bool isActive(string id) {
        AutoLock al(sub_lock);
        return active{id} ?? False;
    }

    #! marks a subscription as stopped, cancelling its event source so a blocked stream thread exits
    private stopSubscription(string id) {
        AutoLock al(sub_lock);
        if (exists active{id}) {
            active{id} = False;
            cancelSourceUnlocked(id);
        }
    }

    #! the connection has closed: stop and cancel all active subscriptions
    connectionClosed() {
        AutoLock al(sub_lock);
        foreach string id in (keys active) {
            active{id} = False;
            cancelSourceUnlocked(id);
        }
    }

    #! cancels a registered cancellable event source (caller must hold @ref sub_lock)
    private cancelSourceUnlocked(string id) {
        *object src = sources{id};
        if (src instanceof GraphQLCancellableSource) {
            try {
                cast<GraphQLCancellableSource>(src).cancel();
            } catch (hash<ExceptionInfo> ex) {
                # ignore cancellation failures
            }
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
