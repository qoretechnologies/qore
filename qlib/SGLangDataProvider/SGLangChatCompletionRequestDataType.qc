# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SGLangDataProvider module definition

/** SGLangChatCompletionRequestDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the SGLangDataProvider module
public namespace SGLangDataProvider {
#! Data type for a single tool call produced by a chat assistant message
public class SGLangChatToolCallDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "id": {
                "display_name": "Tool Call ID",
                "short_desc": "Unique identifier for the tool call",
                "desc": "Unique identifier for this tool call.  The same `id` must be echoed back in "
                    "the corresponding `tool` role message that returns the tool's result.",
                "type": AbstractDataProviderTypeMap."string",
            },
            "type": {
                "display_name": "Tool Type",
                "short_desc": "Type of tool invoked",
                "desc": "Type of tool invoked.  Currently `function` is the only supported type for "
                    "OpenAI-compatible tool calling.",
                "type": AbstractDataProviderTypeMap."string",
                "example_value": "function",
            },
            "function": {
                "display_name": "Function Call",
                "short_desc": "Function name and JSON-encoded arguments",
                "desc": "Hash with `name` (string) and `arguments` (JSON-encoded string) describing the "
                    "function the model wants to invoke.",
                "type": AbstractDataProviderTypeMap."hash",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data type for a single chat message (system, user, assistant, or tool role)
public class SGLangChatMessageDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "role": {
                "display_name": "Role",
                "short_desc": "The role of the message author",
                "desc": "The role of the author of this message.  Determines how the message is "
                    "interpreted by the model.\n\n"
                    "- `system` — high-level instructions and persona for the assistant\n"
                    "- `user` — input from the end user\n"
                    "- `assistant` — previous model output (used for multi-turn conversations)\n"
                    "- `tool` — result returned by a tool invocation (paired with a `tool_call_id`)",
                "type": AbstractDataProviderTypeMap."string",
                "example_value": "user",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "system",
                        "display_name": "System - instructions and persona"},
                    <AllowedValueInfo>{"value": "user",
                        "display_name": "User - end-user input"},
                    <AllowedValueInfo>{"value": "assistant",
                        "display_name": "Assistant - previous model output"},
                    <AllowedValueInfo>{"value": "tool",
                        "display_name": "Tool - tool invocation result"},
                ),
            },
            "content": {
                "display_name": "Content",
                "short_desc": "The text content of the message",
                "desc": "The text content of the message.  May be `NOTHING` on assistant messages that "
                    "only contain `tool_calls`.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "name": {
                "display_name": "Author Name",
                "short_desc": "Optional name of the author",
                "desc": "Optional name identifying the specific author of the message.  May be used "
                    "with `user` or `tool` roles to distinguish multiple participants.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "tool_call_id": {
                "display_name": "Tool Call ID",
                "short_desc": "ID of the tool call this message responds to",
                "desc": "Required when `role` is `tool` — identifies which of the preceding assistant "
                    "message's `tool_calls` this message is returning the result for.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "tool_calls": {
                "display_name": "Tool Calls",
                "short_desc": "Tool invocations emitted by the assistant",
                "desc": "List of tool calls emitted by the assistant.  Present only on assistant "
                    "messages that chose to invoke a tool.",
                "type": new ListDataType("SGLangChatToolCallList", new SGLangChatToolCallDataType(), True),
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data type for token usage reported in chat completion and embedding responses
public class SGLangUsageDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "prompt_tokens": {
                "display_name": "Prompt Tokens",
                "desc": "Number of tokens consumed by the prompt.",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "completion_tokens": {
                "display_name": "Completion Tokens",
                "desc": "Number of tokens emitted in the completion.",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "total_tokens": {
                "display_name": "Total Tokens",
                "desc": "Total number of tokens processed (prompt + completion).",
                "type": AbstractDataProviderTypeMap."*int",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data type for SGLang chat completion requests (OpenAI-compatible)
public class SGLangChatCompletionRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "messages": {
                "display_name": "Messages",
                "short_desc": "List of chat messages forming the conversation",
                "desc": "The conversation history as a list of message objects.  Each message has a "
                    "`role` (`system`, `user`, `assistant`, or `tool`) and `content` (text).\n\n"
                    "**Typical patterns**:\n"
                    "- Single user turn: `[{role: \"user\", content: \"...\"}]`\n"
                    "- With system prompt: `[{role: \"system\", content: \"...\"}, "
                    "{role: \"user\", content: \"...\"}]`\n"
                    "- Multi-turn: include prior `assistant` messages to preserve context\n"
                    "- Tool calling: `assistant` messages carry `tool_calls`; `tool` messages return "
                    "results keyed by `tool_call_id`",
                "type": new ListDataType("SGLangChatMessageList", new SGLangChatMessageDataType(), False),
                "required": True,
                "example_value": (
                    {
                        "role": "system",
                        "content": "You are a helpful assistant.",
                    },
                    {
                        "role": "user",
                        "content": "Summarize the theory of relativity in two sentences.",
                    },
                ),
            },
            "model": {
                "display_name": "Model",
                "short_desc": "Override the model served by the SGLang connection",
                "desc": "Name of the model to use for this request.  Optional; defaults to the model "
                    "configured on the SGLang connection via `getModel()`.\n\n"
                    "**Note**: SGLang serves exactly one model per server, so overriding `model` only "
                    "makes sense when routing to a multi-model gateway.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "temperature": {
                "display_name": "Temperature",
                "short_desc": "Sampling temperature (0-2); higher values give more varied output",
                "desc": "Controls randomness of the output.  Range `0.0` to `2.0`.\n\n"
                    "- `0.0` — deterministic (greedy decoding)\n"
                    "- `0.7` — balanced creativity (typical default for chat)\n"
                    "- `1.5`+ — highly varied / creative\n\n"
                    "Defaults to `1.0` on most SGLang deployments.",
                "type": AbstractDataProviderTypeMap."*softfloat",
                "example_value": 0.7,
            },
            "max_tokens": {
                "display_name": "Max Tokens",
                "short_desc": "Maximum tokens to emit in the completion",
                "desc": "Maximum number of tokens to generate in the completion.  The total token "
                    "count (prompt + completion) must fit within the model's context window; if it "
                    "doesn't, the server returns an error.",
                "type": AbstractDataProviderTypeMap."*softint",
                "example_value": 512,
            },
            "top_p": {
                "display_name": "Top-P",
                "short_desc": "Nucleus sampling cutoff (0-1)",
                "desc": "Nucleus sampling — restrict sampling to tokens whose cumulative probability "
                    "exceeds this threshold.  Range `0.0` to `1.0`.  Usually left at `1.0` and varied "
                    "only in combination with `top_k` or lower `temperature`.",
                "type": AbstractDataProviderTypeMap."*softfloat",
            },
            "top_k": {
                "display_name": "Top-K",
                "short_desc": "Sample only from the K most likely next tokens (SGLang extension)",
                "desc": "Restrict sampling to the K most likely next tokens.  SGLang-specific "
                    "extension to the OpenAI chat API; useful for reducing low-probability "
                    "token noise.  Set to `-1` or omit to disable.",
                "type": AbstractDataProviderTypeMap."*softint",
            },
            "frequency_penalty": {
                "display_name": "Frequency Penalty",
                "short_desc": "Penalize repeated tokens proportional to their frequency",
                "desc": "Penalizes tokens based on how often they have already appeared in the output. "
                    "Range `-2.0` to `2.0`.  Positive values reduce verbatim repetition.",
                "type": AbstractDataProviderTypeMap."*softfloat",
            },
            "presence_penalty": {
                "display_name": "Presence Penalty",
                "short_desc": "Penalize any token that has already appeared",
                "desc": "Penalizes tokens that have already appeared in the output, regardless of "
                    "frequency.  Range `-2.0` to `2.0`.  Positive values encourage topic diversity.",
                "type": AbstractDataProviderTypeMap."*softfloat",
            },
            "stop": {
                "display_name": "Stop Sequences",
                "short_desc": "String or list of strings; generation halts when emitted",
                "desc": "One or more stop sequences.  Generation halts as soon as the model emits "
                    "any of them.  Can be a single string or a list of strings.\n\n"
                    "**Examples**: `\"\\n\\n\"`, `[\"END\", \"STOP\"]`",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "stream": {
                "display_name": "Stream",
                "short_desc": "Stream partial completions as server-sent events",
                "desc": "When `True`, the server streams partial completions as Server-Sent Events "
                    "(SSE) instead of returning a single response.  Most data provider flows expect "
                    "non-streaming (`False` / omitted); enable only when wiring into a streaming "
                    "consumer.",
                "type": AbstractDataProviderTypeMap."*softbool",
            },
            "tools": {
                "display_name": "Tools",
                "short_desc": "OpenAI-style tool definitions the model may call",
                "desc": "List of tool definitions the model may invoke.  Each entry is a hash with a "
                    "`type` field (`function`) and a `function` hash containing `name`, `description`, "
                    "and a JSON-schema `parameters` object.\n\n"
                    "**Example**:\n"
                    "```\n"
                    "[{type: \"function\", function: {\n"
                    "  name: \"get_weather\",\n"
                    "  description: \"Get the weather for a location\",\n"
                    "  parameters: {type: \"object\", properties: {...}, required: [\"location\"]}\n"
                    "}}]\n"
                    "```",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "tool_choice": {
                "display_name": "Tool Choice",
                "short_desc": "Whether and which tool the model must invoke",
                "desc": "Controls tool invocation.\n\n"
                    "- `none` — never call a tool (text-only reply)\n"
                    "- `auto` — model decides (default when `tools` is set)\n"
                    "- `required` — model must call at least one tool",
                "type": AbstractDataProviderTypeMap."*softstring",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "none",
                        "display_name": "None - never call a tool"},
                    <AllowedValueInfo>{"value": "auto",
                        "display_name": "Auto - model decides"},
                    <AllowedValueInfo>{"value": "required",
                        "display_name": "Required - must call a tool"},
                ),
            },
            "response_format": {
                "display_name": "Response Format",
                "short_desc": "Force structured (e.g. JSON) output",
                "desc": "Hash that forces a structured response shape.  The most common use is JSON "
                    "mode: `{type: \"json_object\"}`.\n\n"
                    "For stricter schema enforcement, consider the `/generate` action with "
                    "`json_schema` — SGLang's constrained decoding guarantees schema conformance at "
                    "the token-sampling level.",
                "type": AbstractDataProviderTypeMap."*hash",
                "example_value": {"type": "json_object"},
            },
            "seed": {
                "display_name": "Seed",
                "short_desc": "Deterministic sampling seed",
                "desc": "Integer seed for deterministic sampling.  With a fixed `seed`, `temperature`, "
                    "and prompt, the same request returns the same output (useful for tests and "
                    "reproducibility).",
                "type": AbstractDataProviderTypeMap."*softint",
            },
            "user": {
                "display_name": "User ID",
                "short_desc": "Opaque end-user identifier for abuse tracking",
                "desc": "Opaque identifier representing the end user.  Logged server-side for abuse "
                    "tracking and usage attribution; does not affect generation.",
                "type": AbstractDataProviderTypeMap."*softstring",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}
}
