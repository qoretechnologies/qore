# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file A2aServerHandler.qc @brief A2A Server class definition for the HttpServer module

/*  A2aServerHandler.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! A2A task information
public hashdecl A2aTaskInfo {
    #! Unique task identifier
    string id;

    #! Context identifier
    *string contextId;

    #! Current state
    string state;

    #! Status message (JSON-serialized)
    *string statusMessage;

    #! Message history
    list<hash<auto>> history;

    #! Task artifacts
    list<hash<auto>> artifacts;

    #! Task metadata
    *hash<auto> metadata;

    #! When the task was created
    date createdAt;

    #! When the task was last updated
    date lastUpdatedAt;

    #! Session ID
    *string sessionId;

    #! Condition variable for blocking waits
    Qore::Thread::Condition cond;

    #! Mutex for condition variable
    Qore::Thread::Mutex lck;
}

#! A2aServerHandler class definition; to be registered as a handler in the HttpServer class
/**
*/
public class A2aServerHandler inherits JsonRpcHandler, ServerSentEventHandler {
    public {
        #! Default SSE handler options
        const SseHandlerOpts = {
            # JSON serialization is performed by this object when needed
            "data": "none",
        };

        #! Default session timeout in seconds (1 hour)
        const DefaultSessionTimeoutSecs = 3600;

        #! Timeout in milliseconds for delivering a push notification to a webhook
        /** @since A2aServerHandler 1.0
        */
        const PushNotificationTimeoutMs = 10000;

        #! Header carrying the client's opaque notification token on webhook deliveries
        /** @since A2aServerHandler 1.0
        */
        const A2aNotificationTokenHeader = "X-A2A-Notification-Token";
    }

    private {
        #! Internal key recording the protocol version negotiated when a push config was registered
        /** Stripped from every push config returned to a client.

            @since A2aServerHandler 1.0
        */
        const PushConfigVersionKey = "_a2a_version";
    }

    private {
        #! Agent card metadata
        hash<auto> agent_card;

        #! Extended agent card (optional)
        *hash<auto> extended_agent_card;

        #! Registered skills: id -> skill info
        hash<string, hash<A2aAgentSkill>> skill_map;

        #! Session map; session ID -> session info
        hash<string, hash<A2aSessionInfo>> smap;

        #! Task map; task ID -> task info (global, not per-session)
        hash<string, hash<A2aTaskInfo>> task_map;

        #! Push notification configs; task ID -> list of configs
        hash<string, list<hash<auto>>> push_configs;

        #! Application-level message handler callback
        *code<auto (hash<A2aTaskInfo>, hash<auto>)> message_handler;

        #! Streaming message handler callback: receives (task_info, message, token_callback)
        /** The token_callback is called for each partial message chunk during streaming.
            The handler runs in a background thread.
        */
        *code stream_message_handler;

        #! Context history: contextId -> accumulated message list for multi-turn conversations
        hash<string, list<hash<auto>>> context_history;

        #! Maximum number of messages to keep per context
        int max_context_messages = 100;

        #! Session timeout in seconds
        int session_timeout_secs = DefaultSessionTimeoutSecs;

        #! Task ID counter
        int task_id_counter = 0;

        #! Request ID counter
        int rpc_rid = 0;

        #! Per-session ID counter
        hash<string, int> sidmap;

        #! Push notification config ID counter
        int push_config_id_counter = 0;

        #! Protocol extensions this agent supports, keyed by URI
        hash<string, hash<A2aAgentExtension>> extension_map;
    }

    #! Creates the handler with a logger, agent card, and authenticator
    constructor(Logger::LoggerInterface logger, hash<auto> agent_card,
            HttpServer::AbstractAuthenticator auth)
            : JsonRpcHandler(logger, auth), ServerSentEventHandler(auth, SseHandlerOpts) {
        self.agent_card = agent_card;
        if (agent_card.skills) {
            foreach hash<auto> skill in (agent_card.skills) {
                skill_map{skill.id} = cast<hash<A2aAgentSkill>>(skill);
            }
        }
        setupMethods();
    }

    #! Creates the handler with a logger and agent card (no auth)
    constructor(Logger::LoggerInterface logger, hash<auto> agent_card)
            : JsonRpcHandler(logger, new PermissiveAuthenticator()),
              ServerSentEventHandler(new PermissiveAuthenticator(), SseHandlerOpts) {
        self.agent_card = agent_card;
        if (agent_card.skills) {
            foreach hash<auto> skill in (agent_card.skills) {
                skill_map{skill.id} = cast<hash<A2aAgentSkill>>(skill);
            }
        }
        setupMethods();
    }

    #! Sets the message handler callback
    /** @param handler the callback to handle incoming messages; receives:
            - \c hash<A2aTaskInfo> task_info: the task associated with this message
            - \c hash<auto> message: the A2A message
        and should return a response message hash or NOTHING
    */
    setMessageHandler(code<auto (hash<A2aTaskInfo>, hash<auto>)> handler) {
        message_handler = handler;
    }

    #! Sets the streaming message handler callback
    /** @param handler the callback for streaming message handling; receives:
            - \c hash<A2aTaskInfo> task_info: the task associated with this message
            - \c hash<auto> message: the A2A message
            - \c code token_callback: callback to call for each partial message chunk;
              receives a \c hash<auto> with partial message parts

        The handler is called in a background thread. It should call \c token_callback for each
        token/chunk and return when done. The task will be transitioned to \c Completed automatically
        when the handler returns, or to \c Failed if an exception is thrown.
    */
    setStreamMessageHandler(*code handler) {
        stream_message_handler = handler;
    }

    #! Sets the agent card
    setAgentCard(hash<auto> card) {
        AutoWriteLock al(rwl);
        agent_card = card;
        if (card.skills) {
            skill_map = {};
            foreach hash<auto> skill in (card.skills) {
                skill_map{skill.id} = cast<hash<A2aAgentSkill>>(skill);
            }
        }
    }

    #! Returns the agent card
    hash<auto> getAgentCard() {
        AutoReadLock al(rwl);
        return agent_card;
    }

    #! Sets the extended agent card
    setExtendedAgentCard(hash<auto> card) {
        AutoWriteLock al(rwl);
        extended_agent_card = card;
    }

    #! Registers a skill
    registerSkill(hash<A2aAgentSkill> skill) {
        AutoWriteLock al(rwl);
        skill_map{skill.id} = skill;
        # Update agent card skills list
        if (!agent_card.skills) {
            agent_card.skills = ();
        }
        agent_card.skills += skill;
    }

    #! Unregisters a skill
    /** @param id the skill ID to unregister
        @return True if the skill was unregistered, False if not found
    */
    bool unregisterSkill(string id) {
        AutoWriteLock al(rwl);
        if (!skill_map{id}) {
            return False;
        }
        remove skill_map{id};
        # Remove from agent card skills list
        if (agent_card.skills) {
            agent_card.skills = (map $1, agent_card.skills, $1.id != id);
        }
        return True;
    }

