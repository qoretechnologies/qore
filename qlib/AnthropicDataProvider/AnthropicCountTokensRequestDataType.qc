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
