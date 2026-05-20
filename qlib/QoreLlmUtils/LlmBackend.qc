# -*- mode: qore; indent-tabs-mode: nil -*-

/*  LlmBackend.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! @see LlmBackend
public namespace QoreLlmUtils {

#! Configuration for the LLM backend
/** Specifies how to connect to the LLM — via an OpenAI-compatible HTTP
    endpoint, the Anthropic Messages API, the Google Gemini API, or a custom
    callback (e.g., for routing through an API gateway).
*/
public hashdecl LlmBackendConfig {
    #! Mode: "http" for OpenAI-compatible, "anthropic" for Anthropic Messages API,
    #! "sglang" for SGLang servers (OpenAI-compatible + local defaults),
    #! "gemini" for Google Gemini generateContent,
    #! "callback" for a custom API call function
    string mode;
    #! Base URL of the LLM service (required for "http", "anthropic", and "sglang" modes; optional for "gemini")
    *string url;
    #! Model name to pass in the request
    *string model;
    #! API key for authentication — this is the secret key from your LLM provider (e.g., from the OpenAI or Anthropic dashboard)
    *string api_key;
    #! Maximum tokens to generate; default 4096
    int max_tokens = 4096;
    #! Sampling temperature; default 0.0 (deterministic)
    float temperature = 0.0;
    #! HTTP connect timeout in milliseconds; default 10000
    int connect_timeout_ms = 10000;
    #! HTTP request timeout in milliseconds; default 120000
    int timeout_ms = 120000;
    #! Anthropic API version header; default "2023-06-01"
    string anthropic_version = "2023-06-01";
    #! Maximum retry attempts for transient errors (429, 5xx); default 3;
    #! set to 0 to disable retries
    int max_retries = 3;
    #! Callback mode: a callable that performs the API call and returns
    #! the response hash in OpenAI-compatible format
    *code<hash<auto>(string, string, hash<auto>)> api_callback;
    #! Callback mode method hint passed to @ref api_callback; default "POST"
    string callback_method = "POST";
    #! Callback mode endpoint/path hint passed to @ref api_callback; default "chat/completions"
    string callback_path = "chat/completions";
    #! Initial retry delay in milliseconds for exponential backoff; default 1000
    int retry_delay_ms = 1000;
}

#! Per-call request overrides for LLM calls
/** Lets callers keep a long-lived backend for one provider/connection while
    selecting a different model for individual tasks.  This is important for
    applications that intentionally mix model tiers (for example: a low-cost
    summarization pass, a stronger planning pass, and a premium review pass)
    without rebuilding the backend object.
*/
public hashdecl LlmRequestOptions {
    #! Model name for this call; overrides @ref LlmBackendConfig::model
    *string model;
    #! Maximum tokens for this call; overrides @ref LlmBackendConfig::max_tokens
    *int max_tokens;
    #! Sampling temperature for this call; overrides @ref LlmBackendConfig::temperature
    *float temperature;
    #! JSON schema to request structured output when the backend supports it
    *hash<auto> output_schema;
    #! Provider-specific request body fields merged into the generated body
    *hash<auto> body_options;
}

#! Result from an LLM completion call
public hashdecl LlmCompletionResult {
    #! The generated text content
    string content;
    #! Token usage information (provider-specific)
    hash<auto> usage;
}

#! A single tool call returned by the LLM
public hashdecl ChatToolCall {
    #! Unique ID for this tool call (from the LLM response)
    string id;
    #! Function/tool name to call
    string name;
    #! Parsed arguments hash to pass to the tool
    *hash<auto> arguments;
}

#! A content part in a multimodal message (text or image)
public hashdecl ContentPart {
    #! Part type: "text" for text content, "image" for image data
    string type;
    #! Text content (when type is "text")
    *string text;
    #! Image binary data (when type is "image")
    *binary data;
    #! Image MIME type (when type is "image"), e.g., "image/png", "image/jpeg"
    *string mime_type;
}

#! A message in a multi-turn conversation
public hashdecl ChatMessage {
    #! Message role: "system", "user", "assistant", or "tool"
    string role;
    #! Text content (may be empty for assistant messages that only contain tool_calls)
    *string content;
    #! Multimodal content parts (images + text); when set, takes precedence
    #! over \c content for the wire format sent to the LLM
    *list<hash<ContentPart>> content_parts;
    #! Tool calls requested by the assistant (present when role is "assistant")
    *list<hash<ChatToolCall>> tool_calls;
    #! ID of the tool call this message is responding to (when role is "tool")
    *string tool_call_id;
    #! Tool name (when role is "tool")
    *string name;
}

#! Definition of a tool the LLM can call
public hashdecl ToolDefinition {
    #! Tool name (must match ^[a-zA-Z0-9_-]{1,64}$)
    string name;
    #! Human-readable description of what the tool does
    *string description;
    #! JSON Schema describing the tool's parameters
    *hash<auto> parameters;
}

#! Response from a multi-turn chat call
public hashdecl ChatResponse {
    #! Provider response ID when available
    *string id;
    #! Model used by the provider when available
    *string model;
    #! Why the LLM stopped: "end_turn" (normal), "tool_use" (wants tool calls),
    #! "max_tokens" (hit limit), or "stop_sequence"
    string stop_reason;
    #! Text content (if any)
    *string content;
    #! Tool calls the LLM wants executed (when stop_reason is "tool_use")
    *list<hash<ChatToolCall>> tool_calls;
    #! Token usage information
    hash<auto> usage;
    #! Raw provider response when callers need provider-specific metadata
    *hash<auto> raw;
}

#! A streaming event from a chat completion
public hashdecl ChatStreamEvent {
    #! Event type from the SSE stream (e.g., "content_block_delta", "message_delta")
    *string event;
    #! Text delta (incremental text content in this chunk)
    *string delta;
    #! True when this is the final event (stream complete)
    bool done = False;
    #! Raw event data (the full parsed JSON from the SSE data field)
    *hash<auto> raw;
}

#! Pluggable LLM backend for chat completions
/** Provides a unified interface for making LLM chat completion calls across
    multiple providers.  Supports four backends:

    - \b http: OpenAI-compatible \c /v1/chat/completions with optional
      \c response_format for JSON schema enforcement.
    - \b anthropic: Anthropic Messages API (\c /v1/messages) with tool-use-based
      structured extraction when an output schema is provided.
    - \b gemini: Google Gemini \c generateContent with native function calling
      and structured JSON responses.
    - \b callback: Custom API dispatch via a user-provided callable.

    Thread-safe: the underlying REST client is lazily initialized behind a mutex.


    @par Example: Create an HTTP backend and get a completion
    @code{.py}
%modern
%requires QoreLlmUtils

LlmBackend llm(<LlmBackendConfig>{
    "mode": "http",
    "url": "http://localhost:8080",
    "model": "gpt-4o",
});
hash<LlmCompletionResult> result = llm.complete(
    "You are a helpful assistant.",
    "What are the main benefits of cloud computing?",
);
printf("Response: %s\n", result.content);
    @endcode
*/
public class LlmBackend {
    public {
        #! Map of mode names to the module that supplies the matching provider
        /** Lazy-loaded by @ref getFactory() when a mode is requested but no
            factory has been registered yet.  Modes whose factory ships with
            @ref QoreLlmUtils itself (e.g. \c "callback") are absent from this
            map and are registered when @ref QoreLlmUtils' \c init block runs.
        */
        const ProviderModuleMap = {
            "http":      "OpenAiRestClient",
            "anthropic": "AnthropicRestClient",
            "sglang":    "SGLangRestClient",
            "gemini":    "GeminiRestClient",
        };
    }

    private {
        #! The backend mode
        string mode;
        #! Base URL
        *string url;
        #! Model name
        *string model_name;
        #! API key
        *string api_key;
        #! Max tokens to generate
        int max_tokens;
        #! Sampling temperature
        float temperature;
        #! Connect timeout
        int connect_timeout_ms;
        #! Request timeout
        int timeout_ms;
        #! Anthropic API version
        string anthropic_version;
        #! Retry config
        int max_retries;
        int retry_delay_ms;
        #! Callback for custom API dispatch
        *code<hash<auto>(string, string, hash<auto>)> api_callback;
        #! Method hint passed to callback dispatchers
        string callback_method;
        #! Endpoint/path hint passed to callback dispatchers
        string callback_path;
        #! REST client for HTTP/Anthropic modes (lazy-initialized)
        *RestClientIo::RestClientIo rest_client;
        #! Mutex for thread-safe lazy initialization
        Mutex rest_mutex();
    }

    private {
        #! Provider factories registered by mode string
        static hash<string, AbstractLlmProviderFactory> factory_cache;
        #! Tracks the module that registered each factory (by class name)
        static hash<string, string> factory_module_map;
        #! Serializes access to the factory cache
        static Mutex factory_cache_lock();
    }

