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

    Streaming is not supported in callback mode — use @ref chat()
    or have the gateway expose streaming through a different transport.
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
        return <LlmProviderInfo>{
            "mode": getMode(),
            "display_name": getDisplayName(),
            "module_name": Class::getClass(self).getModuleName(),
            "supports_streaming": False,
            "supports_tools": True,
            "supports_structured_output": True,
            "supports_multimodal": True,
        };
    }
}

} # namespace QoreLlmUtils
