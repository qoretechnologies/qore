# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file McpProtocol.qc @brief MCP wire-protocol primitives shared by the client and server

/*  McpProtocol.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! the %McpClient namespace holds all public definitions in the %McpClient module
public namespace McpClient {

/** @defgroup mcp_meta_keys MCP Reserved \c _meta Keys
    Key names reserved by the MCP specification for protocol-level metadata.

    Any prefix whose second label is \c "modelcontextprotocol" or \c "mcp" is reserved for
    MCP use; implementations must not make assumptions about the values at these keys.
*/
#/@{
#! \c _meta key carrying the protocol version of a request (required on modern requests)
public const MCP_MetaProtocolVersion = "io.modelcontextprotocol/protocolVersion";

#! \c _meta key carrying the client's name and version
public const MCP_MetaClientInfo = "io.modelcontextprotocol/clientInfo";

#! \c _meta key carrying the client capabilities relevant to a request (required on modern requests)
public const MCP_MetaClientCapabilities = "io.modelcontextprotocol/clientCapabilities";

#! \c _meta key carrying the minimum log level the server should emit for a request
public const MCP_MetaLogLevel = "io.modelcontextprotocol/logLevel";

#! \c _meta key carrying the server's name and version in a result
public const MCP_MetaServerInfo = "io.modelcontextprotocol/serverInfo";

#! \c _meta key correlating a notification with its originating \c subscriptions/listen request
public const MCP_MetaSubscriptionId = "io.modelcontextprotocol/subscriptionId";

#! \c _meta key opting a request into progress notifications
public const MCP_MetaProgressToken = "progressToken";
#/@}

#! Identifier of the official MCP Tasks extension
public const MCP_ExtensionTasks = "io.modelcontextprotocol/tasks";

/** @defgroup mcp_apps MCP Apps Extension
    The MCP Apps extension (SEP-1865, spec revision \c 2026-01-26) lets a server ship an
    interactive HTML user interface alongside a tool.  A tool opts in by referencing a
    \c ui:// resource through @ref McpUiToolMeta "_meta.ui"; the host fetches that resource
    with \c resources/read, renders it in a sandboxed iframe, and bridges \c ui/ -prefixed
    JSON-RPC to it over \c postMessage.

    Rendering is a host (browser) concern, so
    @ref McpClient::McpClient "McpClient" negotiates the extension and surfaces the metadata
    and the fetched resource, but does not render anything.
*/
#/@{
#! Identifier of the official MCP Apps (UI) extension
public const MCP_ExtensionUi = "io.modelcontextprotocol/ui";

#! The MIME type of an MCP Apps HTML user interface resource
public const MCP_UiMimeType = "text/html;profile=mcp-app";

#! The URI scheme every MCP Apps user interface resource must use
public const MCP_UiScheme = "ui://";

#! Who may see and call a tool that declares MCP Apps metadata
public enum McpUiVisibility : string {
    #! The tool is visible to, and callable by, the model
    Model = "model",
    #! The tool is callable by the app, over the same server connection only
    App = "app",
}

#! The default \c visibility of a tool that declares MCP Apps metadata
public const McpUiDefaultVisibility = (McpUiVisibility::Model, McpUiVisibility::App);

#! MCP Apps metadata attached to a tool as \c _meta.ui
public hashdecl McpUiToolMeta {
    #! URI of the \c ui:// resource that renders this tool's results
    *string resourceUri;

    #! Who may see and call the tool; see @ref McpUiVisibility
    /** Defaults to @ref McpUiDefaultVisibility when omitted.  A host must not offer a tool to
        the model unless \c "model" is present, and must reject an app-initiated
        \c tools/call unless \c "app" is present.
    */
    *list<string> visibility;
}

