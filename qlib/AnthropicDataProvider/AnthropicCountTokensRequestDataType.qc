# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AnthropicDataProvider module definition

/** AnthropicCountTokensRequestDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Request data type for Anthropic Count Tokens API
public class AnthropicCountTokensRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "model": {
                "display_name": "Model",
                "short_desc": "The model to use for tokenization",
                "desc": "The model to use for tokenization (e.g. `claude-sonnet-4-6-20250514`)",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "claude-sonnet-4-6-20250514",
                "ref_data": "models",
            },
            "messages": {
                "display_name": "Messages",
                "short_desc": "Input messages to count tokens for",
                "desc": "Input messages to count tokens for. Same format as the Messages API",
                "type": new ListDataType("AnthropicCountTokensMessageList",
                    new AnthropicMessageItemDataType(), True),
                "required": True,
            },
            "system": {
                "display_name": "System Prompt",
                "short_desc": "System prompt to include in token count",
                "desc": "System prompt to include in the token count",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "tools": {
                "display_name": "Tools",
                "short_desc": "Tool definitions to include in token count",
                "desc": "Tool definitions to include in the token count. Same format as the Messages API",
                "type": new ListDataType("AnthropicCountTokensToolsList",
                    new AnthropicToolDefinitionDataType(), True),
            },
            "thinking": {
                "display_name": "Extended Thinking",
                "short_desc": "Thinking configuration to include in token count",
                "desc": "Extended thinking configuration to include in the token count. Same format as the "
                    "Messages API",
                "type": new AnthropicThinkingConfigDataType(AutoHashOrNothingType),
            },
            # Flattened action options (not sent directly to API; transformed in doRequestImpl)
            "thinking_mode": {
                "display_name": "Thinking Mode",
                "short_desc": "Whether extended thinking is enabled",
                "desc": "Whether extended thinking is enabled or disabled. Extended thinking allows the model "
                    "to reason through complex problems step by step before responding",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "enabled", "display_name": "Enabled",
                        "short_desc": "Enable extended thinking with a fixed budget"},
                    <AllowedValueInfo>{"value": "disabled", "display_name": "Disabled",
                        "short_desc": "Disable extended thinking"},
                    <AllowedValueInfo>{"value": "adaptive", "display_name": "Adaptive",
                        "short_desc": "Model decides whether to use thinking based on the task"},
                ),
            },
            "thinking_budget_tokens": {
                "display_name": "Thinking Budget Tokens",
                "short_desc": "Maximum tokens for thinking",
                "desc": "The maximum number of tokens to use for extended thinking. Must be at least `1024` "
                    "and less than `max_tokens`. Only applicable when thinking mode is `enabled`",
                "type": AbstractDataProviderTypeMap."*int",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Singleton instance
public const AnthropicCountTokensRequestDataType = new AnthropicCountTokensRequestDataType();

#! Response data type for Anthropic Count Tokens API
public class AnthropicCountTokensResponseDataType inherits HashDataType {
    public {
        const Fields = {
            "input_tokens": {
                "display_name": "Input Tokens",
                "short_desc": "Number of input tokens",
                "desc": "The number of input tokens in the provided messages and system prompt",
                "type": AbstractDataProviderTypeMap."int",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Singleton instance
public const AnthropicCountTokensResponseDataType = new AnthropicCountTokensResponseDataType();
}
