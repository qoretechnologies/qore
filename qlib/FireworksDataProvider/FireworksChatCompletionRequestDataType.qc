# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore FireworksChatCompletionRequestDataType class definition

/** FireworksChatCompletionRequestDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace FireworksDataProvider {
#! Data type for a single tool call produced by a Fireworks assistant message
public class FireworksChatToolCallDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Tool Call ID",
                "short_desc": "Unique identifier for the tool call",
                "desc": "Unique identifier for this tool call.",
                "type": AbstractDataProviderTypeMap."string",
            },
            "type": {
                "display_name": "Tool Type",
                "short_desc": "Type of tool invoked",
                "desc": "Type of tool invoked.  `function` is the only supported type.",
                "type": AbstractDataProviderTypeMap."string",
                "example_value": "function",
            },
            "function": {
                "display_name": "Function Call",
                "short_desc": "Function name and JSON-encoded arguments",
                "desc": "Hash with `name` (string) and `arguments` (JSON-encoded string).",
                "type": AbstractDataProviderTypeMap."hash",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data type for a single chat message
public class FireworksChatMessageDataType inherits HashDataType {
    public {
        const Fields = {
            "role": {
                "display_name": "Role",
                "short_desc": "The role of the message author",
                "desc": "The role of the author of this message: `system`, `user`, `assistant`, "
                    "or `tool`.",
                "type": AbstractDataProviderTypeMap."string",
                "example_value": "user",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "system", "display_name": "System"},
                    <AllowedValueInfo>{"value": "user", "display_name": "User"},
                    <AllowedValueInfo>{"value": "assistant", "display_name": "Assistant"},
                    <AllowedValueInfo>{"value": "tool", "display_name": "Tool"},
                ),
            },
            "content": {
                "display_name": "Content",
                "short_desc": "The text content of the message",
                "desc": "The text content of the message.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "name": {
                "display_name": "Author Name",
                "short_desc": "Optional name of the author",
                "desc": "Optional name identifying the specific author of the message.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "tool_call_id": {
                "display_name": "Tool Call ID",
                "short_desc": "ID of the tool call this message responds to",
                "desc": "Required when `role` is `tool`.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "tool_calls": {
                "display_name": "Tool Calls",
                "short_desc": "Tool invocations emitted by the assistant",
                "desc": "List of tool calls emitted by the assistant.",
                "type": new ListDataType("FireworksChatToolCallList",
                    new FireworksChatToolCallDataType(), True),
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data type for Fireworks AI chat completion requests
public class FireworksChatCompletionRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "model": {
                "display_name": "Model",
                "short_desc": "The Fireworks-hosted model identifier",
                "desc": "Fireworks model identifier in `accounts/<owner>/models/<name>` format, "
                    "e.g. `accounts/fireworks/models/llama-v3p1-405b-instruct`, "
                    "`accounts/fireworks/models/qwen2p5-72b-instruct`.\n\n"
                    "See the [Fireworks model catalog](https://fireworks.ai/models).",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "accounts/fireworks/models/llama-v3p1-405b-instruct",
            },
            "messages": {
                "display_name": "Messages",
                "short_desc": "List of chat messages forming the conversation",
                "desc": "The conversation history as a list of message objects.",
                "type": new ListDataType("FireworksChatMessageList",
                    new FireworksChatMessageDataType(), False),
                "required": True,
                "example_value": (
                    {"role": "user", "content": "Hello!"},
                ),
            },
            "temperature": {
                "display_name": "Temperature",
                "short_desc": "Sampling temperature (0-2)",
                "desc": "Controls randomness of the output.  Range `0.0` to `2.0`.",
                "type": AbstractDataProviderTypeMap."*softfloat",
                "example_value": 0.7,
            },
            "max_tokens": {
                "display_name": "Max Tokens",
                "short_desc": "Maximum tokens to emit in the completion",
                "desc": "Maximum number of tokens to generate.",
                "type": AbstractDataProviderTypeMap."*softint",
                "example_value": 1024,
            },
            "top_p": {
                "display_name": "Top-P",
                "short_desc": "Nucleus sampling cutoff (0-1)",
                "desc": "Nucleus sampling cutoff.",
                "type": AbstractDataProviderTypeMap."*softfloat",
            },
            "top_k": {
                "display_name": "Top-K",
                "short_desc": "Sample only from the K most likely next tokens",
                "desc": "Restrict sampling to the K most likely next tokens.  Fireworks-specific "
                    "extension to the OpenAI chat API.",
                "type": AbstractDataProviderTypeMap."*softint",
            },
            "frequency_penalty": {
                "display_name": "Frequency Penalty",
                "short_desc": "Penalize tokens by frequency",
                "desc": "Penalizes tokens by frequency.  Range `-2.0` to `2.0`.",
                "type": AbstractDataProviderTypeMap."*softfloat",
            },
            "presence_penalty": {
                "display_name": "Presence Penalty",
                "short_desc": "Penalize repeated tokens",
                "desc": "Penalizes tokens that have already appeared.  Range `-2.0` to `2.0`.",
                "type": AbstractDataProviderTypeMap."*softfloat",
            },
            "stop": {
                "display_name": "Stop Sequences",
                "short_desc": "Stop sequences",
                "desc": "Single string or list of strings; generation halts when emitted.",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "stream": {
                "display_name": "Stream",
                "short_desc": "Stream partial completions as SSE",
                "desc": "When `True`, the server streams partial completions as SSE.",
                "type": AbstractDataProviderTypeMap."*softbool",
            },
            "tools": {
                "display_name": "Tools",
                "short_desc": "OpenAI-style tool definitions the model may call",
                "desc": "List of tool definitions the model may invoke.",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "tool_choice": {
                "display_name": "Tool Choice",
                "short_desc": "Whether and which tool the model must invoke",
                "desc": "Tool invocation mode: `none`, `auto`, or `required`.",
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
                "short_desc": "Force structured output (e.g. JSON)",
                "desc": "Hash that forces a structured response shape, e.g. "
                    "`{type: \"json_object\"}` or `{type: \"json_schema\", schema: {...}}`.",
                "type": AbstractDataProviderTypeMap."*hash",
                "example_value": {"type": "json_object"},
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data type for Fireworks AI embedding requests
public class FireworksEmbeddingRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "model": {
                "display_name": "Model",
                "short_desc": "The Fireworks-hosted embedding model",
                "desc": "Fireworks embedding model identifier, e.g. "
                    "`nomic-ai/nomic-embed-text-v1.5`.",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "nomic-ai/nomic-embed-text-v1.5",
            },
            "input": {
                "display_name": "Input",
                "short_desc": "Text(s) to embed",
                "desc": "Text input(s) to convert to embedding vectors.  Can be a single string "
                    "or a list of strings for batch embedding.",
                "type": AbstractDataProviderTypeMap."softlist",
                "required": True,
                "example_value": ("Hello, world!",),
            },
            "encoding_format": {
                "display_name": "Encoding Format",
                "short_desc": "Wire format for the returned vectors",
                "desc": "`float` (default) or `base64`.",
                "type": AbstractDataProviderTypeMap."*softstring",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "float",
                        "display_name": "Float - JSON list of floats"},
                    <AllowedValueInfo>{"value": "base64",
                        "display_name": "Base64 - encoded float32 buffer"},
                ),
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}
}
