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
    endpoint, the Anthropic Messages API, or the Qorus AI Gateway.
*/
public hashdecl LlmBackendConfig {
    #! Mode: "http" for OpenAI-compatible, "anthropic" for Anthropic Messages API, "qorus" for Qorus AI Gateway
    string mode;
    #! Base URL of the LLM service (required for "http" and "anthropic" modes)
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
    #! Initial retry delay in milliseconds for exponential backoff; default 1000
    int retry_delay_ms = 1000;
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
    #! Why the LLM stopped: "end_turn" (normal), "tool_use" (wants tool calls),
    #! "max_tokens" (hit limit), or "stop_sequence"
    string stop_reason;
    #! Text content (if any)
    *string content;
    #! Tool calls the LLM wants executed (when stop_reason is "tool_use")
    *list<hash<ChatToolCall>> tool_calls;
    #! Token usage information
    hash<auto> usage;
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
    multiple providers.  Supports three backends:

    - \b http: OpenAI-compatible \c /v1/chat/completions with optional
      \c response_format for JSON schema enforcement.
    - \b anthropic: Anthropic Messages API (\c /v1/messages) with tool-use-based
      structured extraction when an output schema is provided.
    - \b qorus: Qorus AI Gateway — delegates to \c UserApi::callRestApi().

    Thread-safe: the underlying REST client is lazily initialized behind a mutex.

    @since QoreLlmUtils 1.0

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
%ifndef NoRestClient
        #! REST client for HTTP/Anthropic modes (lazy-initialized)
        *RestClientIo::RestClientIo rest_client;
        #! Mutex for thread-safe lazy initialization
        Mutex rest_mutex();
%endif
    }

    #! Creates an LlmBackend with the given configuration
    /** @param config an @ref LlmBackendConfig hash specifying the backend and parameters

        @throw LLM-CONFIG-ERROR if the mode is invalid or required parameters are missing
        @throw LLM-DEPENDENCY-ERROR if required modules are not available
    */
    constructor(hash<LlmBackendConfig> config) {
        mode = config.mode;
        if (mode != "http" && mode != "anthropic" && mode != "qorus") {
            throw "LLM-CONFIG-ERROR",
                sprintf("invalid mode %y; expected \"http\", \"anthropic\", or \"qorus\"", mode);
        }

        if (mode == "http" || mode == "anthropic") {
            if (!config.url.val()) {
                throw "LLM-CONFIG-ERROR",
                    sprintf("url is required for %y mode", mode);
            }
%ifdef NoRestClient
            throw "LLM-DEPENDENCY-ERROR",
                "RestClient module is required for HTTP/Anthropic mode but is not available";
%endif
%ifdef NoJson
            throw "LLM-DEPENDENCY-ERROR",
                "json module is required for HTTP/Anthropic mode but is not available";
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
            *hash<auto> output_schema) {
        switch (mode) {
            case "http":
                return completeHttp(system_prompt, user_prompt, output_schema);
            case "anthropic":
                return completeAnthropic(system_prompt, user_prompt, output_schema);
            case "qorus":
                return completeQorus(system_prompt, user_prompt, output_schema);
        }
        # unreachable — mode validated in constructor
        throw "LLM-ERROR", sprintf("unsupported mode %y", mode);
    }

    /** @return the configured backend mode ("http", "anthropic", or "qorus") */
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

        @since QoreLlmUtils 1.0
    */
    hash<ChatResponse> chat(list<hash<ChatMessage>> messages,
            *list<hash<ToolDefinition>> tools) {
        switch (mode) {
            case "http":
            case "qorus":
                return chatHttp(messages, tools);
            case "anthropic":
                return chatAnthropic(messages, tools);
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

        @since QoreLlmUtils 1.0
    */
    chatStream(list<hash<ChatMessage>> messages, code callback,
            *list<hash<ToolDefinition>> tools) {
        if (mode == "qorus") {
            throw "LLM-ERROR",
                "streaming is not supported in Qorus mode; use chat() instead";
        }
%ifdef NoStreaming
        throw "LLM-DEPENDENCY-ERROR",
            "HttpClientStreamingIo module is required for streaming";
%endif
%ifndef NoStreaming
%ifndef NoRestClient
        ensureRestClient();

        # Build the request body (same as chat but with stream=true)
        hash<auto> body;
        if (mode == "anthropic") {
            body = buildAnthropicBody(messages, tools);
            body.stream = True;
        } else {
            body = buildHttpBody(messages, tools);
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
%endif
%endif
    }

    #! Estimates the token count for a text string
    /** Uses a simple heuristic of ~4 characters per token (typical for
        English text with GPT-style tokenizers). For accurate counts, use
        the tokenizer module's HFTokenizer directly.

        @param text the text to estimate
        @return estimated token count
    */
    static int estimateTokens(string text) {
        return (text.length() + 3) / 4;
    }

    #! Ensures the REST client is initialized
    private ensureRestClient() {
%ifndef NoRestClient
        if (!rest_client) {
            rest_mutex.lock();
            on_exit rest_mutex.unlock();
            if (!rest_client) {
                rest_client = new RestClientIo::RestClientIo({
                    "url": url,
                    "connect_timeout": connect_timeout_ms,
                    "timeout": timeout_ms,
                    "data": "json",
                });
            }
        }
%endif
    }

    #! Completes via OpenAI-compatible /v1/chat/completions endpoint
    private hash<LlmCompletionResult> completeHttp(string system_prompt, string user_prompt,
            *hash<auto> output_schema) {
%ifndef NoRestClient
        ensureRestClient();

        hash<auto> body = {
            "messages": (
                {"role": "system", "content": system_prompt},
                {"role": "user", "content": user_prompt},
            ),
            "max_tokens": max_tokens,
            "temperature": temperature,
        };
        if (model_name.val()) {
            body.model = model_name;
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
%endif
        throw "LLM-ERROR", "RestClient not available";
    }

    #! Completes via Anthropic Messages API /v1/messages
    private hash<LlmCompletionResult> completeAnthropic(string system_prompt, string user_prompt,
            *hash<auto> output_schema) {
%ifndef NoRestClient
        ensureRestClient();

        hash<auto> body = {
            "model": model_name ?? "claude-sonnet-4-20250514",
            "max_tokens": max_tokens,
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
%endif
        throw "LLM-ERROR", "RestClient not available";
    }

    #! Completes via Qorus AI Gateway
    /** Calls \c UserApi::callRestApi() to route through the Qorus AI Gateway.
        Requires running inside a Qorus context where \c UserApi is available.

        @throw LLM-ERROR if the Qorus API call fails
    */
    private hash<LlmCompletionResult> completeQorus(string system_prompt, string user_prompt,
            *hash<auto> output_schema) {
        hash<auto> body = {
            "messages": (
                {"role": "system", "content": system_prompt},
                {"role": "user", "content": user_prompt},
            ),
            "max_tokens": max_tokens,
            "temperature": temperature,
        };
        if (model_name.val()) {
            body.model = model_name;
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

        hash<auto> resp;
        try {
            # Use dynamic dispatch — UserApi is only available in Qorus context
            resp = call_static_method("UserApi", "callRestApi", "PUT",
                "ai/v1/chat/completions", body);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "UNKNOWN-CLASS") {
                throw "LLM-ERROR",
                    "Qorus mode requires a Qorus runtime context (UserApi not available)";
            }
            throw "LLM-ERROR", sprintf("Qorus AI Gateway call failed: %s", ex.desc), ex;
        }

        string content = resp.choices[0].message.content ?? "";
        hash<auto> usage = resp.usage ?? {};

        return <LlmCompletionResult>{
            "content": content,
            "usage": usage,
        };
    }
    #! Chat via OpenAI-compatible endpoint (also used for Qorus mode)
    private hash<ChatResponse> chatHttp(list<hash<ChatMessage>> messages,
            *list<hash<ToolDefinition>> tools) {
%ifndef NoRestClient
        if (mode != "qorus") {
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
            "max_tokens": max_tokens,
            "temperature": temperature,
        };
        if (model_name.val()) {
            body.model = model_name;
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

        *hash<auto> hdr;
        if (api_key.val()) {
            hdr."Authorization" = "Bearer " + api_key;
        }

        hash<auto> resp;
        string endpoint = (mode == "qorus") ? "ai/v1/chat/completions"
            : "/v1/chat/completions";
        try {
            if (mode == "qorus") {
                resp = call_static_method("UserApi", "callRestApi", "PUT",
                    endpoint, body);
            } else {
                resp = withRetry(hash<auto> sub () {
                    return rest_client.restPost(endpoint, body, hdr);
                });
            }
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "UNKNOWN-CLASS") {
                throw "LLM-ERROR",
                    "Qorus mode requires a Qorus runtime context (UserApi not available)";
            }
            throw "LLM-ERROR", sprintf("chat request failed: %s", ex.desc), ex;
        }

        # For Qorus mode, resp IS the body (no .body wrapper)
        hash<auto> response_data = mode == "qorus" ? resp : resp.body;

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
            "stop_reason": stop_reason,
            "content": msg.content,
            "tool_calls": parsed_tool_calls,
            "usage": response_data.usage ?? {},
        };
%endif
        throw "LLM-ERROR", "RestClient not available";
    }

    #! Chat via Anthropic Messages API
    private hash<ChatResponse> chatAnthropic(list<hash<ChatMessage>> messages,
            *list<hash<ToolDefinition>> tools) {
%ifndef NoRestClient
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
            "model": model_name ?? "claude-sonnet-4-20250514",
            "max_tokens": max_tokens,
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
            "stop_reason": resp.body.stop_reason ?? "end_turn",
            "content": text_content,
            "tool_calls": parsed_tool_calls,
            "usage": resp.body.usage ?? {},
        };
%endif
        throw "LLM-ERROR", "RestClient not available";
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

    #! Builds the HTTP/OpenAI message body for chatStream()
    private hash<auto> buildHttpBody(list<hash<ChatMessage>> messages,
            *list<hash<ToolDefinition>> tools) {
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
            "max_tokens": max_tokens,
            "temperature": temperature,
        };
        if (model_name.val()) {
            body.model = model_name;
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
        return body;
    }

    #! Builds the Anthropic message body for chatStream()
    private hash<auto> buildAnthropicBody(list<hash<ChatMessage>> messages,
            *list<hash<ToolDefinition>> tools) {
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
            "model": model_name ?? "claude-sonnet-4-20250514",
            "max_tokens": max_tokens,
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
        return body;
    }
}

} # namespace QoreLlmUtils
