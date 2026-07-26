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
}

#! Latest MCP protocol version
public const MCP_Version_Latest = McpVersion::V2025_11_25;

#! Map of all known versions for validation
public const AllMcpVersions = {
    McpVersion::V2024_11_05: True,
    McpVersion::V2025_03_26: True,
    McpVersion::V2025_06_18: True,
    McpVersion::V2025_11_25: True,
};

#! MCP Session header name
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

        # Build initialize request
        hash<auto> params += {
            "protocolVersion": MCP_Version_Latest,
            "capabilities": {
                # Client capabilities
            },
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

        # Send initialized notification
        sendNotification("notifications/initialized");

        initialized = True;
        logDebug("MCP client initialized with protocol version: %s", protocol_version);

        return result;
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

        return response.result.tools ?? ();
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

        hash<auto> params += {
            "level": level,
        };

        hash<auto> response = sendRequest("logging/setLevel", params);

        if (response.error) {
            throw "MCP-ERROR", sprintf("logging/setLevel failed: %s", response.error.message);
        }
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
        startNotificationListener();
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
        if (!data.data.val()) {
            return;
        }
        auto msg;
        try {
            msg = parse_json(data.data);
        } catch (hash<ExceptionInfo> ex) {
            logDebug("Failed to parse SSE message: %s: %s", ex.err, ex.desc);
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
                    "Accept": "application/json",
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
        if (streamable_enabled) {
            try {
                return sendRequestStreamable(method, params);
            } catch (hash<ExceptionInfo> ex) {
                if (ex.err == "MCP-STREAMABLE-UNSUPPORTED" && allow_legacy_fallback) {
                    logDebug("Streamable HTTP unsupported (%s); falling back to legacy transport", ex.desc);
                } else {
                    rethrow;
                }
            }
        }

        return sendRequestLegacy(method, params);
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

        if (params) {
            request.params = params;
        }

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
            "Accept": "application/json",
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

        if (params) {
            request.params = params;
        }

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
            if (string(msg.id) == string(request_id)) {
                return msg;
            }
            return;
        }

        handleNotification(msg);
        return;
    }

    #! Internal: Sends a JSON-RPC notification (no response expected)
    private sendNotification(string method, *hash<auto> params) {
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
            "Accept": "application/json",
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