#! Content Security Policy declaration for an MCP Apps user interface resource
/** Each member lists the external origins the interface needs; the host builds the CSP header
    from them.  An omitted or empty member means "no external access" — the secure default.
    The host may restrict further but must never allow an undeclared origin.
*/
public hashdecl McpUiResourceCsp {
    #! Origins for network requests (\c fetch / \c XHR / WebSocket); maps to \c connect-src
    *list<string> connectDomains;

    #! Origins for images, scripts, stylesheets, fonts and media
    /** Maps to the \c img-src, \c script-src, \c style-src, \c font-src and \c media-src
        directives.  Wildcard subdomains are supported, e.g. \c "https://*.example.com".
    */
    *list<string> resourceDomains;

    #! Origins for nested iframes; maps to \c frame-src
    *list<string> frameDomains;

    #! Allowed base URIs for the document; maps to \c base-uri
    *list<string> baseUriDomains;
}

#! MCP Apps metadata attached to a user interface resource as \c _meta.ui
public hashdecl McpUiResourceMeta {
    #! The origins the interface is allowed to reach
    *hash<McpUiResourceCsp> csp;

    #! Browser capabilities the interface requests
    /** Each requested capability is present as a key with an empty hash value; recognized keys
        are \c camera, \c microphone, \c geolocation and \c clipboardWrite.  A host may honor
        these through the iframe \c allow attribute, so an interface must feature-detect rather
        than assume the capability was granted.
    */
    *hash<string, hash<auto>> permissions;

    #! Dedicated sandbox origin for the interface
    /** Useful when the interface needs a stable origin for OAuth callbacks, CORS policies or
        API key allowlists.  The accepted format is host-specific; when omitted the host uses
        its default sandbox origin.
    */
    *string domain;

    #! Whether the host should draw a visible border and background around the interface
    /** Setting this explicitly is recommended, as host defaults vary.
    */
    *bool prefersBorder;
}
#/@}

#! MCP result type discriminator values
/** Every result returned by a @ref McpModernVersions "modern" server carries a \c resultType
    member.  Results from servers implementing earlier protocol revisions omit the member and
    must be treated as @ref McpResultType::Complete "complete".
*/
public enum McpResultType : string {
    #! The request completed and the result contains the final content
    Complete = "complete",
    #! The request is incomplete; more information is needed to process it
    InputRequired = "input_required",
    #! The request was accepted as a task; the result carries a task handle (Tasks extension)
    Task = "task",
}

/** @defgroup mcp_error_codes MCP Protocol Error Codes
    JSON-RPC error codes defined by the MCP specification.

    JSON-RPC 2.0 reserves \c -32000 to \c -32099 for implementation-defined server errors; MCP
    partitions this range so that \c -32000 to \c -32019 remains implementation-defined (with
    existing usage grandfathered) and \c -32020 to \c -32099 is reserved for codes defined by
    the MCP specification itself.
*/
#/@{
#! The HTTP headers do not match the request body, or required headers are missing/malformed
public const MCP_Err_HeaderMismatch = -32020;

#! Processing the request requires a client capability that the client did not declare
public const MCP_Err_MissingRequiredClientCapability = -32021;

#! The server does not implement the protocol version the request declared
public const MCP_Err_UnsupportedProtocolVersion = -32022;

#! Resource not found, as emitted by protocol revisions 2025-11-25 and earlier
/** @note modern servers use the JSON-RPC standard \c -32602 (invalid params) instead, but
    clients must still accept this code from servers implementing earlier revisions
*/
public const MCP_Err_LegacyResourceNotFound = -32002;

#! JSON-RPC 2.0 standard error code: the method does not exist
/** MCP uses the standard JSON-RPC codes for general protocol failures; they are defined here
    so that the client does not have to depend on a server-side JSON-RPC handler module.
*/
public const MCP_Err_MethodNotFound = -32601;

#! JSON-RPC 2.0 standard error code: invalid method parameters
public const MCP_Err_InvalidParams = -32602;

#! JSON-RPC 2.0 standard error code: internal error
public const MCP_Err_InternalError = -32603;
#/@}

