# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file A2aServerConnection.qc @brief A2A Server SSE connection class

/*  A2aServerConnection.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! the %A2aServerHandler namespace holds all public definitions in the %A2aServerHandler module
public namespace A2aServerHandler {
#! A2A server SSE connection for streaming task updates
/** Provides access to the A2A session and helper methods to send task events.
*/
public class A2aServerConnection inherits ServerSentEventHandler::ServerSentEventConnection {
    private {
        #! The session identifier
        string session;

        #! A2A protocol version for this connection
        string a2a_version = A2aVersionV03;
    }

    constructor(A2aServerHandler handler, Socket sock, hash<auto> cx, hash<auto> hdr, string cid)
            : ServerSentEventConnection(handler, sock, cx, hdr, cid) {
        self.session = handler.getSessionId(hdr);
        # Detect version from A2A-Version header on SSE connection
        *string ver = hdr{A2aVersionHeader} ?? hdr{A2aVersionHeader.lwr()};
        if (ver) {
            self.a2a_version = ver;
        }
    }

    string getSession() {
        return session;
    }

    #! Returns the A2A protocol version for this connection
    string getA2aVersion() {
        return a2a_version;
    }

    #! Send a task status update event
    sendTaskStatusUpdate(string task_id, hash<auto> status) {
        sendEvent(task_id, "TaskStatusUpdateEvent", status);
    }

    #! Send a task artifact update event
    sendTaskArtifactUpdate(string task_id, hash<auto> artifact) {
        sendEvent(task_id, "TaskArtifactUpdateEvent", artifact);
    }

    #! Send a task message update event for streaming tokens
    /** @param task_id the task ID
        @param partial the partial message data (e.g., partial text parts)
    */
    sendTaskMessageUpdate(string task_id, hash<auto> partial) {
        sendEvent(task_id, "TaskMessageUpdateEvent", partial);
    }

    #! Send an SSE event formatted for the connection's protocol version
    sendEvent(string task_id, string event_type, hash<auto> event_data) {
        hash<auto> payload;
        if (a2a_version == A2aVersionV10) {
            # v1.0: plain discriminated object
            payload = a2aTranslateSseEventToV10(event_type, event_data + {"id": task_id});
            send(<SseMessageInfo>{
                "data": make_json(payload),
            });
        } else {
            # v0.3: JSON-RPC wrapped
            payload = {
                "jsonrpc": "2.0",
                "method": event_type,
                "params": event_data + {"id": task_id},
            };
            send(<SseMessageInfo>{
                "event": event_type,
                "data": make_json(payload),
            });
        }
    }
}
}
