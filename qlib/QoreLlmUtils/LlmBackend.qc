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

#! Result from generic structured-output repair
public hashdecl LlmStructuredRepairResult {
    #! True when the value could be parsed or coerced as requested
    bool valid;
    #! True when the returned value differs from the supplied value
    bool repaired = False;
    #! Parsed or coerced value
    *auto value;
    #! Stable machine-readable error code
    *string error_code;
    #! Human-readable error detail
    *string error_detail;
    #! Stable machine-readable repair type
    *string repair_kind;
    #! Stable fingerprint for retry / rejection grouping
    *string failure_fingerprint;
}

#! Provider-neutral helpers for parsing and repairing structured LLM output
/** LLM providers differ in how strictly they enforce JSON schema and tool
    argument shapes.  This class centralizes conservative, provider-neutral
    repairs that are safe before a caller applies domain-specific validation:
    fenced JSON extraction, hidden-reasoning tag removal, balanced JSON prefix
    parsing, and schema-guided object/array coercion for nested values that
    were returned as JSON strings.
*/
public class LlmStructuredRepair {
    #! Parses any JSON value, optionally checking the expected JSON type
    static hash<LlmStructuredRepairResult> parseJsonValue(auto raw,
            *string expected_type) {
        if (!exists raw) {
            return makeInvalid("missing-value", "no value was supplied");
        }

        if (raw.typeCode() != NT_STRING) {
            if (expected_type.val()
                    && !valueMatchesJsonType(raw, expected_type)) {
                return makeInvalid("json-type-mismatch",
                    sprintf("expected JSON %s but received %s",
                        expected_type, raw.type()));
            }
            return makeValid(raw, False, NOTHING);
        }

        string original = raw;
        string cleaned = cleanJsonText(original);
        if (!cleaned.val()) {
            return makeInvalid("empty-json-text",
                "the model returned no JSON text");
        }

        try {
            auto parsed = parse_json(cleaned);
            return validateParsed(parsed, expected_type,
                cleaned != trim(original), "json-text-cleaned");
        } catch (hash<ExceptionInfo> ex) {
            *string prefix = extractBalancedJsonPrefix(cleaned);
            if (prefix.val() && prefix != cleaned) {
                try {
                    auto parsed = parse_json(prefix);
                    return validateParsed(parsed, expected_type, True,
                        "json-balanced-prefix");
                } catch (hash<ExceptionInfo> ex2) {
                    return makeInvalid("invalid-json",
                        sprintf("%s; balanced prefix also failed: %s",
                            ex.desc, ex2.desc));
                }
            }
            return makeInvalid("invalid-json", ex.desc);
        }
    }

    #! Parses a JSON object from a hash or JSON string
    static hash<LlmStructuredRepairResult> parseJsonObject(auto raw) {
        return parseJsonValue(raw, "object");
    }

    #! Parses a JSON array from a list or JSON string
    static hash<LlmStructuredRepairResult> parseJsonArray(auto raw) {
        return parseJsonValue(raw, "array");
    }

    #! Coerces nested object / array values according to a JSON schema
    /** Only safe structural coercions are performed.  A string value is
        parsed as JSON when the matching schema position expects an object
        or array.  Other scalar coercions are intentionally left to the
        domain validator because they can change semantics.
    */
    static hash<LlmStructuredRepairResult> coerceBySchema(auto value,
            *hash<auto> schema) {
        if (!schema) {
            return makeValid(value, False, NOTHING);
        }
        return coerceBySchemaIntern(value, schema, "$");
    }

    private static hash<LlmStructuredRepairResult> makeValid(auto value,
            bool repaired, *string repair_kind) {
        hash<LlmStructuredRepairResult> rv = <LlmStructuredRepairResult>{
            "valid": True,
            "repaired": repaired,
            "value": value,
        };
        if (repair_kind.val()) {
            rv.repair_kind = repair_kind;
        }
        return rv;
    }

    private static hash<LlmStructuredRepairResult> makeInvalid(string code,
            string detail) {
        string normalized = regex_subst(trim(detail), "\\s+", " ",
            RE_Global);
        if (normalized.length() > 180) {
            normalized = normalized.substr(0, 180);
        }
        return <LlmStructuredRepairResult>{
            "valid": False,
            "error_code": code,
            "error_detail": detail,
            "failure_fingerprint": code + ":" + normalized,
        };
    }

