# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file A2aClient.qc @brief A2A Client class implementation

/*  A2aClient.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! the %A2aClient namespace holds all public definitions in the %A2aClient module
public namespace A2aClient {

#! A2A task state enum
/** The values are the A2A v0.3 wire representation; use
    @ref A2aClient::a2aTranslateTaskStateToV10() "a2aTranslateTaskStateToV10()" to convert them to the v1.0
    \c TASK_STATE_* form.

    @since json 1.11
*/
public enum A2aTaskState : string {
    #! Task has been submitted and acknowledged but is not yet being processed
    Submitted = "submitted",
    #! Task is currently being processed
    Working = "working",
    #! Task requires additional user input
    InputRequired = "input-required",
    #! Task requires authentication
    AuthRequired = "auth-required",
    #! Task completed successfully
    /** This is a terminal state
    */
    Completed = "completed",
    #! Task failed with an error
    /** This is a terminal state
    */
    Failed = "failed",
    #! Task was canceled
    /** This is a terminal state
    */
    Canceled = "canceled",
    #! Task was rejected by the agent
    /** This is a terminal state
    */
    Rejected = "rejected",
    #! Task is in an unknown or indeterminate state
    Unknown = "unknown",

    #! Task has been submitted and acknowledged but is not yet being processed
    /** @deprecated Use @ref A2aTaskState::Submitted instead; this alias previously had the non-conformant wire
        value \c "pending", which no A2A peer accepts
    */
    Pending = "submitted",
    #! Task is currently being processed
    /** @deprecated Use @ref A2aTaskState::Working instead; this alias previously had the non-conformant wire
        value \c "active", which no A2A peer accepts
    */
    Active = "working",
}

#! The A2A v0.3 task states that are terminal
/** A task in one of these states accepts no further messages and any stream for it must be closed.

    @since A2aClient 1.0
*/
public const A2aTerminalTaskStatesV03 = {
    A2aTaskState::Completed: True,
    A2aTaskState::Failed: True,
    A2aTaskState::Canceled: True,
    A2aTaskState::Rejected: True,
};

#! The A2A v1.0 task states that are terminal
/** @since A2aClient 1.0
*/
public const A2aTerminalTaskStatesV10 = {
    "TASK_STATE_COMPLETED": True,
    "TASK_STATE_FAILED": True,
    "TASK_STATE_CANCELED": True,
    "TASK_STATE_REJECTED": True,
};

#! A2A message role enum
/** @since json 1.11
*/
public enum A2aMessageRole : string {
    #! Message from the user
    User = "user",
    #! Message from the agent
    Agent = "agent",
}

#! A2A part type enum
/** @since json 1.11
*/
public enum A2aPartType : string {
    #! Text content
    TextPart = "text",
    #! File content (inline or URI)
    FilePart = "file",
    #! Structured data content
    DataPart = "data",
}

#! A2A security scheme type enum
/** @since json 1.11
*/
public enum A2aSecuritySchemeType : string {
    #! API key authentication
    ApiKey = "apiKey",
    #! HTTP authentication (e.g. Bearer, Basic)
    Http = "http",
    #! OAuth 2.0
    OAuth2 = "oauth2",
    #! OpenID Connect
    OpenIdConnect = "openIdConnect",
    #! Mutual TLS
    MutualTls = "mutualTLS",
}

/** @defgroup a2a_error_codes A2A JSON-RPC Error Codes
    The error codes assigned by the A2A specification.  These values are identical in v0.3 and v1.0, except that
    \c -32008 and \c -32009 were added in v1.0.

    A2A-specific errors occupy the JSON-RPC implementation-defined server-error range (\c -32000 to \c -32099);
    protocol-level failures use the standard JSON-RPC codes instead (ex: \c -32602 for invalid parameters).

    @since A2aClient 1.0
*/
#/@{
#! The specified task ID does not correspond to an existing or accessible task
public const A2A_ERR_TASK_NOT_FOUND = -32001;

#! An attempt was made to cancel a task that is not in a cancelable state
public const A2A_ERR_TASK_NOT_CANCELABLE = -32002;

