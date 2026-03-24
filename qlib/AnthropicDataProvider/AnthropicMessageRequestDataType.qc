# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AnthropicDataProvider module definition

/** AnthropicMessageRequestDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Message item data type for Anthropic message arrays
public class AnthropicMessageItemDataType inherits HashDataType {
    public {
        const Fields = {
            "role": {
                "display_name": "Role",
                "short_desc": "The role of the message author",
                "desc": "The role of the message author",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "user", "display_name": "User",
                        "short_desc": "A message from the user"},
                    <AllowedValueInfo>{"value": "assistant", "display_name": "Assistant",
                        "short_desc": "A message from the assistant"},
                ),
            },
            "content": {
                "display_name": "Content",
                "short_desc": "The content of the message",
                "desc": "The content of the message. Can be a string for simple text messages, or a list of "
                    "content blocks for multi-part messages (text, images, tool results)",
                "type": AbstractDataProviderTypeMap."auto",
                "required": True,
                "example_value": "Hello, how can I help you?",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Metadata data type for Anthropic API requests
public class AnthropicMetadataDataType inherits HashDataType {
    public {
        const Fields = {
            "user_id": {
                "display_name": "User ID",
                "short_desc": "An external identifier for the user making the request",
                "desc": "An external identifier for the user who is associated with the request. This should be a "
                    "uuid, hash value, or other opaque identifier. Anthropic may use this id to help detect abuse",
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

#! JSON Schema data type for tool input schemas
public class AnthropicJsonSchemaDataType inherits HashDataType {
    public {
        const Fields = {
            "type": {
                "display_name": "Schema Type",
                "short_desc": "The JSON Schema type",
                "desc": "The JSON Schema type. Typically `object` for tool input schemas",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "object",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "object", "display_name": "Object",
                        "short_desc": "An object with named properties"},
                    <AllowedValueInfo>{"value": "array", "display_name": "Array",
                        "short_desc": "An ordered list of items"},
                    <AllowedValueInfo>{"value": "string", "display_name": "String"},
                    <AllowedValueInfo>{"value": "number", "display_name": "Number"},
                    <AllowedValueInfo>{"value": "integer", "display_name": "Integer"},
                    <AllowedValueInfo>{"value": "boolean", "display_name": "Boolean"},
                ),
            },
            "properties": {
                "display_name": "Properties",
                "short_desc": "Property definitions for object schemas",
                "desc": "A hash mapping property names to their JSON Schema definitions. Each property should "
                    "have at least a `type` field",
                "type": AbstractDataProviderTypeMap."*hash",
                "example_value": {"location": {"type": "string", "description": "City name"}},
            },
            "required": {
                "display_name": "Required Properties",
                "short_desc": "List of required property names",
                "desc": "A list of property names that are required in the tool input",
                "type": new ListDataType("AnthropicSchemaRequired", StringType, True),
            },
            "description": {
                "display_name": "Description",
                "short_desc": "Description of the schema",
                "desc": "A description of the schema",
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

#! Tool definition data type for Anthropic tools
public class AnthropicToolDefinitionDataType inherits HashDataType {
    public {
        const Fields = {
            "name": {
                "display_name": "Tool Name",
                "short_desc": "The name of the tool",
                "desc": "The name of the tool. Must match `^[a-zA-Z0-9_-]{1,64}$`",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "get_weather",
            },
            "description": {
                "display_name": "Description",
                "short_desc": "Description of what the tool does",
                "desc": "A detailed description of what the tool does, when it should be used, and what each "
                    "parameter means",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "input_schema": {
                "display_name": "Input Schema",
                "short_desc": "JSON Schema for the tool input parameters",
                "desc": "A JSON Schema object defining the expected parameters for the tool. Must include `type`, "
                    "`properties`, and optionally `required`",
                "type": new AnthropicJsonSchemaDataType(AutoHashOrNothingType),
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Tool choice configuration data type
public class AnthropicToolChoiceDataType inherits HashDataType {
    public {
        const Fields = {
            "type": {
                "display_name": "Type",
                "short_desc": "How the model should use tools",
                "desc": "How the model should use the provided tools",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "auto", "display_name": "Auto",
                        "short_desc": "Model decides whether to use tools"},
                    <AllowedValueInfo>{"value": "any", "display_name": "Any",
                        "short_desc": "Model must use a tool"},
                    <AllowedValueInfo>{"value": "tool", "display_name": "Specific Tool",
                        "short_desc": "Model must use the named tool"},
                    <AllowedValueInfo>{"value": "none", "display_name": "None",
                        "short_desc": "Model must not use tools"},
                ),
            },
            "name": {
                "display_name": "Tool Name",
                "short_desc": "Name of the specific tool to use",
                "desc": "The name of the specific tool to use. Only applicable when `type` is `tool`",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "disable_parallel_tool_use": {
                "display_name": "Disable Parallel Tool Use",
                "short_desc": "Whether to disable parallel tool use",
                "desc": "Whether to disable parallel tool use. Defaults to `False`",
                "type": AbstractDataProviderTypeMap."*bool",
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

#! Extended thinking configuration data type
public class AnthropicThinkingConfigDataType inherits HashDataType {
    public {
        const Fields = {
            "type": {
                "display_name": "Type",
                "short_desc": "Whether extended thinking is enabled",
                "desc": "Whether extended thinking is enabled or disabled",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "enabled", "display_name": "Enabled",
                        "short_desc": "Enable extended thinking with a fixed budget"},
                    <AllowedValueInfo>{"value": "disabled", "display_name": "Disabled",
                        "short_desc": "Disable extended thinking"},
                    <AllowedValueInfo>{"value": "adaptive", "display_name": "Adaptive",
                        "short_desc": "Model decides whether to use thinking based on the task"},
                ),
            },
            "budget_tokens": {
                "display_name": "Budget Tokens",
                "short_desc": "Maximum tokens for thinking",
                "desc": "The maximum number of tokens to use for extended thinking. Must be at least `1024` "
                    "and less than `max_tokens`",
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

#! Output configuration data type for structured output and effort control
public class AnthropicOutputConfigDataType inherits HashDataType {
    public {
        const Fields = {
            "json_schema": {
                "display_name": "JSON Schema",
                "short_desc": "JSON Schema for structured output",
                "desc": "A JSON Schema object that constrains the model's output to match the specified structure. "
                    "Enables structured, machine-readable responses",
                "type": new AnthropicJsonSchemaDataType(AutoHashOrNothingType),
            },
            "effort": {
                "display_name": "Effort",
                "short_desc": "Controls how much effort the model should spend",
                "desc": "Controls how much effort the model should spend on the response",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "low", "display_name": "Low",
                        "short_desc": "Fast, less thorough responses"},
                    <AllowedValueInfo>{"value": "medium", "display_name": "Medium",
                        "short_desc": "Balanced effort"},
                    <AllowedValueInfo>{"value": "high", "display_name": "High",
                        "short_desc": "More thorough responses"},
                    <AllowedValueInfo>{"value": "max", "display_name": "Max",
                        "short_desc": "Maximum effort and thoroughness"},
                ),
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

#! Request data type for Anthropic Messages API
public class AnthropicMessageRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "model": {
                "display_name": "Model",
                "short_desc": "The model that will complete your prompt",
                "desc": "The model that will complete your prompt (e.g. `claude-sonnet-4-6-20250514`)",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "claude-sonnet-4-6-20250514",
                "ref_data": "models",
            },
            "messages": {
                "display_name": "Messages",
                "short_desc": "Input messages for the conversation",
                "desc": "Input messages for the conversation. Each message is a hash with `role` "
                    "(`user` or `assistant`) and `content` (string or list of content blocks)",
                "type": new ListDataType("AnthropicMessageList", new AnthropicMessageItemDataType(), True),
                "required": True,
            },
            "max_tokens": {
                "display_name": "Max Tokens",
                "short_desc": "The maximum number of tokens to generate",
                "desc": "The maximum number of tokens to generate before stopping. Required by the Anthropic API",
                "type": AbstractDataProviderTypeMap."int",
                "required": True,
                "default_value": 1024,
            },
            "system": {
                "display_name": "System Prompt",
                "short_desc": "System prompt for the conversation",
                "desc": "System prompt providing context and instructions. Can be a string or a list of content "
                    "blocks for cache control",
                "type": AbstractDataProviderTypeMap."*string",
                "attr": {"preselected": True},
            },
            "temperature": {
                "display_name": "Temperature",
                "short_desc": "Amount of randomness injected into the response",
                "desc": "Amount of randomness injected into the response. Ranges from `0.0` to `1.0`. "
                    "Use `temperature` closer to `0.0` for analytical/multiple choice tasks, and closer to "
                    "`1.0` for creative and generative tasks. Default: `1.0`",
                "type": AbstractDataProviderTypeMap."*float",
                "attr": {"preselected": True},
            },
            "top_p": {
                "display_name": "Top P",
                "short_desc": "Nucleus sampling parameter",
                "desc": "Use nucleus sampling. In nucleus sampling, we compute the cumulative distribution over "
                    "all the options for each subsequent token in decreasing probability order and cut it off "
                    "once it reaches a particular probability specified by `top_p`",
                "type": AbstractDataProviderTypeMap."*float",
            },
            "top_k": {
                "display_name": "Top K",
                "short_desc": "Only sample from the top K options for each subsequent token",
                "desc": "Only sample from the top K options for each subsequent token. Used to remove "
                    "long-tail low probability responses",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "stop_sequences": {
                "display_name": "Stop Sequences",
                "short_desc": "Custom text sequences that will cause the model to stop generating",
                "desc": "Custom text sequences that will cause the model to stop generating. The model will stop "
                    "when it encounters any of the provided sequences",
                "type": new ListDataType("AnthropicStopSequences", StringType, True),
            },
            "stream": {
                "display_name": "Stream",
                "short_desc": "Whether to stream the response using Server-Sent Events",
                "desc": "Whether to incrementally stream the response using Server-Sent Events (SSE)",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "tools": {
                "display_name": "Tools",
                "short_desc": "Definitions of tools that the model may use",
                "desc": "Definitions of tools that the model may use. Each tool has a `name`, `description`, "
                    "and `input_schema` (JSON Schema object)",
                "type": new ListDataType("AnthropicToolsList", new AnthropicToolDefinitionDataType(), True),
            },
            "tool_choice": {
                "display_name": "Tool Choice",
                "short_desc": "How the model should use the provided tools",
                "desc": "How the model should use the provided tools. Set `type` to control tool usage behavior",
                "type": new AnthropicToolChoiceDataType(AutoHashOrNothingType),
            },
            "thinking": {
                "display_name": "Extended Thinking",
                "short_desc": "Configuration for extended thinking",
                "desc": "Configuration for extended thinking. Budget must be at least `1024` and less than "
                    "`max_tokens`",
                "type": new AnthropicThinkingConfigDataType(AutoHashOrNothingType),
            },
            "metadata": {
                "display_name": "Metadata",
                "short_desc": "Request metadata",
                "desc": "An object describing metadata about the request. Contains `user_id` for abuse detection",
                "type": new AnthropicMetadataDataType(AutoHashOrNothingType),
            },
            "service_tier": {
                "display_name": "Service Tier",
                "short_desc": "Service tier for the request",
                "desc": "Determines which tier of service to use for the request",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "auto", "display_name": "Auto",
                        "short_desc": "Automatically select the best available tier"},
                    <AllowedValueInfo>{"value": "standard_only", "display_name": "Standard Only",
                        "short_desc": "Use only the standard tier, never prioritized capacity"},
                ),
            },
            "output_config": {
                "display_name": "Output Config",
                "short_desc": "Configuration for structured output and effort",
                "desc": "Controls structured output (JSON Schema) and effort level. Use `json_schema` for "
                    "machine-readable output, and `effort` to control response thoroughness",
                "type": new AnthropicOutputConfigDataType(AutoHashOrNothingType),
            },
            "container": {
                "display_name": "Container",
                "short_desc": "Container ID for context reuse",
                "desc": "A container ID for reusing context across multiple requests. The container stores "
                    "conversation state on the server for efficient multi-turn interactions",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "conversation_key": {
                "display_name": "Conversation Key",
                "short_desc": "Key for conversation context persistence",
                "desc": "An opaque key used to load and save conversation context via the configured context store. "
                    "When provided, previous messages are prepended automatically",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Singleton instance
public const AnthropicMessageRequestDataType = new AnthropicMessageRequestDataType();
}
