# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DPQL action session class definition

/** DpqlActionSession.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Action-based DPQL language session for Creator WebSocket integration
/** This class provides an action-first interface for DPQL parsing, validation,
    completions, and context management. It is transport-agnostic and intended
    for use in Creator WS handlers and existing DPQL WebSocket modules.
*/
public class DpqlActionSession {
    public {
        #! Default token format
        const DefaultTokenFormat = "token-span";

        #! Default field format
        const DefaultFieldFormat = "map";

        #! Supported token formats
        const TokenFormats = {
            "token-span": True,
            "dpql": True,
        };

        #! Supported field formats
        const FieldFormats = {
            "map": True,
            "list": True,
        };
    }

    private {
        #! Current provider
        *AbstractDataProvider provider;

        #! Current record type fields
        *hash<string, AbstractDataField> fields;

        #! Current expression map (merged: base + provider-specific)
        *hash<string, hash<DataProviderExpressionInfo>> expressions;

        #! Base expressions always available regardless of provider context
        /** Set via the constructor \c base_expressions option or @ref setBaseExpressions().
            These are merged with provider-specific expressions when context is set;
            provider expressions take priority over base expressions for the same key.

            When no provider context is set, these expressions are still available for
            completions and parsing.
        */
        *hash<string, hash<DataProviderExpressionInfo>> base_expressions;

        #! Provider-specific expressions (from getExpressionsWithOptions()), before merging with base
        /** These are the server-supported operators/functions. In search contexts, only these
            expressions can be used with field references.

            @since DataProvider 3.4
        */
        *hash<string, hash<DataProviderExpressionInfo>> provider_expressions;

        #! Whether this session is in a search expression context (where_cond for find/update/delete)
        /** When True, search expression validation rules are enforced:
            - Field references can only be used with server-supported operators
            - At least one condition must reference a record field
            - Local-only operators cannot contain field references

            @since DataProvider 3.4
        */
        bool search_context = False;

        #! Current record type label
        string record_type = "record";

        #! Current provider spec
        *string provider_spec;

        #! Token format
        string token_format = DefaultTokenFormat;

        #! Field format
        string field_format = DefaultFieldFormat;

        #! Optional logger
        *LoggerInterface logger;
    }

    #! Creates a new session
    /** @param opts optional options:
        - \c token_format: "token-span" or "dpql"
        - \c field_format: "map" or "list"
        - \c logger: optional LoggerInterface
        - \c base_expressions: optional expression map always available regardless of provider context;
          when a provider context is set, provider expressions are merged on top of these (provider
          takes priority for duplicate keys)

        @since DataProvider 3.4
    */
    constructor(*hash<auto> opts) {
        if (opts) {
            if (opts.token_format) {
                if (!TokenFormats{opts.token_format}) {
                    throw "INVALID-ARGUMENT", sprintf("Unsupported token_format %y", opts.token_format);
                }
                token_format = opts.token_format;
            }
            if (opts.field_format) {
                if (!FieldFormats{opts.field_format}) {
                    throw "INVALID-ARGUMENT", sprintf("Unsupported field_format %y", opts.field_format);
                }
                field_format = opts.field_format;
            }
            if (opts.logger) {
                logger = opts.logger;
            }
            if (opts.base_expressions) {
                base_expressions = cast<hash<string, hash<DataProviderExpressionInfo>>>(
                    opts.base_expressions);
                # Initialize expressions with base so they're available before setContext
                expressions = base_expressions;
            }
        }
    }

    #! Sets the logger
    setLogger(*LoggerInterface l) {
        logger = l;
        if (provider) {
            provider.setLogger(l);
        }
    }

