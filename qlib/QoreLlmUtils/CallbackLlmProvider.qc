# -*- mode: qore; indent-tabs-mode: nil -*-

/*  CallbackLlmProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace QoreLlmUtils {

#! LLM provider that dispatches requests through a caller-supplied callback
/** The callback receives an OpenAI-compatible request body and is expected
    to return an OpenAI-compatible response hash.  This mode is used when
    an application gateway, tenant proxy, or metered AI router intercepts
    LLM traffic between the @ref LlmBackend and the underlying provider.

    Streaming is supported when @ref LlmBackendConfig::api_stream_callback
    is set: the provider invokes it with a per-chunk delta callback and
    forwards each OpenAI-shaped chunk to the caller as a
    @ref QoreLlmUtils::ChatStreamEvent.  When @ref api_stream_callback is
    unset, @ref chatStream() falls back to a single @ref chat() call and
    surfaces it as one synthetic content delta plus a terminal \c done
    event — so callers always see a coherent event sequence.
*/
public class CallbackLlmProvider inherits AbstractLlmProvider {
    private {
        #! The caller-supplied dispatcher
        code<hash<auto>(string, string, hash<auto>)> api_callback;
        #! HTTP method hint passed to the dispatcher (default \c "POST")
        string callback_method;
        #! Endpoint/path hint passed to the dispatcher
        string callback_path;
    }

    #! Builds the provider from an @ref LlmBackendConfig
    /** @param config the backend configuration; @ref LlmBackendConfig::api_callback
            is required

        @throw LLM-CONFIG-ERROR \c api_callback is missing
    */
    constructor(hash<LlmBackendConfig> config) : AbstractLlmProvider(config) {
        if (!config.api_callback) {
            throw "LLM-CONFIG-ERROR",
                "api_callback is required for callback mode";
        }
        api_callback = config.api_callback;
        callback_method = config.callback_method;
        callback_path = config.callback_path;
    }

    string getMode() {
        return "callback";
    }

    hash<LlmCompletionResult> complete(string system_prompt, string user_prompt,
            *hash<auto> output_schema, *hash<LlmRequestOptions> opts) {
        hash<auto> body = {
            "messages": (
                {"role": "system", "content": system_prompt},
                {"role": "user", "content": user_prompt},
            ),
            "max_tokens": getRequestMaxOutputTokens(opts),
            "temperature": getRequestTemperature(opts),
        };
        *int reasoning_budget = getRequestReasoningTokenBudget(opts);
        if (reasoning_budget.typeCode() != NT_NOTHING) {
            body.reasoning_token_budget = reasoning_budget;
        }
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
            throw "LLM-ERROR", sprintf("callback API call failed: %s",
                ex.desc), ex;
        }

