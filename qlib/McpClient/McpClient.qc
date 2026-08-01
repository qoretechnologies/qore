# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file McpClient.qc @brief MCP Client class implementation

/*  McpClient.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! MCP protocol version enum
public enum McpVersion : string {
    #! Protocol version 2024-11-05 (initial version)
    V2024_11_05 = "2024-11-05",
    #! Protocol version 2025-03-26 (March 2025)
    V2025_03_26 = "2025-03-26",
    #! Protocol version 2025-06-18 (June 2025 - Enterprise readiness)
    V2025_06_18 = "2025-06-18",
    #! Protocol version 2025-11-25 (November 2025 - Tasks, Extensions)
    V2025_11_25 = "2025-11-25",
    #! Protocol version 2026-07-28 (July 2026 - stateless core, MRTR, subscriptions)
    V2026_07_28 = "2026-07-28",
}

#! Latest MCP protocol version
public const MCP_Version_Latest = McpVersion::V2026_07_28;

#! Latest handshake-based ("legacy") MCP protocol version
/** Protocol revisions up to and including this one establish a session with an
    \c initialize handshake; @ref McpVersion::V2026_07_28 "2026-07-28" and later
    convey the protocol version, client identity, and capabilities as per-request
    metadata instead.

    @see @ref mcp_is_modern_version()
*/
public const MCP_Version_Latest_Legacy = McpVersion::V2025_11_25;

#! Map of all known versions for validation
public const AllMcpVersions = {
    McpVersion::V2024_11_05: True,
    McpVersion::V2025_03_26: True,
    McpVersion::V2025_06_18: True,
    McpVersion::V2025_11_25: True,
    McpVersion::V2026_07_28: True,
};

#! Map of "modern" (stateless, per-request metadata) protocol versions
/** @see @ref mcp_is_modern_version()
*/
public const McpModernVersions = {
    McpVersion::V2026_07_28: True,
};

#! Map of "legacy" (\c initialize handshake) protocol versions
/** @see @ref mcp_is_modern_version()
*/
public const McpLegacyVersions = {
    McpVersion::V2024_11_05: True,
    McpVersion::V2025_03_26: True,
    McpVersion::V2025_06_18: True,
    McpVersion::V2025_11_25: True,
};

#! All supported protocol versions, newest first
/** This is the list reported in the \c data.supported member of an
    @ref MCP_Err_UnsupportedProtocolVersion "UnsupportedProtocolVersionError" and in the
    \c supportedVersions member of a \c server/discover result.
*/
public const McpSupportedVersions = (
    McpVersion::V2026_07_28,
    McpVersion::V2025_11_25,
    McpVersion::V2025_06_18,
    McpVersion::V2025_03_26,
    McpVersion::V2024_11_05,
);

#! MCP Session header name
/** @note protocol sessions were removed in @ref McpVersion::V2026_07_28 "2026-07-28"; this
    header is only used with @ref McpLegacyVersions "legacy" protocol versions
*/
public const McpSessionHeader = "Mcp-Session-Id";

#! MCP log level enum
/** Standard syslog-style log levels for MCP logging protocol.
*/
public enum McpLogLevel : string {
    #! Debug-level messages for detailed troubleshooting
    Debug = "debug",
    #! Informational messages about normal operation
    Info = "info",
    #! Normal but significant conditions
    Notice = "notice",
    #! Warning conditions that may need attention
    Warning = "warning",
    #! Error conditions
    Error = "error",
    #! Critical conditions requiring immediate attention
    Critical = "critical",
    #! Alert conditions requiring action
    Alert = "alert",
    #! Emergency: system is unusable
    Emergency = "emergency",
}

#! Valid log levels for validation
public const AllMcpLogLevels = {
    McpLogLevel::Debug: True,
    McpLogLevel::Info: True,
    McpLogLevel::Notice: True,
    McpLogLevel::Warning: True,
    McpLogLevel::Error: True,
    McpLogLevel::Critical: True,
    McpLogLevel::Alert: True,
    McpLogLevel::Emergency: True,
};

#! MCP completion reference type enum
public enum McpCompletionRefType : string {
    #! Reference a prompt by name
    Prompt = "ref/prompt",
    #! Reference a resource by URI
    Resource = "ref/resource",
}

#! Client info for the MCP client
public hashdecl McpClientInfo {
    #! The name of the client
    string name;

    #! The version of the client
    string version;
}

#! MCP Client options
public hashdecl McpClientOptions {
    #! Connection timeout in milliseconds
    *int connect_timeout;

    #! Request timeout in milliseconds
    *int timeout;

    #! Preferred protocol version (defaults to latest)
    *string protocol_version;

    #! Logger interface
    *LoggerInterface logger;

    #! Enable SSE for server notifications (default: False)
    *bool enable_sse;

    #! SSE reconnect on disconnect (default: True)
    *bool sse_reconnect;

    #! Enable streamable HTTP transport (default: True)
    *bool enable_streamable;

    #! Allow legacy HTTP + SSE fallback (default: True)
    *bool allow_legacy_fallback;

    #! Reconnect streamable GET listener on disconnect (default: True)
    *bool streamable_reconnect;

    #! Default read timeout for streamable HTTP (default: 30s)
    *timeout streamable_read_timeout;

    #! Read timeout for streamable HTTP notification listener (default: 250ms)
    *timeout streamable_listener_timeout;

    #! Enable streamable HTTP GET listener for notifications (default: True)
    *bool enable_streamable_listener;

    #! HTTP protocol selection: "auto" (default), "h1", "h2", or "h3"
    /** Set to "h2" to prevent Alt-Svc HTTP/3 upgrades that may fail on reconnect.
        @see HttpClientIo::HttpClientProtocol
    */
    *string http_protocol;

    #! Try a modern (stateless) protocol version before falling back (default: True)
    /** When @ref True (the default) the client sends its first request using per-request
        protocol metadata and falls back to the \c initialize handshake only if the server
        turns out to speak a handshake-based revision.  Set to @ref False to skip the modern
        attempt entirely.

        @see @ref McpClient::mcp_is_modern_version()
    */
    *bool prefer_modern;

    #! Client capabilities sent with every modern request
    /** A capability is declared by the presence of its key; the value is a per-capability
        settings object, very often empty.  Declaring \c elicitation, \c sampling, or \c roots
        allows the server to ask for that kind of input; see @ref McpClient::McpClient::setCallback().
    */
    *hash<auto> client_capabilities;

    #! Cache list results according to their \c ttlMs hint (default: True)
    *bool cache_list_results;

    #! Maximum MRTR round trips per request before giving up (default: 8)
    /** Bounds a server that keeps asking for input without ever completing the request.
    */
    *int max_input_rounds;

    #! Declare the MCP tasks extension on modern requests (default: True)
    /** With the extension declared, a server may answer a \c tools/call with a durable task
        handle instead of the result; @ref McpClient::McpClient::callTool() polls it to
        completion transparently.  Set to @ref False to require synchronous results.
    */
    *bool enable_tasks;
}

#! Tool information from server
public hashdecl McpToolResult {
    #! The name of the tool
    string name;

    #! The description of the tool
    *string description;

    #! The input schema (JSON Schema)
    *hash<auto> inputSchema;
}

#! Resource information from server
public hashdecl McpResourceResult {
    #! The URI of the resource
    string uri;

    #! The name of the resource
    string name;

    #! The description of the resource
    *string description;

    #! The MIME type of the resource
    *string mimeType;
}

#! Prompt information from server
public hashdecl McpPromptResult {
    #! The name of the prompt
    string name;

    #! The description of the prompt
    *string description;

    #! The arguments to the prompt
    *list<hash<auto>> arguments;
}

#! Resource content from server
public hashdecl McpResourceContent {
    #! The URI of the resource
    string uri;

    #! The MIME type (if provided)
    *string mimeType;

    #! Text content (for text resources)
    *string text;

    #! Binary content as base64 (for binary resources)
    *string blob;
}

#! Prompt message from server
public hashdecl McpPromptMessage {
    #! The role (user or assistant)
    string role;

    #! The content of the message
    hash<auto> content;
}

#! Tool call result
public hashdecl McpToolCallResult {
    #! The content returned by the tool
    list<hash<auto>> content;

    #! Whether the tool call resulted in an error
    *bool isError;
}

#! MCP Client class for connecting to MCP servers
/**
    This class provides a client implementation for the Model Context Protocol (MCP).
    It allows connecting to MCP servers over HTTP with JSON-RPC 2.0 protocol.

    @par Example:
    @code
    McpClient client("http://localhost:8080/mcp");

    # Initialize connection
    hash<auto> info = client.initialize({
        "name": "MyApp",
        "version": "1.0",
    });

    # Call a tool
    hash<auto> result = client.callTool("echo", {"message": "Hello"});

    client.close();
    @endcode
*/
public class McpClient {
    public {
        #! Default connection timeout (30 seconds)
        const DefaultConnectTimeout = 30000;

        #! Default request timeout (60 seconds)
        const DefaultTimeout = 60000;

        #! JSON-RPC version
        const JsonRpcVersion = "2.0";

        #! Default maximum number of MRTR round trips per request
        const DefaultMaxInputRounds = 8;

        #! Methods whose results are cached according to their \c ttlMs hint
        const CacheableMethods = {
            "server/discover": True,
            "tools/list": True,
            "prompts/list": True,
            "resources/list": True,
            "resources/templates/list": True,
            "resources/read": True,
        };

        #! Maps an input-request method to the callback that fulfills it
        /** @see @ref McpClient::McpClient::setCallback() */
        const InputRequestCallbacks = {
            "elicitation/create": "onElicitation",
            "sampling/createMessage": "onSampling",
            "roots/list": "onListRoots",
        };

        #! Maps a callback name to the client capability that registering it implies
        /** @see @ref McpClient::McpClient::setCallback() */
        const CallbackCapabilities = {
            "onElicitation": "elicitation",
            "onSampling": "sampling",
            "onListRoots": "roots",
        };

        #! Notification a server sends first on every \c subscriptions/listen stream
        const McpSubscriptionAcknowledged = "notifications/subscriptions/acknowledged";

        #! Default poll interval used when a task does not suggest one (milliseconds)
        const DefaultTaskPollMs = 500;

        #! Default maximum time to wait for a task when it declares no TTL (milliseconds)
        const DefaultTaskWaitMs = 300000;

        #! Terminal and interactive task states from the tasks extension
        const McpTaskStatusCompleted = "completed";
        const McpTaskStatusFailed = "failed";
        const McpTaskStatusCancelled = "cancelled";
        const McpTaskStatusInputRequired = "input_required";
    }

