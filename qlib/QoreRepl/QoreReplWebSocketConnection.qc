# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreReplWebSocketConnection class definition

/** QoreReplWebSocketConnection.qc Copyright 2025 - 2026 Qore Technologies, s.r.o.

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

%modern

%requires Util

public namespace QoreRepl {

#! WebSocket connection for remote REPL access
/** Each connection gets its own QoreRepl instance for session isolation.

    @par Message Protocol
    All messages are JSON-encoded with a \c type field indicating the message type.

    @par Client to Server Messages
    - \c eval: Evaluate code
    - \c command: Execute REPL command
    - \c interrupt: Interrupt execution (not yet implemented)
    - \c ping: Keepalive ping

    @par Server to Client Messages
    - \c result: Evaluation result
    - \c error: Error response
    - \c prompt: Current prompt state
    - \c output: Captured output (stdout/stderr)
    - \c pong: Response to ping
    - \c session: Session info on connect
    - \c exit: Session termination notification

    @see QoreReplWebSocketHandler
*/
public class QoreReplWebSocketConnection inherits WebSocketHandler::WebSocketConnection {
    public {
        #! Protocol version
        const ProtocolVersion = "1.0";
    }

    private {
        #! Action session implementation
        QoreReplActionSession session;
    }

    #! Creates a new REPL WebSocket connection
    /** @param handler The parent WebSocket handler
        @param repl_options Optional configuration for the QoreRepl instance
    */
    constructor(WebSocketHandler::WebSocketHandler handler, *hash<auto> repl_options)
            : WebSocketConnection(handler) {
        session = new QoreReplActionSession(repl_options);
    }

    #! Called when the connection is registered and ready
    /** Sends session info to the client
    */
    registered() {
        hash<auto> info = session.getSessionInfo();
        sendMessage({
            "type": "session",
            "sessionId": info.sessionId,
            "version": info.version,
            "qoreVersion": info.qoreVersion,
        });
        sendEvents(session.getPromptEvents(NOTHING));
    }

    #! Called when a message is received from the client
    /** @param msg The JSON message string
    */
    gotMessage(string msg) {
        hash<auto> request;
        try {
            request = parse_json(msg);
        } catch (hash<ExceptionInfo> ex) {
            sendError(NOTHING, "JSON-PARSE-ERROR", sprintf("Invalid JSON: %s", ex.desc));
            return;
        }

        string type = request.type ?? "";
        *string id = request.id;

        switch (type) {
            case "eval":
                handleEval(id, request.code);
                break;

            case "command":
                handleCommand(id, request.command);
                break;

            case "complete":
                handleComplete(id, request.input);
                break;

            case "interrupt":
                handleInterrupt(id);
                break;

            case "ping":
                sendMessage({"type": "pong", "id": id});
                break;

            default:
                sendError(id, "UNKNOWN-MESSAGE-TYPE", sprintf("Unknown message type: %y", type));
        }
    }

    #! Called when the connection is closed
    connectionClosed() {
        # Cleanup - the repl instance will be garbage collected
    }

    #! Returns the session ID
    string getSessionId() {
        return session.getSessionId();
    }

    #! Returns the REPL instance
    QoreRepl getRepl() {
        return session.getRepl();
    }

    #! Handles an eval request
    /** @param id Optional request ID for correlation
        @param code The code to evaluate
    */
    private handleEval(*string id, *string code) {
        if (!exists code) {
            sendError(id, "MISSING-PARAMETER", "Missing 'code' parameter");
            return;
        }
        sendEvents(session.handleAction("repl-eval", {
            "code": code,
            "id": id,
        }));
    }

    #! Handles a command request
    /** @param id Optional request ID for correlation
        @param command The REPL command to execute
    */
    private handleCommand(*string id, *string command) {
        if (!exists command) {
            sendError(id, "MISSING-PARAMETER", "Missing 'command' parameter");
            return;
        }
        sendEvents(session.handleAction("repl-command", {
            "command": command,
            "id": id,
        }));
    }

    #! Handles a completion request
    /** @param id Optional request ID for correlation
        @param input The current input string to complete
    */
    private handleComplete(*string id, *string input) {
        if (!exists input) {
            sendError(id, "MISSING-PARAMETER", "Missing 'input' parameter");
            return;
        }
        list<hash<CompletionItem>> items = session.getRepl().getCompletionDetails(input);
        sendMessage({
            "type": "completion",
            "id": id,
            "items": items,
        });
    }

    #! Handles an interrupt request
    /** @param id Optional request ID for correlation

        @note Currently not implemented - Qore doesn't support thread interruption
    */
    private handleInterrupt(*string id) {
        sendEvents(session.handleAction("repl-interrupt", {"id": id}));
    }

    #! Sends the current prompt state
    private sendPrompt() {
        sendEvents(session.getPromptEvents(NOTHING));
    }

    #! Sends an error response
    /** @param id Optional request ID for correlation
        @param error Error code
        @param description Error description
        @param ex Optional exception info for location
    */
    private sendError(*string id, string error, string description, *hash<ExceptionInfo> ex) {
        # Use += to initialize msg so that the variable type stays hash<auto> rather than
        # being inferred as a more specific hash type that wouldn't allow adding .location
        hash<auto> msg += {
            "type": "error",
            "id": id,
            "error": error,
            "description": description,
        };

        if (ex && ex.file) {
            msg.location = {
                "file": ex.file,
                "line": ex.line,
            };
        }

        sendMessage(msg);
    }

    #! Sends a JSON message to the client
    /** @param data The data to send as JSON
    */
    private sendMessage(hash<auto> data) {
        send(make_json(data));
    }

    #! Sends output to the client
    /** @param stream The output stream (stdout or stderr)
        @param text The output text
    */
    sendOutput(string stream, string text) {
        sendMessage({
            "type": "output",
            "stream": stream,
            "text": text,
        });
    }

    private sendEvents(list<hash<auto>> events) {
        foreach hash<auto> evt in (events) {
            string ev = evt.event ?? "";
            hash<auto> data = evt.data ?? {};
            switch (ev) {
                case "result":
                case "repl-result":
                    sendMessage({
                        "type": "result",
                        "id": data.request_id,
                        "value": data.value,
                        "valueType": data.value_type,
                        "duration": data.duration_ms,
                    });
                    break;

                case "error":
                case "repl-error":
                    sendError(data.request_id, data.code ?? "REPL-ERROR", data.message ?? "REPL error",
                        data.details);
                    break;

                case "prompt":
                case "repl-prompt":
                    sendMessage({
                        "type": "prompt",
                        "value": data.value,
                        "needsMoreInput": data.needs_more_input,
                        "indentLevel": data.indent_level ?? 0,
                    });
                    break;

                case "completion":
                case "repl-completion":
                    sendMessage({
                        "type": "completion",
                        "id": data.request_id,
                        "items": data.items ?? (),
                    });
                    break;

                case "exit":
                case "repl-exit":
                    sendMessage({
                        "type": "exit",
                        "reason": data.reason ?? "user_request",
                    });
                    stop();
                    break;
            }
        }
    }
}

} # namespace QoreRepl