/** @defgroup mcp_headers MCP Streamable HTTP Request Headers
    Headers that mirror selected JSON-RPC body fields so that intermediaries can route and
    inspect requests without parsing the body.  Servers must reject requests whose header
    values do not match the corresponding body values.
*/
#/@{
#! Header carrying the request's protocol version
public const McpProtocolVersionHeader = "MCP-Protocol-Version";

#! Header carrying the request's JSON-RPC method name
public const McpMethodHeader = "Mcp-Method";

#! Header carrying \c params.name or \c params.uri
public const McpNameHeader = "Mcp-Name";

#! Prefix for headers mirroring tool parameters annotated with \c x-mcp-header
public const McpParamHeaderPrefix = "Mcp-Param-";

#! Tool input-schema property that designates a parameter for mirroring into an HTTP header
public const McpHeaderAnnotation = "x-mcp-header";

#! Prefix of the Base64 sentinel encoding for header values that are not header-safe
public const McpHeaderEncodingPrefix = "=?base64?";

#! Suffix of the Base64 sentinel encoding for header values that are not header-safe
public const McpHeaderEncodingSuffix = "?=";
#/@}

#! Maximum JSON Schema traversal depth when scanning for \c x-mcp-header annotations
/** Bounds the cost of validating a hostile schema; see the JSON Schema
    composition-keyword resource-use requirements in the MCP specification.
*/
public const McpMaxSchemaDepth = 32;

#! Maximum number of schema nodes traversed when scanning for \c x-mcp-header annotations
public const McpMaxSchemaNodes = 10000;

#! Largest integer that can be represented exactly in a JSON number (2^53 - 1)
public const McpMaxSafeInteger = 9007199254740991;

#! Smallest integer that can be represented exactly in a JSON number (-2^53 + 1)
public const McpMinSafeInteger = -9007199254740991;

#! JSON Schema keywords whose values are subschemas that are not statically reachable
/** A property reached through any of these keywords cannot carry an \c x-mcp-header
    annotation, because the value's location in the instance is not determined by the schema
    alone.
*/
const McpUnreachableSchemaKeywords = (
    "items", "additionalItems", "contains", "not", "if", "then", "else", "additionalProperties",
    "propertyNames", "unevaluatedItems", "unevaluatedProperties",
);

#! JSON Schema keywords whose values are lists of subschemas that are not statically reachable
const McpUnreachableSchemaListKeywords = ("oneOf", "anyOf", "allOf", "prefixItems");

#! JSON Schema keywords whose values are maps of subschemas that are not statically reachable
const McpUnreachableSchemaMapKeywords = ("$defs", "definitions", "patternProperties",
    "dependentSchemas");

#! JSON Schema \c type values that may carry an \c x-mcp-header annotation
/** \c number is deliberately excluded: it has no unambiguous string representation.
*/
const McpHeaderParamTypes = {
    "string": True,
    "integer": True,
    "boolean": True,
};

#! Returns @ref True if the given protocol version uses per-request metadata
/** "Modern" protocol revisions (@ref McpVersion::V2026_07_28 "2026-07-28" and later) are
    stateless: every request carries its protocol version, client identity, and capabilities
    in its \c _meta field.  "Legacy" revisions
    (@ref McpVersion::V2025_11_25 "2025-11-25" and earlier) establish a session with an
    \c initialize handshake instead.

    @param version the protocol version string to test

    @return @ref True if \a version is a modern protocol version, @ref False if it is a legacy
        version or is not a known MCP protocol version

    @see
    - @ref McpModernVersions
    - @ref McpLegacyVersions
*/
public bool sub mcp_is_modern_version(*string version) {
    return exists version && McpModernVersions{version} ? True : False;
}

