# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreReplWebSocketConnection class definition

/** QoreReplWebSocketConnection.qc Copyright 2025 Qore Technologies, s.r.o.

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
        #! The REPL instance for this connection
        QoreRepl repl;

        #! Unique session ID
        string session_id;

        #! Buffer for captured output
        list<hash<auto>> output_buffer = ();
    }

    #! Creates a new REPL WebSocket connection
    /** @param handler The parent WebSocket handler
        @param repl_options Optional configuration for the QoreRepl instance
    */
    constructor(WebSocketHandler::WebSocketHandler handler, *hash<auto> repl_options)
            : WebSocketConnection(handler) {
        repl = new QoreRepl(repl_options);
        session_id = get_random_string(16);
    }

    #! Called when the connection is registered and ready
    /** Sends session info to the client
    */
    registered() {
        sendMessage({
            "type": "session",
            "sessionId": session_id,
            "version": ProtocolVersion,
            "qoreVersion": Qore::VersionString,
        });
        sendPrompt();
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
        return session_id;
    }

    #! Returns the REPL instance
    QoreRepl getRepl() {
        return repl;
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

        date start = now_us();
        *hash<auto> result = repl.eval(code);
        int duration_ms = (now_us() - start).durationMicroseconds() / 1000;

        if (!exists result) {
            # Need more input
            sendPrompt();
            return;
        }

        if (result.exit) {
            sendMessage({
                "type": "exit",
                "reason": "user_request",
            });
            stop();
            return;
        }

        if (result.error) {
            sendError(id, result.error.err, result.error.desc, result.error);
        } else {
            sendMessage({
                "type": "result",
                "id": id,
                "value": QoreRepl::formatValue(result.value),
                "valueType": result.value.fullType(),
                "duration": duration_ms,
            });
        }

        sendPrompt();
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

        # Commands should start with /
        if (command !~ /^\//) {
            command = "/" + command;
        }

        date start = now_us();
        *hash<auto> result = repl.eval(command);
        int duration_ms = (now_us() - start).durationMicroseconds() / 1000;

        if (result.exit) {
            sendMessage({
                "type": "exit",
                "reason": "user_request",
            });
            stop();
            return;
        }

        if (result.error) {
            sendError(id, result.error.err, result.error.desc, result.error);
        } else {
            sendMessage({
                "type": "result",
                "id": id,
                "value": QoreRepl::formatValue(result.value),
                "duration": duration_ms,
            });
        }

        sendPrompt();
    }

    #! Handles an interrupt request
    /** @param id Optional request ID for correlation

        @note Currently not implemented - Qore doesn't support thread interruption
    */
    private handleInterrupt(*string id) {
        sendError(id, "NOT-IMPLEMENTED", "Interrupt is not currently supported");
    }

    #! Sends the current prompt state
    private sendPrompt() {
        sendMessage({
            "type": "prompt",
            "value": repl.getPrompt(),
            "needsMoreInput": repl.needsMoreInput(),
        });
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
}

} # namespace QoreRepl
