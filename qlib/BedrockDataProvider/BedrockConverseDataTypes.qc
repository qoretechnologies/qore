# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore BedrockDataProvider Converse request/response data types

/** BedrockConverseDataTypes.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the BedrockDataProvider module
public namespace BedrockDataProvider {
#! Request data type for the Bedrock Converse action
public class BedrockConverseRequestDataType inherits HashDataType {
    public {
        #! Field descriptions
        const Fields = {
            "modelId": {
                "display_name": "Model ID",
                "short_desc": "Bedrock model identifier",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The Bedrock foundation model ID. Examples: "
                    "`anthropic.claude-3-5-sonnet-20240620-v1:0`, "
                    "`amazon.nova-pro-v1:0`, "
                    "`meta.llama3-1-70b-instruct-v1:0`, "
                    "`mistral.mistral-large-2402-v1:0`",
                "required": True,
                "example_value": "anthropic.claude-3-5-sonnet-20240620-v1:0",
            },
            "messages": {
                "display_name": "Messages",
                "short_desc": "Conversation messages in Bedrock Converse shape",
                "type": AutoListType,
                "desc": "Conversation history; each message is a hash with `role` "
                    "(`user` or `assistant`) and `content` (list of content blocks, "
                    "each a hash like `{text: \"...\"}`, `{image: {...}}`, or "
                    "`{toolUse: {...}}`)",
                "required": True,
                "example_value": (
                    {"role": "user",
                     "content": ({"text": "Summarize Amazon's 2023 10-K."},)},
                ),
            },
            "system": {
                "display_name": "System Prompt",
                "short_desc": "Optional system prompt blocks",
                "type": AutoListOrNothingType,
                "desc": "Optional system instruction blocks; each is a hash like "
                    "`{text: \"...\"}`",
                "example_value": ({"text": "You are a concise financial analyst."},),
            },
            "inferenceConfig": {
                "display_name": "Inference Config",
                "short_desc": "Generation knobs",
                "type": AutoHashOrNothingType,
                "desc": "Generation parameters: "
                    "`maxTokens` (`int`), `temperature` (`float`, 0..1), "
                    "`topP` (`float`), `stopSequences` (`list<string>`)",
                "example_value": {"maxTokens": 1024, "temperature": 0.0},
            },
            "toolConfig": {
                "display_name": "Tool Config",
                "short_desc": "Tool definitions for function calling",
                "type": AutoHashOrNothingType,
                "desc": "Tool configuration: "
                    "`tools` is a list of `{toolSpec: {name, description, "
                    "inputSchema: {json: <schema>}}}`; optional `toolChoice` "
                    "(`{auto: {}}`, `{any: {}}`, or `{tool: {name: \"...\"}}`)",
            },
            "guardrailConfig": {
                "display_name": "Guardrail Config",
                "short_desc": "Bedrock Guardrails to apply",
                "type": AutoHashOrNothingType,
                "desc": "Apply Bedrock Guardrails: "
                    "`guardrailIdentifier` (`string`), `guardrailVersion` "
                    "(`string`), `trace` (`enabled` / `disabled`)",
            },
            "additionalModelRequestFields": {
                "display_name": "Additional Model Request Fields",
                "short_desc": "Model-family-specific request fields",
                "type": AutoHashOrNothingType,
                "desc": "Model-family-specific request fields not covered by the "
                    "unified Converse schema (e.g. Anthropic's `anthropic_beta`)",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Response data type for the Bedrock Converse action
public class BedrockConverseResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "output": {
                "display_name": "Output",
                "short_desc": "Assistant message hash",
                "type": AutoHashType,
                "desc": "Hash with the assistant's response: "
                    "`message.role` is `assistant`, `message.content` is a list "
                    "of content blocks (`{text: \"...\"}`, `{toolUse: {...}}`)",
            },
            "stopReason": {
                "display_name": "Stop Reason",
                "short_desc": "Why generation stopped",
                "type": StringType,
                "desc": "Why generation stopped — one of `end_turn`, `tool_use`, "
                    "`max_tokens`, `stop_sequence`, `content_filtered`, "
                    "`guardrail_intervened`",
            },
            "usage": {
                "display_name": "Usage",
                "short_desc": "Token usage counters",
                "type": AutoHashOrNothingType,
                "desc": "Token usage: `inputTokens`, `outputTokens`, `totalTokens`",
            },
            "metrics": {
                "display_name": "Metrics",
                "short_desc": "Per-call metrics",
                "type": AutoHashOrNothingType,
                "desc": "Per-call metrics such as `latencyMs`",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Request data type for the Bedrock InvokeModel embedding action
public class BedrockEmbedRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "modelId": {
                "display_name": "Model ID",
                "short_desc": "Bedrock embedding model identifier",
                "type": StringType,
                "desc": "The Bedrock embedding model ID. Examples: "
                    "`amazon.titan-embed-text-v2:0` (Titan), "
                    "`cohere.embed-english-v3`, `cohere.embed-multilingual-v3`",
                "required": True,
                "example_value": "amazon.titan-embed-text-v2:0",
            },
            "inputText": {
                "display_name": "Input Text",
                "short_desc": "Text to embed (Titan; single-string)",
                "type": StringOrNothingType,
                "desc": "Text to embed when using a Titan model (single-string body)",
                "example_value": "The quick brown fox jumps over the lazy dog.",
            },
            "texts": {
                "display_name": "Texts",
                "short_desc": "Texts to embed (Cohere; batch)",
                "type": AutoListOrNothingType,
                "desc": "Texts to embed when using a Cohere `embed-*` model "
                    "(batch); the action automatically picks the right body "
                    "shape per model family",
            },
            "input_type": {
                "display_name": "Cohere Input Type",
                "short_desc": "Cohere embedding input-type hint",
                "type": StringOrNothingType,
                "desc": "Cohere embedding input-type hint",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "search_document",
                        "display_name": "Search Document"},
                    <AllowedValueInfo>{"value": "search_query",
                        "display_name": "Search Query"},
                    <AllowedValueInfo>{"value": "classification",
                        "display_name": "Classification"},
                    <AllowedValueInfo>{"value": "clustering",
                        "display_name": "Clustering"},
                ),
            },
            "dimensions": {
                "display_name": "Dimensions",
                "short_desc": "Override embedding dimension (Titan v2 only)",
                "type": IntOrNothingType,
                "desc": "Override the output embedding dimension; supported by "
                    "Titan Text Embeddings v2 (e.g. 256, 512, 1024)",
            },
            "normalize": {
                "display_name": "Normalize",
                "short_desc": "L2-normalize the embedding (Titan v2 only)",
                "type": SoftBoolOrNothingType,
                "desc": "L2-normalize the resulting embedding (Titan v2 only)",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Response data type for the Bedrock embedding action
public class BedrockEmbedResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "embedding": {
                "display_name": "Embedding",
                "short_desc": "Dense vector (Titan single-text)",
                "type": AutoListOrNothingType,
                "desc": "Dense vector for the input text (Titan; single embedding)",
            },
            "embeddings": {
                "display_name": "Embeddings",
                "short_desc": "Dense vectors (Cohere batch)",
                "type": AutoListOrNothingType,
                "desc": "Dense vectors when the model returns multiple embeddings "
                    "(Cohere `embeddings.float`); each element is a `list<float>`",
            },
            "inputTextTokenCount": {
                "display_name": "Input Text Token Count",
                "short_desc": "Token count for the input (Titan)",
                "type": IntOrNothingType,
                "desc": "Token count for the input text (Titan response field)",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Singleton instance — Converse request
public const BedrockConverseRequestDataType = new BedrockConverseRequestDataType();
#! Singleton instance — Converse response
public const BedrockConverseResponseDataType = new BedrockConverseResponseDataType();
#! Singleton instance — Embed request
public const BedrockEmbedRequestDataType = new BedrockEmbedRequestDataType();
#! Singleton instance — Embed response
public const BedrockEmbedResponseDataType = new BedrockEmbedResponseDataType();
}