    #! Registers a provider factory under its mode string
    /** Concrete factory classes call this from their owning module's \c init
        block.  Subsequent @ref LlmBackend constructions with the matching
        @ref LlmBackendConfig::mode dispatch through the registered factory.

        @param factory the new provider factory

        @throw LLM-CONFIG-ERROR a factory for the same mode is already registered
    */
    static registerFactory(AbstractLlmProviderFactory factory) {
        factory_cache_lock.lock();
        on_exit factory_cache_lock.unlock();

        string mode = factory.getMode();
        if (factory_cache{mode}) {
            throw "LLM-CONFIG-ERROR",
                sprintf("LLM provider factory for mode %y has already been "
                    "registered", mode);
        }
        *string module_name = Class::getClass(factory).getModuleName();
        if (exists module_name) {
            factory_module_map{Class::getClass(factory).getName()} = module_name;
        }
        factory_cache{mode} = factory;
    }

    #! Removes a previously registered provider factory
    /** Intended primarily for tests that register a fake provider; safe to
        call when no factory is registered for the given mode (no-op).

        @param mode the mode string previously registered via @ref registerFactory()
    */
    static deregisterFactory(string mode) {
        factory_cache_lock.lock();
        on_exit factory_cache_lock.unlock();

        *AbstractLlmProviderFactory factory = remove factory_cache{mode};
        if (!factory) {
            return;
        }
        try {
            remove factory_module_map{Class::getClass(factory).getName()};
        } catch (hash<ExceptionInfo> ex) {
            # Class metadata may be unavailable during program teardown
            if (ex.err != "PROGRAM-ERROR") {
                rethrow;
            }
        }
    }

    #! Returns the factory for the given mode, lazy-loading its module if needed
    /** When no factory is registered for \a mode, this consults
        @ref ProviderModuleMap and attempts to load the named module.  The
        module's \c init block is expected to call @ref registerFactory()
        for the corresponding mode.

        @param mode the @ref LlmBackendConfig::mode value to resolve

        @return the registered factory, or @ref nothing if the mode is
        unknown and no module can be loaded for it
    */
    static *AbstractLlmProviderFactory getFactory(string mode) {
        if (factory_cache{mode}) {
            return factory_cache{mode};
        }
        *string module_name = ProviderModuleMap{mode};
        if (!module_name) {
            return NOTHING;
        }
        try {
            load_module(module_name);
        } catch (hash<ExceptionInfo> ex) {
            throw "LLM-CONFIG-ERROR",
                sprintf("cannot load provider module %y for mode %y: %s",
                    module_name, mode, ex.desc), ex;
        }
        return factory_cache{mode};
    }

    #! Returns the factory for the given mode or throws an exception
    /** @param mode the @ref LlmBackendConfig::mode value to resolve

        @return the registered factory

        @throw LLM-CONFIG-ERROR the mode is unknown; the message lists the
        currently registered modes and the @ref ProviderModuleMap keys
    */
    static AbstractLlmProviderFactory getFactoryEx(string mode) {
        *AbstractLlmProviderFactory factory = LlmBackend::getFactory(mode);
        if (!factory) {
            throw "LLM-CONFIG-ERROR",
                sprintf("LLM provider mode %y is unknown; registered modes: "
                    "%y; modes available via lazy module load: %y",
                    mode, keys factory_cache, keys ProviderModuleMap);
        }
        return factory;
    }

    #! Returns the list of currently-registered mode strings
    /** @return the modes whose factories are present in the cache; modes
        that are known via @ref ProviderModuleMap but whose modules have
        not yet been lazy-loaded are not included
    */
    static list<string> knownProviderModes() {
        return keys factory_cache;
    }

    #! Creates an LlmBackend with the given configuration
    /** @param config an @ref LlmBackendConfig hash specifying the backend and parameters

        @throw LLM-CONFIG-ERROR if the mode is invalid or required parameters are missing
        @throw LLM-DEPENDENCY-ERROR if required modules are not available
    */
    constructor(hash<LlmBackendConfig> config) {
        mode = config.mode;
        if (mode != "http" && mode != "anthropic" && mode != "sglang"
                && mode != "gemini" && mode != "callback") {
            throw "LLM-CONFIG-ERROR",
                sprintf("invalid mode %y; expected \"http\", \"anthropic\", \"sglang\", "
                    "\"gemini\", or \"callback\"", mode);
        }

        if (mode == "http" || mode == "anthropic" || mode == "sglang") {
            if (!config.url.val()) {
                throw "LLM-CONFIG-ERROR",
                    sprintf("url is required for %y mode", mode);
            }
%ifdef NoJson
            throw "LLM-DEPENDENCY-ERROR",
                "json module is required for HTTP/Anthropic/SGLang mode but is not available";
%endif
            url = config.url;
        }
        if (mode == "gemini") {
            if (!config.api_key.val()) {
                throw "LLM-CONFIG-ERROR",
                    "api_key is required for \"gemini\" mode";
            }
%ifdef NoJson
            throw "LLM-DEPENDENCY-ERROR",
                "json module is required for Gemini mode but is not available";
%endif
            url = config.url;
        }

        model_name = config.model;
        api_key = config.api_key;
        max_tokens = config.max_tokens;
        temperature = config.temperature;
        connect_timeout_ms = config.connect_timeout_ms;
        timeout_ms = config.timeout_ms;
        anthropic_version = config.anthropic_version;
        max_retries = config.max_retries;
        retry_delay_ms = config.retry_delay_ms;
        api_callback = config.api_callback;
        callback_method = config.callback_method;
        callback_path = config.callback_path;
        if (mode == "callback" && !api_callback) {
            throw "LLM-CONFIG-ERROR",
                "api_callback is required for callback mode";
        }
    }

    #! Makes a chat completion call and returns the result
    /** @param system_prompt the system prompt that defines the LLM's role
        @param user_prompt the user message / input text
        @param output_schema optional JSON schema to enforce structured output;
               for HTTP mode this uses \c response_format, for Anthropic mode
               this uses a tool-use extraction pattern
        @return an @ref LlmCompletionResult hash with the content and usage

        @throw LLM-ERROR if the completion request fails
    */
    hash<LlmCompletionResult> complete(string system_prompt, string user_prompt,
            *hash<auto> output_schema, *hash<LlmRequestOptions> opts) {
        switch (mode) {
            case "http":
            case "sglang":
                return completeHttp(system_prompt, user_prompt, output_schema,
                    opts);
            case "anthropic":
                return completeAnthropic(system_prompt, user_prompt,
                    output_schema, opts);
            case "gemini":
                return completeGemini(system_prompt, user_prompt, output_schema,
                    opts);
            case "callback":
                return completeCallback(system_prompt, user_prompt,
                    output_schema, opts);
        }
        # unreachable — mode validated in constructor
        throw "LLM-ERROR", sprintf("unsupported mode %y", mode);
    }

    /** @return the configured backend mode ("http", "anthropic", "sglang", "gemini", or "callback") */
    string getMode() {
        return mode;
    }

    /** @return the configured model name, or @ref nothing if none was set */
    *string getModel() {
        return model_name;
    }

    #! Multi-turn chat with optional tool calling
    /** Sends a list of messages (including conversation history and tool
        results) to the LLM, optionally with tool definitions that the LLM
        can choose to call.

        Use this method for agent loops where you need the LLM to:
        - Decide which tools to call based on the conversation
        - Receive tool results and reason about next steps
        - Maintain multi-turn conversation state

        @param messages list of @ref ChatMessage hashes forming the conversation
        @param tools optional list of @ref ToolDefinition hashes the LLM can call
        @return a @ref ChatResponse hash with stop_reason, content, and optional tool_calls

        @throw LLM-ERROR if the chat request fails
    */
    hash<ChatResponse> chat(list<hash<ChatMessage>> messages,
            *list<hash<ToolDefinition>> tools, *hash<LlmRequestOptions> opts) {
        switch (mode) {
            case "http":
            case "sglang":
            case "callback":
                return chatHttp(messages, tools, opts);
            case "anthropic":
                return chatAnthropic(messages, tools, opts);
            case "gemini":
                return chatGemini(messages, tools, opts);
        }
        throw "LLM-ERROR", sprintf("unsupported mode %y", mode);
    }