    #! Handles a DPQL action
    hash<auto> handleAction(string action, *hash<auto> args) {
        switch (action) {
            case "dpql-set-context":
                return handleSetContext(args);

            case "dpql-parse":
                return handleParse(args);

            case "dpql-get-completions":
                return handleCompletions(args);

            case "dpql-get-tokens":
                return handleTokenize(args);

            case "dpql-format":
                return handleFormat(args);

            case "dpql-validate":
                return handleValidate(args);

            case "dpql-eval":
                return handleEval(args);

            case "dpql-serialize":
                return handleSerialize(args);

            case "dpql-get-signature-help":
                return handleSignatureHelp(args);
        }

        throw "INVALID-ACTION", sprintf("Unsupported action %y", action);
    }

    #! Returns the provider spec
    *string getProviderSpec() {
        return provider_spec;
    }

    #! Returns current fields
    *hash<string, AbstractDataField> getFields() {
        return fields;
    }

    #! Returns current expressions
    *hash<string, hash<DataProviderExpressionInfo>> getExpressions() {
        return expressions;
    }

    #! Returns base expressions
    /** @since DataProvider 3.4
    */
    *hash<string, hash<DataProviderExpressionInfo>> getBaseExpressions() {
        return base_expressions;
    }

    #! Sets base expressions that are always available regardless of provider context
    /** @param exprs the base expression map; these are merged with provider-specific expressions
        when context is set (provider takes priority for duplicate keys)

        @since DataProvider 3.4
    */
    setBaseExpressions(*hash<string, hash<DataProviderExpressionInfo>> exprs) {
        base_expressions = exprs;
        # If no provider context is set, update expressions immediately
        if (!provider) {
            expressions = base_expressions;
        } else {
            # Re-merge with current provider expressions
            expressions = base_expressions + expressions;
        }
    }

    #! Returns current provider
    *AbstractDataProvider getProvider() {
        return provider;
    }

    #! Returns provider-specific expressions (server-supported operators)
    /** These are the expressions returned by the provider's \c getExpressionsWithOptions()
        method, before merging with base expressions.

        @return the provider expression map, or @ref nothing if no provider context is set

        @since DataProvider 3.4
    */
    *hash<string, hash<DataProviderExpressionInfo>> getProviderExpressions() {
        return provider_expressions;
    }

    #! Returns whether this session is in a search expression context
    /** @since DataProvider 3.4
    */
    bool isSearchContext() {
        return search_context;
    }

    #! Sets whether this session is in a search expression context
    /** When set to True, search expression validation rules are enforced during
        parse and validate operations.

        @param val True to enable search context validation

        @since DataProvider 3.4
    */
    setSearchContext(bool val) {
        search_context = val;
    }

    private hash<auto> handleSetContext(*hash<auto> args) {
        if (!args || !args.provider) {
            throw "MISSING-PARAMETER", "Missing 'provider' parameter";
        }
        if (args.options && args.options.typeCode() != NT_HASH) {
            throw "INVALID-PARAMETER", "Parameter 'options' must be a hash";
        }
        if (args.recordOptions && args.recordOptions.typeCode() != NT_HASH) {
            throw "INVALID-PARAMETER", "Parameter 'recordOptions' must be a hash";
        }
        string rtype = args.recordType ?? "record";
        if (rtype != "record" && rtype != "create" && rtype != "update") {
            throw "INVALID-PARAMETER", "Parameter 'recordType' must be record, create, or update";
        }

        AbstractDataProvider next_provider = getProviderFromSpec(args.provider, args.subtype);
        if (logger) {
            next_provider.setLogger(logger);
        }

        *hash<string, AbstractDataField> next_fields = getRecordType(next_provider, rtype, args.options,
            args.recordOptions);
        *hash<string, hash<DataProviderExpressionInfo>> next_expressions
            = next_provider.getExpressionsWithOptions(args.options);

        delete provider;
        provider = next_provider;
        fields = next_fields;
        # Store provider expressions separately for search context validation
        provider_expressions = next_expressions;
        # Merge base expressions with provider expressions; provider takes priority
        expressions = base_expressions + next_expressions;
        record_type = rtype;
        provider_spec = args.provider;

        return {
            "provider": provider_spec,
            "recordType": record_type,
            "recordRequiresSearchOptions": provider.recordRequiresSearchOptions(),
            "fields": formatFields(fields),
            "expressions": getExpressionInfo(expressions),
        };
    }