#! Returns @ref True if the given value can be sent as a plain HTTP header value
/** Per RFC 9110 a header field value consists of visible ASCII characters (0x21 - 0x7e),
    spaces, and horizontal tabs, and may not begin or end with whitespace.  A value that
    would otherwise be safe but looks like the Base64 sentinel must also be encoded, so that
    the encoding is unambiguous.

    @param value the value to test

    @return @ref True if \a value may be sent verbatim, @ref False if it must be encoded with
        @ref mcp_encode_header_value()
*/
public bool sub mcp_header_value_is_safe(string value) {
    if (!value.val()) {
        return True;
    }
    if (!regex(value, "^[\\x21-\\x7e]([\\x20\\x09\\x21-\\x7e]*[\\x21-\\x7e])?$")) {
        return False;
    }
    # a plain value that matches the sentinel pattern must be encoded to avoid ambiguity
    return !mcp_header_value_is_encoded(value);
}

#! Returns @ref True if the given header value uses the Base64 sentinel encoding
/** @param value the raw header value

    @return @ref True if \a value is of the form \c "=?base64?...?="
*/
public bool sub mcp_header_value_is_encoded(string value) {
    return value.size() >= (McpHeaderEncodingPrefix.size() + McpHeaderEncodingSuffix.size())
        && value.equalPartial(McpHeaderEncodingPrefix)
        && value.substr(-McpHeaderEncodingSuffix.size()) == McpHeaderEncodingSuffix;
}

#! Encodes a value for transmission in an MCP request header
/** Values that are already header-safe are returned unchanged; anything else is returned in
    the Base64 sentinel format \c "=?base64?{Base64EncodedValue}?=", where the Base64 payload
    is the UTF-8 representation of the value.

    @param value the value to encode

    @return the header value to send

    @see @ref mcp_decode_header_value()
*/
public string sub mcp_encode_header_value(string value) {
    if (mcp_header_value_is_safe(value)) {
        return value;
    }
    return McpHeaderEncodingPrefix + make_base64_string(convert_encoding(value, "UTF-8"))
        + McpHeaderEncodingSuffix;
}

#! Decodes a value received in an MCP request header
/** Values in the Base64 sentinel format \c "=?base64?{Base64EncodedValue}?=" are decoded to
    their UTF-8 representation; any other value is returned unchanged.

    Servers must decode header values before comparing them to the corresponding request body
    values.

    @param value the raw header value

    @return the decoded value

    @throw MCP-HEADER-ERROR the value uses the Base64 sentinel format but the payload is not
        valid Base64 or not valid UTF-8

    @see @ref mcp_encode_header_value()
*/
public string sub mcp_decode_header_value(string value) {
    if (!mcp_header_value_is_encoded(value)) {
        return value;
    }
    string b64 = value.substr(McpHeaderEncodingPrefix.size(),
        value.size() - McpHeaderEncodingPrefix.size() - McpHeaderEncodingSuffix.size());
    try {
        return parse_base64_string_to_string(b64, "UTF-8");
    } catch (hash<ExceptionInfo> ex) {
        throw "MCP-HEADER-ERROR", sprintf("invalid Base64-encoded header value %y: %s: %s",
            value, ex.err, ex.desc);
    }
}

#! Converts a tool argument value to its canonical header string representation
/** @param value the argument value; must be a string, integer, or boolean

    @return the unencoded string representation of \a value

    @throw MCP-HEADER-ERROR the value is not of a type that may be mirrored into a header
*/
public string sub mcp_header_param_value(auto value) {
    switch (value.typeCode()) {
        case NT_STRING:
            return value;
        case NT_BOOLEAN:
            return value ? "true" : "false";
        case NT_INT:
            if (value > McpMaxSafeInteger || value < McpMinSafeInteger) {
                throw "MCP-HEADER-ERROR", sprintf("integer value %d is outside the range of "
                    "integers that can be represented exactly in JSON (%d - %d)", value,
                    McpMinSafeInteger, McpMaxSafeInteger);
            }
            return value.toString();
    }
    throw "MCP-HEADER-ERROR", sprintf("cannot mirror a value of type %y into an HTTP header; "
        "only \"string\", \"integer\", and \"boolean\" values are supported", value.type());
}