    #! Streams a chat completion via Server-Sent Events (SSE)
    /** Sends a chat request with \c stream=true and returns events as they
        arrive. Each event contains a text delta — the incremental text
        generated so far. Use this for real-time chat interfaces where you
        want to show the response as it's being generated.

        @param messages list of @ref ChatMessage hashes
        @param callback code called for each streaming event with a
            @ref ChatStreamEvent hash; called with \c done=True when complete
        @param tools optional tool definitions

        @throw LLM-ERROR if the request fails
        @throw LLM-DEPENDENCY-ERROR if streaming modules are not available

        @par Example: Stream a response
        @code{.py}
%modern
%requires QoreLlmUtils

LlmBackend llm(<LlmBackendConfig>{"mode": "http", "url": "http://localhost:8080",
    "model": "gpt-4o"});
llm.chatStream(
    (<ChatMessage>{"role": "user", "content": "Tell me a story"},),
    sub (hash<ChatStreamEvent> event) {
        if (event.delta) {
            print(event.delta);
        }
        if (event.done) {
            printf("\n[done]\n");
        }
    },
);
        @endcode
    */
    chatStream(list<hash<ChatMessage>> messages,
            code<nothing(hash<ChatStreamEvent>)> callback,
            *list<hash<ToolDefinition>> tools, *hash<LlmRequestOptions> opts) {
        if (mode == "callback") {
            throw "LLM-ERROR",
                "streaming is not supported in callback mode; use chat() instead";
        }
        if (mode == "gemini") {
            chatStreamGemini(messages, callback, tools, opts);
            return;
        }
        ensureRestClient();

        # Build the request body (same as chat but with stream=true)
        hash<auto> body;
        if (mode == "anthropic") {
            body = buildAnthropicBody(messages, tools, opts);
            body.stream = True;
        } else {
            body = buildHttpBody(messages, tools, opts);
            body.stream = True;
        }

        # Build headers
        hash<auto> hdr = {"Accept": "text/event-stream"};
        if (mode == "anthropic") {
            hdr."anthropic-version" = anthropic_version;
            if (api_key.val()) {
                hdr."x-api-key" = api_key;
            }
        } else {
            if (api_key.val()) {
                hdr."Authorization" = "Bearer " + api_key;
            }
        }

        string endpoint = mode == "anthropic" ? "/v1/messages" : "/v1/chat/completions";

        # Acquire stream and start SSE
        HttpClientIo::HttpClientConnectionManager mgr = rest_client.getConnectionManager();
        HttpClientIo::HttpClientStreamHandle stream = mgr.acquireStream(url);

        # Serialize body
        string json_body = make_json(body);
        hdr."Content-Type" = "application/json";

        stream.startStreaming("POST", endpoint, hdr, False, json_body);

        HttpClientStreamingIo::SseStreamReader reader(stream);

        while (True) {
            *hash<SseMessageInfo> evt = reader.readEvent(timeout_ms);
            if (!evt) {
                callback(<ChatStreamEvent>{"done": True});
                break;
            }
            if (evt.data == "[DONE]") {
                callback(<ChatStreamEvent>{"done": True});
                break;
            }

            # Parse the SSE data as JSON
            *hash<auto> parsed;
            try {
                parsed = parse_json(evt.data);
            } catch () {
                continue;
            }

            # Extract delta text based on provider format
            *string delta;
            if (mode == "anthropic") {
                # Anthropic: {"type": "content_block_delta", "delta": {"text": "..."}}
                if (parsed.type == "content_block_delta" && parsed.delta.text.val()) {
                    delta = parsed.delta.text;
                } else if (parsed.type == "message_stop") {
                    callback(<ChatStreamEvent>{"done": True, "raw": parsed});
                    break;
                }
            } else {
                # OpenAI: {"choices": [{"delta": {"content": "..."}}]}
                if (parsed.choices[0].delta.content.val()) {
                    delta = parsed.choices[0].delta.content;
                }
                if (parsed.choices[0].finish_reason.val()) {
                    callback(<ChatStreamEvent>{
                        "done": True,
                        "delta": delta,
                        "raw": parsed,
                    });
                    break;
                }
            }

            if (delta.val()) {
                callback(<ChatStreamEvent>{
                    "event": evt.event,
                    "delta": delta,
                    "raw": parsed,
                });
            }
        }
    }

    #! OpenAI-compatible chat-completions facade
    /** Accepts an OpenAI \c /v1/chat/completions-style request body, dispatches
        it through this backend's native adapter, and returns an
        OpenAI-compatible response hash.  This lets applications keep one
        external wire contract while using provider-native backends such as
        Gemini internally.

        @param body OpenAI-compatible chat-completions request
        @return OpenAI-compatible chat-completions response

        @throw LLM-ERROR if the backend call fails
    */
    hash<auto> chatCompletions(hash<auto> body) {
        hash<auto> parsed = parseOpenAiChatCompletionsRequest(body);
        list<hash<ChatMessage>> messages =
            cast<list<hash<ChatMessage>>>(parsed.messages);
        *list<hash<ToolDefinition>> tools =
            cast<*list<hash<ToolDefinition>>>(parsed.tools);
        hash<LlmRequestOptions> options =
            cast<hash<LlmRequestOptions>>(parsed.options);
        hash<ChatResponse> response = chat(messages, tools, options);
        return chatResponseToOpenAi(response,
            options.model ?? getRequestModel(NOTHING));
    }

    #! Streaming OpenAI-compatible chat-completions facade
    /** Accepts an OpenAI \c /v1/chat/completions-style request body, streams
        OpenAI-compatible chunks to @a delta_cb, and returns an accumulated
        OpenAI-compatible final response.

        @param body OpenAI-compatible chat-completions request
        @param delta_cb callback receiving OpenAI-compatible stream chunks
        @return accumulated OpenAI-compatible final response

        @throw LLM-ERROR if the backend call fails
    */
    hash<auto> chatCompletionsStream(hash<auto> body,
            code<nothing(hash<auto>)> delta_cb) {
        hash<auto> parsed = parseOpenAiChatCompletionsRequest(body);
        list<hash<ChatMessage>> messages =
            cast<list<hash<ChatMessage>>>(parsed.messages);
        *list<hash<ToolDefinition>> tools =
            cast<*list<hash<ToolDefinition>>>(parsed.tools);
        hash<LlmRequestOptions> options =
            cast<hash<LlmRequestOptions>>(parsed.options);
        hash<auto> state = newOpenAiStreamState(options.model
            ?? getRequestModel(NOTHING));
        chatStream(messages, sub (hash<ChatStreamEvent> event) {
            *hash<auto> chunk = chatStreamEventToOpenAiChunk(\state, event);
            if (chunk) {
                delta_cb(chunk);
            }
        }, tools, options);
        return streamStateToOpenAiResponse(state);
    }

    #! Parses an OpenAI-compatible request for provider-native dispatch
    private hash<auto> parseOpenAiChatCompletionsRequest(hash<auto> body) {
        hash<LlmRequestOptions> options_h = <LlmRequestOptions>{};
        if (body.model.val()) {
            options_h.model = body.model;
        }
        if (exists body.max_completion_tokens) {
            options_h.max_tokens = body.max_completion_tokens;
        } else if (exists body.max_tokens) {
            options_h.max_tokens = body.max_tokens;
        }
        if (exists body.temperature) {
            options_h.temperature = body.temperature;
        }

        hash<auto!> body_options;
        if (body.generationConfig.typeCode() == NT_HASH) {
            body_options.generationConfig = body.generationConfig;
        }
        if (exists body.tool_choice) {
            if (mode == "gemini") {
                *hash<auto> tool_cfg = openAiToolChoiceToGeminiConfig(
                    body.tool_choice);
                if (tool_cfg) {
                    body_options.toolConfig = tool_cfg;
                }
            } else {
                body_options.tool_choice = body.tool_choice;
            }
        }
        *hash<auto> output_schema = getOpenAiResponseSchema(body);
        if (output_schema) {
            if (mode == "gemini") {
                options_h.output_schema = output_schema;
            } else {
                body_options.response_format = body.response_format;
            }
        }
        if (body_options) {
            options_h.body_options = body_options;
        }

        return {
            "messages": openAiMessagesToChatMessages(body.messages ?? ()),
            "tools": openAiToolsToToolDefinitions(body.tools),
            "options": options_h,
        };
    }

    private static list<hash<ChatMessage>> openAiMessagesToChatMessages(
            list<auto> raw_messages) {
        list<hash<ChatMessage>> messages = ();
        foreach auto raw in (raw_messages) {
            if (raw.typeCode() != NT_HASH) {
                continue;
            }
            hash<auto> msg = cast<hash<auto>>(raw);
            hash<ChatMessage> out_h = <ChatMessage>{
                "role": msg.role ?? "user",
            };
            if (msg.content.typeCode() == NT_LIST) {
                list<hash<ContentPart>> parts = ();
                foreach auto raw_part in (msg.content) {
                    if (raw_part.typeCode() != NT_HASH) {
                        continue;
                    }
                    hash<auto> part = cast<hash<auto>>(raw_part);
                    if (part.type == "text" && part.text.val()) {
                        hash<ContentPart> cp = <ContentPart>{
                            "type": "text",
                            "text": part.text,
                        };
                        push parts, cp;
                    } else if (part.type == "image_url") {
                        *hash<ContentPart> cp =
                            dataUrlToContentPart(part.image_url.url);
                        if (cp) {
                            push parts, cp;
                        }
                    } else if (part.type == "file") {
                        *hash<ContentPart> cp =
                            dataUrlToContentPart(part."file".file_data);
                        if (cp) {
                            push parts, cp;
                        }
                    }
                }
                if (parts) {
                    out_h.content_parts = parts;
                }
            } else if (msg.content.val()) {
                out_h.content = msg.content.toString();
            }

            if (msg.tool_calls) {
                list<hash<ChatToolCall>> tool_calls = ();
                foreach auto raw_tc in (msg.tool_calls) {
                    if (raw_tc.typeCode() != NT_HASH) {
                        continue;
                    }
                    hash<auto> tc = cast<hash<auto>>(raw_tc);
                    hash<ChatToolCall> tool_call = <ChatToolCall>{
                        "id": tc.id ?? tc.function.name ?? "tool-call",
                        "name": tc.function.name,
                        "arguments": parseOpenAiToolArguments(
                            tc.function.arguments),
                    };
                    push tool_calls, tool_call;
                }
                if (tool_calls) {
                    out_h.tool_calls = tool_calls;
                }
            }
            if (msg.tool_call_id.val()) {
                out_h.tool_call_id = msg.tool_call_id;
            }
            if (msg.name.val()) {
                out_h.name = msg.name;
            }
            push messages, out_h;
        }
        return messages;
    }