    private hash<auto> handleTokenize(*hash<auto> args) {
        string text = getRequiredStringArg(args, "text");
        list<hash<DpqlTokenInfo>> tokens = DataProvider::getDpqlTokens(text);
        return {
            "tokens": formatTokens(tokens),
        };
    }

    private hash<auto> handleParse(*hash<auto> args) {
        string text = getRequiredStringArg(args, "text");
        *hash<string, AbstractDataField> use_fields = getFieldsOverride(args) ?? fields;
        hash<DpqlParseResult> result = DataProvider::parseDpqlExpressionWithInfo(
            text, expressions, use_fields);
        if (use_fields && result.success) {
            list<hash<DpqlDiagnostic>> diags = DataProvider::validateDpqlExpression(
                text, use_fields, expressions,
                search_context ? provider_expressions : NOTHING);
            if (diags) {
                if (result.diagnostics) {
                    result.diagnostics += diags;
                } else {
                    result.diagnostics = diags;
                }
                result.success = False;
            }
        }
        hash<auto> rv = {
            "success": result.success,
            "diagnostics": result.diagnostics,
            "tokens": formatTokens(result.tokens),
            "expression": result.expression,
        };
        return rv;
    }

    private hash<auto> handleValidate(*hash<auto> args) {
        string text = getRequiredStringArg(args, "text");
        *hash<string, AbstractDataField> use_fields = getFieldsOverride(args) ?? fields;
        if (!use_fields) {
            throw "NO-CONTEXT", "Context must be set before validation";
        }
        list<hash<DpqlDiagnostic>> diags = DataProvider::validateDpqlExpression(text, use_fields, expressions,
            search_context ? provider_expressions : NOTHING);
        return {
            "diagnostics": diags,
        };
    }

