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

        #! Current provider
        *AbstractDataProvider provider;

        #! Current record type fields
        *hash<string, AbstractDataField> fields;

        #! Current expression map
        *hash<string, hash<DataProviderExpressionInfo>> expressions;

        #! Current record type label
        string record_type = "record";

        #! Current provider spec
        *string provider_spec;
    }

    #! Creates a new DPQL WebSocket connection
    /** @param handler The parent WebSocket handler
    */
    constructor(WebSocketHandler::WebSocketHandler handler) : WebSocketConnection(handler) {
        session_id = get_random_string(16);
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
        delete provider;
    }

    #! Handles a context request
    private handleContext(*string id, *string spec, *string subtype, *hash<auto> options,
            *hash<auto> record_options, *string record_type_req) {
        if (!spec) {
            sendError(id, "MISSING-PARAMETER", "Missing 'provider' parameter");
            return;
        }

        if (options && options.typeCode() != NT_HASH) {
            sendError(id, "INVALID-PARAMETER", "Parameter 'options' must be a hash");
            return;
        }

        if (record_options && record_options.typeCode() != NT_HASH) {
            sendError(id, "INVALID-PARAMETER", "Parameter 'recordOptions' must be a hash");
            return;
        }

        string rtype = record_type_req ?? "record";
        if (rtype != "record" && rtype != "create" && rtype != "update") {
            sendError(id, "INVALID-PARAMETER", "Parameter 'recordType' must be record, create, or update");
            return;
        }

        try {
            AbstractDataProvider next_provider = getProviderFromSpec(spec, subtype);
            next_provider.setLogger(handler);

            *hash<string, AbstractDataField> next_fields = getRecordType(next_provider, rtype, options,
                record_options);
            *hash<string, hash<DataProviderExpressionInfo>> next_expressions
                = next_provider.getExpressionsWithOptions(options);

            delete provider;
            provider = next_provider;
            fields = next_fields;
            expressions = next_expressions;
            record_type = rtype;
            provider_spec = spec;

            sendMessage({
                "type": "context",
                "id": id,
                "provider": provider_spec,
                "recordType": record_type,
                "recordRequiresSearchOptions": provider.recordRequiresSearchOptions(),
                "fields": getFieldInfo(fields),
                "expressions": getExpressionInfo(expressions),
            });
        } catch (hash<ExceptionInfo> ex) {
            sendError(id, ex.err ?? "CONTEXT-ERROR", ex.desc ?? "Context error");
        }
    }

    #! Handles a tokenization request
    private handleTokenize(*string id, *string text) {
        if (!text) {
            sendError(id, "MISSING-PARAMETER", "Missing 'text' parameter");
            return;
        }

        list<hash<DpqlTokenInfo>> tokens = DataProvider::getDpqlTokens(text);
        sendMessage({
            "type": "tokens",
            "id": id,
            "tokens": tokens,
        });
    }

    #! Handles a parse request
    private handleParse(*string id, *string text) {
        if (!text) {
            sendError(id, "MISSING-PARAMETER", "Missing 'text' parameter");
            return;
        }

        hash<DpqlParseResult> result = DataProvider::parseDpqlExpressionWithInfo(text, expressions);
        sendMessage({
            "type": "parse",
            "id": id,
            "success": result.success,
            "expression": result.expression,
            "diagnostics": result.diagnostics,
            "tokens": result.tokens,
        });
    }

    #! Handles a validation request
    private handleValidate(*string id, *string text) {
        if (!text) {
            sendError(id, "MISSING-PARAMETER", "Missing 'text' parameter");
            return;
        }

        if (!fields) {
            sendError(id, "NO-CONTEXT", "Context must be set before validation");
            return;
        }

        list<hash<DpqlDiagnostic>> diags = DataProvider::validateDpqlExpression(text, fields, expressions);
        sendMessage({
            "type": "validate",
            "id": id,
            "diagnostics": diags,
        });
    }

    #! Handles a completion request
    private handleComplete(*string id, *string text, *int position) {
        if (!text) {
            sendError(id, "MISSING-PARAMETER", "Missing 'text' parameter");
            return;
        }

        if (!exists position) {
            sendError(id, "MISSING-PARAMETER", "Missing 'position' parameter");
            return;
        }

        if (!position.intp()) {
            sendError(id, "INVALID-PARAMETER", "Parameter 'position' must be an integer");
            return;
        }

        if (!fields) {
            sendError(id, "NO-CONTEXT", "Context must be set before completion");
            return;
        }

        list<hash<DpqlCompletionItem>> items = DataProvider::getDpqlCompletions(text, position, fields,
            expressions);
        sendMessage({
            "type": "completions",
            "id": id,
            "items": items,
        });
    }

    #! Handles a serialize request
    private handleSerialize(*string id, *hash<auto> exp) {
        if (!exp) {
            sendError(id, "MISSING-PARAMETER", "Missing 'expression' parameter");
            return;
        }

        hash<DataProviderExpression> dp_exp;
        try {
            dp_exp = cast<hash<DataProviderExpression>>(exp);
        } catch (hash<ExceptionInfo> ex) {
            sendError(id, "INVALID-PARAMETER", sprintf("Invalid 'expression' value: %s", ex.desc));
            return;
        }

        string dpql;
        try {
            dpql = DataProvider::serializeDpqlExpression(dp_exp, expressions);
        } catch (hash<ExceptionInfo> ex) {
            sendError(id, "SERIALIZE-ERROR", sprintf("Failed to serialize expression: %s", ex.desc));
            return;
        }
        sendMessage({
            "type": "serialize",
            "id": id,
            "dpql": dpql,
        });
    }

    #! Parses a provider spec and returns a data provider
    private AbstractDataProvider getProviderFromSpec(string spec, *string subtype) {
        string name = spec;
        *string dp_opts = (name =~ x/({(?:(?>[^{}]*)|(?0))*})/)[0];
        if (dp_opts) {
            name = replace(name, dp_opts, "");
        } else {
            name =~ s/{}//;
        }

        if (name =~ /^@/) {
            splice name, 0, 1;
            name =~ s/\/+$//;
            list<string> path = name.split("/");
            if (path.size() < 2) {
                throw "CONTEXT-ERROR", sprintf("app action %y is incomplete; expecting @app/action", spec);
            }
            return getProviderForAppAction(path[0], path[1], dp_opts);
        }

        list<string> path = name.split("/");
        AbstractDataProvider prov = getProvider((shift path) + (dp_opts ?? ""), subtype);
        map prov = prov.getChildProviderEx($1), path;
        return prov;
    }

    #! Returns a data provider for an app action spec
    private AbstractDataProvider getProviderForAppAction(string app_name, string action_name, *string opts) {
        if (!ENV.QORE_PROVIDER_INDEX_DIR) {
            DataProvider::registerKnownFactories();
        }
        *hash<string, hash<DataProviderActionInfo>> actions = DataProviderActionCatalog::getActionHash(app_name);
        if (!actions) {
            throw "CONTEXT-ERROR", sprintf("Application %y is unknown", app_name);
        }
        *hash<DataProviderActionInfo> action = actions{action_name};
        if (!action) {
            throw "CONTEXT-ERROR", sprintf("Application %y does not provide action %y", app_name, action_name);
        }
        *hash<auto> options;
        if (opts) {
            options = parse_to_qore_value(opts);
            if (exists options && options.typeCode() != NT_HASH) {
                throw "CONTEXT-ERROR", sprintf("option string for @%s/%s does not parse to type \"hash\"; got "
                    "type %y instead", app_name, action_name, options.type());
            }
        }
        if (!options) {
            options = {};
        }
        hash<DataProviderAppInfo> app = DataProviderActionCatalog::getAppEx(app_name);

        string connection;
        object conn;
        if (app.scheme.val()) {
            connection = options.connection;
            if (!connection) {
                throw "CONTEXT-ERROR", sprintf("app %y action %y: missing required option %y", app.name,
                    action.action, "connection");
            }
            remove options.connection;
            conn = get_connection(connection);
            hash<auto> scheme_info = ConnectionSchemeCache::getSchemeEx(app.scheme);
            if (!scheme_info.schemes{string scheme = conn.getType()}) {
                throw "CONTEXT-ERROR", sprintf("app %y action %y: connection %y has scheme %y; expecting %y",
                    app.name, action.action, connection, scheme, keys scheme_info.schemes);
            }
            if (!conn.hasDataProvider()) {
                throw "CONTEXT-ERROR", sprintf("connection %y has no data provider but should be valid for "
                    "app %y action %y", connection, app.name, action.action);
            }
        }

        foreach hash<auto> i in (action.path_vars.pairIterator()) {
            if (!options{i.key}.val()) {
                throw "CONTEXT-ERROR", sprintf("app %y action %y: required option %y is missing in \"options\"",
                    app.name, action.action, i.key);
            }
        }

        AbstractDataProvider prov;
        try {
            if (conn) {
                prov = conn.getDataProvider(action.subtype);
            } else if (action.cls) {
                prov = DataProviderActionCatalog::getDataProviderForAction(action, \options);
            }

            DataProviderDataContextHelper dch(options);
            foreach string seg in (action.path[1..].split("/")) {
                if (*string var = (seg =~ x/^\{([^\}]+)\}$/)[0]) {
                    seg = remove options{var};
                }
                prov = prov.getChildProviderEx(seg);
            }
        } catch (hash<ExceptionInfo> ex) {
            rethrow "CONTEXT-ERROR", sprintf("error acquiring data provider for app %y action: %y: %s: %s", app.name,
                action.action, ex.err, ex.desc);
        }
        return prov;
    }

    #! Returns a data provider for a connection or factory string
    private AbstractDataProvider getProvider(string name, *string subtype) {
        if (name =~ /{([^}]*)}/) {
            return DataProvider::getFactoryObjectFromStringUseEnv(name);
        }

        *AbstractDataProvider dp = DataProvider::tryLoadProviderForConnectionFromEnv(name, subtype, handler);
        if (dp) {
            return dp;
        }

        throw "CONTEXT-ERROR", sprintf("no connection or datasource connection %y exists in any known data provider",
            name);
    }

    #! Returns record type based on the requested record type
    private *hash<string, AbstractDataField> getRecordType(AbstractDataProvider prov, string rtype,
            *hash<auto> options, *hash<auto> record_options) {
        switch (rtype) {
            case "record":
                if (options || record_options) {
                    return prov.getRecordTypeWithOptions(options, record_options);
                }
                return prov.getRecordType(NOTHING);

            case "create":
                if (options || record_options) {
                    return prov.getCreateRecordTypeWithOptions(options, record_options);
                }
                return prov.getCreateRecordType(NOTHING);

            case "update":
                if (options || record_options) {
                    return prov.getUpdateRecordTypeWithOptions(options, record_options);
                }
                return prov.getUpdateRecordType(NOTHING);
        }

        return prov.getRecordType(NOTHING);
    }

    #! Returns a list of field info hashes
    private list<hash<auto>> getFieldInfo(*hash<string, AbstractDataField> field_map) {
        if (!field_map) {
            return ();
        }

        list<hash<auto>> info = ();
        foreach hash<auto> i in (field_map.pairIterator()) {
            hash<auto> f = i.value.getInfo();
            info += f;
        }
        return info;
    }

    #! Returns expression info with type names instead of objects
    private *hash<auto> getExpressionInfo(*hash<string, hash<DataProviderExpressionInfo>> exp_map) {
        if (!exp_map) {
            return;
        }
        hash<auto> info = {"expressions": exp_map};
        return DataProvider::getInfoAsData(info).expressions;
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