    private static *list<hash<ToolDefinition>> openAiToolsToToolDefinitions(
            auto raw_tools) {
        if (raw_tools.typeCode() != NT_LIST || !raw_tools) {
            return NOTHING;
        }
        list<hash<ToolDefinition>> tools = ();
        foreach auto raw_tool in (raw_tools) {
            if (raw_tool.typeCode() != NT_HASH) {
                continue;
            }
            hash<auto> tool = cast<hash<auto>>(raw_tool);
            if (tool.type == "function" && tool.function.name.val()) {
                push tools, <ToolDefinition>{
                    "name": tool.function.name,
                    "description": tool.function.description,
                    "parameters": tool.function.parameters,
                };
            }
        }
        return tools ? tools : NOTHING;
    }

    private static *hash<ContentPart> dataUrlToContentPart(*string url) {
        if (!url.val()) {
            return NOTHING;
        }
        *list<*string> m = regex_extract(url,
            "^data:([^;]+);base64,(.*)$");
        if (!m || !m[0].val() || !m[1].val()) {
            return NOTHING;
        }
        try {
            return <ContentPart>{
                "type": "image",
                "mime_type": m[0],
                "data": parse_base64_string(m[1]),
            };
        } catch () {
            return NOTHING;
        }
    }

    private static *hash<auto> parseOpenAiToolArguments(auto raw) {
        if (!exists raw) {
            return {};
        }
        if (raw.typeCode() == NT_HASH) {
            return cast<hash<auto>>(raw);
        }
        if (raw.typeCode() == NT_STRING && raw.val()) {
            auto parsed = parse_json(raw);
            return parsed.typeCode() == NT_HASH
                ? cast<hash<auto>>(parsed) : {"value": parsed};
        }
        return {};
    }

    private static *hash<auto> getOpenAiResponseSchema(hash<auto> body) {
        if (body.response_format.typeCode() == NT_HASH
                && body.response_format.type == "json_schema"
                && body.response_format.json_schema.schema.typeCode()
                    == NT_HASH) {
            return body.response_format.json_schema.schema;
        }
        return NOTHING;
    }

    private static *hash<auto> openAiToolChoiceToGeminiConfig(auto choice) {
        if (!exists choice) {
            return NOTHING;
        }
        hash<auto!> cfg;
        if (choice == "none") {
            cfg.functionCallingConfig = {"mode": "NONE"};
        } else if (choice == "required") {
            cfg.functionCallingConfig = {"mode": "ANY"};
        } else if (choice == "auto") {
            cfg.functionCallingConfig = {"mode": "AUTO"};
        } else if (choice.typeCode() == NT_HASH && choice.function.name.val()) {
            cfg.functionCallingConfig = {
                "mode": "ANY",
                "allowedFunctionNames": (choice.function.name,),
            };
        }
        return cfg ? cfg : NOTHING;
    }

    private static hash<auto> chatResponseToOpenAi(hash<ChatResponse> response,
            *string requested_model) {
        hash<auto!> message;
        message.role = "assistant";
        message.content = response.content;
        if (response.tool_calls) {
            list<hash<auto>> tool_calls = ();
            foreach hash<ChatToolCall> tc in (response.tool_calls) {
                push tool_calls, chatToolCallToOpenAi(tc);
            }
            message.tool_calls = tool_calls;
        }
        hash<auto!> rv = {
            "object": "chat.completion",
            "choices": ({
                "index": 0,
                "message": message,
                "finish_reason": genericStopReasonToOpenAi(
                    response.stop_reason),
            },),
            "usage": response.usage,
        };
        if (response.id.val()) {
            rv.id = response.id;
        }
        *string model = response.model ?? requested_model;
        if (model.val()) {
            rv.model = model;
        }
        return rv;
    }

    private static hash<auto> chatToolCallToOpenAi(hash<ChatToolCall> tc) {
        return {
            "id": tc.id,
            "type": "function",
            "function": {
                "name": tc.name,
                "arguments": make_json(tc.arguments ?? {}),
            },
        };
    }

    private static string genericStopReasonToOpenAi(*string reason) {
        switch (reason) {
            case "tool_use":
                return "tool_calls";
            case "max_tokens":
                return "length";
            case "stop_sequence":
                return "content_filter";
            case "end_turn":
                return "stop";
            default:
                return reason ?? "stop";
        }
    }

    private static hash<auto> newOpenAiStreamState(*string model) {
        return {
            "id": NOTHING,
            "model": model,
            "content": "",
            "tool_calls": (),
            "finish_reason": NOTHING,
            "usage": NOTHING,
        };
    }

    private static *hash<auto> chatStreamEventToOpenAiChunk(
            reference<hash<auto>> state, hash<ChatStreamEvent> event) {
        if (event.raw) {
            updateOpenAiStreamStateFromRaw(\state, event.raw);
        }
        hash<auto!> delta;
        if (!event.done && event.delta.val()) {
            state.content += event.delta;
            delta.content = event.delta;
        }
        list<hash<auto>> streamed = getOpenAiToolCallDeltas(\state,
            event.raw);
        if (streamed) {
            delta.tool_calls = streamed;
            if (event.done) {
                state.finish_reason = "tool_calls";
            }
        }
        if (event.done) {
            if (!state.finish_reason.val()) {
                state.finish_reason = "stop";
            }
            return makeOpenAiStreamChunk(state, delta,
                state.finish_reason);
        }
        if (!delta) {
            return NOTHING;
        }
        return makeOpenAiStreamChunk(state, delta, NOTHING);
    }

    private static updateOpenAiStreamStateFromRaw(
            reference<hash<auto>> state, *hash<auto> raw) {
        if (!raw) {
            return;
        }
        if (raw.responseId.val()) {
            state.id = raw.responseId;
        } else if (raw.id.val()) {
            state.id = raw.id;
        }
        if (raw.model.val()) {
            state.model = raw.model;
        }
        if (raw.usageMetadata.typeCode() == NT_HASH) {
            state.usage = normalizeGeminiUsage(raw.usageMetadata);
        } else if (raw.usage.typeCode() == NT_HASH) {
            state.usage = raw.usage;
        }
        *string finish_reason = raw.candidates[0].finishReason;
        if (finish_reason.val()) {
            state.finish_reason = state.tool_calls
                ? "tool_calls" : geminiFinishReasonToOpenAi(finish_reason);
        } else if (raw.choices[0].finish_reason.val()) {
            state.finish_reason = raw.choices[0].finish_reason;
        }
    }

    private static list<hash<auto>> getOpenAiToolCallDeltas(
            reference<hash<auto>> state, *hash<auto> raw) {
        if (!raw) {
            return ();
        }
        list<hash<auto>> streamed = ();
        foreach hash<auto> part in (raw.candidates[0].content.parts ?? ()) {
            if (!part.functionCall) {
                continue;
            }
            int idx = state.tool_calls.size();
            hash<auto> tc = {
                "id": part.functionCall.id ?? sprintf("tool-%d", idx + 1),
                "type": "function",
                "function": {
                    "name": part.functionCall.name,
                    "arguments": make_json(part.functionCall.args ?? {}),
                },
            };
            state.tool_calls += tc;
            streamed += {
                "index": idx,
                "id": tc.id,
                "type": "function",
                "function": tc.function,
            };
        }
        return streamed;
    }

    private static string geminiFinishReasonToOpenAi(*string reason) {
        switch (reason) {
            case "STOP":
                return "stop";
            case "MAX_TOKENS":
                return "length";
            case "SAFETY":
            case "RECITATION":
                return "content_filter";
            default:
                return reason ?? "stop";
        }
    }

    private static hash<auto> makeOpenAiStreamChunk(hash<auto> state,
            *hash<auto> delta, *string finish_reason) {
        hash<auto!> rv = {
            "object": "chat.completion.chunk",
            "choices": ({
                "index": 0,
                "delta": delta ?? {},
                "finish_reason": finish_reason,
            },),
        };
        if (state.id.val()) {
            rv.id = state.id;
        }
        if (state.model.val()) {
            rv.model = state.model;
        }
        if (state.usage) {
            rv.usage = state.usage;
        }
        return rv;
    }

    private static hash<auto> streamStateToOpenAiResponse(hash<auto> state) {
        hash<auto!> message = {
            "role": "assistant",
            "content": state.content.val() ? state.content : NOTHING,
        };
        if (state.tool_calls) {
            message.tool_calls = state.tool_calls;
        }
        hash<auto!> rv = {
            "object": "chat.completion",
            "choices": ({
                "index": 0,
                "message": message,
                "finish_reason": state.finish_reason ?? "stop",
            },),
            "usage": state.usage ?? {},
        };
        if (state.id.val()) {
            rv.id = state.id;
        }
        if (state.model.val()) {
            rv.model = state.model;
        }
        return rv;
    }