    private static string cleanJsonText(string text) {
        string rv = trim(regex_subst(text,
            "<think>[\\s\\S]*?</think>\\s*", "", RE_Global));
        if (rv =~ /^```/) {
            rv = regex_subst(rv, "^```[a-zA-Z0-9_-]*\\s*\n?", "");
            rv = regex_subst(rv, "\n?```\\s*$", "");
            rv = trim(rv);
        }
        return rv;
    }

    private static hash<LlmStructuredRepairResult> validateParsed(auto parsed,
            *string expected_type, bool repaired, *string repair_kind) {
        if (expected_type.val()
                && !valueMatchesJsonType(parsed, expected_type)) {
            return makeInvalid("json-type-mismatch",
                sprintf("expected JSON %s but received %s",
                    expected_type, parsed.type()));
        }
        return makeValid(parsed, repaired, repaired ? repair_kind
            : NOTHING);
    }

    private static bool valueMatchesJsonType(auto value, string json_type) {
        switch (json_type) {
            case "object":
                return value.typeCode() == NT_HASH;
            case "array":
                return value.typeCode() == NT_LIST;
            case "string":
                return value.typeCode() == NT_STRING;
            case "boolean":
                return value.typeCode() == NT_BOOLEAN;
            case "integer":
                return value.typeCode() == NT_INT;
            case "number":
                return value.typeCode() == NT_INT
                    || value.typeCode() == NT_FLOAT
                    || value.typeCode() == NT_NUMBER;
            case "null":
                return value.typeCode() == NT_NOTHING;
            default:
                return True;
        }
    }

    private static *string extractBalancedJsonPrefix(string text) {
        string trimmed = trim(text);
        if (!trimmed.val() || (trimmed[0] != "{" && trimmed[0] != "[")) {
            return NOTHING;
        }

        list<string> stack = ();
        bool in_string = False;
        bool escaped = False;
        for (int i = 0; i < trimmed.length(); ++i) {
            string c = trimmed[i];
            if (in_string) {
                if (escaped) {
                    escaped = False;
                } else if (c == "\\") {
                    escaped = True;
                } else if (c == "\"") {
                    in_string = False;
                }
                continue;
            }
            if (c == "\"") {
                in_string = True;
            } else if (c == "{" || c == "[") {
                push stack, c;
            } else if (c == "}" || c == "]") {
                if (!stack) {
                    return NOTHING;
                }
                string opener = pop stack;
                if ((opener == "{" && c != "}")
                        || (opener == "[" && c != "]")) {
                    return NOTHING;
                }
                if (!stack) {
                    string prefix = trimmed.substr(0, i + 1);
                    string suffix = trim(trimmed.substr(i + 1));
                    return suffix.val() ? prefix : trimmed;
                }
            }
        }
        return NOTHING;
    }

    private static hash<LlmStructuredRepairResult> coerceBySchemaIntern(auto value,
            hash<auto> schema, string path) {
        *string schema_type = getSchemaType(schema);
        if (schema_type == "object") {
            bool repaired = False;
            auto object_value = value;
            if (object_value.typeCode() == NT_STRING) {
                hash<LlmStructuredRepairResult> parsed =
                    parseJsonObject(object_value);
                if (!parsed.valid) {
                    return makeInvalid(parsed.error_code,
                        sprintf("%s at %s", parsed.error_detail, path));
                }
                object_value = parsed.value;
                repaired = True;
            }
            if (object_value.typeCode() != NT_HASH) {
                return makeInvalid("schema-type-mismatch",
                    sprintf("expected object at %s but received %s",
                        path, object_value.type()));
            }

            hash<auto!> out;
            out += cast<hash<auto>>(object_value);
            if (schema.properties.typeCode() == NT_HASH) {
                hash<auto> properties = cast<hash<auto>>(schema.properties);
                foreach auto p in (properties.pairIterator()) {
                    if (!exists out{p.key}
                            || p.value.typeCode() != NT_HASH) {
                        continue;
                    }
                    hash<LlmStructuredRepairResult> child =
                        coerceBySchemaIntern(out{p.key},
                            cast<hash<auto>>(p.value), path + "." + p.key);
                    if (!child.valid) {
                        return child;
                    }
                    if (child.repaired) {
                        out{p.key} = child.value;
                        repaired = True;
                    }
                }
            }
            return makeValid(out, repaired, repaired
                ? "schema-json-string" : NOTHING);
        }

        if (schema_type == "array") {
            bool repaired = False;
            auto array_value = value;
            if (array_value.typeCode() == NT_STRING) {
                hash<LlmStructuredRepairResult> parsed =
                    parseJsonArray(array_value);
                if (!parsed.valid) {
                    return makeInvalid(parsed.error_code,
                        sprintf("%s at %s", parsed.error_detail, path));
                }
                array_value = parsed.value;
                repaired = True;
            }
            if (array_value.typeCode() != NT_LIST) {
                return makeInvalid("schema-type-mismatch",
                    sprintf("expected array at %s but received %s",
                        path, array_value.type()));
            }

            list<auto> out = cast<list<auto>>(array_value);
            if (schema.items.typeCode() == NT_HASH) {
                hash<auto> item_schema = cast<hash<auto>>(schema.items);
                for (int i = 0; i < out.size(); ++i) {
                    hash<LlmStructuredRepairResult> child =
                        coerceBySchemaIntern(out[i], item_schema,
                            sprintf("%s[%d]", path, i));
                    if (!child.valid) {
                        return child;
                    }
                    if (child.repaired) {
                        out[i] = child.value;
                        repaired = True;
                    }
                }
            }
            return makeValid(out, repaired, repaired
                ? "schema-json-string" : NOTHING);
        }

        return makeValid(value, False, NOTHING);
    }