#! The client attempted to use push notifications but the agent does not support them
public const A2A_ERR_PUSH_NOTIFICATION_NOT_SUPPORTED = -32003;

#! The requested operation, or a specific aspect of it, is not supported by this agent
public const A2A_ERR_UNSUPPORTED_OPERATION = -32004;

#! A media type in the request is not supported by the agent or the invoked skill
public const A2A_ERR_CONTENT_TYPE_NOT_SUPPORTED = -32005;

#! The agent returned a response that does not conform to the specification for the method
public const A2A_ERR_INVALID_AGENT_RESPONSE = -32006;

#! The agent does not have an extended agent card configured
public const A2A_ERR_EXTENDED_CARD_NOT_CONFIGURED = -32007;

#! The client did not declare support for an extension the agent marks as required
/** @note added in A2A v1.0
*/
public const A2A_ERR_EXTENSION_SUPPORT_REQUIRED = -32008;

#! The A2A protocol version requested via the \c A2A-Version service parameter is not supported
/** @note added in A2A v1.0
*/
public const A2A_ERR_VERSION_NOT_SUPPORTED = -32009;

#! Maps each A2A error code to its specification error name
public const A2aErrorNameMap = {
    A2A_ERR_TASK_NOT_FOUND: "TaskNotFoundError",
    A2A_ERR_TASK_NOT_CANCELABLE: "TaskNotCancelableError",
    A2A_ERR_PUSH_NOTIFICATION_NOT_SUPPORTED: "PushNotificationNotSupportedError",
    A2A_ERR_UNSUPPORTED_OPERATION: "UnsupportedOperationError",
    A2A_ERR_CONTENT_TYPE_NOT_SUPPORTED: "ContentTypeNotSupportedError",
    A2A_ERR_INVALID_AGENT_RESPONSE: "InvalidAgentResponseError",
    A2A_ERR_EXTENDED_CARD_NOT_CONFIGURED: "ExtendedAgentCardNotConfiguredError",
    A2A_ERR_EXTENSION_SUPPORT_REQUIRED: "ExtensionSupportRequiredError",
    A2A_ERR_VERSION_NOT_SUPPORTED: "VersionNotSupportedError",
};
#/@}

#! The specified task ID does not correspond to an existing or accessible task
/** @note Before the initial release this constant had the value \c -32002, which the A2A specification assigns to
    \c TaskNotCancelableError; it now carries the conformant \c TaskNotFoundError code
*/
public const A2A_ERR_NOT_FOUND = A2A_ERR_TASK_NOT_FOUND;

#! An operation was attempted that conflicts with the task's current state
/** @note Before the initial release this constant had the value \c -32003, which the A2A specification assigns to
    \c PushNotificationNotSupportedError; it now carries the conformant \c TaskNotCancelableError code
*/
public const A2A_ERR_CONFLICT = A2A_ERR_TASK_NOT_CANCELABLE;

#! The parameters provided for the method are invalid
/** The A2A specification does not assign a code for invalid parameters; the standard JSON-RPC
    \c "Invalid params" code is used.

    @note Before the initial release this constant had the value \c -32004, which the A2A specification assigns to
    \c UnsupportedOperationError
*/
public const A2A_ERR_INVALID_ARGUMENT = -32602;

#! Authorization failed
/** @deprecated This is not an A2A protocol error; authorization failures are signaled with HTTP status codes
    (\c 401 and \c 403) as described in section 7 of the A2A specification.  The value was moved out of the
    range the specification assigns so that it can no longer be mistaken for an A2A error.
*/
public const A2A_ERR_UNAUTHORIZED = -32000;

#! Access to the resource is forbidden
/** @deprecated This is not an A2A protocol error; see @ref A2A_ERR_UNAUTHORIZED.  Before the initial release this
    constant had the value \c -32001, which the A2A specification assigns to \c TaskNotFoundError.
*/
public const A2A_ERR_FORBIDDEN = -32010;

#! The service is unavailable
/** @deprecated This is not an A2A protocol error; see @ref A2A_ERR_UNAUTHORIZED.  Before the initial release this
    constant had the value \c -32005, which the A2A specification assigns to \c ContentTypeNotSupportedError.
*/
public const A2A_ERR_SERVICE_UNAVAILABLE = -32011;