    #! Verifies a DPQL expression end to end and dry-runs it against a sample record
    /** Combines syntactic parsing, provider-aware validation, server-side push-down
        classification, and an in-memory evaluation against a sample record so a caller
        (e.g. a design tool) can confirm a generated expression is actually executable
        for the target provider without running a real backend query.

        @param args supports:
        - \c text: (required) the DPQL expression text
        - \c fields: (optional) record-type field override; otherwise the context fields
          set with \c dpql-set-context are used
        - \c record: (optional) a sample record (hash) to evaluate the expression against;
          if not given and a provider context is set, the provider's example record is used
        - \c template_context: (optional) hash of template values used during evaluation

        @return a hash with:
        - \c success: @ref True "True" when the expression parses, has no error-severity
          diagnostics, and either evaluates locally or is fully pushed down to the provider
        - \c diagnostics: combined parse and provider-aware validation diagnostics
        - \c expression: the parsed expression (if parsing succeeded)
        - \c pushdown: when a search-context provider is set, \c {"fully_pushdownable": bool}
          indicating whether the whole expression can be executed server-side
        - \c evaluation: the dry-run result, \c {"executed": bool, "record": ..., "result": ...}
          on success or \c {"executed": False, "record": ..., "error": {...}} on failure
    */
    private hash<auto> handleEval(*hash<auto> args) {
        string text = getRequiredStringArg(args, "text");
        *hash<string, AbstractDataField> use_fields = getFieldsOverride(args) ?? fields;
        *hash<auto> template_context;
        if (exists args.template_context) {
            if (args.template_context.typeCode() != NT_HASH) {
                throw "INVALID-PARAMETER", sprintf("Parameter 'template_context' must be a hash, got %s",
                    args.template_context.type());
            }
            template_context = args.template_context;
        }

        # 1. parse, then provider-aware validation when a record type is known
        hash<DpqlParseResult> result = DataProvider::parseDpqlExpressionWithInfo(text, expressions, use_fields);
        list<hash<DpqlDiagnostic>> diags = result.diagnostics ?? ();
        if (use_fields && result.success) {
            list<hash<DpqlDiagnostic>> vdiags = DataProvider::validateDpqlExpression(text, use_fields, expressions,
                search_context ? provider_expressions : NOTHING);
            if (vdiags) {
                diags += vdiags;
            }
        }
        bool has_errors = (select diags, $1.severity == "error") ? True : False;

        hash<auto> rv = {
            "success": result.success && !has_errors,
            "diagnostics": diags,
            "expression": result.expression,
        };
        if (!result.expression) {
            return rv;
        }
        # strip the optional type so eval/push-down APIs accept the value
        hash<DataProviderExpression> exp = result.expression;

        # 2. server-side push-down classification (only meaningful in a search context):
        # the expression can be pushed down only if every operator it uses is in the
        # provider's server-supported expression set (the same basis the search-context
        # validator uses); operators outside that set would be evaluated client-side
        bool fully_pushdownable = False;
        if (search_context && exists provider_expressions) {
            hash<string, hash<DataProviderExpressionInfo>> server_exprs = provider_expressions;
            fully_pushdownable = allOperatorsServerSupported(exp, server_exprs);
            rv.pushdown = {
                "fully_pushdownable": fully_pushdownable,
            };
        }

        # 3. in-memory dry-run evaluation against a sample record
        *hash<auto> rec = args.record;
        if (!exists rec && provider && use_fields) {
            try {
                rec = provider.getExampleRecordData();
            } catch (hash<ExceptionInfo> ex) {
                # no example record available for this provider; skip the dry run
            }
        }
        if (exists rec) {
            try {
                auto eval_result = AbstractDataProvider::evalGenericExpression(rec, exp, template_context);
                rv.evaluation = {
                    "executed": True,
                    "record": rec,
                    "result": eval_result,
                };
            } catch (hash<ExceptionInfo> ex) {
                rv.evaluation = {
                    "executed": False,
                    "record": rec,
                    "error": {"err": ex.err, "desc": ex.desc},
                };
                # A local evaluation failure is only a real problem when the expression
                # would be evaluated client-side at runtime.  If it is fully pushed down
                # to the provider, the server executes it and the absence of a local
                # implementation is expected, not an error.
                if (!fully_pushdownable) {
                    diags += <DpqlDiagnostic>{
                        "severity": "error",
                        "code": "DPQL-E100",
                        "message": sprintf("expression failed local evaluation and is not "
                            "pushed down to the provider: %s: %s", ex.err, ex.desc),
                        "line": 1,
                        "column": 1,
                        "end_line": 1,
                        "end_column": 1,
                    };
                    rv.diagnostics = diags;
                    rv.success = False;
                }
            }
        }

        return rv;
    }

    #! Returns @ref True "True" if every operator in the expression tree is in the
    #! provider's server-supported expression set
    private bool allOperatorsServerSupported(hash<DataProviderExpression> exp,
            hash<string, hash<DataProviderExpressionInfo>> server_expressions) {
        if (!server_expressions.hasKey(exp.exp)) {
            return False;
        }
        foreach auto arg in (exp.args) {
            if (arg instanceof hash<DataProviderExpression>
                    && !allOperatorsServerSupported(cast<hash<DataProviderExpression>>(arg), server_expressions)) {
                return False;
            }
        }
        return True;
    }

    private hash<auto> handleCompletions(*hash<auto> args) {
        string text = getRequiredStringArg(args, "text");
        int position = getRequiredIntArg(args, "position");
        *hash<string, AbstractDataField> use_fields = getFieldsOverride(args) ?? fields;
        if (!use_fields) {
            throw "NO-CONTEXT", "Context must be set before completion";
        }
        if (exists args.template_context && args.template_context.typeCode() != NT_HASH) {
            throw "INVALID-PARAMETER", sprintf("Parameter 'template_context' must be a hash, got %s",
                args.template_context.type());
        }
        list<hash<DpqlCompletionItem>> items = DataProvider::getDpqlCompletionsWithSearchContext(
            text, position, use_fields, expressions, args.template_context,
            search_context ? provider_expressions : NOTHING);
        return {
            "completions": items,
        };
    }