    #! Handles HTTP requests; dispatches to Agent Card, SSE, or JSON-RPC
    hash<HttpResponseInfo> handleRequest(hash<auto> cx, hash<auto> hdr, *data b) {
        # Serve agent card via GET /.well-known/agent-card.json (v0.3)
        if (cx.hdr.method == "GET" && cx.url.path =~ /\.well-known\/agent-card\.json/) {
            hash<auto> card;
            {
                AutoReadLock al(rwl);
                card = agent_card;
            }
            return AbstractHttpRequestHandler::makeResponse(200, make_json(card),
                {"Content-Type": MimeTypeJson});
        }

        # Serve agent card via GET /.well-known/a2a-agent-card (v1.0)
        if (cx.hdr.method == "GET" && cx.url.path =~ /\.well-known\/a2a-agent-card$/) {
            hash<auto> card;
            {
                AutoReadLock al(rwl);
                card = a2aTranslateAgentCardToV10(agent_card);
            }
            return AbstractHttpRequestHandler::makeResponse(200, make_json(card),
                {"Content-Type": MimeTypeJson});
        }

        # SSE streaming for GET requests with Accept: text/event-stream
        if (cx.hdr.method == "GET" && clientAcceptsSse(hdr)) {
            return ServerSentEventHandler::handleRequest(cx, hdr, b);
        }

        # POST requests
        if (cx.hdr.method == "POST") {
            # Detect A2A protocol version from header before dispatch
            *string a2a_version = hdr{A2aVersionHeader} ?? hdr{A2aVersionHeader.lwr()};
            cx.a2a_version = a2a_version;

            # negotiate protocol extensions; the activated subset is echoed on the response
            cx.a2a_extensions = negotiateExtensions(hdr);

            # a client that does not opt into an extension the agent marks as required cannot be served
            list<string> missing = getMissingRequiredExtensions(cx);
            if (missing) {
                return <HttpResponseInfo>{
                    "code": 200,
                    "body": make_json({
                        "jsonrpc": "2.0",
                        "id": NOTHING,
                        "error": {
                            "code": A2A_ERR_EXTENSION_SUPPORT_REQUIRED,
                            "message": sprintf("this agent requires the following extension%s, which the "
                                "request did not declare support for in the %s header: %y",
                                missing.size() == 1 ? "" : "s", A2aExtensionsHeader, missing),
                        },
                    }),
                    "hdr": {"Content-Type": MimeTypeJson},
                };
            }

            # Intercept streaming methods — return SSE via send_callback for incremental delivery
            # Only use SSE path when the client accepts text/event-stream; otherwise
            # fall through to JSON-RPC dispatch for polling-based streaming
            if (b && clientAcceptsSse(hdr)) {
                try {
                    hash<auto> jsonrpc = parse_json(b);
                    string method = jsonrpc.method ?? "";
                    if (method == "message/stream" || method == "SendStreamingMessage") {
                        # Validate before committing to SSE response
                        if (!jsonrpc.params.message) {
                            return <HttpResponseInfo>{
                                "code": 200,
                                "body": make_json({
                                    "jsonrpc": "2.0", "id": jsonrpc.id,
                                    "error": {"code": A2A_ERR_INVALID_ARGUMENT,
                                        "message": "Missing 'message' parameter"},
                                }),
                                "hdr": {"Content-Type": MimeTypeJson},
                            };
                        }
                        string version = cx.a2a_version
                            ?? a2aDetectVersionFromMethod(jsonrpc.method);
                        # Sync fallback when no stream handler registered
                        if (!stream_message_handler) {
                            hash<auto> params = jsonrpc.params;
                            if (version == A2aVersionV10) {
                                params = a2aTranslateSendMessageParamsToV03(params);
                            }
                            return handleStreamPostSync(cx, params, version);
                        }
                        # Return SSE response with send_callback for incremental delivery
                        return handleStreamPostCallback(jsonrpc, version);
                    }
                } catch () {
                    # JSON parse error — fall through to JsonRpcHandler for proper error response
                }
            }

            hash<HttpResponseInfo> rv = JsonRpcHandler::handleRequest(cx, hdr, b);
            # report which of the requested extensions were actually activated
            hash<auto> ext_hdr = getExtensionResponseHeaders(cx);
            if (ext_hdr) {
                rv.hdr = (rv.hdr ?? {}) + ext_hdr;
            }
            return rv;
        }

        return AbstractHttpRequestHandler::makeResponse(405, "Method not allowed");
    }

    #! Creates a new task
    /** @param context_id optional context ID for grouping related tasks
        @param metadata optional task metadata
        @param session_id optional session ID to associate the task with
        @return the task info hash
    */
    hash<A2aTaskInfo> createTask(*string context_id, *hash<auto> metadata, *string session_id) {
        string task_id = generateTaskId();
        date now = now_us();

        hash<A2aTaskInfo> task = <A2aTaskInfo>{
            "id": task_id,
            "contextId": context_id,
            "state": A2aTaskState::Pending,
            "statusMessage": NOTHING,
            "history": (),
            "artifacts": (),
            "metadata": metadata,
            "createdAt": now,
            "lastUpdatedAt": now,
            "sessionId": session_id,
            "cond": new Qore::Thread::Condition(),
            "lck": new Qore::Thread::Mutex(),
        };

        {
            AutoWriteLock al(rwl);
            task_map{task_id} = task;
        }

        return task;
    }

    #! Updates a task's state with transition validation
    /** @param task_id the task ID
        @param new_state the new state
        @param message optional status message
        @return the updated task info

        @throw A2A-TASK-ERROR if the task is not found
        @throw A2A-CONFLICT if the state transition is invalid
    */
    hash<auto> updateTaskState(string task_id, string new_state, *hash<auto> message) {
        hash<auto!> status_update;
        hash<auto> response;
        Qore::Thread::Condition task_cond;
        Qore::Thread::Mutex task_lck;

        {
            AutoWriteLock al(rwl);
            *hash<A2aTaskInfo> task = task_map{task_id};
            if (!task) {
                throw "A2A-TASK-ERROR", sprintf("Task %y not found", task_id);
            }

            # Validate state transition
            string current_state = task.state;
            if (A2aValidTransitions{current_state} && !A2aValidTransitions{current_state}{new_state}) {
                throw "A2A-CONFLICT", sprintf("Invalid state transition from %y to %y for task %y",
                    current_state, new_state, task_id);
            }

            # Terminal states cannot transition further
            if (current_state == A2aTaskState::Completed || current_state == A2aTaskState::Failed
                    || current_state == A2aTaskState::Canceled || current_state == A2aTaskState::Rejected) {
                throw "A2A-CONFLICT", sprintf("Cannot transition from terminal state %y for task %y",
                    current_state, task_id);
            }

            date now = now_us();
            string ts = format_date("YYYY-MM-DDTHH:mm:SS.xxZ", now);

            task_map{task_id}.state = new_state;
            if (message) {
                task_map{task_id}.statusMessage = make_json(message);
            } else {
                remove task_map{task_id}.statusMessage;
            }
            task_map{task_id}.lastUpdatedAt = now;

            status_update = ({
                "state": new_state,
                "timestamp": ts,
            });
            if (message) {
                status_update.message = message;
            }

            task_cond = task.cond;
            task_lck = task.lck;

            response = getTaskResponse(task_map{task_id});
        }

        # Notify waiting threads (outside write lock)
        task_lck.lock();
        on_exit task_lck.unlock();
        task_cond.broadcast();

        # Send push notifications (outside write lock to avoid deadlock)
        sendTaskPushNotification(task_id, "TaskStatusUpdateEvent", status_update);

        # Notify SSE connections (outside write lock)
        notifySseConnections(task_id, "TaskStatusUpdateEvent", status_update);

        return response;
    }