#! The request timed out
/** @deprecated This is not an A2A protocol error; see @ref A2A_ERR_UNAUTHORIZED.  Before the initial release this
    constant had the value \c -32006, which the A2A specification assigns to \c InvalidAgentResponseError.
*/
public const A2A_ERR_REQUEST_TIMEOUT = -32012;

#! Valid task state transitions
public const A2aValidTransitions = {
    A2aTaskState::Submitted: {
        A2aTaskState::Working: True,
        A2aTaskState::Canceled: True,
        A2aTaskState::Rejected: True,
    },
    A2aTaskState::Working: {
        A2aTaskState::InputRequired: True,
        A2aTaskState::AuthRequired: True,
        A2aTaskState::Completed: True,
        A2aTaskState::Failed: True,
        A2aTaskState::Canceled: True,
    },
    A2aTaskState::InputRequired: {
        A2aTaskState::Working: True,
        A2aTaskState::Completed: True,
        A2aTaskState::Failed: True,
        A2aTaskState::Canceled: True,
    },
    A2aTaskState::AuthRequired: {
        A2aTaskState::Working: True,
        A2aTaskState::Completed: True,
        A2aTaskState::Failed: True,
        A2aTaskState::Canceled: True,
    },
};

#! A2A Agent Card information
public hashdecl A2aAgentCard {
    #! Agent name
    string name;

    #! Agent description
    *string description;

    #! Agent URL
    string url;

    #! Agent version
    *string version;

    #! Provider information
    *hash<auto> provider;

    #! Agent skills
    *list<hash<A2aAgentSkill>> skills;

    #! Agent capabilities
    *hash<A2aAgentCapabilities> capabilities;

    #! Security schemes
    *hash<auto> securitySchemes;

    #! Security requirements
    *list<auto> security;

    #! Default input MIME types
    *list<string> defaultInputModes;

    #! Default output MIME types
    *list<string> defaultOutputModes;

    #! Supported interfaces (v0.3 format, free-form)
    *hash<auto> interfaces;

    #! Whether the agent supports authenticated extended card
    *bool supportsAuthenticatedExtendedCard;

    #! Supported interfaces (v1.0 format)
    /** Each entry describes a protocol binding (JSONRPC, REST, gRPC) with its URL and version
    */
    *list<hash<A2aAgentInterface>> supportedInterfaces;

    #! Documentation URL
    *string documentationUrl;

    #! Icon URL
    *string iconUrl;
}

#! A2A Agent Skill information
public hashdecl A2aAgentSkill {
    #! Unique skill identifier
    string id;

    #! Human-readable skill name
    string name;

    #! Skill description
    *string description;

    #! Supported input MIME types
    *list<string> inputModes;

    #! Supported output MIME types
    *list<string> outputModes;

    #! Tags for categorization
    *list<string> tags;

    #! Example inputs/outputs
    *list<auto> examples;
}

#! A2A Agent Capabilities
public hashdecl A2aAgentCapabilities {
    #! Whether SSE streaming is supported
    *bool streaming;

    #! Whether push notifications are supported
    *bool pushNotifications;

    #! Whether extended agent card is supported
    *bool extendedAgentCard;
}

#! A2A Agent Interface entry (v1.0)
/** Describes a protocol binding endpoint in the v1.0 agent card \c supportedInterfaces array
*/
public hashdecl A2aAgentInterface {
    #! Endpoint URL
    string url;

    #! Protocol binding type (e.g., \c "JSONRPC", \c "REST", \c "GRPC")
    string protocolBinding;

    #! Protocol version (e.g., \c "1.0", \c "0.3")
    string protocolVersion;

    #! Tenant identifier
    *string tenant;
}

#! A2A SendMessage configuration (v1.0)
/** Additional configuration for v1.0 \c SendMessage requests
*/
public hashdecl A2aSendMessageConfiguration {
    #! Accepted output MIME types
    *list<string> acceptedOutputModes;

    #! Number of history messages to include
    *int historyLength;

    #! If True, return immediately after task creation without waiting for completion
    *bool returnImmediately;

    #! Inline push notification configuration
    *hash<auto> taskPushNotificationConfig;
}

