# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file McpSubscriptionStream.qc @brief Long-lived notification stream for modern MCP clients

/*  McpSubscriptionStream.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! the %McpServerHandler namespace holds all public definitions in the %McpServerHandler module
public namespace McpServerHandler {

#! A long-lived server-to-client notification stream opened by \c subscriptions/listen
/** In the MCP 2026-07-28 protocol revision there is no standalone \c GET event stream and
    no \c resources/subscribe RPC.  A client that wants
    server-initiated change notifications sends a \c subscriptions/listen request whose
    <i>response</i> is an SSE stream: the stream stays open and carries only the notification
    types the client explicitly opted in to.

    Every message on the stream — the acknowledgment, each notification, and the final
    response — carries the reserved \c subscriptionId key in its \c _meta member, so a
    client with several concurrent subscriptions can demultiplex them.

    Request-scoped notifications such as \c notifications/progress and
    \c notifications/message are <b>not</b> delivered here; they flow on the response stream of
    the request they relate to.

    @see @ref McpServerHandler::McpServerConnection "McpServerConnection" for the legacy
        session-scoped SSE connection
*/
public class McpSubscriptionStream inherits ServerSentEventHandler::ServerSentEventConnection {
    public {
        #! Notification sent as the first message on every subscription stream
        const AcknowledgedNotification = "notifications/subscriptions/acknowledged";

        #! Map of filter key to the notification method it enables
        const NotificationMethodMap = {
            "toolsListChanged": "notifications/tools/list_changed",
            "promptsListChanged": "notifications/prompts/list_changed",
            "resourcesListChanged": "notifications/resources/list_changed",
            "resourceSubscriptions": "notifications/resources/updated",
            # tasks extension: task state changes, so that a client can stop polling
            "tasksStatus": "notifications/tasks",
        };
    }

    private {
        #! The subscription ID: the JSON-RPC ID of the \c subscriptions/listen request
        auto subscription_id;

        #! The negotiated notification filter
        hash<auto> filter;

        #! Resource URIs this subscription covers, for fast lookup
        hash<string, bool> resource_uris = {};

        #! Whether the graceful-closure response has already been sent
        bool closed = False;

        #! Lock protecting @ref closed
        Mutex lck();
    }

    #! Creates the subscription stream
    /** @param handler the MCP server handler
        @param sock the connection socket
        @param cx the HTTP context; must carry the \c mcp_subscription key set by
            \c McpServerHandler::handleSubscriptionsListen()
        @param hdr the request headers
        @param cid the HTTP connection ID
    */
    constructor(McpServerHandler handler, Socket sock, hash<auto> cx, hash<auto> hdr, string cid)
            : ServerSentEventConnection(handler, sock, cx, hdr, cid) {
        subscription_id = cx.mcp_subscription.id;
        filter = cx.mcp_subscription.notifications ?? {};
        map resource_uris{$1} = True, filter.resourceSubscriptions ?? ();
    }

    #! Returns the subscription ID
    auto getSubscriptionId() {
        return subscription_id;
    }

    #! Returns the negotiated notification filter
    hash<auto> getFilter() {
        return filter;
    }

    #! Returns @ref True if this subscription opted in to the given notification type
    /** @param type the filter key (e.g. \c "toolsListChanged")
        @param uri for \c "resourceSubscriptions", the resource URI that changed

        @return @ref True if the notification must be delivered on this stream
    */
    bool wants(string type, *string uri) {
        if (type == "resourceSubscriptions") {
            return exists uri && resource_uris{uri} ? True : False;
        }
        return filter{type} === True;
    }

    #! Sends the acknowledgment that must precede every other message on the stream
    /** The server must send \c notifications/subscriptions/acknowledged as the first message on
        the subscription and must not send any notification before it.  The \c notifications
        member reflects the subset of the requested filter the server agreed to honor.
    */
    registered() {
        sendSubscriptionMessage(make_jsonrpc_request(AcknowledgedNotification, "2.0", NOTHING, {
            "_meta": {
                McpClient::MCP_MetaSubscriptionId: subscription_id,
            },
            "notifications": filter,
        }));
    }

    #! Delivers a notification on this stream if the client opted in to it
    /** The JSON-RPC method is derived from \a type through @ref NotificationMethodMap, so a
        notification can never be delivered under a method that does not correspond to the
        filter key the client opted in to.

        @param type the filter key (e.g. \c "toolsListChanged")
        @param params the notification parameters
        @param uri for resource updates, the URI that changed

        @throw MCP-SUBSCRIPTION-ERROR \a type is not a known notification type
    */
    notify(string type, *hash<auto> params, *string uri) {
        *string method = NotificationMethodMap{type};
        if (!method.val()) {
            throw "MCP-SUBSCRIPTION-ERROR", sprintf("unknown notification type %y; known "
                "types: %y", type, keys NotificationMethodMap);
        }
        if (!wants(type, uri)) {
            return;
        }
        hash<auto> np = (params ?? {}) + {
            "_meta": (params._meta ?? {}) + {
                McpClient::MCP_MetaSubscriptionId: subscription_id,
            },
        };
        sendSubscriptionMessage(make_jsonrpc_request(method, "2.0", NOTHING, np));
    }

    #! Ends the subscription gracefully
    /** Responds to the originating \c subscriptions/listen request with an empty result before
        closing the stream, so that the client can tell an orderly shutdown from an abrupt
        transport drop.  A client that sees the response knows the subscription closed cleanly;
        a transport that closes without one indicates an unexpected disconnect, which the
        client may treat as a trigger to reconnect.
    */
    closeGracefully() {
        {
            lck.lock();
            on_exit lck.unlock();
            if (closed) {
                return;
            }
            closed = True;
        }
        try {
            sendSubscriptionMessage(make_jsonrpc_response("2.0", subscription_id, {
                "resultType": McpClient::McpResultType::Complete,
                "_meta": {
                    McpClient::MCP_MetaSubscriptionId: subscription_id,
                },
            }));
        } catch (hash<ExceptionInfo> ex) {
            # the peer may already be gone; closing is still correct
        }
        # end the stream gracefully: AbstractSseConnection::close() drains the queued events
        # before finishing, whereas stop() aborts and would discard the response just queued
        *HttpServerAsyncIo::AbstractSseConnection conn = getAsyncSseConnection();
        if (conn) {
            conn.close();
        } else {
            stop();
        }
    }

    #! Pushes a raw JSON-RPC message onto the stream
    private sendSubscriptionMessage(string body) {
        send(<SseMessageInfo>{
            "data": body,
        });
    }
}

} # namespace McpServerHandler