    private {
        #! The server URL
        string url;

        #! The request path extracted from the URL
        string url_path;

        #! Async I/O connection manager for all HTTP requests
        HttpClientIo::HttpClientConnectionManager mgr;

        #! Connect timeout in milliseconds
        int connect_timeout;

        #! Transfer timeout in milliseconds
        int io_timeout;

        #! HTTP protocol selection (forwarded to HttpClientConnectionManager)
        *string http_protocol;

        #! MCP session ID
        *string session_id;

        #! Negotiated protocol version
        *string protocol_version;

        #! Server info after initialization
        *hash<auto> server_info;

        #! Server capabilities
        *hash<auto> server_capabilities;

        #! Whether we're initialized
        bool initialized = False;

        #! Request ID counter
        int request_id = 0;

        #! Lock for thread safety
        Mutex lck();

        #! Default headers to include in all requests (e.g., auth tokens)
        hash<string, string> default_headers = {};

        #! Logger interface
        *LoggerInterface logger;

        #! Event callbacks
        hash<string, code> callbacks;

        #! Streamable HTTP enabled
        bool streamable_enabled = True;

        #! Allow legacy fallback
        bool allow_legacy_fallback = True;

        #! Streamable GET reconnect
        bool streamable_reconnect = True;

        #! Streamable read timeout
        timeout streamable_read_timeout = 30s;
        timeout streamable_listener_timeout = 250ms;

        #! Streamable GET listener enabled
        bool streamable_listener_enabled = True;

        #! SSE observable for async notifications (replaces the old
        #! synchronous SseStreamReader polling loop).  Dispatches events
        #! via AsyncIoController's worker pool — no dedicated reader
        #! thread, no blocking reads; observer callbacks fire as soon as
        #! each event lands in the queue.
        *HttpClientStreamingIo::SseObservable listener_obs;

        #! Observer registered on listener_obs — kept so we can deregister
        *object listener_observer;

        #! Listener state
        bool listener_started = False;

        #! Listener lock
        Mutex listener_lck();

        #! Pending requests waiting for responses
        hash<string, hash<auto>> pending_requests;

        #! In-flight synchronous request IDs (for cancellation on close)
        Mutex inflight_lck();
        hash<string, bool> inflight_requests;

        #! Whether the peer's protocol era has been determined
        /** The era is a property of the server, not of an individual request, so it is
            determined once and cached for the life of this object.
        */
        bool era_known = False;

        #! Whether the server speaks a modern (stateless) protocol version
        bool modern = False;

        #! Whether to attempt a modern protocol version first
        bool prefer_modern = True;

        #! Client capabilities declared on every modern request
        hash<auto> client_capabilities = {};

        #! Client identity declared on every modern request
        *hash<McpClientInfo> client_info;

        #! The protocol version this client asks for
        string requested_version = MCP_Version_Latest;

        #! Whether to cache list results according to their \c ttlMs hints
        bool cache_list_results = True;

        #! Maximum MRTR round trips per request
        int max_input_rounds = DefaultMaxInputRounds;

        #! Cached tool input schemas, for \c x-mcp-header mirroring; tool name -> schema
        hash<string, hash<auto>> tool_schemas = {};

        #! Cached cacheable results; cache key -> {"expires": date, "result": hash}
        hash<string, hash<auto>> result_cache = {};

        #! The result of the last \c server/discover call
        *hash<auto> discovery;

        #! Minimum log level requested on each modern request, if any
        *string request_log_level;

        #! The JSON-RPC ID of the active \c subscriptions/listen request
        *int subscription_id;

        #! The notification filter the server acknowledged for the active subscription
        *hash<auto> acknowledged_filter;

        #! Whether the deprecated HTTP+SSE transport (protocol 2024-11-05) is in use
        /** In that transport the client opens an event stream with \c GET, learns where to
            post from an \c endpoint event, and receives every response on the stream rather
            than in the body of its own POST.
        */
        bool http_sse_transport = False;

        #! The path to post JSON-RPC messages to, from the \c endpoint event
        *string sse_post_endpoint;

        #! Whether an \c endpoint event is currently being waited for
        /** A server may announce an endpoint on any event stream it serves — the Qore server
            does so for every \c GET stream, for the benefit of 2024-11-05 clients.  Only the
            client decides which transport it uses, so the announcement is honored only while
            the HTTP+SSE transport is actually being established; otherwise a notification
            listener opened on a Streamable HTTP session would silently switch transports and
            strand every subsequent request.
        */
        bool sse_endpoint_wanted = False;

        #! Signalled when @ref sse_post_endpoint becomes known
        Mutex endpoint_lck();
        Condition endpoint_cond();

        #! Queues awaiting responses delivered on the event stream; request ID -> queue
        hash<string, Queue> sse_pending;

        #! Guards @ref sse_pending
        Mutex sse_pending_lck();
    }

    #! Creates the MCP client
    /**
        @param url the URL of the MCP server endpoint
        @param opts optional client options

        @throw MCP-CLIENT-ERROR if URL is invalid
    */
    constructor(string url, *hash<auto> opts) {
        self.url = url;
        url_path = parse_url(url).path ?? "/";

        # Parse options
        connect_timeout = opts.connect_timeout ?? DefaultConnectTimeout;
        io_timeout = opts.timeout ?? DefaultTimeout;
        self.logger = opts.logger;
        if (exists opts.enable_streamable) {
            streamable_enabled = opts.enable_streamable;
        }
        if (exists opts.allow_legacy_fallback) {
            allow_legacy_fallback = opts.allow_legacy_fallback;
        }
        if (exists opts.streamable_reconnect) {
            streamable_reconnect = opts.streamable_reconnect;
        }
        if (exists opts.streamable_read_timeout) {
            streamable_read_timeout = opts.streamable_read_timeout;
        }
        if (exists opts.streamable_listener_timeout) {
            streamable_listener_timeout = opts.streamable_listener_timeout;
        }
        if (exists opts.enable_streamable_listener) {
            streamable_listener_enabled = opts.enable_streamable_listener;
        }
        if (exists opts.http_protocol) {
            http_protocol = opts.http_protocol;
        }
        if (opts.headers.typeCode() == NT_HASH) {
            default_headers += cast<hash<string, string>>(opts.headers);
        }
        if (exists opts.prefer_modern) {
            prefer_modern = opts.prefer_modern;
        }
        if (opts.client_capabilities.typeCode() == NT_HASH) {
            client_capabilities = opts.client_capabilities;
        }
        if (exists opts.cache_list_results) {
            cache_list_results = opts.cache_list_results;
        }
        if (opts.max_input_rounds.typeCode() == NT_INT && opts.max_input_rounds > 0) {
            max_input_rounds = opts.max_input_rounds;
        }
        # Declare the tasks extension by default: this client polls task handles to completion
        # transparently, so a server that decides to run a call as a task costs the caller
        # nothing.  A server that sees no such capability must never return a task handle.
        if (opts.enable_tasks !== False && !client_capabilities.extensions.hasKey(MCP_ExtensionTasks)) {
            client_capabilities.extensions{MCP_ExtensionTasks} = {};
        }
        if (opts.protocol_version.typeCode() == NT_STRING) {
            if (!AllMcpVersions{opts.protocol_version}) {
                throw "MCP-CLIENT-ERROR", sprintf("unknown MCP protocol version %y; known "
                    "versions: %y", opts.protocol_version, McpSupportedVersions);
            }
            requested_version = opts.protocol_version;
            # pinning a handshake-based version means the modern probe would be pointless
            if (!mcp_is_modern_version(requested_version)) {
                prefer_modern = False;
            }
        }

        mgr = createConnectionManager();

        logDebug("McpClient created for URL: %s", url);
    }

    #! Destructor - closes the connection
    destructor() {
        close();
    }

    #! Returns the server URL
    string getUrl() {
        return url;
    }

    #! Initializes the MCP connection
    /**
        @param client_info information about this client
        @return server information including capabilities and protocol version

        @throw MCP-INIT-ERROR if initialization fails
        @throw MCP-VERSION-ERROR if no compatible protocol version is available
    */
    hash<auto> initialize(hash<McpClientInfo> client_info) {
        if (initialized) {
            throw "MCP-INIT-ERROR", "Client is already initialized";
        }
        self.client_info = client_info;

        # Determine which protocol era the server speaks.  With a modern server there is no
        # handshake at all: the "initialization" is a single server/discover call that reports
        # the server's identity, capabilities, and supported versions.
        if (prefer_modern && probeModernEra()) {
            hash<auto> result = discover();
            initialized = True;
            logDebug("MCP client using stateless protocol version: %s", protocol_version);
            # report the same shape a legacy initialize() returns, so that callers that only
            # read protocolVersion/capabilities/serverInfo work unchanged across both eras
            return {
                "protocolVersion": protocol_version,
                "capabilities": server_capabilities ?? {},
                "serverInfo": server_info,
            } + (result.instructions.val() ? {"instructions": result.instructions} : {});
        }

        # Build initialize request
        hash<auto> params += {
            "protocolVersion": mcp_is_modern_version(requested_version)
                ? MCP_Version_Latest_Legacy
                : requested_version,
            "capabilities": client_capabilities,
            "clientInfo": {
                "name": client_info.name,
                "version": client_info.version,
            },
        };

        logDebug("Sending initialize request");

        hash<auto> response = sendRequest("initialize", params);

        if (response.error) {
            throw "MCP-INIT-ERROR", sprintf("Initialize failed: %s", response.error.message);
        }

        hash<auto> result = response.result;

        # Store server info
        server_info = result.serverInfo;
        server_capabilities = result.capabilities;

        # Negotiate protocol version
        string server_version = result.protocolVersion;
        if (!AllMcpVersions{server_version}) {
            throw "MCP-VERSION-ERROR", sprintf("Server returned unsupported protocol version: %s", server_version);
        }
        protocol_version = server_version;
        era_known = True;
        modern = False;

        # Send initialized notification
        sendNotification("notifications/initialized");

        initialized = True;
        logDebug("MCP client initialized with protocol version: %s", protocol_version);

        return result;
    }

    #! Returns @ref True if the deprecated HTTP+SSE transport is in use
    /** On that transport, introduced with protocol 2024-11-05, the client posts to an endpoint
        announced on the event stream and receives every response on that stream.  The session
        is identified by the endpoint itself rather than by an \c Mcp-Session-Id header, so
        @ref getSessionId() returns @ref nothing even though the connection is session-scoped.

        @return @ref True if the client is using the HTTP+SSE transport

        @see @ref getSsePostEndpoint()
    */
    bool isHttpSseTransport() {
        return http_sse_transport;
    }

    #! Returns the endpoint the server announced for posting messages, if any
    /** @return the path this client posts JSON-RPC messages to on the HTTP+SSE transport, or
        @ref nothing if that transport is not in use
    */
    *string getSsePostEndpoint() {
        endpoint_lck.lock();
        on_exit endpoint_lck.unlock();
        return sse_post_endpoint;
    }

    #! Returns @ref True if the server speaks a modern (stateless) protocol version
    /** @return @ref True if requests are served statelessly with per-request protocol
        metadata, @ref False if the server uses the \c initialize handshake

        @note only meaningful after @ref initialize() or @ref discover() has run
    */
    bool isModern() {
        return modern;
    }