#! A2A Task information
public hashdecl A2aTask {
    #! Unique task identifier
    string id;

    #! Context identifier for grouping related tasks
    *string contextId;

    #! Current task status
    hash<A2aTaskStatus> status;

    #! Message history
    *list<hash<A2aMessage>> history;

    #! Task artifacts
    *list<hash<A2aArtifact>> artifacts;

    #! Task metadata
    *hash<auto> metadata;
}

#! A2A Task Status
public hashdecl A2aTaskStatus {
    #! Current state
    string state;

    #! Optional status message
    *hash<A2aMessage> message;

    #! Timestamp of the status update
    *string timestamp;
}

#! A2A Message
public hashdecl A2aMessage {
    #! Unique message identifier
    *string messageId;

    #! Message role: user or agent
    string role;

    #! Message parts
    list<hash<auto>> parts;

    #! Referenced task IDs
    *list<string> referenceTaskIds;

    #! Timestamp
    *string timestamp;

    #! Message metadata
    *hash<auto> metadata;

    #! Context identifier (v1.0: present on the message itself)
    *string contextId;

    #! Task identifier (v1.0: for multi-turn continuation)
    *string taskId;

    #! Extension URIs (v1.0)
    *list<string> extensions;
}

#! A2A Artifact
public hashdecl A2aArtifact {
    #! Unique artifact identifier
    *string artifactId;

    #! Artifact name
    *string name;

    #! Artifact description
    *string description;

    #! Artifact parts
    list<hash<auto>> parts;

    #! Artifact metadata
    *hash<auto> metadata;

    #! Extension URIs (v1.0)
    *list<string> extensions;
}

#! A2A Push Notification Configuration
public hashdecl A2aPushNotificationConfig {
    #! Webhook URL for push notifications
    string url;

    #! Authentication for the webhook
    *hash<auto> authentication;

    #! Token for verification
    *string token;
}

#! A2A Text Part
public hashdecl A2aTextPart {
    #! Part type: "text"
    string type = "text";

    #! Text content
    string text;

    #! Part metadata
    *hash<auto> metadata;
}

#! A2A File Part
public hashdecl A2aFilePart {
    #! Part type: "file"
    string type = "file";

    #! File information
    hash<auto> file;

    #! Part metadata
    *hash<auto> metadata;
}

#! A2A Data Part
public hashdecl A2aDataPart {
    #! Part type: "data"
    string type = "data";

    #! Structured data
    auto data;

    #! Part metadata
    *hash<auto> metadata;
}

#! A2A Client class for connecting to A2A agents
/** Provides a client for the Agent-to-Agent (A2A) protocol, enabling agent-to-agent
    collaboration via JSON-RPC 2.0 over HTTP(S) with SSE streaming.

    Unlike MCP, A2A does not require an initialize() handshake. Instead, agents
    are discovered via their Agent Card at \c /.well-known/agent-card.json.

    @par Example:
    @code{.py}
    A2aClient client("http://localhost:8080");

    # Get agent card
    hash<auto> card = client.getAgentCard();

    # Send a message
    hash<auto> task = client.sendMessage({
        "role": "user",
        "parts": ({"type": "text", "text": "Hello!"},),
    });
    @endcode

    @since json 1.11
*/
public class A2aClient {
    public {
        #! Default connect timeout (30 seconds)
        const DefaultConnectTimeout = 30000;

        #! Default I/O timeout (60 seconds)
        const DefaultTimeout = 60000;

        #! Default agent card path
        const AgentCardPath = "/.well-known/agent-card.json";

        #! JSON-RPC version
        const JsonRpcVersion = "2.0";
    }