    #! Estimates the token count for a text string
    /** When a tokenizer is provided, returns the exact token count.
        Otherwise uses a heuristic of ~4 characters per token (typical
        for English text with GPT-style tokenizers).

        @param text the text to estimate
        @param tokenizer optional HuggingFace tokenizer for exact counting;
            pass a \c Tokenizer::HFTokenizer instance for accurate results
        @return token count (exact with tokenizer, approximate without)
    */
    static int estimateTokens(string text, *auto tokenizer) {
        if (tokenizer) {
            try {
                hash<auto> enc = call_object_method(tokenizer, "encode", text);
                return enc.input_ids.size();
            } catch () {
                # Fall through to heuristic
            }
        }
        return (text.length() + 3) / 4;
    }

    #! Ensures the REST client is initialized
    private ensureRestClient() {
        if (!rest_client) {
            rest_mutex.lock();
            on_exit rest_mutex.unlock();
            if (!rest_client) {
                if (mode == "gemini") {
                    hash<auto!> opts = {
                        "apikey": api_key,
                        "connect_timeout": connect_timeout_ms,
                        "timeout": timeout_ms,
                    };
                    if (url.val()) {
                        opts.url = url;
                    }
                    rest_client = new GeminiRestClient::GeminiRestClientIo(opts);
                } else {
                    rest_client = new RestClientIo::RestClientIo({
                        "url": url,
                        "connect_timeout": connect_timeout_ms,
                        "timeout": timeout_ms,
                        "data": "json",
                    });
                }
            }
        }
    }

    #! Completes via OpenAI-compatible /v1/chat/completions endpoint
    private hash<LlmCompletionResult> completeHttp(string system_prompt, string user_prompt,
            *hash<auto> output_schema, *hash<LlmRequestOptions> opts) {
        ensureRestClient();

        hash<auto> body = {
            "messages": (
                {"role": "system", "content": system_prompt},
                {"role": "user", "content": user_prompt},
            ),
            "max_tokens": getRequestMaxTokens(opts),
            "temperature": getRequestTemperature(opts),
        };
        *string request_model = getRequestModel(opts);
        if (request_model.val()) {
            body.model = request_model;
        }
        if (output_schema) {
            body.response_format = {
                "type": "json_schema",
                "json_schema": {
                    "name": output_schema.name ?? "output",
                    "strict": True,
                    "schema": output_schema,
                },
            };
        }
        body = mergeRequestBodyOptions(body, opts);

        *hash<auto> hdr;
        if (api_key.val()) {
            hdr."Authorization" = "Bearer " + api_key;
        }

        hash<auto> resp;
        try {
            resp = withRetry(hash<auto> sub () {
                return rest_client.restPost("/v1/chat/completions", body, hdr);
            });
        } catch (hash<ExceptionInfo> ex) {
            throw "LLM-ERROR", sprintf("HTTP completion request failed: %s", ex.desc), ex;
        }

        string content = resp.body.choices[0].message.content ?? "";
        hash<auto> usage = resp.body.usage ?? {};

        return <LlmCompletionResult>{
            "content": content,
            "usage": usage,
        };
    }

    #! Completes via Google Gemini generateContent
    private hash<LlmCompletionResult> completeGemini(string system_prompt,
            string user_prompt, *hash<auto> output_schema,
            *hash<LlmRequestOptions> opts) {
        ensureRestClient();

        hash<auto> body = buildGeminiBody((
            <ChatMessage>{"role": "system", "content": system_prompt},
            <ChatMessage>{"role": "user", "content": user_prompt},
        ), NOTHING, output_schema, opts);

        hash<auto> resp;
        try {
            resp = withRetry(hash<auto> sub () {
                return rest_client.restPost(getGeminiGeneratePath(opts), body);
            });
        } catch (hash<ExceptionInfo> ex) {
            throw "LLM-ERROR", sprintf("Gemini completion request failed: %s",
                ex.desc), ex;
        }

        return <LlmCompletionResult>{
            "content": extractGeminiText(resp.body),
            "usage": normalizeGeminiUsage(resp.body.usageMetadata),
        };
    }

    #! Completes via Anthropic Messages API /v1/messages
    private hash<LlmCompletionResult> completeAnthropic(string system_prompt, string user_prompt,
            *hash<auto> output_schema, *hash<LlmRequestOptions> opts) {
        ensureRestClient();

        hash<auto> body = {
            "model": getRequestModel(opts) ?? "claude-sonnet-4-20250514",
            "max_tokens": getRequestMaxTokens(opts),
            "system": system_prompt,
            "messages": (
                {"role": "user", "content": user_prompt},
            ),
        };

        # When an output schema is provided, use tool-use extraction pattern:
        # define a tool whose input_schema is the desired output, then force
        # the model to call it via tool_choice
        if (output_schema) {
            string tool_name = output_schema.name ?? "extract";
            body.tools = (
                {
                    "name": tool_name,
                    "description": "Extract structured data matching the schema",
                    "input_schema": output_schema,
                },
            );
            body.tool_choice = {"type": "tool", "name": tool_name};
        }
        body = mergeRequestBodyOptions(body, opts);

        hash<auto> hdr = {
            "anthropic-version": anthropic_version,
        };
        if (api_key.val()) {
            hdr."x-api-key" = api_key;
        }

        hash<auto> resp;
        try {
            resp = withRetry(hash<auto> sub () {
                return rest_client.restPost("/v1/messages", body, hdr);
            });
        } catch (hash<ExceptionInfo> ex) {
            throw "LLM-ERROR", sprintf("Anthropic completion request failed: %s", ex.desc), ex;
        }

        # Extract content from response
        string content = "";
        list<auto> content_blocks = resp.body.content ?? ();
        foreach hash<auto> block in (content_blocks) {
            if (block.type == "text") {
                content = block.text;
                break;
            } else if (block.type == "tool_use") {
                # For tool-use extraction, the structured output is in the input field
                content = make_json(block.input);
                break;
            }
        }

        hash<auto> usage = resp.body.usage ?? {};

        return <LlmCompletionResult>{
            "content": content,
            "usage": usage,
        };
    }

