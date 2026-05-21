# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AzureOpenAiDataProvider chat-completion + embedding data types

/** AzureOpenAiChatCompletionDataTypes.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the AzureOpenAiDataProvider module
public namespace AzureOpenAiDataProvider {
#! Request data type for the Azure OpenAI Chat Completion action
public class AzureOpenAiChatCompletionRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "deployment": {
                "display_name": "Deployment",
                "short_desc": "Azure OpenAI deployment name",
                "type": StringType,
                "desc": "The Azure OpenAI deployment name to call. Each Azure "
                    "resource defines one or more deployments, each bound to a "
                    "specific underlying OpenAI model (e.g. `gpt-4o`, "
                    "`gpt-4o-mini`, `o3`)",
                "required": True,
                "example_value": "gpt-4o",
            },
            "messages": {
                "display_name": "Messages",
                "short_desc": "OpenAI chat-completion messages",
                "type": AutoListType,
                "desc": "Conversation history in OpenAI chat-completion shape; "
                    "each entry is a hash with `role` (`system` / `user` / "
                    "`assistant` / `tool`) and `content` (string or list of "
                    "content parts for multimodal)",
                "required": True,
                "example_value": (
                    {"role": "system", "content": "You are a helpful assistant."},
                    {"role": "user", "content": "Hello."},
                ),
            },
            "max_tokens": {
                "display_name": "Max Tokens",
                "short_desc": "Maximum tokens to generate",
                "type": IntOrNothingType,
                "desc": "Maximum tokens to generate in the response",
                "example_value": 1024,
            },
            "temperature": {
                "display_name": "Temperature",
                "short_desc": "Sampling temperature",
                "type": FloatOrNothingType,
                "desc": "Sampling temperature (0..2); 0 is deterministic",
                "example_value": 0.0,
            },
            "top_p": {
                "display_name": "Top-P",
                "short_desc": "Nucleus sampling probability mass",
                "type": FloatOrNothingType,
                "desc": "Nucleus sampling probability mass (0..1)",
            },
            "response_format": {
                "display_name": "Response Format",
                "short_desc": "Structured-output shape",
                "type": AutoHashOrNothingType,
                "desc": "Structured-output shape; "
                    "`{\"type\": \"json_object\"}` for JSON mode or "
                    "`{\"type\": \"json_schema\", \"json_schema\": {...}}` for "
                    "strict-schema responses",
            },
            "tools": {
                "display_name": "Tools",
                "short_desc": "Function-calling tool definitions",
                "type": AutoListOrNothingType,
                "desc": "Function-calling tool definitions; each tool is "
                    "`{\"type\": \"function\", \"function\": {name, description, "
                    "parameters: <JSON schema>}}`",
            },
            "tool_choice": {
                "display_name": "Tool Choice",
                "short_desc": "Tool-selection policy",
                "type": AbstractDataProviderType::anyType,
                "desc": "Tool-selection policy: `\"auto\"`, `\"none\"`, "
                    "`\"required\"`, or `{type:\"function\",function:{name}}`",
            },
            "stop": {
                "display_name": "Stop Sequences",
                "short_desc": "Stop sequences",
                "type": AutoListOrNothingType,
                "desc": "Up to four stop sequences that terminate generation",
            },
            "seed": {
                "display_name": "Seed",
                "short_desc": "Deterministic-sampling seed",
                "type": IntOrNothingType,
                "desc": "Seed for deterministic sampling (best-effort)",
            },
            "user": {
                "display_name": "User",
                "short_desc": "End-user identifier for abuse tracking",
                "type": StringOrNothingType,
                "desc": "Stable identifier for the end-user making the request "
                    "(passed to Azure abuse monitoring)",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Response data type for the Azure OpenAI Chat Completion action
public class AzureOpenAiChatCompletionResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {
                "display_name": "ID",
                "short_desc": "Completion identifier",
                "type": StringOrNothingType,
                "desc": "Azure OpenAI completion identifier",
            },
            "object": {
                "display_name": "Object",
                "short_desc": "Object type discriminator",
                "type": StringOrNothingType,
                "desc": "Object type discriminator (typically `chat.completion`)",
            },
            "created": {
                "display_name": "Created",
                "short_desc": "Unix timestamp",
                "type": IntOrNothingType,
                "desc": "Unix timestamp when the completion was created",
            },
            "model": {
                "display_name": "Model",
                "short_desc": "Underlying model name",
                "type": StringOrNothingType,
                "desc": "The underlying OpenAI model that produced the response",
            },
            "choices": {
                "display_name": "Choices",
                "short_desc": "List of completion choices",
                "type": AutoListOrNothingType,
                "desc": "List of completion choices; each has `index`, "
                    "`message: {role, content, tool_calls?}`, `finish_reason`",
            },
            "usage": {
                "display_name": "Usage",
                "short_desc": "Token usage counters",
                "type": AutoHashOrNothingType,
                "desc": "Token counters: `prompt_tokens`, `completion_tokens`, "
                    "`total_tokens`",
            },
            "prompt_filter_results": {
                "display_name": "Prompt Filter Results",
                "short_desc": "Azure Content Safety prompt-side results",
                "type": AutoListOrNothingType,
                "desc": "Azure Content Safety annotations on the prompt "
                    "(Azure-specific addition over the OpenAI shape)",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Request data type for the Azure OpenAI Embedding action
public class AzureOpenAiEmbeddingRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "deployment": {
                "display_name": "Deployment",
                "short_desc": "Azure OpenAI embedding-model deployment name",
                "type": StringType,
                "desc": "The Azure OpenAI deployment name bound to an embedding "
                    "model (e.g. `text-embedding-3-small`, "
                    "`text-embedding-3-large`, `text-embedding-ada-002`)",
                "required": True,
                "example_value": "text-embedding-3-small",
            },
            "input": {
                "display_name": "Input",
                "short_desc": "Text(s) to embed",
                "type": AbstractDataProviderType::anyType,
                "desc": "Text to embed; either a single string or a list of "
                    "strings for batch embedding",
                "required": True,
                "example_value": "The quick brown fox jumps over the lazy dog.",
            },
            "dimensions": {
                "display_name": "Dimensions",
                "short_desc": "Output embedding dimension",
                "type": IntOrNothingType,
                "desc": "Number of dimensions in the output embedding (supported "
                    "by `text-embedding-3-*` models)",
            },
            "user": {
                "display_name": "User",
                "short_desc": "End-user identifier",
                "type": StringOrNothingType,
                "desc": "Stable end-user identifier passed to abuse monitoring",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Response data type for the Azure OpenAI Embedding action
public class AzureOpenAiEmbeddingResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "object": {
                "display_name": "Object",
                "short_desc": "Object type discriminator (usually `list`)",
                "type": StringOrNothingType,
                "desc": "Object type discriminator (typically `list`)",
            },
            "data": {
                "display_name": "Data",
                "short_desc": "List of embeddings",
                "type": AutoListOrNothingType,
                "desc": "List of embedding objects; each has `index`, "
                    "`embedding` (`list<float>`), `object`",
            },
            "model": {
                "display_name": "Model",
                "short_desc": "Underlying model name",
                "type": StringOrNothingType,
                "desc": "The underlying OpenAI embedding model",
            },
            "usage": {
                "display_name": "Usage",
                "short_desc": "Token usage counters",
                "type": AutoHashOrNothingType,
                "desc": "Token counters: `prompt_tokens`, `total_tokens`",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Singleton instances
public const AzureOpenAiChatCompletionRequestDataType  = new AzureOpenAiChatCompletionRequestDataType();
public const AzureOpenAiChatCompletionResponseDataType = new AzureOpenAiChatCompletionResponseDataType();
public const AzureOpenAiEmbeddingRequestDataType       = new AzureOpenAiEmbeddingRequestDataType();
public const AzureOpenAiEmbeddingResponseDataType      = new AzureOpenAiEmbeddingResponseDataType();
}