    private {
        #! The server URL
        string url;

        #! The request path extracted from the URL
        string url_path;

        #! Async I/O connection manager for all HTTP requests (JSON-RPC and SSE streaming)
        HttpClientIo::HttpClientConnectionManager mgr;

        #! Connect timeout in milliseconds
        int connect_timeout;

        #! Transfer timeout in milliseconds
        int timeout;

        #! JSON-RPC request ID counter
        int rpc_id = 0;

        #! Cached agent card
        *hash<auto> agent_card;

        #! Logger
        *LoggerInterface logger;

        #! Mutex for thread safety
        Mutex lck();

        #! SSE stop flag for streaming operations
        bool sse_stop_flag = False;

        #! SSE listener counter
        Counter sse_cnt();

        #! Callbacks for events
        hash<string, code> callbacks;

        #! Detected/configured A2A protocol version
        string protocol_version = A2aVersionV03;

        #! Extra headers for all requests (e.g., auth tokens)
        hash<auto> default_headers;

        #! Protocol for the connection manager ("h1", "h2", "h3", or "auto")
        string protocol = "auto";
    }

    #! Creates the A2aClient with the given URL and options
    /** @param url the URL of the A2A agent
        @param opts optional client options:
            - \c connect_timeout: connection timeout in milliseconds (default: 30000)
            - \c timeout: transfer timeout in milliseconds (default: 60000)
            - \c logger: a LoggerInterface object for logging
    */
    constructor(string url, *hash<auto> opts) {
        self.url = url;
        url_path = parse_url(url).path ?? "/";
        connect_timeout = opts.connect_timeout ?? DefaultConnectTimeout;
        timeout = opts.timeout ?? DefaultTimeout;
        logger = opts.logger;
        default_headers = opts.headers ?? {};

        if (opts.http_version == "1.0" || opts.http_version == "1.1") {
            protocol = "h1";
        }
        mgr = createConnectionManager();
    }

    #! Destroys the client and cleans up resources
    destructor() {
        close();
    }

    #! Returns the server URL
    string getUrl() {
        return url;
    }

    #! Returns the detected or configured A2A protocol version
    /** @return \c "0.3" or \c "1.0"
    */
    string getProtocolVersion() {
        return protocol_version;
    }

    #! Sets the A2A protocol version to use
    /** Overrides auto-detection from the agent card.
        @param version the protocol version (\c "0.3" or \c "1.0")
    */
    setProtocolVersion(string version) {
        protocol_version = version;
    }