    private hash<auto> handleFormat(*hash<auto> args) {
        string text = getRequiredStringArg(args, "text");
        hash<DpqlParseResult> result = DataProvider::parseDpqlExpressionWithInfo(text, expressions);
        string formatted;
        if (result.success && result.expression) {
            formatted = DataProvider::serializeDpqlExpression(result.expression, expressions);
        } else {
            formatted = text;
        }
        return {
            "formatted": formatted,
            "success": result.success,
        };
    }

    private hash<auto> handleSerialize(*hash<auto> args) {
        if (!args || !args.expression) {
            throw "MISSING-PARAMETER", "Missing 'expression' parameter";
        }
        hash<DataProviderExpression> dp_exp;
        try {
            dp_exp = DataProvider::deserializeExpressionFromJson(args.expression);
        } catch (hash<ExceptionInfo> ex) {
            throw "INVALID-PARAMETER", sprintf("Invalid 'expression' value: %s", ex.desc);
        }
        string dpql = DataProvider::serializeDpqlExpression(dp_exp, expressions);
        return {
            "dpql": dpql,
        };
    }

    private hash<auto> handleSignatureHelp(*hash<auto> args) {
        string text = getRequiredStringArg(args, "text");
        int position = getRequiredIntArg(args, "position");
        *hash<DpqlSignatureHelp> help = DataProvider::getDpqlSignatureHelp(text, position,
            expressions);
        if (!help) {
            return {
                "signature": NOTHING,
            };
        }
        return {
            "signature": help,
        };
    }

    private string getRequiredStringArg(*hash<auto> args, string name) {
        if (!args || !exists args{name}) {
            throw "MISSING-PARAMETER", sprintf("Missing '%s' parameter", name);
        }
        if (args{name}.typeCode() != NT_STRING) {
            throw "INVALID-PARAMETER", sprintf("Parameter '%s' must be a string", name);
        }
        return args{name};
    }

    private int getRequiredIntArg(*hash<auto> args, string name) {
        if (!args || !exists args{name}) {
            throw "MISSING-PARAMETER", sprintf("Missing '%s' parameter", name);
        }
        if (!args{name}.intp()) {
            throw "INVALID-PARAMETER", sprintf("Parameter '%s' must be an integer", name);
        }
        return args{name};
    }

    private list<hash<auto>> formatTokens(list<hash<DpqlTokenInfo>> tokens) {
        if (token_format == "dpql") {
            return tokens;
        }
        return map toTokenSpan($1), tokens, shouldEmitToken($1);
    }

    private bool shouldEmitToken(hash<DpqlTokenInfo> tok) {
        if (tok.type == DPQL_TOK_WHITESPACE || tok.type == DPQL_TOK_NEWLINE || tok.type == DPQL_TOK_EOF) {
            return False;
        }
        return True;
    }

    private hash<auto> toTokenSpan(hash<DpqlTokenInfo> tok) {
        return {
            "type": mapTokenType(tok.type),
            "start": {
                "row": tok.line - 1,
                "column": tok.column - 1,
            },
            "end": {
                "row": tok.end_line - 1,
                "column": tok.end_column - 1,
            },
            "text": tok.value,
        };
    }