#! Scans a tool input schema for \c x-mcp-header annotations
/** Walks the schema, recording every \c x-mcp-header annotation along with the instance path
    it applies to and whether that path is statically reachable (i.e. reachable from the schema
    root through a chain consisting solely of \c properties keys).

    @param schema the tool's \c inputSchema

    @return a list of hashes with the following keys:
    - \c path: the list of property names leading to the annotated property
    - \c name: the raw \c x-mcp-header annotation value
    - \c type: the annotated property's declared \c type, if any
    - \c reachable: @ref True if the property is statically reachable

    @note the traversal is bounded by @ref McpMaxSchemaDepth and @ref McpMaxSchemaNodes so
        that a hostile schema cannot be used as a denial-of-service vector
*/
public list<hash<auto>> sub mcp_scan_header_annotations(*hash<auto> schema) {
    list<hash<auto>> found = ();
    int nodes = 0;
    mcp_scan_schema_intern(schema, True, (), 0, \found, \nodes);
    return found;
}

#! Recursive worker for @ref mcp_scan_header_annotations()
nothing sub mcp_scan_schema_intern(*hash<auto> schema, bool reachable,
        list<string> path, int depth, reference<list<hash<auto>>> found, reference<int> nodes) {
    if (!schema || depth > McpMaxSchemaDepth || ++nodes > McpMaxSchemaNodes) {
        return;
    }

    if (schema{McpHeaderAnnotation}.typeCode() == NT_STRING) {
        found += {
            "path": path,
            "name": schema{McpHeaderAnnotation},
            "type": schema.type,
            "reachable": reachable && path,
        };
    }

    # "properties" values stay statically reachable; the keys are property names, never
    # annotation keys, so annotations can never be confused with property names here
    if (schema.properties.typeCode() == NT_HASH) {
        foreach hash<auto> i in (schema.properties.pairIterator()) {
            if (i.value.typeCode() == NT_HASH) {
                mcp_scan_schema_intern(i.value, reachable, path + i.key, depth + 1, \found,
                    \nodes);
            }
        }
    }

    foreach string key in (McpUnreachableSchemaKeywords) {
        if (schema{key}.typeCode() == NT_HASH) {
            mcp_scan_schema_intern(schema{key}, False, path, depth + 1, \found, \nodes);
        }
    }
    foreach string key in (McpUnreachableSchemaListKeywords) {
        if (schema{key}.typeCode() == NT_LIST) {
            foreach auto sub_schema in (schema{key}) {
                if (sub_schema.typeCode() == NT_HASH) {
                    mcp_scan_schema_intern(sub_schema, False, path, depth + 1, \found, \nodes);
                }
            }
        }
    }
    foreach string key in (McpUnreachableSchemaMapKeywords) {
        if (schema{key}.typeCode() == NT_HASH) {
            foreach auto sub_schema in (schema{key}.iterator()) {
                if (sub_schema.typeCode() == NT_HASH) {
                    mcp_scan_schema_intern(sub_schema, False, path, depth + 1, \found, \nodes);
                }
            }
        }
    }
}