    #! Retrieves the agent card, trying v1.0 path first then falling back to v0.3
    /** Auto-detects protocol version from the agent card format.
        If the card has \c supportedInterfaces, protocol version is set to \c "1.0".

        @return the agent card information (always in v0.3 internal format)

        @throw A2A-HTTP-ERROR if the HTTP request fails
        @throw A2A-AGENT-CARD-ERROR if the response cannot be parsed
    */
    hash<auto> getAgentCard() {
        try {
            # Try v1.0 path first
            string card_path = AgentCardPathV10;
            *string response_body;
            bool got_card = False;

            hash<auto> req_headers = {
                "Accept": MimeTypeJson,
            } + default_headers;

            try {
                hash<auto> resp = mgr.request(url, "GET", card_path, req_headers);
                if (resp.status_code >= 200 && resp.status_code < 300) {
                    response_body = resp.body;
                    got_card = True;
                }
            } catch () {
                # v1.0 path not available, fall through to v0.3
            }

            # Fall back to v0.3 path
            if (!got_card) {
                card_path = AgentCardPath;
                hash<auto> resp = mgr.request(url, "GET", card_path, req_headers);
                if (resp.status_code < 200 || resp.status_code >= 300) {
                    throw "A2A-HTTP-ERROR", sprintf("Failed to get agent card: HTTP %d",
                        resp.status_code);
                }
                response_body = resp.body;
            }

            hash<auto> card = parse_json(response_body);

            # Auto-detect protocol version from card structure
            if (card.supportedInterfaces) {
                protocol_version = A2aVersionV10;
                # Translate v1.0 card to v0.3 internal format
                card = a2aTranslateAgentCardToV03(card);
            } else {
                protocol_version = A2aVersionV03;
            }

            lck.lock();
            on_exit lck.unlock();
            agent_card = card;
            return card;
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "A2A-HTTP-ERROR") {
                rethrow;
            }
            throw "A2A-AGENT-CARD-ERROR", sprintf("Failed to get agent card: %s: %s", ex.err, ex.desc);
        }
    }

    #! Retrieves the authenticated extended agent card via JSON-RPC
    /** @return the extended agent card

        @throw A2A-ERROR if the JSON-RPC call fails
    */
    hash<auto> getAuthenticatedExtendedCard() {
        return doRpcCall("agent/getAuthenticatedExtendedCard");
    }

    #! Sends a message to the agent
    /** @param msg the message to send, must include \c role and \c parts
        @param config optional configuration including \c contextId, \c metadata, etc.

        @return the task resulting from the message

        @throw A2A-ERROR if the JSON-RPC call fails
    */
    hash<auto> sendMessage(hash<auto> msg, *hash<auto> config) {
        # Ensure messageId is set — required by v1.0 and some v0.3 servers
        if (!msg.messageId) {
            msg.messageId = get_random_bytes(16).toHex();
        }
        hash<auto!> params = ({
            "message": msg,
        });
        if (config) {
            params += config;
        }
        return doRpcCall("message/send", params);
    }

    #! Sends a message with SSE streaming for real-time updates
    /** @param msg the message to send
        @param on_event callback called for each SSE event; receives \c (string event_type, hash event_data)
        @param config optional configuration

        @throw A2A-ERROR if the request fails
    */
    sendMessageStream(hash<auto> msg, code<auto (string, hash<auto>)> on_event, *hash<auto> config) {
        # Ensure messageId is set
        if (!msg.messageId) {
            msg.messageId = get_random_bytes(16).toHex();
        }
        hash<auto!> params;
        params.message = msg;
        if (config) {
            params += config;
        }
        doRpcStream("message/stream", params, on_event);
    }

    #! Gets task information by ID
    /** @param id the task ID
        @param opts optional parameters (e.g., \c historyLength)

        @return the task information

        @throw A2A-ERROR if the task is not found or the call fails
    */
    hash<auto> getTask(string id, *hash<auto> opts) {
        hash<auto> params = {"id": id};
        if (opts) {
            params += opts;
        }
        return doRpcCall("tasks/get", params);
    }

    #! Cancels a task
    /** @param id the task ID to cancel

        @return the updated task information

        @throw A2A-ERROR if the task cannot be canceled
    */
    hash<auto> cancelTask(string id) {
        return doRpcCall("tasks/cancel", {"id": id});
    }

    #! Lists tasks with optional filtering
    /** @param opts optional parameters for filtering/pagination (e.g., \c contextId, \c limit, \c offset)

        @return task list result

        @throw A2A-ERROR if the call fails
    */
    hash<auto> listTasks(*hash<auto> opts) {
        return doRpcCall("tasks/list", opts);
    }

    #! Resubscribes to SSE events for an existing task
    /** @param id the task ID
        @param on_event callback for SSE events

        @throw A2A-ERROR if the resubscription fails
    */
    resubscribe(string id, code<auto (string, hash<auto>)> on_event) {
        doRpcStream("tasks/resubscribe", {"id": id}, on_event);
    }

    #! Sets push notification configuration for a task
    /** @param task_id the task ID
        @param config the push notification configuration

        @return the result of the operation

        @throw A2A-ERROR if the call fails
    */
    hash<auto> setPushNotificationConfig(string task_id, hash<auto> config) {
        return doRpcCall("tasks/pushNotificationConfig/set", {
            "id": task_id,
            "pushNotificationConfig": config,
        });
    }

    #! Gets push notification configuration for a task
    /** @param task_id the task ID

        @return the push notification configuration

        @throw A2A-ERROR if the call fails
    */
    hash<auto> getPushNotificationConfig(string task_id) {
        return doRpcCall("tasks/pushNotificationConfig/get", {"id": task_id});
    }

    #! Lists push notification configurations for a task
    /** @param task_id the task ID

        @return list of push notification configurations

        @throw A2A-ERROR if the call fails
    */
    hash<auto> listPushNotificationConfigs(string task_id) {
        return doRpcCall("tasks/pushNotificationConfig/list", {"id": task_id});
    }

    #! Deletes push notification configuration for a task
    /** @param task_id the task ID
        @param push_id the push notification config ID to delete

        @return the result of the operation

        @throw A2A-ERROR if the call fails
    */
    hash<auto> deletePushNotificationConfig(string task_id, string push_id) {
        return doRpcCall("tasks/pushNotificationConfig/delete", {
            "id": task_id,
            "pushNotificationConfigId": push_id,
        });
    }

    #! Sets a callback for the given event
    /** @param name the callback name (e.g., \c "onTaskStatusUpdate", \c "onTaskArtifactUpdate")
        @param cb the callback code
    */
    setCallback(string name, code cb) {
        lck.lock();
        on_exit lck.unlock();
        callbacks{name} = cb;
    }

    #! Removes a callback
    /** @param name the callback name to remove
    */
    removeCallback(string name) {
        lck.lock();
        on_exit lck.unlock();
        remove callbacks{name};
    }

    #! Returns the cached agent card, if any
    *hash<auto> getCachedAgentCard() {
        lck.lock();
        on_exit lck.unlock();
        return agent_card;
    }

    #! Closes the client and releases resources
    close() {
        sse_stop_flag = True;
        # Wait for any active SSE streams to finish
        sse_cnt.waitForZero();
        lck.lock();
        on_exit lck.unlock();
        remove agent_card;
        remove callbacks;
        mgr.closeAll();
        # Recreate the manager so the client can be reused after close
        mgr = createConnectionManager();
        sse_stop_flag = False;
    }

    #! Performs a JSON-RPC call and returns the result
    /** @param method the JSON-RPC method name (v0.3 format, e.g., \c "message/send")
        @param params optional parameters

        @return the result from the JSON-RPC response (always in v0.3 internal format)

        @throw A2A-ERROR if the JSON-RPC response contains an error
        @throw A2A-HTTP-ERROR if the HTTP request fails
    */
    private hash<auto> doRpcCall(string method, *hash<auto> params) {
        int id;
        {
            lck.lock();
            on_exit lck.unlock();
            id = ++rpc_id;
        }

        # Translate method name and params for v1.0 servers
        string wire_method = method;
        hash<auto> wire_headers;
        if (protocol_version == A2aVersionV10) {
            wire_method = a2aTranslateMethodToV10(method);
            wire_headers{A2aVersionHeader} = A2aVersionV10;
            # Translate SendMessage params to v1.0 format
            if (method == "message/send" && params) {
                params = a2aTranslateSendMessageParamsToV10(params);
            }
        }

        hash<auto> request = {
            "jsonrpc": JsonRpcVersion,
            "method": wire_method,
            "id": id,
        };
        if (params) {
            request.params = params;
        }

        string body = make_json(request);
        logDebug("A2A RPC request: method=%y wire_method=%y id=%d version=%y",
            method, wire_method, id, protocol_version);

        try {
            hash<auto> req_headers = {
                "Content-Type": MimeTypeJson,
                "Accept": MimeTypeJson,
            } + default_headers + wire_headers;
            hash<auto> resp = mgr.request(url, "POST", url_path, req_headers, body, timeout);

            if (resp.status_code < 200 || resp.status_code >= 300) {
                throw "A2A-HTTP-ERROR", sprintf("HTTP %d calling %y",
                    resp.status_code, method);
            }

            hash<auto> result = parse_json(resp.body);

            if (result.error) {
                throw "A2A-ERROR", sprintf("A2A error %d: %s", result.error.code, result.error.message),
                    result.error;
            }

            hash<auto> rv = result.result ?? {};

            # Translate v1.0 response back to v0.3 internal format
            if (protocol_version == A2aVersionV10 && rv) {
                # v1.0 SendMessageResponse can be either Task or Message (oneof)
                # Task has "status" field; Message has "message" wrapper or "role" field
                if (rv.status) {
                    # Task response — translate states/roles/parts
                    rv = a2aTranslateTaskToV03(rv);
                } else if (rv.message) {
                    # Message response wrapped: {"message": {...}} — unwrap and translate
                    rv = a2aTranslateMessageToV03(rv.message);
                } else if (rv.role) {
                    # Bare Message response — translate directly
                    rv = a2aTranslateMessageToV03(rv);
                }
                # Agent card responses (have "supportedInterfaces" field)
                if (rv.supportedInterfaces) {
                    rv = a2aTranslateAgentCardToV03(rv);
                }
                # List responses (have "tasks" field)
                if (rv.tasks) {
                    rv.tasks = map a2aTranslateTaskToV03($1), rv.tasks;
                }
            }

            return rv;
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "A2A-ERROR" || ex.err == "A2A-HTTP-ERROR") {
                rethrow;
            }
            throw "A2A-ERROR", sprintf("Error calling A2A method %y: %s: %s", method, ex.err, ex.desc);
        }
    }

    #! Performs a JSON-RPC call with SSE streaming response
    /** Uses HttpClientIo and SseStreamReader for incremental SSE event reading
        across HTTP/1.x, HTTP/2, and HTTP/3.

        @param method the JSON-RPC method name
        @param params the request parameters
        @param on_event callback for each SSE event; receives (event_type, event_data)
    */
    private doRpcStream(string method, hash<auto> params, code<auto (string, hash<auto>)> on_event) {
        int id;
        {
            lck.lock();
            on_exit lck.unlock();
            id = ++rpc_id;
        }

        # Translate method name and params for v1.0 servers
        string wire_method = method;
        hash<auto> extra_headers;
        if (protocol_version == A2aVersionV10) {
            wire_method = a2aTranslateMethodToV10(method);
            extra_headers{A2aVersionHeader} = A2aVersionV10;
            if (method == "message/stream" && params) {
                params = a2aTranslateSendMessageParamsToV10(params);
            }
        }

        hash<auto> request = {
            "jsonrpc": JsonRpcVersion,
            "method": wire_method,
            "id": id,
            "params": params,
        };

        string body = make_json(request);
        logDebug("A2A RPC stream request: method=%y wire_method=%y id=%d version=%y",
            method, wire_method, id, protocol_version);

        sse_cnt.inc();
        on_exit sse_cnt.dec();

        hash<auto> stream_headers = {
            "Accept": MimeTypeSse,
            "Content-Type": MimeTypeJson,
        } + default_headers + extra_headers;
        HttpClientIo::HttpClientStreamHandle stream = mgr.acquireAndStartStreaming(
            url, "POST", url_path, stream_headers, False, body);
        on_exit {
            # Ensure the streaming connection is properly closed so it's not
            # returned to the connection pool with unread SSE data
            try {
                stream.cancel();
            } catch () {}
        }
        try {
            HttpClientStreamingIo::SseStreamReader reader(stream);

            while (!sse_stop_flag) {
                *hash<SseMessageInfo> evt = reader.readEvent(timeout);
                if (!evt) {
                    if (reader.isDone()) {
                        break;
                    }
                    # Timeout — continue if not stopped
                    continue;
                }
                if (!evt.data.val()) {
                    continue;
                }
                try {
                    hash<auto> data = parse_json(evt.data);
                    string event_type = evt.event ?? "message";

                    if (protocol_version == A2aVersionV10) {
                        hash<auto> translated = a2aTranslateSseEventToV03(data);
                        event_type = translated.event_type;
                        data = translated.data;
                    }

                    on_event(event_type, data);
                } catch (hash<ExceptionInfo> ex) {
                    logDebug("Error parsing SSE event: %s: %s", ex.err, ex.desc);
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "SOCKET-CONNECT-ERROR" || ex.err == "SOCKET-ERROR"
                    || ex.err == "SOCKET-CLOSED" || ex.err == "HTTPCLIENT-STREAM-CLOSED") {
                rethrow;
            }
            throw "A2A-ERROR", sprintf("Stream request failed for %y: %s: %s", method, ex.err, ex.desc);
        }
    }

    #! Creates a new connection manager with current settings
    private HttpClientIo::HttpClientConnectionManager createConnectionManager() {
        return new HttpClientIo::HttpClientConnectionManager(
            <HttpClientIo::HttpClientConnectionManagerOptions>{
                "connect_timeout": connect_timeout,
                "request_timeout": timeout,
                "protocol": protocol,
            });
    }

    #! Logs a debug message
    private logDebug(string fmt) {
        if (logger) {
            logger.debug(vsprintf(fmt, argv));
        }
    }
}

} # namespace A2aClient