    #! Adds an artifact to a task
    /** @param task_id the task ID
        @param artifact the artifact to add

        @throw A2A-TASK-ERROR if the task is not found
    */
    addArtifact(string task_id, hash<auto> artifact) {
        {
            AutoWriteLock al(rwl);
            *hash<A2aTaskInfo> task = task_map{task_id};
            if (!task) {
                throw "A2A-TASK-ERROR", sprintf("Task %y not found", task_id);
            }

            task_map{task_id}.artifacts += artifact;
            task_map{task_id}.lastUpdatedAt = now_us();
        }

        # Send push notifications (outside write lock)
        sendTaskPushNotification(task_id, "TaskArtifactUpdateEvent", artifact);

        # Notify SSE connections (outside write lock)
        notifySseConnections(task_id, "TaskArtifactUpdateEvent", artifact);
    }

    #! Returns the number of active sessions
    int getSessionCount() {
        AutoReadLock al(rwl);
        return smap.size();
    }

    #! Returns the number of active tasks
    int getTaskCount() {
        AutoReadLock al(rwl);
        return task_map.size();
    }

    #! Sets the maximum number of messages to keep per context for conversation memory
    /** @param limit the maximum number of messages to keep per context (must be > 0)
    */
    setMaxContextMessages(int limit) {
        if (limit < 1) {
            throw "A2A-INVALID-ARGUMENT", "max_context_messages must be > 0";
        }
        AutoWriteLock al(rwl);
        max_context_messages = limit;
    }

    #! Returns the accumulated context history for a given context ID
    /** @param context_id the context identifier
        @return the list of messages accumulated for this context, or an empty list if none
    */
    list<hash<auto>> getContextHistory(string context_id) {
        AutoReadLock al(rwl);
        return context_history{context_id} ?? ();
    }

    #! Clears context history
    /** @param context_id if provided, clears only that context; otherwise clears all contexts
    */
    clearContextHistory(*string context_id) {
        AutoWriteLock al(rwl);
        if (context_id) {
            remove context_history{context_id};
        } else {
            context_history = {};
        }
    }

    private hash<auto> messageSend(hash<auto> cx, *hash<auto> params) {
        try {
            return messageSendIntern(cx, params);
        } catch (hash<ExceptionInfo> ex) {
            error("Error in message/send: %s: %s", ex.err, ex.desc);
            return makeJsonRpcError(A2A_ERR_INVALID_ARGUMENT, sprintf("%s: %s", ex.err, ex.desc));
        }
    }

    private hash<auto> messageSendIntern(hash<auto> cx, *hash<auto> params) {
        if (!params.message) {
            throw "A2A-INVALID-REQUEST", "Missing 'message' parameter";
        }

        # Detect protocol version: header takes precedence, then method name
        string version = cx.a2a_version ?? a2aDetectVersionFromMethod(cx.method);

        # Normalize v1.0 params to v0.3 internal format
        if (version == A2aVersionV10) {
            params = a2aTranslateSendMessageParamsToV03(params);
        }
        params = normalizeSendParamsContext(params);

        # Create a new task
        hash<A2aTaskInfo> task = createTask(params.contextId, params.metadata, params.sessionId);
        string task_id = task.id;

        # Build full message history: prepend context history if contextId is provided
        hash<auto> handler_message = params.message;
        if (params.contextId) {
            list<hash<auto>> prior_history;
            {
                AutoReadLock al(rwl);
                prior_history = context_history{params.contextId} ?? ();
            }
            if (prior_history) {
                handler_message.history = prior_history;
            }
        }

        # Add the incoming message to task history
        {
            AutoWriteLock al(rwl);
            task_map{task_id}.history += params.message;
        }

        # Transition to active
        updateTaskState(task_id, A2aTaskState::Active);

        # Call message handler if registered
        *hash<auto> response;
        if (message_handler) {
            try {
                response = message_handler(task, handler_message);
                if (response) {
                    # Add response to task history and complete the task
                    {
                        AutoWriteLock al(rwl);
                        task_map{task_id}.history += response;
                    }
                    # Only complete if not already in a terminal state
                    string current_state;
                    {
                        AutoReadLock al(rwl);
                        current_state = task_map{task_id}.state;
                    }
                    if (current_state != A2aTaskState::Completed
                            && current_state != A2aTaskState::Failed
                            && current_state != A2aTaskState::Canceled
                            && current_state != A2aTaskState::Rejected) {
                        updateTaskState(task_id, A2aTaskState::Completed, response);
                    }
                }
            } catch (hash<ExceptionInfo> ex) {
                if (ex.err == "A2A-CONFLICT") {
                    # Task already in terminal state - ignore
                } else {
                    error("Message handler error: %s: %s", ex.err, ex.desc);
                    # Only fail if not already in a terminal state
                    try {
                        updateTaskState(task_id, A2aTaskState::Failed, {
                            "role": "agent",
                            "parts": ({"type": "text", "text": sprintf("Error: %s: %s", ex.err, ex.desc)},),
                        });
                    } catch () {
                        # Ignore if state transition fails
                    }
                }
            }
        }

        # Accumulate context history if contextId is provided
        if (params.contextId) {
            AutoWriteLock al(rwl);
            if (!context_history{params.contextId}) {
                context_history{params.contextId} = ();
            }
            context_history{params.contextId} += params.message;
            if (response) {
                context_history{params.contextId} += response;
            }
            # Trim oldest messages if over the limit
            if (context_history{params.contextId}.size() > max_context_messages) {
                int excess = context_history{params.contextId}.size() - max_context_messages;
                splice context_history{params.contextId}, 0, excess;
            }
        }

        AutoReadLock al(rwl);
        return getTaskResponse(task_map{task_id}, NOTHING, version);
    }

