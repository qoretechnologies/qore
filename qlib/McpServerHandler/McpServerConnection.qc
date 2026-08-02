# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file McpServerConnection.qc @brief MCP Server class definition for the HttpServer module

/*  McpServerConnection.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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
#! MCP server connection for SSE streams
/** Provides access to the MCP session and protocol version for the SSE connection,
    and helper methods to send MCP notifications.
*/
public class McpServerConnection inherits ServerSentEventHandler::ServerSentEventConnection {
    private {
        #! The session identifier
        string session;

        #! The protocol version
        string version;

        #! Tools changed notification
        const McpToolsChangedNotification = "notifications/tools/list_changed";

        #! Resources changed notification
        const McpResourcesChangedNotification = "notifications/resources/list_changed";

        #! Prompts changed notification
        const McpPromptsChangedNotification = "notifications/prompts/list_changed";
    }

    constructor(McpServerHandler handler, Socket sock, hash<auto> cx, hash<auto> hdr, string cid,
            string version = McpClient::MCP_Version_Latest)
            : ServerSentEventConnection(handler, sock, cx, hdr, cid) {
        self.version = version;
        self.session = handler.getSessionId(hdr);
    }

    string getSession() {
        return session;
    }

    string getProtocolVersion() {
        return version;
    }

    #! Send tools changed notification
    sendToolsChangedNotification() {
        sendNotification(McpToolsChangedNotification);
    }

    #! Send resources changed notification
    sendResourcesChangedNotification() {
        sendNotification(McpResourcesChangedNotification);
    }

    #! Send prompts changed notification
    sendPromptsChangedNotification() {
        sendNotification(McpPromptsChangedNotification);
    }

    sendNotification(string method, *hash<auto> info) {
        send(<SseMessageInfo>{
            "data": make_jsonrpc_request(method, "2.0", NOTHING, info),
        });
    }

    #! Sends a server-initiated JSON-RPC request on the event stream
    /** @param method the method to call on the client
        @param info the request parameters

        @return the JSON-RPC ID the request was sent with
    */
    int sendRequest(string method, hash<auto> info) {
        int id = cast<McpServerHandler>(handler).getNextIdForSession(session);
        sendRequestWithId(id, method, info);
        return id;
    }

    #! Sends a server-initiated JSON-RPC request using a caller-supplied ID
    /** Callers that must register a waiter for the response have to know the ID <i>before</i>
        the request goes out, otherwise the client's answer can arrive before the waiter
        exists — and a waiter registered under a different ID is never signalled at all.

        @param id the JSON-RPC ID to send the request with
        @param method the method to call on the client
        @param info the request parameters
    */
    sendRequestWithId(auto id, string method, hash<auto> info) {
        send(<SseMessageInfo>{
            "data": make_jsonrpc_request(method, "2.0", id, info),
        });
    }

    registered() {
        if (version == McpVersion::V2024_11_05) {
            McpServerHandler mcphandler = cast<McpServerHandler>(handler);
            if ((*string endpoint = mcphandler.getCompatV1Endpoint(cx, hdr)).val()) {
                endpoint += "?session=" + session;
                mcphandler.info("Sending %s endpoint to client: %y", McpVersion::V2024_11_05, endpoint);
                send(<SseMessageInfo>{
                    "event": "endpoint",
                    "data": endpoint,
                });
            } else {
                throw "MCP-VERSION-ERROR", "This MCP server has no defined endpoint for 2024-11-05-compatible "
                    "JSON-RPC commands";
            }
        }
    }

    #! Pushes an unencoded message on the connection's message queue
    /** The message will be encoded with \c ServerSentEventUtil::ws_encode_message() before sending
    */
    send(hash<SseMessageInfo> msg) {
        if (!msg.id) {
            msg.id = cast<McpServerHandler>(handler).getNextIdForSession(session);
        }
        ServerSentEventConnection::send(msg);
    }
}
}