    #! Queries the server's supported protocol versions, capabilities, and identity
    /** Sends \c server/discover, which every modern server must implement.  Calling it is
        optional — a client may invoke any RPC directly and handle an
        @ref McpClient::MCP_Err_UnsupportedProtocolVersion "UnsupportedProtocolVersionError" if
        its preferred version is not supported — but it retrieves everything about the server
        in one request instead of probing with separate \c tools/list, \c prompts/list, and
        \c resources/list calls.

        The negotiated protocol version, server capabilities, and server identity are recorded
        on this object as a side effect.

        @return the \c DiscoverResult hash with \c supportedVersions, \c capabilities, and
            optionally \c instructions members

        @throw MCP-VERSION-ERROR the server and this client have no protocol version in common
        @throw MCP-ERROR the request failed

        @note not available with a @ref McpClient::McpLegacyVersions "legacy" server, which has
            no such method; use @ref initialize() instead
    */
    hash<auto> discover() {
        if (era_known && !modern) {
            throw "MCP-DEPRECATED-METHOD", sprintf("server/discover was introduced in MCP "
                "protocol version %s; this server speaks %s, where the equivalent information "
                "comes from initialize()", MCP_Version_Latest, protocol_version ?? "a legacy version");
        }
        hash<auto> response = sendRequest("server/discover");
        if (response.error) {
            throw "MCP-ERROR", sprintf("server/discover failed: %s", response.error.message);
        }
        discovery = response.result;
        server_capabilities = discovery.capabilities;
        server_info = discovery._meta{MCP_MetaServerInfo};

        # confirm that the version we are speaking is one the server actually supports
        if (discovery.supportedVersions.typeCode() == NT_LIST
            && !discovery.supportedVersions.contains(protocol_version)) {
            *string agreed = selectVersion(discovery.supportedVersions);
            if (!agreed.val()) {
                throw "MCP-VERSION-ERROR", sprintf("no mutually supported MCP protocol version: "
                    "this client supports %y, the server supports %y", McpSupportedVersions,
                    discovery.supportedVersions);
            }
            protocol_version = agreed;
        }
        return discovery;
    }

    #! Returns the cached \c server/discover result, if any
    *hash<auto> getDiscovery() {
        return discovery;
    }