    private hash<auto> messageStream(hash<auto> cx, *hash<auto> params) {
        # Fall back to synchronous message/send if no streaming handler is registered
        if (!stream_message_handler) {
            return messageSend(cx, params);
        }

        try {
            return messageStreamIntern(cx, params);
        } catch (hash<ExceptionInfo> ex) {
            error("Error in message/stream: %s: %s", ex.err, ex.desc);
            return makeJsonRpcError(A2A_ERR_INVALID_ARGUMENT, sprintf("%s: %s", ex.err, ex.desc));
        }
    }

    private hash<auto> messageStreamIntern(hash<auto> cx, *hash<auto> params) {
        if (!params.message) {
            throw "A2A-INVALID-REQUEST", "Missing 'message' parameter";
        }

        # Detect protocol version
        string version = cx.a2a_version ?? a2aDetectVersionFromMethod(cx.method);

        # Normalize v1.0 params to v0.3 internal format
        if (version == A2aVersionV10) {
            params = a2aTranslateSendMessageParamsToV03(params);
        }
        params = normalizeSendParamsContext(params);

        # Create a new task
        hash<A2aTaskInfo> task = createTask(params.contextId, params.metadata, params.sessionId);
        string task_id = task.id;

        # Build handler message with context history
        hash<auto> handler_message = params.message;
        if (params.contextId) {
            list<hash<auto>> prior_history;
            {
                AutoReadLock al(rwl);
                prior_history = context_history{params.contextId} ?? ();
            }
            if (prior_history) {
                handler_message.history = prior_history;
            }
        }

        # Add incoming message to task history
        {
            AutoWriteLock al(rwl);
            task_map{task_id}.history += params.message;
        }

        # Transition to active
        updateTaskState(task_id, A2aTaskState::Active);

        # Get initial task response to return immediately
        hash<auto> initial_response;
        {
            AutoReadLock al(rwl);
            initial_response = getTaskResponse(task_map{task_id}, NOTHING, version);
        }

        # Capture variables for background thread
        code handler = stream_message_handler;
        *string context_id = params.contextId;
        hash<auto> user_message = params.message;

        # Spawn background thread for streaming
        background streamMessageBackground(task_id, task, handler_message, handler, context_id, user_message);

        return initial_response;
    }

    #! Background thread for streaming message processing
    private streamMessageBackground(string task_id, hash<A2aTaskInfo> task, hash<auto> handler_message,
            code handler, *string context_id, hash<auto> user_message) {
        list<hash<auto>> assembled_parts = ();

        # Token callback that the handler calls for each chunk
        code token_callback = sub (hash<auto> partial) {
            # Track parts for assembly
            if (partial.parts) {
                assembled_parts += partial.parts;
            }

            # Broadcast partial update via SSE
            notifySseConnections(task_id, "TaskMessageUpdateEvent", partial);

            # Also send push notifications
            sendTaskPushNotification(task_id, "TaskMessageUpdateEvent", partial);
        };

        try {
            handler(task, handler_message, token_callback);

            # Assemble final response from collected parts
            hash<auto> response = {
                "role": "agent",
                "parts": assembled_parts,
            };

            # Add response to task history
            {
                AutoWriteLock al(rwl);
                task_map{task_id}.history += response;
            }

            # Accumulate context history
            if (context_id) {
                AutoWriteLock al(rwl);
                if (!context_history{context_id}) {
                    context_history{context_id} = ();
                }
                context_history{context_id} += user_message;
                context_history{context_id} += response;
                # Trim oldest messages if over the limit
                if (context_history{context_id}.size() > max_context_messages) {
                    int excess = context_history{context_id}.size() - max_context_messages;
                    splice context_history{context_id}, 0, excess;
                }
            }

            updateTaskState(task_id, A2aTaskState::Completed, response);
        } catch (hash<ExceptionInfo> ex) {
            error("Stream message handler error: %s: %s", ex.err, ex.desc);
            try {
                updateTaskState(task_id, A2aTaskState::Failed, {
                    "role": "agent",
                    "parts": ({"type": "text", "text": sprintf("Error: %s: %s", ex.err, ex.desc)},),
                });
            } catch () {
                # Ignore if state transition fails
            }
        }
    }

    private hash<auto> tasksGet(hash<auto> cx, *hash<auto> params) {
        if (!params.id) {
            return makeJsonRpcError(A2A_ERR_INVALID_ARGUMENT, "Missing 'id' parameter");
        }

        string version = cx.a2a_version ?? a2aDetectVersionFromMethod(cx.method);

        AutoReadLock al(rwl);
        *hash<A2aTaskInfo> task = task_map{params.id};
        if (!task) {
            return makeJsonRpcError(A2A_ERR_NOT_FOUND, sprintf("Task %y not found", params.id));
        }

        return getTaskResponse(task, params.historyLength, version);
    }