    private string mapTokenType(int tok_type) {
        switch (tok_type) {
            case DPQL_TOK_FIELD: return "field";
            case DPQL_TOK_STRING: return "string";
            case DPQL_TOK_NUMBER: return "number";
            case DPQL_TOK_BOOLEAN: return "keyword";
            case DPQL_TOK_NULL: return "keyword";
            case DPQL_TOK_IDENTIFIER: return "identifier";
            case DPQL_TOK_DATE: return "constant";
            case DPQL_TOK_BINARY: return "constant";
            case DPQL_TOK_LIST: return "constant";
            case DPQL_TOK_HASH: return "constant";
            case DPQL_TOK_OP_EQ: return "operator";
            case DPQL_TOK_OP_NE: return "operator";
            case DPQL_TOK_OP_LT: return "operator";
            case DPQL_TOK_OP_LE: return "operator";
            case DPQL_TOK_OP_GT: return "operator";
            case DPQL_TOK_OP_GE: return "operator";
            case DPQL_TOK_OP_AND: return "operator";
            case DPQL_TOK_OP_OR: return "operator";
            case DPQL_TOK_OP_NOT: return "operator";
            case DPQL_TOK_OP_REGEX: return "operator";
            case DPQL_TOK_REGEX: return "string";
            case DPQL_TOK_LPAREN: return "punctuation";
            case DPQL_TOK_RPAREN: return "punctuation";
            case DPQL_TOK_COMMA: return "punctuation";
            case DPQL_TOK_TEMPLATE: return "variable";
            case DPQL_TOK_ERROR: return "error";
        }
        return "identifier";
    }

    private auto formatFields(*hash<string, AbstractDataField> field_map) {
        if (field_format == "list") {
            return getFieldInfoList(field_map);
        }
        return getFieldInfoMap(field_map);
    }

    private *hash<string, AbstractDataField> getFieldsOverride(*hash<auto> args) {
        if (!args || !args.fields) {
            return NOTHING;
        }
        if (args.fields.typeCode() != NT_HASH) {
            throw "INVALID-PARAMETER", "Parameter 'fields' must be a hash";
        }
        if (args.fields.values()) {
            auto first = args.fields.values()[0];
            if (first instanceof AbstractDataField) {
                return cast<hash<string, AbstractDataField>>(args.fields);
            }
        }
        return convertToDataFields(args.fields);
    }

    private hash<string, AbstractDataField> convertToDataFields(hash<auto> fields_info) {
        hash<string, AbstractDataField> result;
        foreach string name in (keys fields_info) {
            hash<auto> info = fields_info{name};
            AbstractDataProviderType type = AbstractDataProviderType::get(info.type ?? info."type.name" ?? "any");
            result{name} = new QoreDataField(name, info.desc ?? info.short_desc, type);
        }
        return result;
    }

    private list<hash<auto>> getFieldInfoList(*hash<string, AbstractDataField> field_map) {
        if (!field_map) {
            return ();
        }

        list<hash<auto>> info = ();
        foreach hash<auto> i in (field_map.pairIterator()) {
            info += i.value.getInfo();
        }
        return info;
    }

    private hash<string, hash<auto>> getFieldInfoMap(*hash<string, AbstractDataField> field_map) {
        if (!field_map) {
            return {};
        }
        hash<string, hash<auto>> info = {};
        foreach hash<auto> i in (field_map.pairIterator()) {
            info{i.key} = i.value.getInfo();
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
            try {
                conn = call_function("get_connection", connection);
            } catch (hash<ExceptionInfo> ex) {
                rethrow "CONTEXT-ERROR", sprintf("unable to resolve connection %y: %s: %s", connection, ex.err,
                    ex.desc);
            }
            hash<auto> scheme_info;
            try {
                scheme_info = call_function("ConnectionSchemeCache::getSchemeEx", app.scheme);
            } catch (hash<ExceptionInfo> ex) {
                rethrow "CONTEXT-ERROR", sprintf("ConnectionProvider module is required for scheme %y: %s: %s",
                    app.scheme, ex.err, ex.desc);
            }
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

        *AbstractDataProvider dp = DataProvider::tryLoadProviderForConnectionFromEnv(name, subtype, logger);
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
}

} # namespace DataProvider