    #! Determines whether the server speaks a modern protocol version
    /** Sends \c server/discover using per-request protocol metadata and inspects the outcome:
        - a successful result, or a recognized modern JSON-RPC error, identifies a modern
          server; an
          @ref McpClient::MCP_Err_UnsupportedProtocolVersion "UnsupportedProtocolVersionError"
          additionally names a version to retry with;
        - anything else — an HTTP \c 4xx whose body is not a recognized modern error, or a
          transport-level rejection — identifies a legacy server.

        The result is cached for the life of this object, since the era is a property of the
        server rather than of an individual request.

        @return @ref True if the server is modern
    */
    private bool probeModernEra() {
        if (era_known) {
            return modern;
        }
        # assume modern for the duration of the probe so the request is built accordingly
        modern = True;
        protocol_version = requested_version;
        try {
            hash<auto> response = sendModernRequest("server/discover", NOTHING, True);
            if (response.error && !isModernErrorCode(response.error.code)) {
                # Only a *recognized* modern error identifies a modern server.  Any other
                # JSON-RPC error means the request was understood as something else entirely —
                # a handshake-based server rejecting an unknown method, or complaining that no
                # session was established — and the client must fall back rather than assume
                # the server speaks a stateless version.
                throw "MCP-LEGACY-SERVER", sprintf("server/discover was rejected with "
                    "JSON-RPC error %y: %s", response.error.code, response.error.message);
            }
            era_known = True;
            return True;
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "MCP-VERSION-ERROR") {
                # the server named the versions it supports: it is modern, but has nothing in
                # common with us; surface that rather than silently falling back
                era_known = True;
                rethrow;
            }
            if (ex.err != "MCP-LEGACY-SERVER") {
                # a transport failure is not evidence about the protocol era
                modern = False;
                protocol_version = NOTHING;
                rethrow;
            }
        }
        logDebug("server does not speak a stateless protocol version; using the initialize handshake");
        modern = False;
        era_known = True;
        protocol_version = NOTHING;
        return False;
    }

    #! Returns @ref True if the given JSON-RPC error code is defined by the MCP specification
    /** Only a modern server can produce these, so their presence in a \c 400 response body
        identifies the server's era.
    */
    private static bool isModernErrorCode(auto code) {
        switch (code) {
            case MCP_Err_HeaderMismatch:
            case MCP_Err_MissingRequiredClientCapability:
            case MCP_Err_UnsupportedProtocolVersion:
                return True;
        }
        return False;
    }

    #! Returns @ref True if the list names a handshake-based version this client supports
    private static bool hasLegacyVersion(auto supported) {
        if (supported.typeCode() != NT_LIST) {
            return False;
        }
        foreach auto v in (supported) {
            if (v.typeCode() == NT_STRING && McpLegacyVersions{v}) {
                return True;
            }
        }
        return False;
    }

    #! Returns the newest modern version supported by both this client and the server
    private static *string selectVersion(auto supported) {
        if (supported.typeCode() != NT_LIST) {
            return;
        }
        foreach string v in (McpSupportedVersions) {
            if (mcp_is_modern_version(v) && supported.contains(v)) {
                return v;
            }
        }
        return;
    }

    #! Checks if the client is initialized
    /**
        @return True if initialized, False otherwise
    */
    bool isInitialized() {
        return initialized;
    }

    #! Gets the negotiated protocol version
    /**
        @return the protocol version string or NOTHING if not initialized
    */
    *string getProtocolVersion() {
        return protocol_version;
    }

    #! Sets a default header to include in all requests
    /** @param name the header name
        @param value the header value
    */
    setDefaultHeader(string name, string value) {
        default_headers{name} = value;
    }

    #! Gets server information
    /**
        @return server info hash or NOTHING if not initialized
    */
    *hash<auto> getServerInfo() {
        return server_info;
    }

    #! Gets server capabilities
    /**
        @return server capabilities hash or NOTHING if not initialized
    */
    *hash<auto> getServerCapabilities() {
        return server_capabilities;
    }

    #! Sends a ping request to check server health
    /**
        @return True if server responds successfully

        @throw MCP-PING-ERROR if ping fails
    */
    bool ping() {
        ensureInitialized();
        checkLegacyOnlyMethod("ping", "close the response stream to signal cancellation, and "
            "rely on transport-level keep-alives for liveness");

        hash<auto> response = sendRequest("ping");

        if (response.error) {
            throw "MCP-PING-ERROR", sprintf("Ping failed: %s", response.error.message);
        }

        return True;
    }

    #! Lists available tools from the server
    /**
        @return list of tool information

        @throw MCP-ERROR if the request fails
    */
    list<auto> listTools() {
        ensureInitialized();

        hash<auto> response = sendRequest("tools/list");

        if (response.error) {
            throw "MCP-ERROR", sprintf("tools/list failed: %s", response.error.message);
        }

        list<auto> tools = response.result.tools ?? ();
        if (!modern) {
            return tools;
        }

        # Cache each tool's input schema so that tools/call can mirror the parameters the
        # server annotated with x-mcp-header into HTTP headers, and exclude any tool whose
        # annotations violate the specification — one malformed definition must not prevent
        # the other tools from being used.
        hash<string, hash<auto>> schemas = {};
        list<auto> rv = ();
        foreach auto tool in (tools) {
            if (tool.typeCode() != NT_HASH || tool.name.typeCode() != NT_STRING) {
                continue;
            }
            if (*string err = mcp_validate_tool_header_annotations(tool.inputSchema)) {
                logWarn("excluding tool %y from tools/list: %s", tool.name, err);
                continue;
            }
            if (tool.inputSchema.typeCode() == NT_HASH) {
                schemas{tool.name} = tool.inputSchema;
            }
            rv += tool;
        }
        {
            lck.lock();
            on_exit lck.unlock();
            tool_schemas = schemas;
        }
        return rv;
    }

    #! Calls a tool on the server
    /**
        @param name the name of the tool to call
        @param arguments optional arguments for the tool
        @return the tool call result

        @throw MCP-ERROR if the tool call fails
        @throw MCP-TOOL-ERROR if the tool returns an error
    */
    hash<auto> callTool(string name, *hash<auto> arguments) {
        ensureInitialized();

        hash<auto> params += {
            "name": name,
        };
        if (arguments) {
            params.arguments = arguments;
        }

        hash<auto> response = sendRequest("tools/call", params);

        if (response.error) {
            throw "MCP-ERROR", sprintf("tools/call failed: %s", response.error.message);
        }

        hash<auto> result = response.result;

        # Under the tasks extension the server decides whether a call runs as a task; when it
        # does, it answers with a handle instead of the result.  Poll it to completion so that
        # callers see the same thing either way.
        if (result.resultType == McpResultType::Task) {
            result = awaitTask(result.task.taskId, result.task);
        }

        # Check for error at result level (MCP spec)
        if (result.isError) {
            throw "MCP-ERROR", sprintf("Tool '%s' returned an error", name), result;
        }

        # Also check for error in content items
        if (result.content) {
            foreach hash<auto> item in (result.content) {
                if (item.isError) {
                    throw "MCP-ERROR", sprintf("Tool '%s' returned an error: %s", name, item.text ?? "unknown error"),
                        result;
                }
            }
        }

        return result;
    }

    #! Calls a tool as an MCP task and returns the CreateTaskResult
    /**
        @param name the name of the tool to call
        @param arguments optional arguments for the tool
        @param task optional MCP task parameters such as \c ttl
        @param meta optional request metadata such as \c progressToken or
            \c clientOperationId
        @return the \c CreateTaskResult hash with a \c task member describing the
            created (or, for a repeated \c clientOperationId, the existing) task

        @throw MCP-ERROR if the task request fails or the server returns no task

        @note Pass a \c clientOperationId in \a meta to make task creation
            idempotent: repeating the call with the same id returns the existing
            task instead of starting a new one.
    */
    hash<auto> callToolAsTask(string name, *hash<auto> arguments,
            *hash<auto> task, *hash<auto> meta) {
        ensureInitialized();

        hash<auto!> params = {
            "name": name,
            "task": task ?? {},
        };
        if (arguments) {
            params.arguments = arguments;
        }
        if (meta) {
            params._meta = meta;
        }

        hash<auto> response = sendRequest("tools/call", params);
        if (response.error) {
            throw "MCP-ERROR", sprintf("task tools/call failed: %s",
                response.error.message);
        }
        if (!response.result.task.taskId.val()) {
            throw "MCP-ERROR", sprintf("task tools/call returned no task: %y",
                response.result);
        }
        return response.result;
    }

    #! Polls a task to completion and returns the result the call would have produced
    /** Polls \c tasks/get at the interval the server suggests until the task reaches a
        terminal state, fulfilling any input requests the task raises along the way.

        @param taskId the task to poll
        @param task the initial task state, if already known
        @param timeout_ms maximum time to wait; defaults to the task's \c ttlMs

        @return the result the underlying operation would have returned synchronously

        @throw MCP-TASK-ERROR the task failed, was cancelled, or did not finish in time

        @note only available with a server that supports the
            @ref McpClient::MCP_ExtensionTasks "tasks extension"
    */
    hash<auto> awaitTask(string taskId, *hash<auto> task, *int timeout_ms) {
        ensureInitialized();
        date deadline = now_us()
            + milliseconds(timeout_ms ?? task.ttlMs ?? DefaultTaskWaitMs);
        int poll_ms = task.pollIntervalMs ?? DefaultTaskPollMs;
        hash<auto> state = task ?? {};

        while (True) {
            switch (state.status) {
                case McpTaskStatusCompleted:
                    return state.result ?? {};
                case McpTaskStatusFailed:
                    throw "MCP-TASK-ERROR", sprintf("task %y failed: %s", taskId,
                        state.error.message ?? "unknown error"), state;
                case McpTaskStatusCancelled:
                    throw "MCP-TASK-ERROR", sprintf("task %y was cancelled%s", taskId,
                        state.statusMessage.val() ? ": " + state.statusMessage : ""), state;
                case McpTaskStatusInputRequired:
                    updateTask(taskId, fulfillInputRequests(state.inputRequests ?? {}));
                    break;
            }
            if (now_us() >= deadline) {
                throw "MCP-TASK-ERROR", sprintf("task %y did not reach a terminal state within "
                    "the timeout; last status: %y", taskId, state.status);
            }
            usleep(milliseconds(poll_ms));
            state = getTask(taskId);
            if (state.pollIntervalMs.typeCode() == NT_INT && state.pollIntervalMs > 0) {
                poll_ms = state.pollIntervalMs;
            }
        }
    }

    #! Supplies requested input to a task that is waiting for it
    /** @param taskId the task to update
        @param input_responses the client's responses, keyed to the task's \c inputRequests

        @return the \c tasks/update acknowledgment

        @throw MCP-ERROR the request failed
    */
    hash<auto> updateTask(string taskId, hash<auto> input_responses) {
        ensureInitialized();

        hash<auto> response = sendRequest("tasks/update", {
            "taskId": taskId,
            "inputResponses": input_responses,
        });
        if (response.error) {
            throw "MCP-ERROR", sprintf("tasks/update failed: %s", response.error.message);
        }
        return response.result ?? {};
    }

    #! Gets the current status of an MCP task
    /**
        @param taskId the ID of the task to query

        @return the task status hash (\c taskId, \c status, and, when set,
            \c statusMessage and \c clientOperationId)

        @throw MCP-ERROR if the request fails or the task is unknown
    */
    hash<auto> getTask(string taskId) {
        ensureInitialized();

        hash<auto> response = sendRequest("tasks/get", {
            "taskId": taskId,
        });
        if (response.error) {
            throw "MCP-ERROR", sprintf("tasks/get failed: %s",
                response.error.message);
        }
        return response.result;
    }

    #! Blocks until an MCP task completes and returns the original operation result
    /** The server blocks the \c tasks/result request until the task reaches a
        terminal state or the timeout elapses, then returns the same payload the
        underlying operation would have produced synchronously.

        @param taskId the ID of the task to wait for
        @param timeout_ms optional maximum time to wait in milliseconds; if not
            given the server uses the task's \c ttl

        @return the original operation result (e.g. the \c tools/call result with
            \c content / \c isError members)

        @throw MCP-ERROR if the request fails, the task is cancelled, or it does
            not complete within the timeout
    */
    hash<auto> getTaskResult(string taskId, *int timeout_ms) {
        ensureInitialized();
        if (modern) {
            # tasks/result was removed when tasks became an extension; poll instead
            return awaitTask(taskId, NOTHING, timeout_ms);
        }

        hash<auto!> params = {
            "taskId": taskId,
        };
        if (timeout_ms) {
            params.timeout = timeout_ms;
        }

        hash<auto> response = sendRequest("tasks/result", params);
        if (response.error) {
            throw "MCP-ERROR", sprintf("tasks/result failed: %s",
                response.error.message);
        }
        return response.result;
    }

    #! Cancels an in-progress MCP task
    /**
        @param taskId the ID of the task to cancel
        @param reason optional human-readable cancellation reason reported to the
            server and surfaced in the task status

        @return the \c tasks/cancel result hash

        @throw MCP-ERROR if the request fails or the task cannot be cancelled
    */
    hash<auto> cancelTask(string taskId, *string reason) {
        ensureInitialized();

        hash<auto!> params = {
            "taskId": taskId,
        };
        if (reason.val()) {
            params.reason = reason;
        }

        hash<auto> response = sendRequest("tasks/cancel", params);
        if (response.error) {
            throw "MCP-ERROR", sprintf("tasks/cancel failed: %s",
                response.error.message);
        }
        return response.result;
    }

    #! Calls a tool as an MCP task and blocks until its result is available
    /** Convenience wrapper that combines @ref callToolAsTask() and
        @ref getTaskResult(), then raises an exception if the tool reported an
        error either at the top level (\c isError) or in any returned content
        item.

        @param name the name of the tool to call
        @param arguments optional arguments for the tool
        @param task optional MCP task parameters such as \c ttl
        @param meta optional request metadata such as \c progressToken or
            \c clientOperationId
        @param timeout_ms optional maximum time to wait in milliseconds for the
            task to complete; if not given the server uses the task's \c ttl

        @return the original tool-call result (with \c content / \c isError
            members)

        @throw MCP-ERROR if the task request fails, the task does not complete in
            time, or the tool reports an error (the offending result is passed as
            the exception argument)
    */
    hash<auto> callToolTaskResult(string name, *hash<auto> arguments,
            *hash<auto> task, *hash<auto> meta, *int timeout_ms) {
        hash<auto> created = callToolAsTask(name, arguments, task, meta);
        hash<auto> result = getTaskResult(created.task.taskId, timeout_ms);

        if (result.isError) {
            throw "MCP-ERROR", sprintf("Tool '%s' returned an error", name),
                result;
        }
        if (result.content) {
            foreach hash<auto> item in (result.content) {
                if (item.isError) {
                    throw "MCP-ERROR", sprintf("Tool '%s' returned an error: %s",
                        name, item.text ?? "unknown error"), result;
                }
            }
        }

        return result;
    }

    #! Lists available resources from the server
    /**
        @return list of resource information

        @throw MCP-ERROR if the request fails
    */
    list<auto> listResources() {
        ensureInitialized();

        hash<auto> response = sendRequest("resources/list");

        if (response.error) {
            throw "MCP-ERROR", sprintf("resources/list failed: %s", response.error.message);
        }

        return response.result.resources ?? ();
    }

    #! Reads a resource from the server
    /**
        @param uri the URI of the resource to read
        @return the resource content

        @throw MCP-ERROR if the request fails
    */
    hash<auto> readResource(string uri) {
        ensureInitialized();

        hash<auto> params += {
            "uri": uri,
        };

        hash<auto> response = sendRequest("resources/read", params);

        if (response.error) {
            throw "MCP-ERROR", sprintf("resources/read failed: %s", response.error.message);
        }

        return response.result;
    }

    #! Lists available prompts from the server
    /**
        @return list of prompt information

        @throw MCP-ERROR if the request fails
    */
    list<auto> listPrompts() {
        ensureInitialized();

        hash<auto> response = sendRequest("prompts/list");

        if (response.error) {
            throw "MCP-ERROR", sprintf("prompts/list failed: %s", response.error.message);
        }

        return response.result.prompts ?? ();
    }

    #! Gets a prompt from the server
    /**
        @param name the name of the prompt
        @param arguments optional arguments for the prompt
        @return the prompt with messages

        @throw MCP-ERROR if the request fails
    */
    hash<auto> getPrompt(string name, *hash<auto> arguments) {
        ensureInitialized();

        hash<auto> params += {
            "name": name,
        };
        if (arguments) {
            params.arguments = arguments;
        }

        hash<auto> response = sendRequest("prompts/get", params);

        if (response.error) {
            throw "MCP-ERROR", sprintf("prompts/get failed: %s", response.error.message);
        }

        return response.result;
    }

    #! Requests completion suggestions
    /**
        @param ref the reference (prompt or resource) to complete for
        @param argument the argument being completed
        @return completion suggestions

        @throw MCP-ERROR if the request fails
    */
    hash<auto> complete(hash<auto> ref, hash<auto> argument) {
        ensureInitialized();

        hash<auto> params += {
            "ref": ref,
            "argument": argument,
        };

        hash<auto> response = sendRequest("completion/complete", params);

        if (response.error) {
            throw "MCP-ERROR", sprintf("completion/complete failed: %s", response.error.message);
        }

        return response.result;
    }

    #! Sets the server log level
    /**
        @param level the log level to set

        @throw MCP-ERROR if the request fails
        @throw MCP-INVALID-LOG-LEVEL if the level is not valid
    */
    setLogLevel(string level) {
        ensureInitialized();

        if (!AllMcpLogLevels{level}) {
            throw "MCP-INVALID-LOG-LEVEL", sprintf("Invalid log level: %s", level);
        }

        checkLegacyOnlyMethod("logging/setLevel", sprintf("set the log level per request with "
            "setRequestLogLevel(), which sends %y in each request's \"_meta\" field",
            MCP_MetaLogLevel));

        hash<auto> params += {
            "level": level,
        };

        hash<auto> response = sendRequest("logging/setLevel", params);

        if (response.error) {
            throw "MCP-ERROR", sprintf("logging/setLevel failed: %s", response.error.message);
        }
    }

    #! Sets the minimum log level the server should emit for this client's requests
    /** In protocol versions from @ref McpClient::McpVersion::V2026_07_28 "2026-07-28" the log
        level is a per-request value rather than connection state: it travels in each request's
        \c _meta field, and a server must not emit \c notifications/message for a request that
        did not set it.

        @param level the minimum log level, or @ref nothing to stop requesting log messages

        @throw MCP-INVALID-LOG-LEVEL the level is not valid

        @note with a @ref McpClient::McpLegacyVersions "legacy" server use @ref setLogLevel()
            instead; logging is deprecated as of 2026-07-28
    */
    setRequestLogLevel(*string level) {
        if (level.val() && !AllMcpLogLevels{level}) {
            throw "MCP-INVALID-LOG-LEVEL", sprintf("Invalid log level: %s", level);
        }
        lck.lock();
        on_exit lck.unlock();
        request_log_level = level;
    }

    #! Internal: throws if a method removed in the modern protocol is called against a modern server
    private checkLegacyOnlyMethod(string method, string alternative) {
        if (!modern) {
            return;
        }
        throw "MCP-DEPRECATED-METHOD", sprintf("method %y was removed in MCP protocol version "
            "%s, which this server speaks; instead, %s", method, protocol_version, alternative);
    }

    #! Registers an event callback
    /**
        @param event the event name (onToolsChanged, onResourcesChanged, onPromptsChanged, onLogMessage, onError)
        @param callback the callback code to invoke

        Supported events:
        - onToolsChanged: called when the server's tool list changes
        - onResourcesChanged: called when the server's resource list changes
        - onPromptsChanged: called when the server's prompt list changes
        - onLogMessage: called when the server sends a log message (receives: string level, *auto data, *string logger)
        - onError: called when an error occurs (receives: string error_message)
        - onConnected: called when SSE connection is established
        - onDisconnected: called when SSE connection is closed
    */
    setCallback(string event, code callback) {
        lck.lock();
        on_exit lck.unlock();

        callbacks{event} = callback;

        # Registering a handler for a server-to-client interaction implies the capability: a
        # server must never ask for something the client did not declare, and a client must
        # never declare something it cannot answer.  Keeping the two in step here removes a
        # class of mismatch that is otherwise only discovered at the moment a tool asks.
        if (*string cap = CallbackCapabilities{event}) {
            if (!client_capabilities.hasKey(cap)) {
                client_capabilities{cap} = {};
                if (initialized && !modern) {
                    # a handshake-based server was told our capabilities once, at initialize
                    logWarn("callback %y was registered after initialize(); the server was "
                        "told this client does not support %y and will not use it", event, cap);
                }
            }
        }
    }

    #! Removes a callback and the capability it implied
    /** @param event the event name to remove

        @note removing a callback for a server-to-client interaction also withdraws the
            corresponding capability, so that the server stops asking for something this client
            can no longer answer
    */
    removeInputCallback(string event) {
        lck.lock();
        on_exit lck.unlock();

        remove callbacks{event};
        if (*string cap = CallbackCapabilities{event}) {
            remove client_capabilities{cap};
        }
    }

    #! Removes an event callback
    /**
        @param event the event name to remove
    */
    removeCallback(string event) {
        lck.lock();
        on_exit lck.unlock();

        remove callbacks{event};
    }

    #! Starts the SSE listener for server notifications
    /**
        This starts a background thread that listens for Server-Sent Events
        from the MCP server. Call this after initialize() to receive notifications.

        @throw MCP-NOT-INITIALIZED if client is not initialized
    */
    startSseListener() {
        ensureInitialized();
        if (modern) {
            # the standalone GET event stream was removed; open a subscription stream covering
            # the notification types the old listener delivered
            listen({
                "toolsListChanged": True,
                "promptsListChanged": True,
                "resourcesListChanged": True,
            });
            return;
        }
        startNotificationListener();
    }

    #! Opens a long-lived notification stream for the requested notification types
    /** In protocol versions from @ref McpClient::McpVersion::V2026_07_28 "2026-07-28" there is
        no standalone event stream: a client that wants server-initiated change notifications
        sends a \c subscriptions/listen request whose <i>response</i> is an SSE stream carrying
        only the notification types it opted in to.  Notifications are dispatched through the
        same callbacks as the legacy listener (\c onToolsChanged, \c onResourcesChanged,
        \c onPromptsChanged, and \c onResourceUpdated).

        Request-scoped notifications such as \c notifications/progress and
        \c notifications/message are not delivered here; they arrive on the response stream of
        the request they relate to.

        @param filter the notification types to subscribe to:
        - \c toolsListChanged (bool)
        - \c promptsListChanged (bool)
        - \c resourcesListChanged (bool)
        - \c resourceSubscriptions (list of resource URIs)

        @return @ref True if the stream was opened

        @throw MCP-NOT-INITIALIZED the client is not initialized
        @throw MCP-DEPRECATED-METHOD the server speaks a legacy protocol version; use
            @ref startSseListener() instead

        @note the server acknowledges with the subset of \a filter it agreed to honor; check
            @ref getAcknowledgedFilter() to see what it will actually send
    */
    bool listen(hash<auto> filter) {
        ensureInitialized();
        if (!modern) {
            throw "MCP-DEPRECATED-METHOD", sprintf("subscriptions/listen was introduced in MCP "
                "protocol version %s; this server speaks %s, where notifications are delivered "
                "on the standalone event stream opened by startSseListener()",
                MCP_Version_Latest, protocol_version);
        }
        {
            listener_lck.lock();
            on_exit listener_lck.unlock();
            if (listener_started) {
                return True;
            }
        }

        int id = getNextRequestId();
        hash<auto> request = {
            "jsonrpc": JsonRpcVersion,
            "id": id,
            "method": "subscriptions/listen",
            "params": {
                "_meta": getRequestMeta(),
                "notifications": filter,
            },
        };
        string body = make_json(request);

        HttpClientStreamingIo::SseObservable obs;
        object observer;
        try {
            obs = new HttpClientStreamingIo::SseObservable(mgr, url, "POST", url_path,
                getModernHeaders("subscriptions/listen", request.params), body,
                {"serialization": "none"});
            observer = new McpClientSseObserver(self);
            obs.registerObserver(observer);
            obs.observersReady();
        } catch (hash<ExceptionInfo> ex) {
            logDebug("subscriptions/listen failed: %s: %s", ex.err, ex.desc);
            return False;
        }

        listener_lck.lock();
        on_exit listener_lck.unlock();
        listener_obs = obs;
        listener_observer = observer;
        listener_started = True;
        subscription_id = id;
        return True;
    }

    #! Returns the notification filter the server acknowledged for the active subscription
    /** The server omits notification types it does not support, so this may be a subset of
        the filter passed to @ref listen().

        @return the acknowledged filter, or @ref nothing if no acknowledgment has arrived yet
    */
    *hash<auto> getAcknowledgedFilter() {
        listener_lck.lock();
        on_exit listener_lck.unlock();
        return acknowledged_filter;
    }

    #! Stops the SSE listener
    stopSseListener() {
        stopNotificationListener();
    }

    #! Checks if SSE listener is running
    /**
        @return True if SSE listener is active
    */
    bool isSseListenerRunning() {
        return listener_started;
    }

    #! Starts the notification listener via the SseObservable pattern
    /** Uses @ref HttpClientStreamingIo::SseObservable — notifications are
        dispatched on the global AsyncIoController's worker pool as soon
        as the SSE framer produces an event, not polled on a dedicated
        thread.  Callbacks (\c onProgress, \c onConnected, etc.) are
        invoked from the worker pool via an observer adapter.

        Returns True if the listener started cleanly; False on any
        failure to open the stream.
    */
    private bool startNotificationListener() {
        {
            listener_lck.lock();
            on_exit listener_lck.unlock();
            if (listener_started) {
                return True;
            }
        }

        hash<auto> headers = default_headers;
        if (session_id) {
            headers{McpSessionHeader} = session_id;
        }

        HttpClientStreamingIo::SseObservable obs;
        object observer;
        try {
            obs = new HttpClientStreamingIo::SseObservable(mgr, url, "GET",
                url_path, headers, NOTHING, {"serialization": "none"});
            # Adapter: translate SseObservable events to McpClient callbacks
            observer = new McpClientSseObserver(self);
            obs.registerObserver(observer);
            obs.observersReady();
        } catch (hash<ExceptionInfo> ex) {
            logDebug("Listener start failed: %s: %s", ex.err, ex.desc);
            return False;
        }

        {
            listener_lck.lock();
            on_exit listener_lck.unlock();
            listener_obs = obs;
            listener_observer = observer;
            listener_started = True;
        }
        return True;
    }

    #! Stops the notification listener
    private stopNotificationListener() {
        *HttpClientStreamingIo::SseObservable obs;
        {
            listener_lck.lock();
            on_exit listener_lck.unlock();
            if (!listener_started) {
                return;
            }
            obs = listener_obs;
            listener_obs = NOTHING;
            listener_observer = NOTHING;
            listener_started = False;
            subscription_id = NOTHING;
            acknowledged_filter = NOTHING;
        }
        if (obs) {
            try {
                obs.stopEvents();
            } catch () {
            }
        }
    }

    #! Called by the SseObservable observer adapter when an event arrives
    /** Parses the JSON payload and dispatches through the existing
        handleStreamableMessage → handleNotification pipeline so observer
        delivery is indistinguishable from the old polling path for
        everything downstream.
    */
    onSseData(hash<auto> data) {
        # The deprecated HTTP+SSE transport opens with an "endpoint" event naming the path to
        # post messages to; it carries a URL, not JSON.
        if (data.event == "endpoint") {
            setSsePostEndpoint(data.data);
            return;
        }
        if (!data.data.val()) {
            return;
        }
        auto msg;
        try {
            msg = parse_json(data.data);
        } catch (hash<ExceptionInfo> ex) {
            logDebug("Failed to parse SSE message (event %y, keys %y): %s: %s: %y", data.event,
                keys data, ex.err, ex.desc, data.data);
            invokeCallback("onError", (("Failed to parse SSE message: " + ex.desc),));
            return;
        }
        handleStreamableMessage(msg, -1);
    }

    #! Called by the observer adapter on SSE connection established
    onSseConnected() {
        invokeCallback("onConnected");
    }

    #! Called by the observer adapter on SSE connection closed
    onSseClosed() {
        invokeCallback("onDisconnected");
    }


    #! Closes the MCP connection
    close() {
        # Cancel any in-flight requests before closing connections
        cancelInflightRequests();

        stopSseListener();

        initialized = False;
        session_id = NOTHING;
        protocol_version = NOTHING;
        server_info = NOTHING;
        server_capabilities = NOTHING;
        discovery = NOTHING;
        # cached list results and tool schemas describe the server as it was; the client may be
        # reused against a server that has been restarted with a different tool set
        clearResultCache();
        {
            lck.lock();
            on_exit lck.unlock();
            tool_schemas = {};
        }
        # release anything still waiting for a response that will now never arrive, rather
        # than leaving those threads to block until their timeout
        {
            sse_pending_lck.lock();
            on_exit sse_pending_lck.unlock();
            foreach Queue q in (sse_pending.iterator()) {
                q.push({
                    "jsonrpc": JsonRpcVersion,
                    "error": {
                        "code": MCP_Err_InternalError,
                        "message": "the client was closed before the response arrived",
                    },
                });
            }
            sse_pending = {};
        }
        # the endpoint belongs to the session that just ended; a reopened stream announces a
        # new one.  The protocol era and transport are properties of the server, so
        # era_known/modern/http_sse_transport are deliberately retained.
        {
            endpoint_lck.lock();
            on_exit endpoint_lck.unlock();
            sse_post_endpoint = NOTHING;
        }
        mgr.closeAll();
        # Recreate the manager so the client can be reused after close
        mgr = createConnectionManager();

        logDebug("MCP client closed");
    }

    #! Sends notifications/cancelled for all in-flight requests
    /** Cancellation notifications are submitted asynchronously via
        \c submitRequestAsync() and then deterministically flushed to the
        network via \c flushPendingWrites() on the underlying connection.
        This guarantees the QUIC packets are on the wire before \c close()
        proceeds to shut down the connection manager, without waiting for
        the server's HTTP response.
    */
    private cancelInflightRequests() {
        # On the modern Streamable HTTP transport, closing the response stream *is* the
        # cancellation signal: each request has its own stream, so the transport-level
        # disconnect is unambiguous and no notifications/cancelled message is expected.
        if (modern) {
            return;
        }
        *list<string> ids;
        {
            inflight_lck.lock();
            on_exit inflight_lck.unlock();
            ids = keys inflight_requests;
        }
        if (!ids) {
            return;
        }
        logDebug("Cancelling %d in-flight request(s): %y", ids.size(), ids);

        # Submit all cancellation notifications concurrently, then flush
        # the underlying connection to guarantee packets are on the wire
        hash<string, bool> flushed_connections;
        foreach string id in (ids) {
            try {
                hash<auto> notif = {
                    "jsonrpc": JsonRpcVersion,
                    "method": "notifications/cancelled",
                    "params": {
                        "requestId": id.toInt(),
                        "reason": "Client closing",
                    },
                };
                string body = make_json(notif);
                logDebug("Sending cancellation (async): %s", body);

                hash<auto> headers = {
                    "Content-Type": "application/json",
                    "Accept": "application/json, text/event-stream",
                } + default_headers;
                if (session_id) {
                    headers{McpSessionHeader} = session_id;
                }

                hash<auto> result = mgr.submitRequestAsync(url, "POST", url_path, headers, body);

                # Flush pending writes on the connection to ensure QUIC
                # packets are sent before closeAll() cancels operations
                HttpClientIo::HttpClientConnection conn = result.stream.getConnection();
                string conn_key = conn.getConnectionId().toString();
                if (!flushed_connections{conn_key}) {
                    conn.flushPendingWrites();
                    flushed_connections{conn_key} = True;
                }
            } catch (hash<ExceptionInfo> ex) {
                logDebug("Failed to submit cancellation for request %s: %s: %s",
                    id, ex.err, ex.desc);
            }
        }
    }

    #! Gets the session ID
    /**
        @return the MCP session ID or NOTHING if not set
    */
    *string getSessionId() {
        return session_id;
    }

    #! Sets the session ID (used internally)
    setSessionId(string sid) {
        session_id = sid;
    }

    #! Internal: Ensures the client is initialized
    private ensureInitialized() {
        if (!initialized) {
            throw "MCP-NOT-INITIALIZED", "Client is not initialized. Call initialize() first.";
        }
    }

    #! Internal: Gets the next request ID
    private int getNextRequestId() {
        lck.lock();
        on_exit lck.unlock();

        return ++request_id;
    }

    #! Internal: Sends a JSON-RPC request with streamable HTTP, falling back to legacy
    private hash<auto> sendRequest(string method, *hash<auto> params) {
        if (modern) {
            return sendModernRequest(method, params);
        }
        if (http_sse_transport) {
            return sendRequestHttpSse(method, params);
        }
        if (streamable_enabled) {
            try {
                return sendRequestStreamable(method, params);
            } catch (hash<ExceptionInfo> ex) {
                if (ex.err == "MCP-STREAMABLE-UNSUPPORTED" && allow_legacy_fallback) {
                    logDebug("Streamable HTTP unsupported (%s); trying the HTTP+SSE transport",
                        ex.desc);
                    # A server that rejects the POST may be serving the deprecated HTTP+SSE
                    # transport of protocol 2024-11-05, where the endpoint to post to is
                    # announced on an event stream the client opens with GET.
                    if (initHttpSseTransport()) {
                        return sendRequestHttpSse(method, params);
                    }
                    logDebug("No HTTP+SSE endpoint announced; falling back to legacy POST");
                } else {
                    rethrow;
                }
            }
        }

        return sendRequestLegacy(method, params);
    }

    #! Internal: Sends a request using a modern (stateless) protocol version
    /** Carries the protocol version, client identity, and client capabilities in the request's
        \c _meta member and mirrors the routing-relevant fields into HTTP headers.  Handles
        version renegotiation, header-mismatch recovery, and the Multi Round-Trip Request
        pattern transparently, so that the calling code is identical in both protocol eras.

        @param method the JSON-RPC method
        @param params the request parameters
        @param probing if @ref True, an HTTP response that does not identify a modern server
            raises \c MCP-LEGACY-SERVER instead of a protocol error

        @return the JSON-RPC response hash
    */
    private hash<auto> sendModernRequest(string method, *hash<auto> params, *bool probing) {
        if (*hash<auto> cached = getCachedResult(method, params)) {
            logDebug("Using cached %s result", method);
            return {"jsonrpc": JsonRpcVersion, "result": cached};
        }

        hash<auto> req_params = params ?? {};
        bool retried_version = False;
        bool retried_headers = False;
        int rounds = 0;

        while (True) {
            hash<auto> response = sendModernRequestOnce(method, req_params, probing);

            if (response.error) {
                # renegotiate the protocol version and retry once
                if (response.error.code == MCP_Err_UnsupportedProtocolVersion && !retried_version) {
                    *string agreed = selectVersion(response.error.data.supported);
                    if (!agreed.val()) {
                        # The server understood the request well enough to report its versions
                        # but supports no stateless version we speak.  If it named a
                        # handshake-based version we do support, that is the era to use.
                        if (probing && allow_legacy_fallback
                            && hasLegacyVersion(response.error.data.supported)) {
                            throw "MCP-LEGACY-SERVER", sprintf("server supports only "
                                "handshake-based protocol versions: %y",
                                response.error.data.supported);
                        }
                        throw "MCP-VERSION-ERROR", sprintf("no mutually supported MCP protocol "
                            "version: this client supports %y, the server supports %y",
                            McpSupportedVersions, response.error.data.supported ?? ());
                    }
                    logDebug("Server does not support %s; retrying with %s", protocol_version,
                        agreed);
                    protocol_version = agreed;
                    retried_version = True;
                    continue;
                }
                # the tool's input schema may have changed under us, adding or removing an
                # x-mcp-header annotation; refresh it and retry once
                if (response.error.code == MCP_Err_HeaderMismatch && !retried_headers
                    && method == "tools/call") {
                    logDebug("Header mismatch on tools/call: %s; refreshing tool schemas",
                        response.error.message);
                    retried_headers = True;
                    refreshToolSchemas();
                    continue;
                }
                return response;
            }

            # Multi Round-Trip Requests: the server needs more information before it can
            # complete the request.  Gather it, then retry the *original* request with the
            # responses attached and a new JSON-RPC ID, echoing the server's state verbatim.
            if (response.result.resultType == McpResultType::InputRequired) {
                if (++rounds > max_input_rounds) {
                    throw "MCP-INPUT-REQUIRED-ERROR", sprintf("server asked for additional "
                        "input more than %d times while handling %y without completing the "
                        "request", max_input_rounds, method);
                }
                req_params = buildInputRetryParams(method, params, response.result);
                continue;
            }

            cacheResult(method, params, response.result);
            return response;
        }
    }

    #! Internal: performs a single modern request/response round trip
    private hash<auto> sendModernRequestOnce(string method, *hash<auto> params, *bool probing) {
        int id = getNextRequestId();
        string id_str = id.toString();

        hash<auto> req_params = (params ?? {}) + {
            "_meta": (params._meta ?? {}) + getRequestMeta(),
        };
        hash<auto> request = {
            "jsonrpc": JsonRpcVersion,
            "id": id,
            "method": method,
            "params": req_params,
        };
        string body = make_json(request);
        logDebug("Sending modern request: %s", body);

        {
            inflight_lck.lock();
            on_exit inflight_lck.unlock();
            inflight_requests{id_str} = True;
        }
        on_exit {
            inflight_lck.lock();
            on_exit inflight_lck.unlock();
            remove inflight_requests{id_str};
        }

        hash<auto> resp = mgr.request(url, "POST", url_path, getModernHeaders(method, req_params),
            body, streamable_read_timeout);
        logDebug("Response status: %d", resp.status_code);

        auto msg = parseModernResponseBody(resp, id);

        if ((resp.status_code / 100) == 2) {
            if (msg.typeCode() != NT_HASH) {
                throw "MCP-ERROR", sprintf("no JSON-RPC response received for %y", method);
            }
            return msg;
        }

        # A modern server uses 400 for version, capability, and header errors, and 404 for an
        # unimplemented method; in every case the body is a recognizable JSON-RPC error.  A
        # body that is not one identifies a server that does not host a modern MCP endpoint.
        if (msg.typeCode() == NT_HASH && msg.hasKey("error")) {
            return msg;
        }
        if (probing) {
            throw "MCP-LEGACY-SERVER", sprintf("HTTP %d with no MCP JSON-RPC error body",
                resp.status_code);
        }
        throw "MCP-HTTP-ERROR", sprintf("HTTP error: %d %s", resp.status_code, resp.body ?? "");
    }

    #! Internal: parses a modern response body, which may be JSON or a single-request SSE stream
    private auto parseModernResponseBody(hash<auto> resp, int request_id) {
        if (!resp.body.val()) {
            return;
        }
        *string content_type = resp.headers."content-type";
        if (content_type && content_type =~ /text\/event-stream/) {
            # request-scoped notifications may precede the response on this stream
            return parseSseResponseBody(resp.body, request_id);
        }
        try {
            return parse_json(resp.body);
        } catch (hash<ExceptionInfo> ex) {
            return;
        }
    }

    #! Internal: returns the \c _meta members every modern request must carry
    private hash<auto> getRequestMeta() {
        hash<auto!> rv = {
            MCP_MetaProtocolVersion: protocol_version ?? requested_version,
            MCP_MetaClientCapabilities: client_capabilities,
        };
        if (client_info) {
            rv{MCP_MetaClientInfo} = {
                "name": client_info.name,
                "version": client_info.version,
            };
        }
        if (request_log_level.val()) {
            rv{MCP_MetaLogLevel} = request_log_level;
        }
        return rv;
    }

    #! Internal: returns the HTTP headers for a modern request
    /** Mirrors the routing-relevant body fields into headers so that intermediaries can route
        and inspect requests without parsing the body.  The server rejects any request whose
        headers do not match its body, so these are derived from the body, never guessed.
    */
    private hash<auto> getModernHeaders(string method, hash<auto> params) {
        hash<auto> headers = {
            "Content-Type": "application/json",
            "Accept": "application/json, text/event-stream",
            McpProtocolVersionHeader: params._meta{MCP_MetaProtocolVersion},
            McpMethodHeader: method,
        } + default_headers;

        if (*string name = mcp_name_header_value(method, params)) {
            headers{McpNameHeader} = mcp_encode_header_value(name);
        }
        if (method == "tools/call" && params.name.typeCode() == NT_STRING) {
            if (*hash<auto> schema = tool_schemas{params.name}) {
                try {
                    headers += mcp_extract_header_params(schema, params.arguments);
                } catch (hash<ExceptionInfo> ex) {
                    # a tool with invalid annotations is excluded by listTools(); if one is
                    # called anyway, send no Mcp-Param-* headers rather than malformed ones
                    logDebug("Not mirroring parameters of tool %y into headers: %s: %s",
                        params.name, ex.err, ex.desc);
                }
            }
        }
        return headers;
    }

    #! Internal: builds the parameters for an MRTR retry
    /** The retry is an independent request: it carries a new JSON-RPC ID, the original
        parameters, the client's responses to the server's input requests, and the server's
        opaque state echoed back byte for byte.
    */
    private hash<auto> buildInputRetryParams(string method, *hash<auto> params,
            hash<auto> result) {
        hash<auto!> rv = params ?? {};
        if (result.inputRequests) {
            rv.inputResponses = (rv.inputResponses ?? {}) + fulfillInputRequests(result.inputRequests);
        }
        # the state is opaque: never inspect, parse, or modify it, and never send one that the
        # server did not provide
        if (result.requestState.typeCode() == NT_STRING) {
            rv.requestState = result.requestState;
        } else {
            remove rv.requestState;
        }
        return rv;
    }

    #! Internal: invokes the registered callbacks to satisfy a server's input requests
    private hash<auto> fulfillInputRequests(hash<auto> input_requests) {
        hash<auto!> rv = {};
        foreach hash<auto> i in (input_requests.pairIterator()) {
            *string method = i.value.method;
            *string event = InputRequestCallbacks{method ?? ""};
            if (!event.val()) {
                throw "MCP-INPUT-REQUIRED-ERROR", sprintf("server requested input using "
                    "unsupported method %y", method);
            }
            *code cb;
            {
                lck.lock();
                on_exit lck.unlock();
                cb = callbacks{event};
            }
            if (!cb) {
                throw "MCP-INPUT-REQUIRED-ERROR", sprintf("server requested %y but no %y "
                    "callback is registered; declare the capability only when a callback is "
                    "set", method, event);
            }
            rv{i.key} = cb(i.value.params ?? {});
        }
        return rv;
    }

    #! Internal: returns the cache key for a cacheable request, if it is cacheable
    private *string getCacheKey(string method, *hash<auto> params) {
        if (!cache_list_results || !CacheableMethods{method}) {
            return;
        }
        return method + "\0" + make_json(params - "_meta" ?? {});
    }

    #! Internal: returns a cached result for a request, if one is present and still fresh
    private *hash<auto> getCachedResult(string method, *hash<auto> params) {
        *string key = getCacheKey(method, params);
        if (!key.val()) {
            return;
        }
        lck.lock();
        on_exit lck.unlock();
        *hash<auto> entry = result_cache{key};
        if (!entry) {
            return;
        }
        if (entry.expires < now_us()) {
            remove result_cache{key};
            return;
        }
        return entry.result;
    }

    #! Internal: caches a result according to its \c ttlMs freshness hint
    private cacheResult(string method, *hash<auto> params, *hash<auto> result) {
        *string key = getCacheKey(method, params);
        if (!key.val() || result.ttlMs.typeCode() != NT_INT || result.ttlMs <= 0) {
            return;
        }
        lck.lock();
        on_exit lck.unlock();
        result_cache{key} = {
            "expires": now_us() + milliseconds(result.ttlMs),
            "result": result,
        };
    }

    #! Discards every cached result
    /** Call after receiving a \c listChanged notification for a list whose \c ttlMs has not
        yet elapsed.
    */
    clearResultCache() {
        lck.lock();
        on_exit lck.unlock();
        result_cache = {};
    }

    #! Internal: refreshes the cached tool input schemas used for header mirroring
    private refreshToolSchemas() {
        clearResultCache();
        try {
            listTools();
        } catch (hash<ExceptionInfo> ex) {
            logDebug("Could not refresh tool schemas: %s: %s", ex.err, ex.desc);
        }
    }

    #! Internal: opens the deprecated HTTP+SSE transport and waits for its \c endpoint event
    /** @return @ref True if the server announced an endpoint, so the transport can be used
    */
    private bool initHttpSseTransport() {
        {
            # the endpoint, not the transport flag, is what makes the transport usable: after
            # close() the transport is still known but the stream that announced the endpoint
            # is gone and has to be reopened
            endpoint_lck.lock();
            on_exit endpoint_lck.unlock();
            if (sse_post_endpoint.val()) {
                return True;
            }
            # from here until an endpoint arrives, an endpoint event selects this transport
            sse_endpoint_wanted = True;
        }
        if (!startNotificationListener()) {
            clearSseEndpointWanted();
            return False;
        }
        if (!waitForSsePostEndpoint(streamable_read_timeout).val()) {
            # the stream opened but no endpoint arrived: this is not an HTTP+SSE server
            clearSseEndpointWanted();
            stopNotificationListener();
            return False;
        }
        clearSseEndpointWanted();
        return True;
    }

    #! Internal: stops honoring \c endpoint events announced on the event stream
    private clearSseEndpointWanted() {
        endpoint_lck.lock();
        on_exit endpoint_lck.unlock();
        sse_endpoint_wanted = False;
    }

    #! Internal: Sends a request over the deprecated HTTP+SSE transport (protocol 2024-11-05)
    /** In this transport the POST carries the request and is answered with \c 202; the
        JSON-RPC response itself arrives asynchronously on the event stream opened with \c GET.
        The waiter is therefore registered before the POST goes out.

        @param method the JSON-RPC method
        @param params the request parameters

        @return the JSON-RPC response hash

        @throw MCP-TRANSPORT-ERROR the server never announced an endpoint to post to
        @throw MCP-TIMEOUT no response arrived on the event stream in time
    */
    private hash<auto> sendRequestHttpSse(string method, *hash<auto> params) {
        # reopen the event stream if it is not up: on this transport the endpoint to post to is
        # scoped to the stream, so a client reused after close() has to learn it again
        if (!initHttpSseTransport()) {
            throw "MCP-TRANSPORT-ERROR", "the server did not announce an endpoint to post "
                "messages to; the HTTP+SSE transport cannot be used";
        }
        *string endpoint = waitForSsePostEndpoint(streamable_read_timeout);
        if (!endpoint.val()) {
            throw "MCP-TRANSPORT-ERROR", "the server did not announce an endpoint to post "
                "messages to; the HTTP+SSE transport cannot be used";
        }

        int id = getNextRequestId();
        string id_str = id.toString();

        Queue q();
        {
            sse_pending_lck.lock();
            on_exit sse_pending_lck.unlock();
            sse_pending{id_str} = q;
        }
        on_exit {
            sse_pending_lck.lock();
            on_exit sse_pending_lck.unlock();
            remove sse_pending{id_str};
        }

        hash<auto!> request = {
            "jsonrpc": JsonRpcVersion,
            "id": id,
            "method": method,
        };
        # always send a "params" object, even when empty: some conforming servers reject a
        # request that omits it entirely, and every official SDK sends one
        request.params = params ?? {};
        string body = make_json(request);
        logDebug("Sending HTTP+SSE request: %s", body);

        hash<auto> headers = {
            "Content-Type": "application/json",
            "Accept": "application/json, text/event-stream",
        } + default_headers;

        hash<auto> resp = mgr.request(url, "POST", endpoint, headers, body);
        if ((resp.status_code / 100) != 2) {
            throw "MCP-HTTP-ERROR", sprintf("HTTP error posting to %y: %d %s", endpoint,
                resp.status_code, resp.body ?? "");
        }

        try {
            return q.get(io_timeout);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "QUEUE-TIMEOUT") {
                throw "MCP-TIMEOUT", sprintf("no response to %y arrived on the event stream "
                    "within %d ms", method, io_timeout);
            }
            rethrow;
        }
    }

    #! Internal: Sends a JSON-RPC notification over the HTTP+SSE transport
    private sendNotificationHttpSse(string method, *hash<auto> params) {
        *string endpoint = waitForSsePostEndpoint(streamable_read_timeout);
        if (!endpoint.val()) {
            return;
        }
        hash<auto!> request = {
            "jsonrpc": JsonRpcVersion,
            "method": method,
        };
        if (params) {
            request.params = params;
        }
        mgr.request(url, "POST", endpoint, {
            "Content-Type": "application/json",
            "Accept": "application/json, text/event-stream",
        } + default_headers, make_json(request));
    }

    #! Internal: Sends a JSON-RPC request using legacy HTTP transport
    private hash<auto> sendRequestLegacy(string method, *hash<auto> params) {
        int id = getNextRequestId();
        string id_str = id.toString();

        hash<auto> request = {
            "jsonrpc": JsonRpcVersion,
            "id": id,
            "method": method,
        };

        # always send a "params" object, even when empty: some conforming servers reject a
        # request that omits it entirely, and every official SDK sends one
        request.params = params ?? {};

        string body = make_json(request);

        logDebug("Sending request: %s", body);

        # Track in-flight request for cancellation on close()
        {
            inflight_lck.lock();
            on_exit inflight_lck.unlock();
            inflight_requests{id_str} = True;
        }
        on_exit {
            inflight_lck.lock();
            on_exit inflight_lck.unlock();
            remove inflight_requests{id_str};
        }

        # Build headers
        hash<auto> headers = {
            "Content-Type": "application/json",
            "Accept": "application/json, text/event-stream",
        } + default_headers;
        if (session_id) {
            headers{McpSessionHeader} = session_id;
        }

        hash<auto> resp = mgr.request(url, "POST", url_path, headers, body);

        logDebug("Response status: %d", resp.status_code);

        # Check for session ID in response headers
        if (resp.headers{McpSessionHeader.lwr()}) {
            session_id = resp.headers{McpSessionHeader.lwr()};
            logDebug("Received session ID: %s", session_id);
        }

        if (resp.status_code < 200 || resp.status_code >= 300) {
            throw "MCP-HTTP-ERROR", sprintf("HTTP error: %d", resp.status_code);
        }

        return parse_json(resp.body);
    }

    #! Internal: Sends a JSON-RPC request using streamable HTTP
    private hash<auto> sendRequestStreamable(string method, *hash<auto> params) {
        int id = getNextRequestId();
        string id_str = id.toString();

        hash<auto> request = {
            "jsonrpc": JsonRpcVersion,
            "id": id,
            "method": method,
        };

        # always send a "params" object, even when empty: some conforming servers reject a
        # request that omits it entirely, and every official SDK sends one
        request.params = params ?? {};

        string body = make_json(request);
        logDebug("Sending streamable request: %s", body);

        # Track in-flight request for cancellation on close()
        {
            inflight_lck.lock();
            on_exit inflight_lck.unlock();
            inflight_requests{id_str} = True;
        }
        on_exit {
            inflight_lck.lock();
            on_exit inflight_lck.unlock();
            remove inflight_requests{id_str};
        }

        hash<auto> headers = {
            "Content-Type": "application/json",
            "Accept": "application/json, text/event-stream",
        } + default_headers;
        if (session_id) {
            headers{McpSessionHeader} = session_id;
        }

        hash<auto> resp;
        try {
            resp = mgr.request(url, "POST", url_path, headers, body, streamable_read_timeout);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "SOCKET-CONNECT-ERROR" || ex.err == "SOCKET-ERROR"
                    || ex.err == "SOCKET-CLOSED" || ex.err == "HTTPCLIENT-CONNECT-ERROR"
                    || ex.err == "HTTPCLIENT-REQUEST-ERROR") {
                rethrow;
            }
            throw "MCP-STREAMABLE-ERROR", sprintf("Streamable HTTP request failed: %s: %s", ex.err, ex.desc);
        }

        # Check for session ID in response headers
        if (resp.headers{McpSessionHeader.lwr()}) {
            session_id = resp.headers{McpSessionHeader.lwr()};
            logDebug("Received session ID: %s", session_id);
        }

        if ((resp.status_code / 100) != 2) {
            if (resp.status_code == 404 || resp.status_code == 405 || resp.status_code == 415) {
                throw "MCP-STREAMABLE-UNSUPPORTED",
                    sprintf("Streamable HTTP unsupported: %d %s", resp.status_code, resp.body ?? "");
            }
            throw "MCP-HTTP-ERROR", sprintf("HTTP error: %d %s", resp.status_code, resp.body ?? "");
        }

        *string content_type = resp.headers."content-type";
        if (content_type && content_type =~ /text\/event-stream/) {
            # Parse SSE events from the response body
            return parseSseResponseBody(resp.body, id);
        }

        return parse_json(resp.body);
    }

    #! Internal: Parses SSE events from a complete response body
    private hash<auto> parseSseResponseBody(auto body, int request_id) {
        string body_str = body.typeCode() == NT_BINARY ? body.toString() : string(body);
        # Split by double-newline (SSE event boundaries)
        list<string> event_blocks = body_str.split("\n\n");
        for (int i = 0; i < event_blocks.size(); ++i) {
            string block = event_blocks[i];
            if (!block.val()) {
                continue;
            }
            # Ensure trailing \n\n for parser
            *hash<SseMessageInfo> evt = Socket::parseServerSentEvent(block + "\n\n");
            if (!evt || !evt.data.val()) {
                continue;
            }
            auto msg;
            try {
                msg = parse_json(evt.data);
            } catch (hash<ExceptionInfo> ex) {
                logDebug("Failed to parse SSE message: %s: %s", ex.err, ex.desc);
                invokeCallback("onError", (("Failed to parse SSE message: " + ex.desc),));
                continue;
            }
            *hash<auto> response = handleStreamableMessage(msg, request_id);
            if (response) {
                return response;
            }
        }

        throw "MCP-ERROR", "No response received from streamable SSE response";
    }

    #! Internal: Handles parsed streamable message payloads
    private *hash<auto> handleStreamableMessage(auto msg, int request_id) {
        if (msg.type() == "list") {
            foreach auto entry in (msg) {
                *hash<auto> resp = handleStreamableMessage(entry, request_id);
                if (resp) {
                    return resp;
                }
            }
            return;
        }

        if (msg.type() != "hash") {
            return;
        }

        if (exists msg.id) {
            # A message carrying both an ID and a method is a request from the server, not a
            # response to ours.  Protocol revisions up to 2025-11-25 let a server ask the
            # client for an LLM completion, user input, or its root list this way; from
            # 2026-07-28 the server uses Multi Round-Trip Requests instead and must never send
            # a request on a stream.
            if (msg.method.typeCode() == NT_STRING) {
                # Answer on a dedicated thread: this runs on an async I/O worker, where the
                # synchronous request API used to post the answer back would deadlock against
                # the very request that is waiting for it, and where a slow user callback
                # would stall event delivery for every other stream.
                background self.handleServerRequest(msg);
                return;
            }
            if (string(msg.id) == string(request_id)) {
                return msg;
            }
            # on the HTTP+SSE transport every response arrives here rather than in the body of
            # the POST that sent the request, so hand it to the thread waiting for it
            if (deliverSseResponse(msg)) {
                return;
            }
            return;
        }

        handleNotification(msg);
        return;
    }

    #! Internal: records the endpoint announced by an \c endpoint event and wakes any waiter
    private setSsePostEndpoint(*string endpoint) {
        if (!endpoint.val()) {
            return;
        }
        # the event carries either an absolute URL or a path; only the path is used, since the
        # connection manager is already bound to the origin
        string path = endpoint;
        if (path =~ /^https?:\/\//) {
            path = parse_url(path).path ?? "/";
        }
        endpoint_lck.lock();
        on_exit endpoint_lck.unlock();
        if (!sse_endpoint_wanted) {
            # this stream was not opened to establish the HTTP+SSE transport; the announcement
            # is informational and must not change the transport in use
            logDebug("Ignoring an endpoint event on a stream that is not establishing the "
                "HTTP+SSE transport: %y", path);
            return;
        }
        logDebug("HTTP+SSE transport: posting messages to %y", path);
        sse_post_endpoint = path;
        http_sse_transport = True;
        endpoint_cond.broadcast();
    }

    #! Internal: waits for the \c endpoint event that opens the HTTP+SSE transport
    private *string waitForSsePostEndpoint(timeout tmo) {
        endpoint_lck.lock();
        on_exit endpoint_lck.unlock();
        if (!sse_post_endpoint.val()) {
            endpoint_cond.wait(endpoint_lck, tmo);
        }
        return sse_post_endpoint;
    }

    #! Internal: routes a response that arrived on the event stream to the thread awaiting it
    /** @return @ref True if the message was claimed by a waiter
    */
    private bool deliverSseResponse(hash<auto> msg) {
        string id = msg.id.toString();
        Queue q;
        {
            sse_pending_lck.lock();
            on_exit sse_pending_lck.unlock();
            if (!sse_pending{id}) {
                return False;
            }
            q = sse_pending{id};
        }
        q.push(msg);
        return True;
    }

    #! Internal: Answers a server-initiated JSON-RPC request received on the event stream
    /** Dispatches to the same callbacks that satisfy Multi Round-Trip Request input requests,
        so a tool that asks for elicitation, sampling, or roots behaves identically whichever
        protocol revision is in use, and sends the result back to the server as a JSON-RPC
        response.
    */
    handleServerRequest(hash<auto> message) {
        string method = message.method;
        logDebug("Received server request %y (id %y)", method, message.id);

        *string event = InputRequestCallbacks{method};
        if (!event.val()) {
            sendServerResponse(message.id, NOTHING, {
                "code": MCP_Err_MethodNotFound,
                "message": sprintf("this client does not implement method %y", method),
            });
            return;
        }

        *code cb;
        {
            lck.lock();
            on_exit lck.unlock();
            cb = callbacks{event};
        }
        if (!cb) {
            sendServerResponse(message.id, NOTHING, {
                "code": MCP_Err_MethodNotFound,
                "message": sprintf("this client declared support for %y but has no %y callback "
                    "registered", method, event),
            });
            return;
        }

        try {
            auto result = cb(message.params ?? {});
            sendServerResponse(message.id, result.typeCode() == NT_HASH ? result : {});
        } catch (hash<ExceptionInfo> ex) {
            logDebug("Callback %s threw handling %s: %s: %s", event, method, ex.err, ex.desc);
            sendServerResponse(message.id, NOTHING, {
                "code": MCP_Err_InternalError,
                "message": sprintf("%s: %s", ex.err, ex.desc),
            });
        }
    }

    #! Internal: Posts a JSON-RPC response to a server-initiated request
    private sendServerResponse(auto id, *hash<auto> result, *hash<auto> error_info) {
        hash<auto!> response = {
            "jsonrpc": JsonRpcVersion,
            "id": id,
        };
        if (error_info) {
            response.error = error_info;
        } else {
            response.result = result ?? {};
        }
        string body = make_json(response);
        logDebug("Answering server request: %s", body);

        hash<auto> headers = {
            "Content-Type": "application/json",
            "Accept": "application/json, text/event-stream",
        } + default_headers;
        if (session_id) {
            headers{McpSessionHeader} = session_id;
        }
        try {
            mgr.request(url, "POST", url_path, headers, body);
        } catch (hash<ExceptionInfo> ex) {
            logDebug("Failed to answer server request %y: %s: %s", id, ex.err, ex.desc);
            invokeCallback("onError", (sprintf("failed to answer server request: %s", ex.desc),));
        }
    }

    #! Internal: Sends a JSON-RPC notification (no response expected)
    private sendNotification(string method, *hash<auto> params) {
        if (http_sse_transport) {
            sendNotificationHttpSse(method, params);
            return;
        }
        hash<auto> request = {
            "jsonrpc": JsonRpcVersion,
            "method": method,
        };

        if (params) {
            request.params = params;
        }

        string body = make_json(request);

        logDebug("Sending notification: %s", body);

        # Build headers
        hash<auto> headers = {
            "Content-Type": "application/json",
            "Accept": "application/json, text/event-stream",
        } + default_headers;
        if (session_id) {
            headers{McpSessionHeader} = session_id;
        }

        mgr.request(url, "POST", url_path, headers, body);

        # We don't expect a response for notifications
    }

    #! Internal: Invokes an event callback
    invokeCallback(string event, *list<auto> args) {
        *code cb;
        {
            lck.lock();
            on_exit lck.unlock();
            cb = callbacks{event};
        }

        if (cb) {
            try {
                call_function_args(cb, args);
            } catch (hash<ExceptionInfo> ex) {
                logDebug("Callback %s threw exception: %s: %s", event, ex.err, ex.desc);
            }
        }
    }

    #! Internal: Handles a server notification
    private handleNotification(hash<auto> message) {
        string method = message.method;

        switch (method) {
            case "notifications/tools/list_changed":
                invokeCallback("onToolsChanged");
                break;

            case "notifications/resources/list_changed":
                invokeCallback("onResourcesChanged");
                break;

            case "notifications/prompts/list_changed":
                invokeCallback("onPromptsChanged");
                break;

            case "notifications/message":
                # Log message notification
                if (message.params) {
                    invokeCallback("onLogMessage", (
                        message.params.level,
                        message.params.data,
                        message.params.logger,
                    ));
                }
                break;

            case "notifications/progress":
                if (message.params) {
                    invokeCallback("onProgress", (message.params,));
                }
                break;

            case "notifications/resources/updated":
                invokeCallback("onResourceUpdated", (message.params.uri,));
                break;

            case McpSubscriptionAcknowledged:
                # the server reports the subset of the requested filter it will honor
                {
                    listener_lck.lock();
                    on_exit listener_lck.unlock();
                    acknowledged_filter = message.params.notifications ?? {};
                }
                invokeCallback("onSubscribed", (message.params.notifications ?? {},));
                break;

            default:
                logDebug("Unknown notification: %s", method);
        }
    }

    #! Creates a new connection manager with current settings
    private HttpClientIo::HttpClientConnectionManager createConnectionManager() {
        return new HttpClientIo::HttpClientConnectionManager(
            <HttpClientIo::HttpClientConnectionManagerOptions>{
                "connect_timeout": connect_timeout,
                "request_timeout": io_timeout,
                "protocol": http_protocol ?? "auto",
            });
    }

    #! Internal: Logs a debug message
    private logDebug(string fmt, ...) {
        if (logger) {
            string msg = vsprintf(fmt, argv);
            logger.debug(msg);
        }
    }

    #! Internal: Logs a warning message
    private logWarn(string fmt, ...) {
        if (logger) {
            string msg = vsprintf(fmt, argv);
            logger.warn(msg);
        }
    }
}

