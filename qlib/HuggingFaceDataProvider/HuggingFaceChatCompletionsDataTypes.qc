# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore HuggingFaceDataProvider module definition

/** HuggingFaceChatCompletionsDataTypes.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the HuggingFaceDataProvider module
public namespace HuggingFaceDataProvider {
#! Chat message data type
public class HuggingFaceChatMessageDataType inherits HashDataType {
    public {
        const Fields = {
            "role": {
                "display_name": "Role",
                "short_desc": "Message role",
                "desc": "The role of the message sender",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "system", "display_name": "System"},
                    <AllowedValueInfo>{"value": "user", "display_name": "User"},
                    <AllowedValueInfo>{"value": "assistant", "display_name": "Assistant"},
                ),
                "example_value": "user",
            },
            "content": {
                "display_name": "Content",
                "short_desc": "Message content",
                "desc": "The content of the message",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "What is machine learning?",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public const HuggingFaceChatMessageDataType = new HuggingFaceChatMessageDataType();

#! Chat completions request data type
public class HuggingFaceChatCompletionsRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "model": {
                "display_name": "Model",
                "short_desc": "Model ID for inference",
                "desc": "The model ID to use for inference "
                    "(e.g. `meta-llama/Llama-3.1-8B-Instruct`)",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "ref_data": "models",
                "example_value": "meta-llama/Llama-3.1-8B-Instruct",
            },
            "messages": {
                "display_name": "Messages",
                "short_desc": "List of chat messages",
                "desc": "A list of messages comprising the conversation so far",
                "type": new ListDataType("HuggingFaceChatMessages",
                    HuggingFaceChatMessageDataType, True),
                "required": True,
            },
            "max_tokens": {
                "display_name": "Max Tokens",
                "short_desc": "Maximum tokens to generate",
                "desc": "The maximum number of tokens to generate in the response",
                "type": AbstractDataProviderTypeMap."*int",
                "example_value": 1024,
            },
            "temperature": {
                "display_name": "Temperature",
                "short_desc": "Sampling temperature",
                "desc": "Sampling temperature between `0.0` and `2.0`. Higher values make output more "
                    "random, lower values make it more deterministic",
                "type": AbstractDataProviderTypeMap."*float",
                "example_value": 0.7,
            },
            "top_p": {
                "display_name": "Top P",
                "short_desc": "Nucleus sampling threshold",
                "desc": "Nucleus sampling parameter. The model considers tokens with `top_p` "
                    "probability mass",
                "type": AbstractDataProviderTypeMap."*float",
            },
            "stop": {
                "display_name": "Stop Sequences",
                "short_desc": "Stop sequences",
                "desc": "Up to 4 sequences where the model will stop generating further tokens",
                "type": AbstractDataProviderType::get(
                    new Type("softlist<string>")),
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public const HuggingFaceChatCompletionsRequestDataType = new HuggingFaceChatCompletionsRequestDataType();

#! Chat choice message data type
public class HuggingFaceChatChoiceMessageDataType inherits HashDataType {
    public {
        const Fields = {
            "role": {
                "display_name": "Role",
                "short_desc": "Message role",
                "desc": "The role of the message (typically `assistant`)",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "content": {
                "display_name": "Content",
                "short_desc": "Message content",
                "desc": "The content of the message",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public const HuggingFaceChatChoiceMessageDataType = new HuggingFaceChatChoiceMessageDataType();

#! Chat choice data type
public class HuggingFaceChatChoiceDataType inherits HashDataType {
    public {
        const Fields = {
            "index": {
                "display_name": "Index",
                "short_desc": "Choice index",
                "desc": "The index of the choice in the list",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "message": {
                "display_name": "Message",
                "short_desc": "Generated message",
                "desc": "The generated message",
                "type": HuggingFaceChatChoiceMessageDataType,
            },
            "finish_reason": {
                "display_name": "Finish Reason",
                "short_desc": "Why generation stopped",
                "desc": "The reason the model stopped generating tokens "
                    "(e.g. `stop`, `length`, `tool_calls`)",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public const HuggingFaceChatChoiceDataType = new HuggingFaceChatChoiceDataType();

#! Chat usage data type
public class HuggingFaceChatUsageDataType inherits HashDataType {
    public {
        const Fields = {
            "prompt_tokens": {
                "display_name": "Prompt Tokens",
                "short_desc": "Number of tokens in the prompt",
                "desc": "The number of tokens in the prompt",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "completion_tokens": {
                "display_name": "Completion Tokens",
                "short_desc": "Number of tokens in the completion",
                "desc": "The number of tokens in the generated completion",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "total_tokens": {
                "display_name": "Total Tokens",
                "short_desc": "Total tokens used",
                "desc": "The total number of tokens used (prompt + completion)",
                "type": AbstractDataProviderTypeMap."*int",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public const HuggingFaceChatUsageDataType = new HuggingFaceChatUsageDataType();

#! Chat completions response data type
public class HuggingFaceChatCompletionsResponseDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "ID",
                "short_desc": "Completion ID",
                "desc": "A unique identifier for the completion",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "object": {
                "display_name": "Object",
                "short_desc": "Object type",
                "desc": "The object type (typically `chat.completion`)",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "created": {
                "display_name": "Created",
                "short_desc": "Unix timestamp of creation",
                "desc": "Unix timestamp when the completion was created",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "model": {
                "display_name": "Model",
                "short_desc": "Model used for completion",
                "desc": "The model used for the completion",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "choices": {
                "display_name": "Choices",
                "short_desc": "List of completion choices",
                "desc": "A list of chat completion choices",
                "type": new ListDataType("HuggingFaceChatChoices",
                    HuggingFaceChatChoiceDataType, True),
            },
            "usage": {
                "display_name": "Usage",
                "short_desc": "Token usage statistics",
                "desc": "Token usage statistics for the completion",
                "type": HuggingFaceChatUsageDataType,
            },
            "output_text": {
                "display_name": "Output Text",
                "short_desc": "Extracted text from first choice",
                "desc": "Convenience field: extracted text content from the first choice's message",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public const HuggingFaceChatCompletionsResponseDataType = new HuggingFaceChatCompletionsResponseDataType();
}