    private static *string getSchemaType(hash<auto> schema) {
        if (schema.type.typeCode() == NT_STRING) {
            return schema.type;
        }
        if (schema.type.typeCode() == NT_LIST) {
            foreach auto t in (schema.type) {
                if (t.typeCode() == NT_STRING && t != "null") {
                    return t;
                }
            }
        }
        if (schema.properties.typeCode() == NT_HASH) {
            return "object";
        }
        if (schema.items.typeCode() == NT_HASH) {
            return "array";
        }
        return NOTHING;
    }
}

#! Pluggable LLM backend facade for chat completions
/** Provides a provider-neutral interface for making LLM chat completion
    calls across multiple providers.  The constructor looks up the provider
    factory in @ref LlmProviderRegistry by @ref LlmBackendConfig::mode and
    delegates all per-mode behavior to the resulting
    @ref AbstractLlmProvider instance.

    Built-in modes (after the matching REST client module is loaded):

    - \b http: OpenAI-compatible \c /v1/chat/completions
      (@ref OpenAiRestClient)
    - \b sglang: SGLang's OpenAI-compatible endpoint
      (@ref SGLangRestClient)
    - \b anthropic: Anthropic Messages API
      (@ref AnthropicRestClient)
    - \b gemini: Google Gemini \c generateContent
      (@ref GeminiRestClient)
    - \b callback: caller-supplied dispatcher (built into QoreLlmUtils)

    Additional modes can be added by registering new factories via
    @ref LlmProviderRegistry::registerFactory().

    Thread-safe: provider implementations are responsible for any
    underlying connection lifecycle and concurrency.

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
        #! The backend mode string (matches @ref LlmBackendConfig::mode)
        string mode;
        #! The provider implementation looked up from @ref LlmProviderRegistry
        AbstractLlmProvider provider;
    }

    #! Creates an LlmBackend with the given configuration
    /** Resolves the provider factory for @ref LlmBackendConfig::mode via
        @ref LlmProviderRegistry::getFactoryEx(); the resulting provider
        is responsible for any further configuration validation.

        @param config an @ref LlmBackendConfig hash specifying the backend
            and parameters

        @throw LLM-CONFIG-ERROR the mode is unknown, the provider's required
            fields are missing, or the provider module cannot be loaded
        @throw LLM-DEPENDENCY-ERROR required modules are not available
    */
    constructor(hash<LlmBackendConfig> config) {
        mode = config.mode;
        provider = LlmProviderRegistry::getFactoryEx(mode).create(config);
    }

    /** @return the configured backend mode string */
    string getMode() {
        return mode;
    }

    /** @return the configured model name, or @ref nothing if none was set */
    *string getModel() {
        return provider.getModel();
    }

    /** @return the underlying @ref AbstractLlmProvider instance — exposed
        for advanced callers that need provider-specific metadata; in
        normal use the facade methods on this class are sufficient
    */
    AbstractLlmProvider getProvider() {
        return provider;
    }

    #! Makes a single-turn completion call
    /** @param system_prompt the system prompt that defines the LLM's role
        @param user_prompt the user message / input text
        @param output_schema optional JSON schema for structured output
        @param opts per-call request overrides

        @return an @ref LlmCompletionResult hash with content and usage

        @throw LLM-ERROR the completion request failed
    */
    hash<LlmCompletionResult> complete(string system_prompt, string user_prompt,
            *hash<auto> output_schema, *hash<LlmRequestOptions> opts) {
        return provider.complete(system_prompt, user_prompt, output_schema,
            opts);
    }

    #! Multi-turn chat with optional tool calling
    /** Sends a list of messages (including conversation history and tool
        results) to the LLM, optionally with tool definitions that the LLM
        can choose to call.

        @param messages list of @ref ChatMessage hashes forming the conversation
        @param tools optional list of @ref ToolDefinition hashes the LLM can call
        @param opts per-call request overrides

        @return a @ref ChatResponse hash with stop_reason, content, and optional tool_calls

        @throw LLM-ERROR the chat request failed
    */
    hash<ChatResponse> chat(list<hash<ChatMessage>> messages,
            *list<hash<ToolDefinition>> tools, *hash<LlmRequestOptions> opts) {
        return provider.chat(messages, tools, opts);
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
        @param opts per-call request overrides

        @throw LLM-ERROR the request failed or streaming is not supported
            by the provider

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
        provider.chatStream(messages, callback, tools, opts);
    }

    #! OpenAI-compatible chat-completions facade
    /** Accepts an OpenAI \c /v1/chat/completions-style request body, dispatches
        it through this backend's native provider, and returns an
        OpenAI-compatible response hash.  This lets applications keep one
        external wire contract while using provider-native backends such as
        Gemini internally.

        @param body OpenAI-compatible chat-completions request

        @return OpenAI-compatible chat-completions response

        @throw LLM-ERROR the backend call failed
    */
    hash<auto> chatCompletions(hash<auto> body) {
        hash<auto> parsed = parseOpenAiChatCompletionsRequest(body);
        list<hash<ChatMessage>> messages =
            cast<list<hash<ChatMessage>>>(parsed.messages);
        *list<hash<ToolDefinition>> tools =
            cast<*list<hash<ToolDefinition>>>(parsed.tools);
        hash<LlmRequestOptions> options =
            cast<hash<LlmRequestOptions>>(parsed.options);
        *string forced_tool_name = getForcedOpenAiToolName(body, tools);
        hash<ChatResponse> response = chat(messages, tools, options);
        hash<auto> rv = synthesizeOpenAiForcedToolCall(chatResponseToOpenAi(
            response, options.model ?? provider.getModel(), tools),
            forced_tool_name, tools);
        if (shouldRetryForcedToolAsJson(rv, forced_tool_name)) {
            *hash<auto> retry = retryForcedToolAsJson(messages, tools, options,
                forced_tool_name);
            if (retry && openAiResponseHasToolCall(retry)) {
                return retry;
            }
        }
        if (shouldRetryRequiredToolNoOutput(rv, body, tools)) {
            *hash<auto> retry = retryRequiredToolNoOutput(messages, tools,
                options, forced_tool_name);
            if (retry && openAiResponseHasToolCall(retry)) {
                return retry;
            }
        }
        return rv;
    }

    #! Streaming OpenAI-compatible chat-completions facade
    /** Accepts an OpenAI \c /v1/chat/completions-style request body, streams
        OpenAI-compatible chunks to @a delta_cb, and returns an accumulated
        OpenAI-compatible final response.

        @param body OpenAI-compatible chat-completions request
        @param delta_cb callback receiving OpenAI-compatible stream chunks

        @return accumulated OpenAI-compatible final response

        @throw LLM-ERROR the backend call failed
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
            ?? provider.getModel());
        chatStream(messages, sub (hash<ChatStreamEvent> event) {
            *hash<auto> chunk = chatStreamEventToOpenAiChunk(\state, event);
            if (chunk) {
                delta_cb(chunk);
            }
        }, tools, options);
        hash<auto> rv = synthesizeOpenAiForcedToolCall(
            streamStateToOpenAiResponse(state),
            getForcedOpenAiToolName(body, tools), tools);
        *string forced_tool_name = getForcedOpenAiToolName(body, tools);
        if (shouldRetryForcedToolAsJson(rv, forced_tool_name)) {
            *hash<auto> retry = retryForcedToolAsJson(messages, tools, options,
                forced_tool_name);
            if (retry && openAiResponseHasToolCall(retry)) {
                return retry;
            }
        }
        if (shouldRetryRequiredToolNoOutput(rv, body, tools)) {
            *hash<auto> retry = retryRequiredToolNoOutput(messages, tools,
                options, forced_tool_name);
            if (retry && openAiResponseHasToolCall(retry)) {
                return retry;
            }
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

    #! Parses an OpenAI-compatible request for provider-native dispatch
    private hash<auto> parseOpenAiChatCompletionsRequest(hash<auto> body) {
        hash<LlmRequestOptions> options_h = <LlmRequestOptions>{};
        if (body.model.val()) {
            options_h.model = body.model;
        }
        if (exists body.max_output_tokens) {
            options_h.max_output_tokens = body.max_output_tokens;
        } else if (exists body.max_completion_tokens) {
            options_h.max_output_tokens = body.max_completion_tokens;
        } else if (exists body.max_tokens) {
            options_h.max_tokens = body.max_tokens;
        }
        if (exists body.reasoning_token_budget) {
            options_h.reasoning_token_budget = body.reasoning_token_budget;
        }
        if (exists body.temperature) {
            options_h.temperature = body.temperature;
        }

        hash<auto!> body_options;
        if (body.generationConfig.typeCode() == NT_HASH) {
            body_options.generationConfig = body.generationConfig;
        }
        if (exists body.tool_choice) {
            provider.applyOpenAiToolChoice(\body_options, body.tool_choice);
        }
        *hash<auto> output_schema = getOpenAiResponseSchema(body);
        if (output_schema) {
            provider.applyOpenAiResponseFormat(\body_options, \options_h,
                output_schema, body.response_format);
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
                        if (part.cache_control.typeCode() == NT_HASH) {
                            cp.cache_control = part.cache_control;
                        }
                        push parts, cp;
                    } else if (part.type == "image_url") {
                        *hash<ContentPart> cp =
                            dataUrlToContentPart(part.image_url.url, "image",
                                NOTHING);
                        if (cp) {
                            push parts, cp;
                        }
                    } else if (part.type == "file") {
                        *hash<ContentPart> cp =
                            dataUrlToContentPart(part."file".file_data,
                                "file", part."file".filename);
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
                    "cache_control": tool.cache_control,
                };
            }
        }
        return tools ? tools : NOTHING;
    }

    private static *hash<ContentPart> dataUrlToContentPart(*string url,
            string part_type, *string filename) {
        if (!url.val()) {
            return NOTHING;
        }
        *list<*string> m = regex_extract(url,
            "^data:([^;]+);base64,(.*)$");
        if (!m || !m[0].val() || !m[1].val()) {
            return NOTHING;
        }
        try {
            hash<ContentPart> rv = <ContentPart>{
                "type": part_type,
                "mime_type": m[0],
                "data": parse_base64_string(m[1]),
            };
            if (filename.val()) {
                rv.filename = filename;
            }
            return rv;
        } catch () {
            return NOTHING;
        }
    }

    private static *hash<auto> parseOpenAiToolArguments(auto raw) {
        hash<LlmStructuredRepairResult> result =
            LlmStructuredRepair::parseJsonObject(raw);
        return result.valid ? cast<hash<auto>>(result.value) : {};
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

    private static hash<auto> chatResponseToOpenAi(hash<ChatResponse> response,
            *string requested_model, *list<hash<ToolDefinition>> tools) {
        hash<auto!> message;
        message.role = "assistant";
        message.content = response.content;
        if (response.tool_calls) {
            list<hash<auto>> tool_calls = ();
            foreach hash<ChatToolCall> tc in (response.tool_calls) {
                push tool_calls, chatToolCallToOpenAi(tc, tools);
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
        if (response.provider_diagnostics.typeCode() == NT_HASH) {
            rv.provider_diagnostics = response.provider_diagnostics;
        }
        return rv;
    }

    private static hash<auto> chatToolCallToOpenAi(hash<ChatToolCall> tc,
            *list<hash<ToolDefinition>> tools) {
        hash<auto> args = coerceToolArguments(tc.name, tc.arguments ?? {},
            tools);
        return {
            "id": tc.id,
            "type": "function",
            "function": {
                "name": tc.name,
                "arguments": make_json(args),
            },
        };
    }

    private static *string getForcedOpenAiToolName(hash<auto> body,
            *list<hash<ToolDefinition>> tools) {
        if (!exists body.tool_choice) {
            return NOTHING;
        }
        auto choice = body.tool_choice;
        if (choice.typeCode() == NT_HASH && choice.function.name.val()) {
            return choice.function.name;
        }
        if (choice.typeCode() == NT_STRING && choice == "required"
                && tools && tools.size() == 1) {
            return tools[0].name;
        }
        return NOTHING;
    }

    private static hash<auto> synthesizeOpenAiForcedToolCall(
            hash<auto> response, *string forced_tool_name,
            *list<hash<ToolDefinition>> tools) {
        if (!forced_tool_name.val()
                || response.choices.typeCode() != NT_LIST
                || !response.choices) {
            return response;
        }
        list<auto> choices = cast<list<auto>>(response.choices);
        if (choices[0].typeCode() != NT_HASH) {
            return response;
        }
        hash<auto> choice = cast<hash<auto>>(choices[0]);
        if (choice.message.typeCode() != NT_HASH) {
            return response;
        }
        hash<auto> message = cast<hash<auto>>(choice.message);
        if (message.tool_calls) {
            return response;
        }
        *hash<auto> args = parseOpenAiForcedToolContent(message.content);
        if (!args) {
            return response;
        }
        args = coerceToolArguments(forced_tool_name, args, tools);

        hash<auto!> new_message;
        new_message += message;
        new_message.tool_calls = ({
            "id": "forced_tool_call_1",
            "type": "function",
            "function": {
                "name": forced_tool_name,
                "arguments": make_json(args),
            },
        },);

        hash<auto!> new_choice;
        new_choice += choice;
        new_choice.message = new_message;
        new_choice.finish_reason = "tool_calls";
        choices[0] = new_choice;

        hash<auto!> rv;
        rv += response;
        rv.choices = choices;
        return rv;
    }

    private *hash<auto> retryForcedToolAsJson(
            list<hash<ChatMessage>> messages,
            *list<hash<ToolDefinition>> tools,
            hash<LlmRequestOptions> options, *string forced_tool_name) {
        if (!forced_tool_name.val()) {
            return NOTHING;
        }
        *hash<ToolDefinition> forced_tool = findToolDefinition(tools,
            forced_tool_name);
        if (!forced_tool || forced_tool.parameters.typeCode() != NT_HASH) {
            return NOTHING;
        }

        list<hash<ChatMessage>> retry_messages = messages;
        push retry_messages, <ChatMessage>{
            "role": "system",
            "content": "The previous forced function-call response was "
                "malformed. Return only a JSON object matching the requested "
                "function arguments schema; do not call a tool.",
        };

        hash<LlmRequestOptions> retry_options = options;
        retry_options.output_schema = forced_tool.parameters;
        retry_options.body_options = stripForcedToolBodyOptions(
            options.body_options);

        hash<ChatResponse> retry_response = chat(retry_messages, NOTHING,
            retry_options);
        return synthesizeOpenAiForcedToolCall(chatResponseToOpenAi(
            retry_response, retry_options.model ?? provider.getModel(), tools),
            forced_tool_name, tools);
    }

    private *hash<auto> retryRequiredToolNoOutput(
            list<hash<ChatMessage>> messages,
            *list<hash<ToolDefinition>> tools,
            hash<LlmRequestOptions> options, *string forced_tool_name) {
        if (!tools) {
            return NOTHING;
        }
        list<hash<ChatMessage>> retry_messages = messages;
        push retry_messages, <ChatMessage>{
            "role": "system",
            "content": "The previous response returned no content and no "
                "function call even though a tool call was required. Call "
                "exactly one available function now and provide valid JSON "
                "arguments for that function.",
        };

        hash<ChatResponse> retry_response = chat(retry_messages, tools,
            options);
        return synthesizeOpenAiForcedToolCall(chatResponseToOpenAi(
            retry_response, options.model ?? provider.getModel(), tools),
            forced_tool_name, tools);
    }

    private static *hash<ToolDefinition> findToolDefinition(
            *list<hash<ToolDefinition>> tools, string name) {
        if (!tools) {
            return NOTHING;
        }
        foreach hash<ToolDefinition> tool in (tools) {
            if (tool.name == name) {
                return tool;
            }
        }
        return NOTHING;
    }

    private static hash<auto> coerceToolArguments(string tool_name,
            hash<auto> args, *list<hash<ToolDefinition>> tools) {
        *hash<ToolDefinition> tool = findToolDefinition(tools, tool_name);
        if (!tool || tool.parameters.typeCode() != NT_HASH) {
            return args;
        }
        hash<LlmStructuredRepairResult> result =
            LlmStructuredRepair::coerceBySchema(args, tool.parameters);
        return result.valid ? cast<hash<auto>>(result.value) : args;
    }

    private static *hash<auto> stripForcedToolBodyOptions(
            *hash<auto> body_options) {
        if (!body_options) {
            return NOTHING;
        }
        hash<auto!> rv;
        rv += body_options;
        remove rv.tool_choice;
        remove rv.toolConfig;
        return rv ? rv : NOTHING;
    }

    private static bool shouldRetryForcedToolAsJson(hash<auto> response,
            *string forced_tool_name) {
        if (!forced_tool_name.val() || openAiResponseHasToolCall(response)
                || response.choices.typeCode() != NT_LIST
                || !response.choices) {
            return False;
        }
        hash<auto> choice = cast<hash<auto>>(
            cast<list<auto>>(response.choices)[0]);
        string finish_reason = choice.finish_reason ?? "";
        return finish_reason.upr() == "MALFORMED_FUNCTION_CALL";
    }

    private static bool shouldRetryRequiredToolNoOutput(hash<auto> response,
            hash<auto> request, *list<hash<ToolDefinition>> tools) {
        if (!isOpenAiToolChoiceRequired(request.tool_choice)
                || !tools
                || openAiResponseHasToolCall(response)
                || response.choices.typeCode() != NT_LIST
                || !response.choices) {
            return False;
        }
        hash<auto> choice = cast<hash<auto>>(
            cast<list<auto>>(response.choices)[0]);
        hash<auto> message = choice.message.typeCode() == NT_HASH
            ? cast<hash<auto>>(choice.message) : {};
        if (message.content.val()) {
            return False;
        }
        string finish_reason = (choice.finish_reason ?? "").toString();
        if (finish_reason == "content_filter"
                || finish_reason == "length"
                || finish_reason.upr() == "MALFORMED_FUNCTION_CALL") {
            return False;
        }
        hash<auto> usage = response.usage.typeCode() == NT_HASH
            ? cast<hash<auto>>(response.usage) : {};
        int output_tokens = (usage.completion_tokens
            ?? usage.output_tokens ?? 0).toInt();
        return output_tokens == 0 || finish_reason == "stop"
            || !finish_reason.val();
    }

    private static bool isOpenAiToolChoiceRequired(auto choice) {
        if (!exists choice) {
            return False;
        }
        return choice == "required"
            || (choice.typeCode() == NT_HASH
                && choice.function.name.val());
    }

    private static bool openAiResponseHasToolCall(hash<auto> response) {
        if (response.choices.typeCode() != NT_LIST || !response.choices) {
            return False;
        }
        hash<auto> choice = cast<hash<auto>>(
            cast<list<auto>>(response.choices)[0]);
        return choice.message.tool_calls.typeCode() == NT_LIST
            && choice.message.tool_calls.size() > 0;
    }

    private static *hash<auto> parseOpenAiForcedToolContent(*string content) {
        if (!content.val()) {
            return NOTHING;
        }
        hash<LlmStructuredRepairResult> result =
            LlmStructuredRepair::parseJsonObject(content);
        return result.valid ? cast<hash<auto>>(result.value) : NOTHING;
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

    private *hash<auto> chatStreamEventToOpenAiChunk(
            reference<hash<auto>> state, hash<ChatStreamEvent> event) {
        if (event.raw) {
            provider.updateOpenAiStreamStateFromRaw(\state, event.raw);
        }
        hash<auto!> delta;
        if (!event.done && event.delta.val()) {
            state.content += event.delta;
            delta.content = event.delta;
        }
        list<hash<auto>> streamed = provider.getOpenAiToolCallDeltas(\state,
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
}

} # namespace QoreLlmUtils