        return <LlmCompletionResult>{
            "content": resp.choices[0].message.content ?? "",
            "usage": resp.usage ?? {},
        };
    }

    hash<ChatResponse> chat(list<hash<ChatMessage>> messages,
            *list<hash<ToolDefinition>> tools, *hash<LlmRequestOptions> opts) {
        hash<auto> body = buildOpenAiChatBody(messages, tools, opts);

        hash<auto> resp;
        try {
            resp = api_callback(callback_method, callback_path, body);
        } catch (hash<ExceptionInfo> ex) {
            throw "LLM-ERROR", sprintf("callback chat request failed: %s",
                ex.desc), ex;
        }

        return parseOpenAiChatResponse(resp);
    }

    #! Streams a chat completion through the caller-supplied stream callback
    /** When @ref LlmBackendConfig::api_stream_callback is set, builds the
        OpenAI-shaped chat-completions body, dispatches it through the
        caller's stream callback, and forwards each OpenAI-shaped chunk
        the callback emits as a @ref QoreLlmUtils::ChatStreamEvent to
        @a callback.

        When @ref api_stream_callback is unset, falls back to the
        non-streaming @ref chat() implementation and emits the result as
        a single synthetic content delta event followed by a terminal
        \c done event.  This preserves the contract that
        @ref chatStream() always fires at least one @c done event so the
        caller's accumulator terminates cleanly.

        @param messages list of @ref ChatMessage hashes
        @param callback receives each @ref ChatStreamEvent as it arrives,
            with \c done=True on the final event
        @param tools optional tool definitions
        @param opts per-call request overrides

        @throw LLM-ERROR the upstream call failed

        @par Example: route an agent's streaming chat through an
        application-level gateway that adds tenant headers and forwards
        the SSE response to the live UI

        @code{.py}
LlmBackend backend(<LlmBackendConfig>{
    "mode": "callback",
    "api_callback": hash<auto> sub (string m, string p, hash<auto> body) {
        # Non-streaming fallback path; required for the agent loop's
        # max-iterations safety net
        return tenant_gateway.dispatch(body, NOTHING);
    },
    "api_stream_callback": sub (string m, string p, hash<auto> body, code<nothing(hash<auto>)> dcb) {
        # Forward each upstream chunk to the caller; the gateway already
        # parses SSE and surfaces structured chunks
        tenant_gateway.dispatch(body, sub (hash<auto> chunk) { dcb(chunk); });
    },
});
backend.chatStream(history,
    sub (hash<ChatStreamEvent> ev) {
        if (ev.delta.val()) {
            ui.append(ev.delta);
        }
        if (ev.done) {
            ui.finish();
        }
    });
        @endcode
    */
    chatStream(list<hash<ChatMessage>> messages,
            code<nothing(hash<ChatStreamEvent>)> callback,
            *list<hash<ToolDefinition>> tools, *hash<LlmRequestOptions> opts) {
        if (!config_h.api_stream_callback) {
            # Fallback: single non-streamed call surfaced as one delta +
            # terminal done.  Mirrors the contract a real streaming
            # provider would produce so the caller's event accumulator
            # always sees a coherent sequence.
            hash<ChatResponse> resp = chat(messages, tools, opts);
            if (resp.content.val()) {
                callback(<ChatStreamEvent>{
                    "delta": resp.content,
                    "done": False,
                });
            }
            # Synthesize an OpenAI-shaped terminal chunk so callers that
            # accumulate via `event.raw` see a consistent shape.
            hash<auto> terminal_raw = {
                "object": "chat.completion.chunk",
                "choices": ({
                    "index": 0,
                    "delta": {},
                    "finish_reason": resp.tool_calls
                        ? "tool_calls" : "stop",
                },),
            };
            if (resp.id.val()) {
                terminal_raw.id = resp.id;
            }
            if (resp.model.val()) {
                terminal_raw.model = resp.model;
            }
            if (resp.usage) {
                terminal_raw.usage = resp.usage;
            }
            callback(<ChatStreamEvent>{
                "done": True,
                "raw": terminal_raw,
            });
            return;
        }

        hash<auto> body = buildOpenAiChatBody(messages, tools, opts);
        body.stream = True;
        body.stream_options = {"include_usage": True};

        bool saw_terminal = False;
        try {
            config_h.api_stream_callback(callback_method, callback_path,
                body, sub (hash<auto> chunk) {
                    *hash<ChatStreamEvent> event =
                        openAiChunkToChatStreamEvent(chunk);
                    if (event) {
                        if (event.done) {
                            saw_terminal = True;
                        }
                        callback(event);
                    }
                });
        } catch (hash<ExceptionInfo> ex) {
            throw "LLM-ERROR", sprintf("callback chat stream failed: %s",
                ex.desc), ex;
        }

        # Safety net for malformed providers that close the stream
        # without an OpenAI-style finish_reason chunk: ensure the
        # caller's accumulator always sees a terminal `done` event so it
        # never deadlocks waiting for one.  Skipped when the upstream
        # already emitted a terminal chunk (the common case).
        if (!saw_terminal) {
            callback(<ChatStreamEvent>{"done": True});
        }
    }

    #! Translates an OpenAI-shaped streaming chunk into a @ref ChatStreamEvent
    /** Maps the conventional OpenAI \c chat.completion.chunk shape
        (\c choices[0].delta.content + \c choices[0].finish_reason) into
        @ref ChatStreamEvent fields.  Tool-call deltas, usage, and any
        other provider-specific metadata stay on @ref ChatStreamEvent::raw
        so caller-side accumulators can introspect them without losing
        information.

        @return @ref nothing for chunks that carry no actionable signal
            (no content delta, no terminal flag — typically the initial
            \c role assistant chunk OpenAI emits before content); a
            populated event otherwise
    */
    private static *hash<ChatStreamEvent> openAiChunkToChatStreamEvent(hash<auto> chunk) {
        *hash<auto> choice = chunk.choices[0];
        *string content_delta = choice.delta.content;
        bool done = choice.finish_reason.val() ? True : False;
        # Suppress completely empty intermediate chunks (e.g. the role
        # frame OpenAI emits first).  Tool-call deltas reach the caller
        # via `raw` so they aren't lost.
        bool has_tool_call_delta = choice.delta.tool_calls
            && choice.delta.tool_calls.typeCode() == NT_LIST
            && choice.delta.tool_calls.size() > 0;
        if (!content_delta.val() && !done && !has_tool_call_delta) {
            return;
        }
        return <ChatStreamEvent>{
            "delta": content_delta,
            "raw": chunk,
            "done": done,
        };
    }

    #! Builds an OpenAI-shaped chat-completions request body
    private hash<auto> buildOpenAiChatBody(list<hash<ChatMessage>> messages,
            *list<hash<ToolDefinition>> tools,
            *hash<LlmRequestOptions> opts) {
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
            "max_tokens": getRequestMaxOutputTokens(opts),
            "temperature": getRequestTemperature(opts),
        };
        *int reasoning_budget = getRequestReasoningTokenBudget(opts);
        if (reasoning_budget.typeCode() != NT_NOTHING) {
            body.reasoning_token_budget = reasoning_budget;
        }
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

    #! Parses an OpenAI-shaped chat-completions response into a @ref ChatResponse
    private static hash<ChatResponse> parseOpenAiChatResponse(hash<auto> resp) {
        hash<auto> choice = resp.choices[0];
        hash<auto> msg = choice.message;

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
            "id": resp.id,
            "model": resp.model,
            "stop_reason": stop_reason,
            "content": msg.content,
            "tool_calls": parsed_tool_calls,
            "usage": resp.usage ?? {},
            "raw": resp,
        };
    }
}

#! Factory for @ref CallbackLlmProvider
public class CallbackLlmProviderFactory inherits AbstractLlmProviderFactory {
    string getMode() {
        return "callback";
    }

    string getDisplayName() {
        return "Caller-supplied callback dispatcher";
    }

    AbstractLlmProvider create(hash<LlmBackendConfig> config) {
        return new CallbackLlmProvider(config);
    }

    hash<LlmProviderInfo> getInfo() {
        # Streaming support depends on whether the caller populates
        # `api_stream_callback` on the LlmBackendConfig.  The static
        # `supports_streaming` flag reflects the capability of the
        # provider class (`chatStream()` is implemented and produces a
        # coherent event sequence even when the stream callback is unset
        # — falling back to a single delta + done from the sync chat()).
        return <LlmProviderInfo>{
            "mode": getMode(),
            "display_name": getDisplayName(),
            "module_name": Class::getClass(self).getModuleName(),
            "supports_streaming": True,
            "supports_tools": True,
            "supports_structured_output": True,
            "supports_multimodal": True,
        };
    }
}

} # namespace QoreLlmUtils