#! Validates the \c x-mcp-header annotations in a tool input schema
/** Clients using the Streamable HTTP transport must reject tool definitions whose
    \c x-mcp-header annotations violate the specification's constraints, excluding the
    offending tool from the result of \c tools/list so that one malformed definition does not
    prevent other tools from being used.

    @param schema the tool's \c inputSchema

    @return @ref nothing if the schema is valid, otherwise a human-readable description of the
        first constraint violation found, suitable for logging

    @see @ref mcp_extract_header_params()
*/
public *string sub mcp_validate_tool_header_annotations(*hash<auto> schema) {
    hash<string, bool> seen;
    foreach hash<auto> ann in (mcp_scan_header_annotations(schema)) {
        string path = ann.path.join(".");
        if (!ann.reachable) {
            return sprintf("%s annotation %y at %y is not statically reachable from the schema "
                "root through \"properties\" keys alone", McpHeaderAnnotation, ann.name,
                path.val() ? path : "(schema root)");
        }
        if (!ann.name.val()) {
            return sprintf("%s annotation at %y is empty", McpHeaderAnnotation, path);
        }
        if (!regex(ann.name, "^[-!#$%&'*+.^_`|~0-9A-Za-z]+$")) {
            return sprintf("%s annotation %y at %y is not a valid HTTP field-name token",
                McpHeaderAnnotation, ann.name, path);
        }
        string lname = ann.name.lwr();
        if (seen{lname}) {
            return sprintf("%s annotation %y at %y is not unique (compared case-insensitively)",
                McpHeaderAnnotation, ann.name, path);
        }
        seen{lname} = True;
        if (ann.type.typeCode() != NT_STRING || !McpHeaderParamTypes{ann.type}) {
            return sprintf("%s annotation %y at %y is applied to a property of type %y; only "
                "\"string\", \"integer\", and \"boolean\" are supported", McpHeaderAnnotation,
                ann.name, path, ann.type ?? "(none)");
        }
    }
    return;
}

#! Returns the \c Mcp-Param-* headers for a tool call
/** Reads the instance value at the exact property path of each annotated property; when no
    value is present at that path, the header is omitted.

    @param schema the tool's \c inputSchema
    @param arguments the tool call arguments

    @return a hash of header name to encoded header value; empty if the schema has no
        \c x-mcp-header annotations or none of them match a supplied argument

    @throw MCP-HEADER-ERROR the schema's annotations are invalid, or an argument value cannot
        be represented in a header

    @see @ref mcp_validate_tool_header_annotations()
*/
public hash<string, string> sub mcp_extract_header_params(*hash<auto> schema,
        *hash<auto> arguments) {
    hash<string, string> rv = {};
    if (!schema) {
        return rv;
    }
    if (*string err = mcp_validate_tool_header_annotations(schema)) {
        throw "MCP-HEADER-ERROR", err;
    }
    foreach hash<auto> ann in (mcp_scan_header_annotations(schema)) {
        auto value = arguments;
        bool present = True;
        foreach string key in (ann.path) {
            if (value.typeCode() != NT_HASH || !value.hasKey(key)) {
                present = False;
                break;
            }
            value = value{key};
        }
        # a null value is equivalent to an absent value: the header must be omitted
        if (!present || !exists value) {
            continue;
        }
        rv{McpParamHeaderPrefix + ann.name} = mcp_encode_header_value(
            mcp_header_param_value(value));
    }
    return rv;
}

#! Returns the value for the \c Mcp-Name header for the given method and parameters
/** @param method the JSON-RPC method name
    @param params the request parameters

    @return the unencoded \c Mcp-Name value, or @ref nothing if the method does not require
        the header

    @note the returned value must be passed through @ref mcp_encode_header_value() before
        being sent
*/
public *string sub mcp_name_header_value(string method, *hash<auto> params) {
    switch (method) {
        case "tools/call":
        case "prompts/get":
            return params.name.typeCode() == NT_STRING ? params.name : NOTHING;
        case "resources/read":
            return params.uri.typeCode() == NT_STRING ? params.uri : NOTHING;
    }
    return;
}

#! Returns the protocol version declared by a request, if any
/** @param params the request's \c params member; any value is accepted, since a JSON-RPC
        request may carry positional parameters or none at all

    @return the value of the @ref MCP_MetaProtocolVersion \c _meta key, or @ref nothing if the
        request does not declare one (i.e. it is a legacy request)
*/
public *string sub mcp_request_protocol_version(auto params) {
    if (params.typeCode() != NT_HASH) {
        return;
    }
    auto v = params._meta{MCP_MetaProtocolVersion};
    return v.typeCode() == NT_STRING ? v : NOTHING;
}

} # namespace McpClient
