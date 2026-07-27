# -*- mode: qore; indent-tabs-mode: nil -*-
#! DPQL WebSocket connection class definition

/*  DpqlWebSocketConnection.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the DataProvider namespace
public namespace DataProvider {

#! WebSocket connection for DPQL language server access
/** Each connection manages its own data-provider context (record type and
    expression map) for context-aware DPQL services.

    @par Message Protocol
    All messages are JSON-encoded with a \c type field indicating the message type.

    @par Client to Server Messages
    - \c context: Set data-provider context
    - \c tokenize: Tokenize a DPQL expression
    - \c parse: Parse a DPQL expression (with diagnostics)
    - \c validate: Validate DPQL expression against fields
    - \c complete: Provide completions at a cursor position
    - \c serialize: Serialize a DPQL expression hash to text
    - \c ping: Keepalive ping

    @par Server to Client Messages
    - \c session: Session info on connect
    - \c context: Context summary response
    - \c tokens: Tokenization response
    - \c parse: Parse response with diagnostics
    - \c validate: Validation response
    - \c completions: Completion response
    - \c serialize: Serialization response
    - \c pong: Response to ping
    - \c error: Error response

    @see DpqlWebSocketHandler
*/
public class DpqlWebSocketConnection inherits WebSocketHandler::WebSocketConnection {
    public {
        #! Protocol version
        const ProtocolVersion = "1.0";
    }

    private {
        #! Unique session ID
        string session_id;

        #! Action session implementation
        DpqlActionSession session;
    }

    #! Creates a new DPQL WebSocket connection
    /** @param handler The parent WebSocket handler
    */
    constructor(WebSocketHandler::WebSocketHandler handler) : WebSocketConnection(handler) {
        session_id = get_random_string(16);
        session = new DpqlActionSession({
            "token_format": "dpql",
            "field_format": "list",
            "logger": handler,
        });
    }

    #! Called when the connection is registered and ready
    registered() {
        sendMessage({
            "type": "session",
            "sessionId": session_id,
            "version": ProtocolVersion,
            "qoreVersion": Qore::VersionString,
        });
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
            case "context":
                handleContext(id, request.provider, request.subtype, request.options, request.recordOptions,
                    request.recordType);
                break;

            case "tokenize":
                handleTokenize(id, request.text);
                break;

            case "parse":
                handleParse(id, request.text);
                break;

            case "validate":
                handleValidate(id, request.text);
                break;

            case "complete":
                handleComplete(id, request.text, request.position);
                break;

            case "serialize":
                handleSerialize(id, request.expression);
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
        # No explicit cleanup required
    }

    #! Handles a context request
    private handleContext(*string id, *string spec, *string subtype, *hash<auto> options,
            *hash<auto> record_options, *string record_type_req) {
        try {
            hash<auto> data = session.handleAction("dpql-set-context", {
                "provider": spec,
                "subtype": subtype,
                "options": options,
                "recordOptions": record_options,
                "recordType": record_type_req,
            });
            sendMessage({
                "type": "context",
                "id": id,
                "provider": data.provider,
                "recordType": data.recordType,
                "recordRequiresSearchOptions": data.recordRequiresSearchOptions,
                "fields": data.fields,
                "expressions": data.expressions,
            });
        } catch (hash<ExceptionInfo> ex) {
            sendError(id, ex.err ?? "CONTEXT-ERROR", ex.desc ?? "Context error");
        }
    }

    #! Handles a tokenization request
    private handleTokenize(*string id, *string text) {
        try {
            hash<auto> data = session.handleAction("dpql-get-tokens", {"text": text});
            sendMessage({
                "type": "tokens",
                "id": id,
                "tokens": data.tokens,
            });
        } catch (hash<ExceptionInfo> ex) {
            sendError(id, ex.err ?? "TOKENIZE-ERROR", ex.desc ?? "Tokenize error");
        }
    }

    #! Handles a parse request
    private handleParse(*string id, *string text) {
        try {
            hash<auto> data = session.handleAction("dpql-parse", {"text": text});
            sendMessage({
                "type": "parse",
                "id": id,
                "success": data.success,
                "expression": data.expression,
                "diagnostics": data.diagnostics,
                "tokens": data.tokens,
            });
        } catch (hash<ExceptionInfo> ex) {
            sendError(id, ex.err ?? "PARSE-ERROR", ex.desc ?? "Parse error");
        }
    }

    #! Handles a validation request
    private handleValidate(*string id, *string text) {
        try {
            hash<auto> data = session.handleAction("dpql-validate", {"text": text});
            sendMessage({
                "type": "validate",
                "id": id,
                "diagnostics": data.diagnostics,
            });
        } catch (hash<ExceptionInfo> ex) {
            sendError(id, ex.err ?? "VALIDATE-ERROR", ex.desc ?? "Validate error");
        }
    }

    #! Handles a completion request
    private handleComplete(*string id, *string text, *int position) {
        try {
            hash<auto> data = session.handleAction("dpql-get-completions", {
                "text": text,
                "position": position,
            });
            sendMessage({
                "type": "completions",
                "id": id,
                "items": data.completions,
            });
        } catch (hash<ExceptionInfo> ex) {
            sendError(id, ex.err ?? "COMPLETE-ERROR", ex.desc ?? "Completion error");
        }
    }

    #! Handles a serialize request
    private handleSerialize(*string id, *hash<auto> exp) {
        try {
            hash<auto> data = session.handleAction("dpql-serialize", {"expression": exp});
            sendMessage({
                "type": "serialize",
                "id": id,
                "dpql": data.dpql,
            });
        } catch (hash<ExceptionInfo> ex) {
            sendError(id, ex.err ?? "SERIALIZE-ERROR", ex.desc ?? "Serialize error");
        }
    }

    #! Sends an error response
    private sendError(*string id, string error, string description, *list<hash<DpqlDiagnostic>> diagnostics) {
        hash<auto> msg += {
            "type": "error",
            "id": id,
            "error": error,
            "description": description,
        };

        if (diagnostics) {
            msg.diagnostics = diagnostics;
        }

        sendMessage(msg);
    }

    #! Sends a JSON message to the client
    private sendMessage(hash<auto> data) {
        send(make_json(data));
    }
}

} # namespace DataProvider
