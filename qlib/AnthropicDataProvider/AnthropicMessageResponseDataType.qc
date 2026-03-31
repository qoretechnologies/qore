# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AnthropicDataProvider module definition

/** AnthropicMessageResponseDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the AnthropicDataProvider module
public namespace AnthropicDataProvider {
#! Container data type for Anthropic API responses
public class AnthropicContainerDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Container ID",
                "short_desc": "Unique container identifier",
                "desc": "Unique identifier for the container used in the request",
                "type": AbstractDataProviderTypeMap."string",
            },
            "expires_at": {
                "display_name": "Expires At",
                "short_desc": "ISO 8601 timestamp when the container expires",
                "desc": "ISO 8601 timestamp when the container expires and stored context is no longer available",
                "type": AbstractDataProviderTypeMap."string",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }

    #! Creates the object with the given base type
    constructor(Type base_type) : HashDataType(base_type) {
        addQoreFields(Fields);
    }
}

#! Usage statistics data type for Anthropic API responses
public class AnthropicUsageDataType inherits HashDataType {
    public {
        const Fields = {
            "input_tokens": {
                "display_name": "Input Tokens",
                "short_desc": "Number of input tokens",
                "desc": "The number of input tokens consumed",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "output_tokens": {
                "display_name": "Output Tokens",
                "short_desc": "Number of output tokens",
                "desc": "The number of output tokens generated",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "cache_creation_input_tokens": {
                "display_name": "Cache Creation Input Tokens",
                "short_desc": "Tokens used to create cache entries",
                "desc": "The number of input tokens used to create cache entries",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "cache_read_input_tokens": {
                "display_name": "Cache Read Input Tokens",
                "short_desc": "Tokens read from cache",
                "desc": "The number of input tokens read from cache",
                "type": AbstractDataProviderTypeMap."*int",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }

    #! Creates the object with the given base type
    constructor(Type base_type) : HashDataType(base_type) {
        addQoreFields(Fields);
    }
}

#! Tool call data type for extracted tool_use content blocks
public class AnthropicToolCallDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Tool Use ID",
                "short_desc": "Unique identifier for this tool use",
                "desc": "Unique identifier for this tool use block",
                "type": AbstractDataProviderTypeMap."string",
            },
            "type": {
                "display_name": "Type",
                "short_desc": "Content block type",
                "desc": "Content block type, always `tool_use`",
                "type": AbstractDataProviderTypeMap."string",
            },
            "name": {
                "display_name": "Tool Name",
                "short_desc": "Name of the tool being called",
                "desc": "The name of the tool being called",
                "type": AbstractDataProviderTypeMap."string",
            },
            "input": {
                "display_name": "Input",
                "short_desc": "Tool input parameters",
                "desc": "The input parameters for the tool call, as defined by the tool's `input_schema`",
                "type": AbstractDataProviderTypeMap."*hash",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Content block data type for Anthropic message content
public class AnthropicContentBlockDataType inherits HashDataType {
    public {
        const Fields = {
            "type": {
                "display_name": "Block Type",
                "short_desc": "The type of content block",
                "desc": "The type of content block",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "text", "display_name": "Text",
                        "short_desc": "A text content block"},
                    <AllowedValueInfo>{"value": "tool_use", "display_name": "Tool Use",
                        "short_desc": "A tool invocation block"},
                    <AllowedValueInfo>{"value": "thinking", "display_name": "Thinking",
                        "short_desc": "An extended thinking block"},
                    <AllowedValueInfo>{"value": "redacted_thinking", "display_name": "Redacted Thinking",
                        "short_desc": "A redacted extended thinking block"},
                ),
            },
            "text": {
                "display_name": "Text",
                "short_desc": "The text content",
                "desc": "The text content. Present when `type` is `text`",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "id": {
                "display_name": "Tool Use ID",
                "short_desc": "Unique identifier for a tool use block",
                "desc": "Unique identifier for this tool use block. Present when `type` is `tool_use`",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "name": {
                "display_name": "Tool Name",
                "short_desc": "Name of the tool being called",
                "desc": "The name of the tool being called. Present when `type` is `tool_use`",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "input": {
                "display_name": "Tool Input",
                "short_desc": "Tool input parameters",
                "desc": "The input parameters for the tool call. Present when `type` is `tool_use`",
                "type": AbstractDataProviderTypeMap."*hash",
            },
            "thinking": {
                "display_name": "Thinking",
                "short_desc": "Extended thinking content",
                "desc": "The thinking content. Present when `type` is `thinking`",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }

    #! Creates the object with the given base type
    constructor(Type base_type) : HashDataType(base_type) {
        addQoreFields(Fields);
    }
}

#! Response data type for Anthropic Messages API
public class AnthropicMessageResponseDataType inherits HashDataType {
    public {
        #! Response fields
        const Fields = {
            "id": {
                "display_name": "Message ID",
                "short_desc": "Unique message identifier",
                "desc": "Unique object identifier. Format: `msg_` followed by alphanumeric characters",
                "type": AbstractDataProviderTypeMap."string",
            },
            "type": {
                "display_name": "Object Type",
                "short_desc": "Object type",
                "desc": "Object type. Always `message` for Messages API responses",
                "type": AbstractDataProviderTypeMap."string",
            },
            "role": {
                "display_name": "Role",
                "short_desc": "Conversational role of the generated message",
                "desc": "Conversational role of the generated message. Always `assistant`",
                "type": AbstractDataProviderTypeMap."string",
            },
            "content": {
                "display_name": "Content",
                "short_desc": "Content blocks generated by the model",
                "desc": "Content generated by the model. Array of content blocks, each with a `type` field. "
                    "Text blocks have `type: \"text\"` and `text` field. Tool use blocks have "
                    "`type: \"tool_use\"`, `id`, `name`, and `input` fields",
                "type": new ListDataType("AnthropicContentBlocks", new AnthropicContentBlockDataType(), True),
            },
            "model": {
                "display_name": "Model",
                "short_desc": "The model that handled the request",
                "desc": "The model that handled the request",
                "type": AbstractDataProviderTypeMap."string",
            },
            "stop_reason": {
                "display_name": "Stop Reason",
                "short_desc": "The reason the model stopped generating",
                "desc": "The reason the model stopped generating tokens",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "end_turn", "display_name": "End Turn",
                        "short_desc": "Natural stop", "desc": "The model reached a natural stopping point"},
                    <AllowedValueInfo>{"value": "max_tokens", "display_name": "Max Tokens",
                        "short_desc": "Hit max_tokens limit", "desc": "Output capped by the `max_tokens` parameter"},
                    <AllowedValueInfo>{"value": "stop_sequence", "display_name": "Stop Sequence",
                        "short_desc": "Matched a stop sequence", "desc": "One of the provided `stop_sequences` was generated"},
                    <AllowedValueInfo>{"value": "tool_use", "display_name": "Tool Use",
                        "short_desc": "Model invoked a tool", "desc": "The model invoked one or more tools"},
                ),
            },
            "stop_sequence": {
                "display_name": "Stop Sequence",
                "short_desc": "Which custom stop sequence was generated, if any",
                "desc": "Which custom stop sequence was generated. Only present if `stop_reason` is "
                    "`stop_sequence`",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "usage": {
                "display_name": "Usage",
                "short_desc": "Token usage statistics",
                "desc": "Billing and rate-limit usage",
                "type": new AnthropicUsageDataType(AutoHashOrNothingType),
            },
            "container": {
                "display_name": "Container",
                "short_desc": "Container info for context reuse",
                "desc": "Container information for reusing context across requests. Contains `id` and "
                    "`expires_at` fields",
                "type": new AnthropicContainerDataType(AutoHashOrNothingType),
            },
            "output_text": {
                "display_name": "Output Text",
                "short_desc": "Extracted text content (convenience)",
                "desc": "Convenience field: all text content blocks concatenated. Not part of the raw API "
                    "response; added by the data provider for easier downstream use",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "tool_calls": {
                "display_name": "Tool Calls",
                "short_desc": "Extracted tool use blocks (convenience)",
                "desc": "Convenience field: all `tool_use` content blocks extracted. Not part of the raw API "
                    "response; added by the data provider",
                "type": new ListDataType("AnthropicToolCalls", new AnthropicToolCallDataType(), True),
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }

    #! Creates the object with the given base type
    constructor(Type base_type) : HashDataType(base_type) {
        addQoreFields(Fields);
    }
}

#! Singleton instance
public const AnthropicMessageResponseDataType = new AnthropicMessageResponseDataType();
}