    #! Completes via the configured callback function
    /** Calls the user-provided \c api_callback to dispatch the request.
        The callback must accept (string method, string path, hash body)
        and return the response in OpenAI-compatible format.

        @throw LLM-ERROR if the callback fails
    */
    private hash<LlmCompletionResult> completeCallback(string system_prompt,
            string user_prompt, *hash<auto> output_schema,
            *hash<LlmRequestOptions> opts) {
        hash<auto> body = {
            "messages": (
                {"role": "system", "content": system_prompt},
                {"role": "user", "content": user_prompt},
            ),
            "max_tokens": getRequestMaxTokens(opts),
            "temperature": getRequestTemperature(opts),
        };
        *string request_model = getRequestModel(opts);
        if (request_model.val()) {
            body.model = request_model;
        }
        if (output_schema) {
            body.response_format = {
                "type": "json_schema",
                "json_schema": {
                    "name": output_schema.name ?? "output",
                    "strict": True,
                    "schema": output_schema,
                },
            };
        }
        body = mergeRequestBodyOptions(body, opts);

        hash<auto> resp;
        try {
            resp = api_callback(callback_method, callback_path, body);
        } catch (hash<ExceptionInfo> ex) {
            throw "LLM-ERROR", sprintf("callback API call failed: %s", ex.desc), ex;
        }

        string content = resp.choices[0].message.content ?? "";
        hash<auto> usage = resp.usage ?? {};

        return <LlmCompletionResult>{
            "content": content,
            "usage": usage,
        };
    }
    #! Chat via an OpenAI-compatible endpoint or callback dispatcher
    private hash<ChatResponse> chatHttp(list<hash<ChatMessage>> messages,
            *list<hash<ToolDefinition>> tools, *hash<LlmRequestOptions> opts) {
        if (mode != "callback") {
            ensureRestClient();
        }

        # Build messages in OpenAI format
        list<hash<auto>> api_messages = ();
        foreach hash<ChatMessage> msg in (messages) {
            hash<auto!> api_msg();
            api_msg.role = msg.role;
            if (msg.content_parts) {
                # Multimodal: build content block array for OpenAI
                list<auto> parts = ();
                foreach hash<ContentPart> part in (msg.content_parts) {
                    if (part.type == "image" && part.data) {
                        string b64 = make_base64_string(part.data);
                        push parts, {
                            "type": "image_url",
                            "image_url": {
                                "url": sprintf("data:%s;base64,%s",
                                    part.mime_type ?? "image/png", b64),
                            },
                        };
                    } else if (part.type == "text" && part.text.val()) {
                        push parts, {
                            "type": "text",
                            "text": part.text,
                        };
                    }
                }
                api_msg.content = parts;
            } else if (msg.content.val()) {
                api_msg.content = msg.content;
            }
            if (msg.tool_calls) {
                list<auto> tc_list = ();
                foreach hash<ChatToolCall> tc in (msg.tool_calls) {
                    push tc_list, {
                        "id": tc.id,
                        "type": "function",
                        "function": {
                            "name": tc.name,
                            "arguments": make_json(tc.arguments ?? {}),
                        },
                    };
                }
                api_msg.tool_calls = tc_list;
            }
            if (msg.tool_call_id.val()) {
                api_msg.tool_call_id = msg.tool_call_id;
            }
            if (msg.name.val() && msg.role == "tool") {
                api_msg.name = msg.name;
            }
            push api_messages, api_msg;
        }

        hash<auto> body = {
            "messages": api_messages,
            "max_tokens": getRequestMaxTokens(opts),
            "temperature": getRequestTemperature(opts),
        };
        *string request_model = getRequestModel(opts);
        if (request_model.val()) {
            body.model = request_model;
        }

        # Add tool definitions
        if (tools) {
            list<auto> tool_defs = ();
            foreach hash<ToolDefinition> td in (tools) {
                push tool_defs, {
                    "type": "function",
                    "function": {
                        "name": td.name,
                        "description": td.description ?? "",
                        "parameters": td.parameters
                            ?? {"type": "object", "properties": {}},
                    },
                };
            }
            body.tools = tool_defs;
        }
        body = mergeRequestBodyOptions(body, opts);

        *hash<auto> hdr;
        if (api_key.val()) {
            hdr."Authorization" = "Bearer " + api_key;
        }

        hash<auto> resp;
        string endpoint = (mode == "callback") ? callback_path
            : "/v1/chat/completions";
        try {
            if (mode == "callback") {
                resp = api_callback(callback_method, endpoint, body);
            } else {
                resp = withRetry(hash<auto> sub () {
                    return rest_client.restPost(endpoint, body, hdr);
                });
            }
        } catch (hash<ExceptionInfo> ex) {
            throw "LLM-ERROR", sprintf("chat request failed: %s", ex.desc), ex;
        }

        # For callback mode, resp IS the body (no .body wrapper from RestClientIo)
        hash<auto> response_data = mode == "callback" ? resp : resp.body;

        hash<auto> choice = response_data.choices[0];
        hash<auto> msg = choice.message;

        # Normalize finish_reason
        string stop_reason;
        switch (choice.finish_reason) {
            case "stop":
                stop_reason = "end_turn";
                break;
            case "tool_calls":
                stop_reason = "tool_use";
                break;
            case "length":
                stop_reason = "max_tokens";
                break;
            default:
                stop_reason = choice.finish_reason ?? "end_turn";
        }

        # Parse tool calls if present
        *list<hash<ChatToolCall>> parsed_tool_calls;
        if (msg.tool_calls) {
            parsed_tool_calls = ();
            foreach hash<auto> tc in (msg.tool_calls) {
                *hash<auto> args;
                if (tc.function.arguments.val()) {
                    if (tc.function.arguments.typeCode() == NT_STRING) {
                        args = parse_json(tc.function.arguments);
                    } else {
                        args = tc.function.arguments;
                    }
                }
                push parsed_tool_calls, <ChatToolCall>{
                    "id": tc.id,
                    "name": tc.function.name,
                    "arguments": args,
                };
            }
        }

        return <ChatResponse>{
            "id": response_data.id,
            "model": response_data.model,
            "stop_reason": stop_reason,
            "content": msg.content,
            "tool_calls": parsed_tool_calls,
            "usage": response_data.usage ?? {},
            "raw": response_data,
        };
    }

    #! Chat via Anthropic Messages API
    private hash<ChatResponse> chatAnthropic(list<hash<ChatMessage>> messages,
            *list<hash<ToolDefinition>> tools, *hash<LlmRequestOptions> opts) {
        ensureRestClient();

        # Extract system message and build Anthropic message list
        *string system_prompt;
        list<hash<auto>> api_messages = ();

        # Collect tool result messages to group consecutive ones
        list<hash<auto>> pending_tool_results = ();

        foreach hash<ChatMessage> msg in (messages) {
            if (msg.role == "system") {
                system_prompt = msg.content;
                continue;
            }

            # Flush any pending tool results before a non-tool message
            if (msg.role != "tool" && pending_tool_results) {
                push api_messages, {
                    "role": "user",
                    "content": pending_tool_results,
                };
                pending_tool_results = ();
            }

            if (msg.role == "tool") {
                # Accumulate tool results — Anthropic requires them grouped
                push pending_tool_results, {
                    "type": "tool_result",
                    "tool_use_id": msg.tool_call_id,
                    "content": msg.content ?? "",
                };
                continue;
            }

            if (msg.role == "assistant" && msg.tool_calls) {
                # Convert assistant tool_calls to Anthropic content blocks
                list<auto> content_blocks = ();
                if (msg.content.val()) {
                    push content_blocks, {"type": "text", "text": msg.content};
                }
                foreach hash<ChatToolCall> tc in (msg.tool_calls) {
                    push content_blocks, {
                        "type": "tool_use",
                        "id": tc.id,
                        "name": tc.name,
                        "input": tc.arguments ?? {},
                    };
                }
                push api_messages, {
                    "role": "assistant",
                    "content": content_blocks,
                };
                continue;
            }

            # Regular user or assistant message
            if (msg.content_parts) {
                # Multimodal: build content block array for Anthropic
                list<auto> parts = ();
                foreach hash<ContentPart> part in (msg.content_parts) {
                    if (part.type == "image" && part.data) {
                        string b64 = make_base64_string(part.data);
                        push parts, {
                            "type": "image",
                            "source": {
                                "type": "base64",
                                "media_type": part.mime_type ?? "image/png",
                                "data": b64,
                            },
                        };
                    } else if (part.type == "text" && part.text.val()) {
                        push parts, {
                            "type": "text",
                            "text": part.text,
                        };
                    }
                }
                push api_messages, {
                    "role": msg.role,
                    "content": parts,
                };
            } else {
                push api_messages, {
                    "role": msg.role,
                    "content": msg.content ?? "",
                };
            }
        }

        # Flush any remaining tool results
        if (pending_tool_results) {
            push api_messages, {
                "role": "user",
                "content": pending_tool_results,
            };
        }

        hash<auto> body = {
            "model": getRequestModel(opts) ?? "claude-sonnet-4-20250514",
            "max_tokens": getRequestMaxTokens(opts),
            "messages": api_messages,
        };
        if (system_prompt.val()) {
            body.system = system_prompt;
        }

        # Add tool definitions
        if (tools) {
            list<auto> tool_defs = ();
            foreach hash<ToolDefinition> td in (tools) {
                push tool_defs, {
                    "name": td.name,
                    "description": td.description ?? "",
                    "input_schema": td.parameters
                        ?? {"type": "object", "properties": {}},
                };
            }
            body.tools = tool_defs;
        }
        body = mergeRequestBodyOptions(body, opts);

        hash<auto> hdr = {
            "anthropic-version": anthropic_version,
        };
        if (api_key.val()) {
            hdr."x-api-key" = api_key;
        }

        hash<auto> resp;
        try {
            resp = withRetry(hash<auto> sub () {
                return rest_client.restPost("/v1/messages", body, hdr);
            });
        } catch (hash<ExceptionInfo> ex) {
            throw "LLM-ERROR", sprintf("Anthropic chat request failed: %s", ex.desc), ex;
        }

        # Parse response content blocks
        *string text_content;
        *list<hash<ChatToolCall>> parsed_tool_calls;
        list<auto> content_blocks = resp.body.content ?? ();

        foreach hash<auto> block in (content_blocks) {
            if (block.type == "text") {
                text_content = block.text;
            } else if (block.type == "tool_use") {
                if (!parsed_tool_calls) {
                    parsed_tool_calls = ();
                }
                push parsed_tool_calls, <ChatToolCall>{
                    "id": block.id,
                    "name": block.name,
                    "arguments": block.input,
                };
            }
        }

        return <ChatResponse>{
            "id": resp.body.id,
            "model": resp.body.model,
            "stop_reason": resp.body.stop_reason ?? "end_turn",
            "content": text_content,
            "tool_calls": parsed_tool_calls,
            "usage": resp.body.usage ?? {},
            "raw": resp.body,
        };
    }

    #! Chat via Google Gemini generateContent
    private hash<ChatResponse> chatGemini(list<hash<ChatMessage>> messages,
            *list<hash<ToolDefinition>> tools, *hash<LlmRequestOptions> opts) {
        ensureRestClient();

        hash<auto> body = buildGeminiBody(messages, tools,
            opts.output_schema, opts);
        hash<auto> resp;
        try {
            resp = withRetry(hash<auto> sub () {
                return rest_client.restPost(getGeminiGeneratePath(opts), body);
            });
        } catch (hash<ExceptionInfo> ex) {
            throw "LLM-ERROR", sprintf("Gemini chat request failed: %s",
                ex.desc), ex;
        }

        hash<ChatResponse> rv = parseGeminiChatResponse(resp.body);
        rv.model = getGeminiModelName(opts);
        return rv;
    }