    private hash<auto> tasksCancel(hash<auto> cx, *hash<auto> params) {
        if (!params.id) {
            return makeJsonRpcError(A2A_ERR_INVALID_ARGUMENT, "Missing 'id' parameter");
        }

        try {
            return updateTaskState(params.id, A2aTaskState::Canceled);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "A2A-TASK-ERROR") {
                return makeJsonRpcError(A2A_ERR_NOT_FOUND, ex.desc);
            }
            if (ex.err == "A2A-CONFLICT") {
                return makeJsonRpcError(A2A_ERR_CONFLICT, ex.desc);
            }
            rethrow;
        }
    }

    private hash<auto> tasksList(hash<auto> cx, *hash<auto> params) {
        string version = cx.a2a_version ?? a2aDetectVersionFromMethod(cx.method);

        AutoReadLock al(rwl);

        list<hash<auto>> tasks = ();
        foreach string task_id in (keys task_map) {
            hash<A2aTaskInfo> task = task_map{task_id};
            # Filter by contextId if provided
            if (params.contextId && task.contextId != params.contextId) {
                continue;
            }
            tasks += getTaskResponse(task, NOTHING, version);
        }

        return {"tasks": tasks};
    }

    private hash<auto> tasksResubscribe(hash<auto> cx, *hash<auto> params) {
        if (!params.id) {
            return makeJsonRpcError(A2A_ERR_INVALID_ARGUMENT, "Missing 'id' parameter");
        }

        string version = cx.a2a_version ?? a2aDetectVersionFromMethod(cx.method);

        AutoReadLock al(rwl);
        *hash<A2aTaskInfo> task = task_map{params.id};
        if (!task) {
            return makeJsonRpcError(A2A_ERR_NOT_FOUND, sprintf("Task %y not found", params.id));
        }

        return getTaskResponse(task, NOTHING, version);
    }

    private hash<auto> pushNotificationConfigSet(hash<auto> cx, *hash<auto> params) {
        if (!params.id) {
            return makeJsonRpcError(A2A_ERR_INVALID_ARGUMENT, "Missing 'id' parameter");
        }
        if (!params.pushNotificationConfig) {
            return makeJsonRpcError(A2A_ERR_INVALID_ARGUMENT, "Missing 'pushNotificationConfig' parameter");
        }

        AutoWriteLock al(rwl);
        if (!task_map{params.id}) {
            return makeJsonRpcError(A2A_ERR_NOT_FOUND, sprintf("Task %y not found", params.id));
        }
        if (!push_configs{params.id}) {
            push_configs{params.id} = ();
        }
        # record the negotiated version so that webhook deliveries use the body shape and media type that the
        # registering client understands
        hash<auto> config = params.pushNotificationConfig + {
            "id": sprintf("%d", ++push_config_id_counter),
            PushConfigVersionKey: cx.a2a_version ?? a2aDetectVersionFromMethod(cx.method),
        };
        push_configs{params.id} += config;

        return {"id": params.id, "pushNotificationConfigId": config.id};
    }

    private hash<auto> pushNotificationConfigGet(hash<auto> cx, *hash<auto> params) {
        if (!params.id) {
            return makeJsonRpcError(A2A_ERR_INVALID_ARGUMENT, "Missing 'id' parameter");
        }

        AutoReadLock al(rwl);
        if (!task_map{params.id}) {
            return makeJsonRpcError(A2A_ERR_NOT_FOUND, sprintf("Task %y not found", params.id));
        }

        return {
            "id": params.id,
            "pushNotificationConfig": (push_configs{params.id}[0] ?? {}) - PushConfigVersionKey,
        };
    }

    private hash<auto> pushNotificationConfigList(hash<auto> cx, *hash<auto> params) {
        if (!params.id) {
            return makeJsonRpcError(A2A_ERR_INVALID_ARGUMENT, "Missing 'id' parameter");
        }

        AutoReadLock al(rwl);
        if (!task_map{params.id}) {
            return makeJsonRpcError(A2A_ERR_NOT_FOUND, sprintf("Task %y not found", params.id));
        }

        return {
            "id": params.id,
            "pushNotificationConfigs": (map $1 - PushConfigVersionKey, push_configs{params.id} ?? ()),
        };
    }

    private hash<auto> pushNotificationConfigDelete(hash<auto> cx, *hash<auto> params) {
        if (!params.id) {
            return makeJsonRpcError(A2A_ERR_INVALID_ARGUMENT, "Missing 'id' parameter");
        }
        if (!params.pushNotificationConfigId) {
            return makeJsonRpcError(A2A_ERR_INVALID_ARGUMENT,
                "Missing 'pushNotificationConfigId' parameter");
        }

        AutoWriteLock al(rwl);
        if (!task_map{params.id}) {
            return makeJsonRpcError(A2A_ERR_NOT_FOUND, sprintf("Task %y not found", params.id));
        }
        *list<hash<auto>> configs = push_configs{params.id};
        if (!configs) {
            return makeJsonRpcError(A2A_ERR_NOT_FOUND,
                sprintf("No push notification configs found for task %y", params.id));
        }
        # Filter out the config with the matching ID
        list<hash<auto>> new_configs = (map $1, configs, $1.id != params.pushNotificationConfigId);
        if (new_configs.size() == configs.size()) {
            return makeJsonRpcError(A2A_ERR_NOT_FOUND,
                sprintf("Push notification config %y not found for task %y",
                    params.pushNotificationConfigId, params.id));
        }
        if (new_configs) {
            push_configs{params.id} = new_configs;
        } else {
            remove push_configs{params.id};
        }

        return {"id": params.id};
    }

    private hash<auto> getAuthenticatedExtendedCardMethod(hash<auto> cx, *hash<auto> params) {
        AutoReadLock al(rwl);
        if (extended_agent_card) {
            return extended_agent_card;
        }
        # Return the standard agent card if no extended card is set
        return agent_card;
    }

    #! Handles POST-based SSE streaming via send_callback for incremental delivery
    /** Creates a task, spawns a background thread to run the stream handler, and returns
        an HTTP response with a \c send_callback closure that yields SSE events as they
        are produced.  This approach works through HTTP proxy infrastructure (remote
        services) and over both HTTP/1.1 (chunked transfer) and HTTP/2 (DATA frames).
    */
    private hash<HttpResponseInfo> handleStreamPostCallback(hash<auto> jsonrpc, string version) {
        hash<auto> params = jsonrpc.params;

        # Normalize v1.0 params to v0.3 internal format
        if (version == A2aVersionV10) {
            params = a2aTranslateSendMessageParamsToV03(params);
        }
        params = normalizeSendParamsContext(params);

        # Create task
        hash<A2aTaskInfo> task = createTask(params.contextId, params.metadata, params.sessionId);
        string task_id = task.id;

        # Build handler message with context history
        hash<auto> handler_message = params.message;
        if (params.contextId) {
            list<hash<auto>> prior_history;
            {
                AutoReadLock al(rwl);
                prior_history = context_history{params.contextId} ?? ();
            }
            if (prior_history) {
                handler_message.history = prior_history;
            }
        }

        # Add incoming message to task history
        {
            AutoWriteLock al(rwl);
            task_map{task_id}.history += params.message;
        }

        # Transition to active
        updateTaskState(task_id, A2aTaskState::Active);

        # Queue for producer-consumer: background thread pushes SSE event strings,
        # send_callback pops them.  False sentinel signals end of stream.
        Queue queue();

        # Spawn background thread that runs the stream handler and pushes SSE events
        background streamMessageCallbackBackground(task_id, task, handler_message,
            stream_message_handler, params.contextId, params.message, queue, version);

        # Return send_callback that yields SSE events from the queue.
        # Use Content-Type with charset parameter to avoid triggering HttpServer's SSE
        # dedicated socket mode (which checks for exact MimeTypeSse match at the end of
        # handleRequest), while still being recognized as SSE by clients (regex match).
        return <HttpResponseInfo>{
            "code": 200,
            "hdr": {"Content-Type": MimeTypeSse + "; charset=utf-8", "Cache-Control": "no-cache"},
            "send_callback": auto sub () {
                auto val = queue.get();
                if (val.typeCode() != NT_STRING) {
                    return NOTHING;
                }
                return val;
            },
        };
    }

    #! Background thread for send_callback-based SSE streaming
    private streamMessageCallbackBackground(string task_id, hash<A2aTaskInfo> task,
            hash<auto> handler_message, code handler, *string context_id,
            hash<auto> user_message, Queue queue, string version) {
        list<hash<auto>> assembled_parts = ();

        code token_callback = sub (hash<auto> partial) {
            if (partial.parts) {
                assembled_parts += partial.parts;
            }
            # Push SSE event to queue for send_callback to deliver
            queue.push(formatSseEvent(task_id, "TaskMessageUpdateEvent", partial, version));
            # Also notify GET-based SSE connections and push notifications
            notifySseConnections(task_id, "TaskMessageUpdateEvent", partial);
            sendTaskPushNotification(task_id, "TaskMessageUpdateEvent", partial);
        };

        try {
            handler(task, handler_message, token_callback);

            # Assemble final response from collected parts
            hash<auto> response = {
                "role": "agent",
                "parts": assembled_parts,
            };

            # Add response to task history
            {
                AutoWriteLock al(rwl);
                task_map{task_id}.history += response;
            }

            # Accumulate context history
            if (context_id) {
                AutoWriteLock al(rwl);
                if (!context_history{context_id}) {
                    context_history{context_id} = ();
                }
                context_history{context_id} += user_message;
                context_history{context_id} += response;
                if (context_history{context_id}.size() > max_context_messages) {
                    int excess = context_history{context_id}.size() - max_context_messages;
                    splice context_history{context_id}, 0, excess;
                }
            }

            updateTaskState(task_id, A2aTaskState::Completed, response);

            # Push final status event
            hash<auto> final_status;
            {
                AutoReadLock al(rwl);
                final_status = getTaskResponse(task_map{task_id}, NOTHING, version).status;
            }
            queue.push(formatSseEvent(task_id, "TaskStatusUpdateEvent", final_status, version));
        } catch (hash<ExceptionInfo> ex) {
            error("Stream handler error: %s: %s", ex.err, ex.desc);
            try {
                updateTaskState(task_id, A2aTaskState::Failed, {
                    "role": "agent",
                    "parts": ({"type": "text", "text": sprintf("Error: %s: %s", ex.err, ex.desc)},),
                });
            } catch () {}
            hash<auto> fail_status;
            {
                AutoReadLock al(rwl);
                fail_status = getTaskResponse(task_map{task_id}, NOTHING, version).status;
            }
            try {
                queue.push(formatSseEvent(task_id, "TaskStatusUpdateEvent", fail_status, version));
            } catch () {}
        }

        # Signal end of stream
        queue.push(False);
    }

    #! Handles streaming POST fallback when no stream handler is registered
    private hash<HttpResponseInfo> handleStreamPostSync(hash<auto> cx, hash<auto> params,
            string version) {
        # Use synchronous message handler
        hash<A2aTaskInfo> task = createTask(params.contextId, params.metadata, params.sessionId);
        string task_id = task.id;

        {
            AutoWriteLock al(rwl);
            task_map{task_id}.history += params.message;
        }
        updateTaskState(task_id, A2aTaskState::Active);

        *hash<auto> response;
        if (message_handler) {
            try {
                response = message_handler(task, params.message);
                if (response) {
                    {
                        AutoWriteLock al(rwl);
                        task_map{task_id}.history += response;
                    }
                    updateTaskState(task_id, A2aTaskState::Completed, response);
                }
            } catch (hash<ExceptionInfo> ex) {
                error("Sync handler error in stream POST: %s: %s", ex.err, ex.desc);
                try {
                    updateTaskState(task_id, A2aTaskState::Failed, {
                        "role": "agent",
                        "parts": ({"type": "text",
                            "text": sprintf("Error: %s: %s", ex.err, ex.desc)},),
                    });
                } catch () {}
            }
        }

        hash<auto> final_status;
        {
            AutoReadLock al(rwl);
            final_status = getTaskResponse(task_map{task_id}, NOTHING, version).status;
        }
        string body = formatSseEvent(task_id, "TaskStatusUpdateEvent", final_status, version);

        return <HttpResponseInfo>{
            "code": 200,
            "body": body,
            "hdr": {"Content-Type": MimeTypeSse, "Cache-Control": "no-cache"},
        };
    }

    #! Formats a single SSE event string based on protocol version
    private string formatSseEvent(string task_id, string event_type, hash<auto> data,
            string version) {
        if (version == A2aVersionV10) {
            hash<auto> v10_event = a2aTranslateSseEventToV10(event_type, data + {"id": task_id});
            return sprintf("data: %s\n\n", make_json(v10_event));
        } else {
            hash<auto> payload = {
                "jsonrpc": "2.0",
                "method": event_type,
                "params": data + {"id": task_id},
            };
            return sprintf("event: %s\ndata: %s\n\n", event_type, make_json(payload));
        }
    }

    #! Returns a task response hash suitable for JSON-RPC response
    /** @param task the task info
        @param history_length optional limit on number of history messages to include
        @param version A2A protocol version for wire format (\c "0.3" or \c "1.0"); default v0.3
    */
    private hash<auto> getTaskResponse(hash<A2aTaskInfo> task, *int history_length,
            *string version) {
        string ts = format_date("YYYY-MM-DDTHH:mm:SS.xxZ", task.lastUpdatedAt);

        hash<auto!> status = ({
            "state": task.state,
            "timestamp": ts,
        });
        if (task.statusMessage) {
            status.message = parse_json(task.statusMessage);
        }

        hash<auto!> response = ({
            "id": task.id,
            "status": status,
        });
        if (task.contextId) {
            response.contextId = task.contextId;
        }

        # Include history with optional length limit
        if (task.history) {
            if (history_length > 0 && task.history.size() > history_length) {
                response.history = task.history[(task.history.size() - history_length)..];
            } else {
                response.history = task.history;
            }
        }

        if (task.artifacts) {
            response.artifacts = task.artifacts;
        }
        if (task.metadata) {
            response.metadata = task.metadata;
        }

        # Translate to v1.0 wire format if needed
        if (version == A2aVersionV10) {
            response = a2aTranslateTaskToV10(response);
        }

        return response;
    }

    #! Makes a JSON-RPC error response
    private hash<auto> makeJsonRpcError(int code, string message) {
        throw "JSONRPC-ERROR", message, code;
    }

    #! Lifts the context ID from the message onto the params for internal use
    /** Neither the v0.3 \c MessageSendParams object nor the v1.0 \c SendMessageRequest defines a top-level
        context ID; both carry it on the \c Message.  The rest of the handler works with a params-level
        \c contextId, so it is derived here.  A params-level value sent by a lenient client is honored when the
        message does not carry one.

        @param params the send-message parameters

        @return the parameters with \c contextId set from the message when either carries one

        @since A2aServerHandler 1.0
    */
    private hash<auto> normalizeSendParamsContext(hash<auto> params) {
        *string ctx = params.message.contextId ?? params.contextId;
        if (!ctx.val()) {
            return params;
        }
        return params + {"contextId": ctx};
    }

    #! Registers a protocol extension that this agent supports
    /** Registering an extension both advertises it in the agent card's \c capabilities.extensions array and
        makes it eligible for activation when a client requests it with the \c A2A-Extensions service
        parameter.

        @param ext the extension to support

        @throw A2A-EXTENSION-ERROR if an extension with the same URI is already registered

        @note An agent that registers a \c required extension rejects any request that does not opt into it.

        @since A2aServerHandler 1.0
    */
    registerExtension(hash<A2aAgentExtension> ext) {
        AutoWriteLock al(rwl);
        if (extension_map{ext.uri}) {
            throw "A2A-EXTENSION-ERROR", sprintf("extension %y is already registered", ext.uri);
        }
        extension_map{ext.uri} = ext;
        agent_card.capabilities.extensions = map extension_map{$1}, keys extension_map;
    }

    #! Returns the extensions this agent supports, keyed by URI
    /** @since A2aServerHandler 1.0
    */
    hash<string, hash<A2aAgentExtension>> getSupportedExtensions() {
        AutoReadLock al(rwl);
        return extension_map ?? {};
    }

    #! Determines which requested extensions are activated for a request
    /** @param hdr the request headers

        @return the activated extension URIs, in the order the client requested them

        @since A2aServerHandler 1.0
    */
    private list<string> negotiateExtensions(hash<auto> hdr) {
        *string value;
        foreach string key in (keys hdr) {
            if (key.lwr() == A2aExtensionsHeader.lwr()) {
                value = hdr{key};
                break;
            }
        }
        list<string> requested = a2aParseExtensionsHeader(value);
        if (!requested) {
            return ();
        }
        AutoReadLock al(rwl);
        # an extension the agent does not implement is ignored rather than rejected
        # note: map over an empty source yields NOTHING, so coerce to an empty list
        return (map $1, requested, exists extension_map{$1}) ?? ();
    }

    #! Returns the URIs of registered extensions marked as required that the client did not request
    /** @param cx the request context

        @return the missing required extension URIs

        @since A2aServerHandler 1.0
    */
    private list<string> getMissingRequiredExtensions(hash<auto> cx) {
        hash<string, bool> active = map {$1: True}, cx.a2a_extensions ?? ();
        AutoReadLock al(rwl);
        return (map $1, keys extension_map, extension_map{$1}.required && !active{$1}) ?? ();
    }

    #! Returns the response headers echoing the extensions activated for a request
    /** @param cx the request context

        @return a hash with the \c A2A-Extensions service parameter, or an empty hash when no extension was
        activated so that the header is omitted entirely

        @since A2aServerHandler 1.0
    */
    private hash<auto> getExtensionResponseHeaders(hash<auto> cx) {
        *string value = a2aMakeExtensionsHeader(cx.a2a_extensions);
        return value ? {A2aExtensionsHeader: value} : {};
    }

    #! Sends push notifications for a task event
    private sendTaskPushNotification(string task_id, string event_type, hash<auto> event_data) {
        *list<hash<auto>> configs;
        {
            AutoReadLock al(rwl);
            configs = push_configs{task_id};
        }

        if (!configs) {
            return;
        }

        # the body depends only on the protocol version, so build it at most once per version rather than
        # once per config; the v0.3 body requires a read lock to render the current task
        hash<string, string> body_cache;

        foreach hash<auto> config in (configs) {
            try {
                string version = config{PushConfigVersionKey} ?? A2aVersionV03;
                if (!exists body_cache{version}) {
                    body_cache{version} =
                        make_json(makePushNotificationPayload(task_id, event_type, event_data, version));
                }
                # Deliver push notification via HTTP POST
                Qore::HTTPClient push_client({"url": config.url, "timeout": PushNotificationTimeoutMs});
                hash<auto> hdr = {
                    "Content-Type": version == A2aVersionV10 ? MimeTypeA2aJson : MimeTypeJson,
                };
                # the client's opaque notification token is echoed back in a dedicated header
                if (config.token.val()) {
                    hdr{A2aNotificationTokenHeader} = config.token;
                }
                push_client.send(body_cache{version}, "POST", NOTHING, hdr);
            } catch (hash<ExceptionInfo> ex) {
                error("Push notification delivery failed for task %y to %y: %s: %s",
                    task_id, config.url, ex.err, ex.desc);
            }
        }
    }

    #! Builds the webhook body for a push notification
    /** The A2A specification defines the webhook body independently of the JSON-RPC binding:
        - v0.3 posts the bare @ref A2aClient::A2aTaskInfo "Task" object
        - v1.0 posts a \c StreamResponse object, a single-key hash discriminated by \c task, \c message,
          \c statusUpdate, or \c artifactUpdate

        Neither version wraps the body in a JSON-RPC envelope.

        @param task_id the task the event belongs to
        @param event_type the v0.3 event type name (ex: \c "TaskStatusUpdateEvent")
        @param event_data the event payload in v0.3 form
        @param version the A2A protocol version negotiated when the push config was registered

        @return the webhook body to serialize

        @since A2aServerHandler 1.0
    */
    private hash<auto> makePushNotificationPayload(string task_id, string event_type, hash<auto> event_data,
            string version) {
        if (version == A2aVersionV10) {
            # a2aTranslateSseEventToV10() already returns the single-key StreamResponse hash and renames
            # "id" to "taskId"
            return a2aTranslateSseEventToV10(event_type, event_data + {"id": task_id});
        }
        # v0.3: post the current Task object
        AutoReadLock al(rwl);
        *hash<A2aTaskInfo> task = task_map{task_id};
        if (task) {
            return getTaskResponse(task, NOTHING, A2aVersionV03) + {"kind": "task"};
        }
        # the task disappeared between the event and delivery; fall back to the event payload
        return event_data + {"id": task_id};
    }

    #! Sends an event to SSE connections belonging to the task's session
    private notifySseConnections(string task_id, string event_type, hash<auto> event_data) {
        list<A2aServerConnection> conns;
        {
            AutoReadLock al(rwl);
            *string session_id = task_map{task_id}.sessionId;
            if (session_id && smap{session_id}) {
                # Only notify SSE connections in the task's session
                foreach A2aServerConnection conn in (smap{session_id}.ssemap.iterator()) {
                    conns += conn;
                }
            } else {
                # No session ID on task: broadcast to all sessions
                foreach string sid in (keys smap) {
                    foreach A2aServerConnection conn in (smap{sid}.ssemap.iterator()) {
                        conns += conn;
                    }
                }
            }
        }

        # Each connection formats the event based on its own protocol version
        foreach A2aServerConnection conn in (conns) {
            try {
                conn.sendEvent(task_id, event_type, event_data);
            } catch (hash<ExceptionInfo> ex) {
                error("SSE notification failed: %s: %s", ex.err, ex.desc);
            }
        }
    }

    #! Generates a unique task ID
    private string generateTaskId() {
        AutoWriteLock al(rwl);
        return sprintf("task-%d-%d", getpid(), ++task_id_counter);
    }

    #! Returns the next request ID
    string getNextRequestId() {
        AutoWriteLock al(rwl);
        return "s" + rpc_rid++;
    }

    int getNextIdForSession(string session) {
        AutoWriteLock al(rwl);
        return ++sidmap{session};
    }

    string getSessionId(hash<auto> hdr) {
        string session;
%ifdef NoUuid
        session = get_random_string(50);
%else
        session = UUID::get();
%endif
        AutoWriteLock al(rwl);
        smap{session} = <A2aSessionInfo>{
            "session": session,
            "ssemap": {},
            "last_activity": now_us(),
        };
        return session;
    }

    #! Called when an SSE connection is established
    ServerSentEventConnection getConnectionImpl(Socket sock, hash<auto> cx, hash<auto> hdr, string cid) {
        return new A2aServerConnection(self, sock, cx, hdr, cid);
    }

    private static bool clientAcceptsSse(hash<auto> hdr) {
        *string astr = hdr.accept;
        if (!astr.val()) {
            return False;
        }

        do {
            astr =~ s/;[^,]*//;
        } while (astr.rfind(";") != -1);
        astr =~ s/ +//g;
        hash<auto> aih;
        map aih.$1 = True, astr.split(",");
        return aih{MimeTypeSse} || aih."*/*";
    }

    #! Called while holding the write lock to register the new connection
    private registerNewConnectionIntern(string cid, ServerSentEventConnection conn) {
        ServerSentEventHandler::registerNewConnectionIntern(cid, conn);
        A2aServerConnection a2a_conn = cast<A2aServerConnection>(conn);
        smap{a2a_conn.getSession()}.ssemap{conn.uniqueHash()} = a2a_conn;
    }

    #! Sets up JSON-RPC method dispatch for both v0.3 and v1.0 method names
    private setupMethods() {
        # Helper to create a method entry
        auto make_method = hash<auto> sub (string name, string text,
                code<hash<auto> (hash<auto>, *hash<auto>)> func, string help) {
            return {
                "name": name,
                "text": text,
                "function": func,
                "help": help,
                "include_context": True,
            };
        };

        # Register both v0.3 and v1.0 method names pointing to the same handlers
        addMethods((
            # message/send (v0.3) / SendMessage (v1.0)
            make_method("^message/send$", "message/send", \messageSend(),
                "A2A send message to agent"),
            make_method("^SendMessage$", "SendMessage", \messageSend(),
                "A2A v1.0 send message to agent"),

            # message/stream (v0.3) / SendStreamingMessage (v1.0)
            make_method("^message/stream$", "message/stream", \messageStream(),
                "A2A send message with streaming response"),
            make_method("^SendStreamingMessage$", "SendStreamingMessage", \messageStream(),
                "A2A v1.0 send streaming message"),

            # tasks/get (v0.3) / GetTask (v1.0)
            make_method("^tasks/get$", "tasks/get", \tasksGet(),
                "A2A get task status"),
            make_method("^GetTask$", "GetTask", \tasksGet(),
                "A2A v1.0 get task"),

            # tasks/cancel (v0.3) / CancelTask (v1.0)
            make_method("^tasks/cancel$", "tasks/cancel", \tasksCancel(),
                "A2A cancel a task"),
            make_method("^CancelTask$", "CancelTask", \tasksCancel(),
                "A2A v1.0 cancel task"),

            # tasks/list (v0.3) / ListTasks (v1.0)
            make_method("^tasks/list$", "tasks/list", \tasksList(),
                "A2A list tasks"),
            make_method("^ListTasks$", "ListTasks", \tasksList(),
                "A2A v1.0 list tasks"),

            # tasks/resubscribe (v0.3) / SubscribeToTask (v1.0)
            make_method("^tasks/resubscribe$", "tasks/resubscribe", \tasksResubscribe(),
                "A2A resubscribe to task events"),
            make_method("^SubscribeToTask$", "SubscribeToTask", \tasksResubscribe(),
                "A2A v1.0 subscribe to task"),

            # tasks/pushNotificationConfig/set (v0.3) / CreateTaskPushNotificationConfig (v1.0)
            make_method("^tasks/pushNotificationConfig/set$", "tasks/pushNotificationConfig/set",
                \pushNotificationConfigSet(), "A2A set push notification config"),
            make_method("^CreateTaskPushNotificationConfig$", "CreateTaskPushNotificationConfig",
                \pushNotificationConfigSet(), "A2A v1.0 create push notification config"),

            # tasks/pushNotificationConfig/get (v0.3) / GetTaskPushNotificationConfig (v1.0)
            make_method("^tasks/pushNotificationConfig/get$", "tasks/pushNotificationConfig/get",
                \pushNotificationConfigGet(), "A2A get push notification config"),
            make_method("^GetTaskPushNotificationConfig$", "GetTaskPushNotificationConfig",
                \pushNotificationConfigGet(), "A2A v1.0 get push notification config"),

            # tasks/pushNotificationConfig/list (v0.3) / ListTaskPushNotificationConfigs (v1.0)
            make_method("^tasks/pushNotificationConfig/list$", "tasks/pushNotificationConfig/list",
                \pushNotificationConfigList(), "A2A list push notification configs"),
            make_method("^ListTaskPushNotificationConfigs$", "ListTaskPushNotificationConfigs",
                \pushNotificationConfigList(), "A2A v1.0 list push notification configs"),

            # tasks/pushNotificationConfig/delete (v0.3) / DeleteTaskPushNotificationConfig (v1.0)
            make_method("^tasks/pushNotificationConfig/delete$", "tasks/pushNotificationConfig/delete",
                \pushNotificationConfigDelete(), "A2A delete push notification config"),
            make_method("^DeleteTaskPushNotificationConfig$", "DeleteTaskPushNotificationConfig",
                \pushNotificationConfigDelete(), "A2A v1.0 delete push notification config"),

            # agent/getAuthenticatedExtendedCard (v0.3) / GetExtendedAgentCard (v1.0)
            make_method("^agent/getAuthenticatedExtendedCard$", "agent/getAuthenticatedExtendedCard",
                \getAuthenticatedExtendedCardMethod(), "A2A get extended agent card"),
            make_method("^GetExtendedAgentCard$", "GetExtendedAgentCard",
                \getAuthenticatedExtendedCardMethod(), "A2A v1.0 get extended agent card"),
        ));
    }
}

} # namespace A2aServerHandler

namespace Priv {
hashdecl A2aSessionInfo {
    # Session ID
    string session;

    # Active SSE connections
    hash<string, A2aServerConnection> ssemap;

    # Last activity timestamp
    date last_activity;

    # Task IDs associated with this session
    *list<string> task_ids;
}

}