#! Observer adapter that routes SseObservable events to the McpClient
/** Inherits from @ref DataProvider::Observer so it satisfies the
    abstract @ref DataProvider::Observer::update() signature required by
    @ref DataProvider::Observable::registerObserver().

    The adapter translates the three SSE event types
    (\c EVENT_SSE_CONNECTED, \c EVENT_SSE_DATA, \c EVENT_SSE_CLOSED) into
    the McpClient's existing callback API so downstream consumers don't
    have to change.
*/
public class McpClientSseObserver inherits DataProvider::Observer {
    private {
        McpClient client;
        #! True after onSseConnected has been fired to the client
        /** EVENT_SSE_CONNECTED fires as soon as the SSE request has been
            submitted — before the server has necessarily processed it and
            registered the stream.  Defer firing onSseConnected until the
            first real SSE signal (data or heartbeat) arrives, which
            confirms the server has fully registered the stream and can
            deliver server-initiated notifications.
        */
        bool connected_fired = False;
    }

    constructor(McpClient client) {
        self.client = client;
    }

    #! Observer::update hook
    update(string event_id, hash<auto> data_) {
        if (!client) {
            return;
        }
        switch (event_id) {
            case HttpClientStreamingIo::EVENT_SSE_DATA:
                fireConnectedOnce();
                client.onSseData(data_);
                break;
            case HttpClientStreamingIo::EVENT_SSE_HEARTBEAT:
                fireConnectedOnce();
                break;
            case HttpClientStreamingIo::EVENT_SSE_CLOSED:
                client.onSseClosed();
                break;
        }
    }

    private fireConnectedOnce() {
        if (connected_fired) {
            return;
        }
        connected_fired = True;
        client.onSseConnected();
    }
}

} # namespace McpClient