    #! Streams Gemini generateContent SSE events as generic ChatStreamEvent deltas
    private chatStreamGemini(list<hash<ChatMessage>> messages,
            code<nothing(hash<ChatStreamEvent>)> callback,
            *list<hash<ToolDefinition>> tools, *hash<LlmRequestOptions> opts) {
        hash<auto> body = buildGeminiBody(messages, tools,
            opts.output_schema, opts);
        GeminiRestClient::GeminiResponseSseStreamIo stream =
            getGeminiClient().openStreamingConnection(
                getGeminiBareModelName(getGeminiModelName(opts)), body,
                NOTHING, timeout_ms);
        on_exit stream.close();

        while (True) {
            *hash<SseMessageInfo> evt = stream.readEvent(timeout_ms);
            if (!evt) {
                callback(<ChatStreamEvent>{"done": True});
                break;
            }
            if (!evt.data.val() || evt.data == "[DONE]") {
                callback(<ChatStreamEvent>{"done": True});
                break;
            }

            *hash<auto> parsed;
            try {
                parsed = parse_json(evt.data);
            } catch () {
                continue;
            }

            string delta = extractGeminiText(parsed);
            *string finish_reason = parsed.candidates[0].finishReason;
            if (delta.val()) {
                callback(<ChatStreamEvent>{
                    "event": evt.event,
                    "delta": delta,
                    "raw": parsed,
                });
            }
            if (finish_reason.val()) {
                callback(<ChatStreamEvent>{"done": True, "raw": parsed});
                break;
            }
        }
    }
    #! Executes an HTTP call with retry and exponential backoff for transient errors
    /** Retries on 429 (rate limit) and 5xx (server error) responses.
        Uses exponential backoff: delay * 2^attempt.

        @param call a callable that performs the HTTP request and returns the response
        @return the response from the successful call

        @throw LLM-ERROR if all retry attempts fail
    */
    private auto withRetry(code call) {
        int attempt = 0;
        while (True) {
            try {
                return call();
            } catch (hash<ExceptionInfo> ex) {
                # Check if this is a retryable error
                bool retryable = False;
                if (ex.arg.typeCode() == NT_HASH) {
                    int status = ex.arg.status_code ?? 0;
                    if (status == 429 || status >= 500) {
                        retryable = True;
                    }
                }

                if (!retryable || attempt >= max_retries) {
                    rethrow;
                }

                ++attempt;
                # Exponential backoff: delay_ms * 2^(attempt-1)
                int delay = retry_delay_ms * (1 << (attempt - 1));

                # Check for Retry-After header (429 responses)
                if (ex.arg.typeCode() == NT_HASH && ex.arg.headers."retry-after".val()) {
                    int retry_after = ex.arg.headers."retry-after".toInt();
                    if (retry_after > 0) {
                        delay = retry_after * 1000;
                    }
                }

                usleep(delay * 1000);  # usleep takes microseconds
            }
        }
    }

    #! Returns the Gemini REST client
    private GeminiRestClient::GeminiRestClientIo getGeminiClient() {
        ensureRestClient();
        return cast<GeminiRestClient::GeminiRestClientIo>(rest_client);
    }

    #! Returns the request model, allowing per-call task routing
    private *string getRequestModel(*hash<LlmRequestOptions> opts) {
        return opts.model ?? model_name;
    }

    #! Returns the max-token setting for this request
    private int getRequestMaxTokens(*hash<LlmRequestOptions> opts) {
        return opts.max_tokens ?? max_tokens;
    }

    #! Returns the temperature setting for this request
    private float getRequestTemperature(*hash<LlmRequestOptions> opts) {
        return opts.temperature ?? temperature;
    }

    #! Merges provider-specific request body options after generic fields
    private static hash<auto> mergeRequestBodyOptions(hash<auto> body,
            *hash<LlmRequestOptions> opts) {
        if (!opts.body_options) {
            return body;
        }
        hash<auto!> rv;
        rv += body;
        foreach string key in (keys opts.body_options) {
            rv{key} = opts.body_options{key};
        }
        return rv;
    }

    #! Returns the Gemini model, defaulting to the stable Flash model
    private string getGeminiModelName(*hash<LlmRequestOptions> opts) {
        return getRequestModel(opts) ?? "gemini-2.5-flash";
    }

    #! Returns the Gemini generateContent REST path
    private string getGeminiGeneratePath(*hash<LlmRequestOptions> opts) {
        return getGeminiModelResource(getGeminiModelName(opts))
            + ":generateContent";
    }

    #! Returns the Gemini model resource for REST calls
    private static string getGeminiModelResource(string model) {
        return model =~ /^models\// ? model : "models/" + model;
    }

    #! Returns the bare Gemini model name required by streaming helper APIs
    private static string getGeminiBareModelName(string model) {
        return regex_subst(model, "^models/", "");
    }

    #! Builds a Gemini generateContent request body
    private hash<auto> buildGeminiBody(list<hash<ChatMessage>> messages,
            *list<hash<ToolDefinition>> tools,
            *hash<auto> output_schema, *hash<LlmRequestOptions> opts) {
        list<hash<auto>> contents = ();
        list<hash<auto>> system_parts = ();

        foreach hash<ChatMessage> msg in (messages) {
            if (msg.role == "system") {
                list<hash<auto>> parts = buildGeminiContentParts(msg);
                if (parts) {
                    system_parts += parts;
                }
                continue;
            }

            list<hash<auto>> parts;
            if (msg.role == "tool") {
                parts = ({
                    "functionResponse": {
                        "name": msg.name ?? msg.tool_call_id ?? "tool_result",
                        "response": parseGeminiToolResponse(msg.content),
                    },
                },);
                push contents, {"role": "user", "parts": parts};
                continue;
            }

            parts = buildGeminiContentParts(msg);
            if (msg.role == "assistant" && msg.tool_calls) {
                foreach hash<ChatToolCall> tc in (msg.tool_calls) {
                    push parts, {
                        "functionCall": {
                            "name": tc.name,
                            "args": tc.arguments ?? {},
                        },
                    };
                }
            }
            if (!parts) {
                parts = ({"text": msg.content ?? ""},);
            }
            push contents, {
                "role": msg.role == "assistant" ? "model" : "user",
                "parts": parts,
            };
        }

        hash<auto!> body;
        body.contents = contents;
        if (system_parts) {
            body.systemInstruction = {"parts": system_parts};
        }

        hash<auto!> generation_config = {
            "maxOutputTokens": getRequestMaxTokens(opts),
            "temperature": getRequestTemperature(opts),
        };
        if (output_schema) {
            generation_config.responseMimeType = "application/json";
            generation_config.responseSchema = sanitizeGeminiSchema(
                output_schema);
        }
        body.generationConfig = generation_config;

        if (tools && tools.size()) {
            list<hash<auto>> declarations = ();
            foreach hash<ToolDefinition> td in (tools) {
                push declarations, {
                    "name": td.name,
                    "description": td.description ?? "",
                    "parameters": sanitizeGeminiSchema(td.parameters)
                        ?? {"type": "object", "properties": {}},
                };
            }
            body.tools = ({"functionDeclarations": declarations},);
        }
        body = mergeGeminiRequestBodyOptions(body, opts);
        return body;
    }

    #! Returns a Gemini-compatible copy of a JSON schema.
    /** Gemini's function declarations and response schemas accept only a
        subset of JSON Schema / OpenAPI.  In particular,
        @c additionalProperties causes generateContent to reject the request
        with HTTP 400 even when it is a boolean.  Strip unsupported schema
        keywords at the provider boundary so callers can keep using stricter
        OpenAI/Anthropic schemas and local validators.
    */
    private static *hash<auto> sanitizeGeminiSchema(*hash<auto> schema) {
        if (!schema) {
            return NOTHING;
        }
        return cast<hash<auto>>(sanitizeGeminiSchemaValue(schema));
    }

    private static auto sanitizeGeminiSchemaValue(auto value) {
        if (value.typeCode() == NT_HASH) {
            hash<auto> rv;
            foreach string key in (keys value) {
                if (key == "additionalProperties") {
                    continue;
                }
                rv{key} = sanitizeGeminiSchemaValue(value{key});
            }
            return rv;
        }
        if (value.typeCode() == NT_LIST) {
            list<auto> rv = ();
            foreach auto item in (value) {
                push rv, sanitizeGeminiSchemaValue(item);
            }
            return rv;
        }
        return value;
    }

    #! Merges provider-specific request body options into a Gemini request
    private static hash<auto> mergeGeminiRequestBodyOptions(hash<auto> body,
            *hash<LlmRequestOptions> opts) {
        if (!opts.body_options) {
            return body;
        }
        hash<auto!> rv;
        rv += body;
        foreach string key in (keys opts.body_options) {
            if (key == "generationConfig"
                    && rv.generationConfig.typeCode() == NT_HASH
                    && opts.body_options{key}.typeCode() == NT_HASH) {
                hash<auto!> gc;
                gc += rv.generationConfig;
                gc += opts.body_options{key};
                rv.generationConfig = gc;
            } else {
                rv{key} = opts.body_options{key};
            }
        }
        return rv;
    }

    #! Builds Gemini content parts from a generic chat message
    private static list<hash<auto>> buildGeminiContentParts(hash<ChatMessage> msg) {
        list<hash<auto>> parts = ();
        if (msg.content_parts) {
            foreach hash<ContentPart> part in (msg.content_parts) {
                if (part.type == "image" && part.data) {
                    push parts, {
                        "inlineData": {
                            "mimeType": part.mime_type ?? "image/png",
                            "data": make_base64_string(part.data),
                        },
                    };
                } else if (part.type == "text" && part.text.val()) {
                    push parts, {"text": part.text};
                }
            }
        } else if (msg.content.val()) {
            push parts, {"text": msg.content};
        }
        return parts;
    }

    #! Parses an OpenAI-style tool-result string into Gemini functionResponse.response
    private static hash<auto> parseGeminiToolResponse(*string content) {
        if (!content.val()) {
            return {};
        }
        try {
            auto parsed = parse_json(content);
            return parsed.typeCode() == NT_HASH
                ? cast<hash<auto>>(parsed)
                : {"result": parsed};
        } catch () {
            return {"result": content};
        }
    }

    #! Parses a Gemini response into a generic ChatResponse
    private static hash<ChatResponse> parseGeminiChatResponse(hash<auto> body) {
        hash<auto> cand = body.candidates[0] ?? {};
        list<string> text_parts = ();
        *list<hash<ChatToolCall>> tool_calls;
        int tc_index;
        foreach hash<auto> part in (cand.content.parts ?? ()) {
            if (part.text.val()) {
                push text_parts, part.text;
            } else if (part.functionCall) {
                if (!tool_calls) {
                    tool_calls = ();
                }
                push tool_calls, <ChatToolCall>{
                    "id": part.functionCall.id ?? sprintf("gemini-tool-%d",
                        ++tc_index),
                    "name": part.functionCall.name,
                    "arguments": part.functionCall.args ?? {},
                };
            }
        }

        return <ChatResponse>{
            "id": body.responseId ?? body.id,
            "model": body.model,
            "stop_reason": tool_calls ? "tool_use"
                : mapGeminiFinishReason(cand.finishReason),
            "content": text_parts ? text_parts.join("") : NOTHING,
            "tool_calls": tool_calls,
            "usage": normalizeGeminiUsage(body.usageMetadata),
            "raw": body,
        };
    }

    #! Extracts text parts from a Gemini response/chunk
    private static string extractGeminiText(hash<auto> body) {
        list<string> parts = ();
        foreach hash<auto> part in (body.candidates[0].content.parts ?? ()) {
            if (part.text.val()) {
                push parts, part.text;
            }
        }
        return parts.join("");
    }

    #! Maps Gemini usage metadata to the OpenAI-style counters used by callers
    private static hash<auto> normalizeGeminiUsage(auto usage) {
        int prompt_tokens = usage.promptTokenCount ?? 0;
        int completion_tokens = usage.candidatesTokenCount ?? 0;
        int total_tokens = usage.totalTokenCount
            ?? (prompt_tokens + completion_tokens);
        return {
            "prompt_tokens": prompt_tokens,
            "completion_tokens": completion_tokens,
            "total_tokens": total_tokens,
            "input_tokens": prompt_tokens,
            "output_tokens": completion_tokens,
        };
    }

    #! Maps Gemini finishReason values to QoreLlmUtils stop reasons
    private static string mapGeminiFinishReason(*string reason) {
        switch (reason) {
            case "STOP":
                return "end_turn";
            case "MAX_TOKENS":
                return "max_tokens";
            case "SAFETY":
            case "RECITATION":
                return "stop_sequence";
            default:
                return reason ?? "end_turn";
        }
    }

    #! Builds the HTTP/OpenAI message body for chatStream()
    private hash<auto> buildHttpBody(list<hash<ChatMessage>> messages,
            *list<hash<ToolDefinition>> tools, *hash<LlmRequestOptions> opts) {
        list<hash<auto>> api_messages = ();
        foreach hash<ChatMessage> msg in (messages) {
            hash<auto!> api_msg();
            api_msg.role = msg.role;
            if (msg.content_parts) {
                list<auto> parts = ();
                foreach hash<ContentPart> part in (msg.content_parts) {
                    if (part.type == "image" && part.data) {
                        string b64 = make_base64_string(part.data);
                        push parts, {
                            "type": "image_url",
                            "image_url": {
                                "url": sprintf("data:%s;base64,%s",
                                    part.mime_type ?? "image/png", b64),
                            },
                        };
                    } else if (part.type == "text" && part.text.val()) {
                        push parts, {"type": "text", "text": part.text};
                    }
                }
                api_msg.content = parts;
            } else if (msg.content.val()) {
                api_msg.content = msg.content;
            }
            if (msg.tool_calls) {
                list<auto> tc_list = ();
                foreach hash<ChatToolCall> tc in (msg.tool_calls) {
                    push tc_list, {
                        "id": tc.id,
                        "type": "function",
                        "function": {
                            "name": tc.name,
                            "arguments": make_json(tc.arguments ?? {}),
                        },
                    };
                }
                api_msg.tool_calls = tc_list;
            }
            if (msg.tool_call_id.val()) {
                api_msg.tool_call_id = msg.tool_call_id;
            }
            if (msg.name.val() && msg.role == "tool") {
                api_msg.name = msg.name;
            }
            push api_messages, api_msg;
        }

        hash<auto> body = {
            "messages": api_messages,
            "max_tokens": getRequestMaxTokens(opts),
            "temperature": getRequestTemperature(opts),
        };
        *string request_model = getRequestModel(opts);
        if (request_model.val()) {
            body.model = request_model;
        }
        if (tools) {
            list<auto> tool_defs = ();
            foreach hash<ToolDefinition> td in (tools) {
                push tool_defs, {
                    "type": "function",
                    "function": {
                        "name": td.name,
                        "description": td.description ?? "",
                        "parameters": td.parameters
                            ?? {"type": "object", "properties": {}},
                    },
                };
            }
            body.tools = tool_defs;
        }
        return mergeRequestBodyOptions(body, opts);
    }

    #! Builds the Anthropic message body for chatStream()
    private hash<auto> buildAnthropicBody(list<hash<ChatMessage>> messages,
            *list<hash<ToolDefinition>> tools, *hash<LlmRequestOptions> opts) {
        *string sys_prompt;
        list<hash<auto>> api_messages = ();
        list<hash<auto>> pending_tool_results = ();

        foreach hash<ChatMessage> msg in (messages) {
            if (msg.role == "system") {
                sys_prompt = msg.content;
                continue;
            }
            if (msg.role != "tool" && pending_tool_results) {
                push api_messages, {"role": "user", "content": pending_tool_results};
                pending_tool_results = ();
            }
            if (msg.role == "tool") {
                push pending_tool_results, {
                    "type": "tool_result",
                    "tool_use_id": msg.tool_call_id,
                    "content": msg.content ?? "",
                };
                continue;
            }
            if (msg.role == "assistant" && msg.tool_calls) {
                list<auto> content_blocks = ();
                if (msg.content.val()) {
                    push content_blocks, {"type": "text", "text": msg.content};
                }
                foreach hash<ChatToolCall> tc in (msg.tool_calls) {
                    push content_blocks, {
                        "type": "tool_use",
                        "id": tc.id,
                        "name": tc.name,
                        "input": tc.arguments ?? {},
                    };
                }
                push api_messages, {"role": "assistant", "content": content_blocks};
                continue;
            }
            if (msg.content_parts) {
                list<auto> parts = ();
                foreach hash<ContentPart> part in (msg.content_parts) {
                    if (part.type == "image" && part.data) {
                        string b64 = make_base64_string(part.data);
                        push parts, {
                            "type": "image",
                            "source": {
                                "type": "base64",
                                "media_type": part.mime_type ?? "image/png",
                                "data": b64,
                            },
                        };
                    } else if (part.type == "text" && part.text.val()) {
                        push parts, {"type": "text", "text": part.text};
                    }
                }
                push api_messages, {"role": msg.role, "content": parts};
            } else {
                push api_messages, {"role": msg.role, "content": msg.content ?? ""};
            }
        }
        if (pending_tool_results) {
            push api_messages, {"role": "user", "content": pending_tool_results};
        }

        hash<auto> body = {
            "model": getRequestModel(opts) ?? "claude-sonnet-4-20250514",
            "max_tokens": getRequestMaxTokens(opts),
            "messages": api_messages,
        };
        if (sys_prompt.val()) {
            body.system = sys_prompt;
        }
        if (tools) {
            list<auto> tool_defs = ();
            foreach hash<ToolDefinition> td in (tools) {
                push tool_defs, {
                    "name": td.name,
                    "description": td.description ?? "",
                    "input_schema": td.parameters
                        ?? {"type": "object", "properties": {}},
                };
            }
            body.tools = tool_defs;
        }
        return mergeRequestBodyOptions(body, opts);
    }
}

} # namespace QoreLlmUtils
