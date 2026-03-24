# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GeminiDataProvider module definition

/** GeminiDataProviderDefs.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the GeminiDataProvider module
public namespace GeminiDataProvider {
#! Application name constant
public const AppName = "Gemini";

#! Content part data type for inline data
public class GeminiInlineDataDataType inherits HashDataType {
    public {
        const Fields = {
            "mime_type": {
                "display_name": "MIME Type",
                "short_desc": "The MIME type of the inline data",
                "desc": "The IANA standard MIME type of the source data",
                "type": AbstractDataProviderTypeMap."string",
            },
            "data": {
                "display_name": "Data",
                "short_desc": "Base64-encoded binary data",
                "desc": "Base64-encoded binary data (e.g., image, audio, video)",
                "type": AbstractDataProviderTypeMap."string",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Content part data type
public class GeminiContentPartDataType inherits HashDataType {
    public {
        const Fields = {
            "text": {
                "display_name": "Text",
                "short_desc": "The text content of the part",
                "desc": "The text content of the part; mutually exclusive with `inline_data`",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
        addField(new QoreDataField({
            "name": "inline_data",
            "display_name": "Inline Data",
            "short_desc": "Inline media data",
            "desc": "Inline media data such as images, audio, or video; mutually exclusive with `text`",
            "type": new GeminiInlineDataDataType(),
        }));
    }
}

#! Content data type (a message with role and parts)
public class GeminiContentDataType inherits HashDataType {
    public {
        const Fields = {
            "role": {
                "display_name": "Role",
                "short_desc": "The role of the content author",
                "desc": "The role of the content author: `user` or `model`",
                "type": AbstractDataProviderTypeMap."string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "User",
                        "value": "user",
                        "desc": "Content from the user",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Model",
                        "value": "model",
                        "desc": "Content from the model",
                    },
                ),
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
        addField(new QoreDataField({
            "name": "parts",
            "display_name": "Parts",
            "short_desc": "The content parts",
            "desc": "Ordered list of content parts that make up this message",
            "type": new ListDataType("ContentPartList", new GeminiContentPartDataType()),
        }));
    }
}

#! Generation configuration data type
public class GeminiGenerationConfigDataType inherits HashDataType {
    public {
        const Fields = {
            "temperature": {
                "display_name": "Temperature",
                "short_desc": "Controls randomness in generation",
                "desc": "Controls the randomness of the output. Values range from `0.0` to `2.0`. Higher values "
                    "produce more random output",
                "type": AbstractDataProviderTypeMap."*float",
            },
            "topP": {
                "display_name": "Top P",
                "short_desc": "Nucleus sampling parameter",
                "desc": "The maximum cumulative probability of tokens to consider when sampling. Used for nucleus "
                    "sampling",
                "type": AbstractDataProviderTypeMap."*float",
            },
            "topK": {
                "display_name": "Top K",
                "short_desc": "Top-K sampling parameter",
                "desc": "The maximum number of tokens to consider when sampling",
                "type": AbstractDataProviderTypeMap."*softint",
            },
            "maxOutputTokens": {
                "display_name": "Max Output Tokens",
                "short_desc": "Maximum number of tokens to generate",
                "desc": "The maximum number of tokens to generate in the response",
                "type": AbstractDataProviderTypeMap."*softint",
            },
            "candidateCount": {
                "display_name": "Candidate Count",
                "short_desc": "Number of response candidates",
                "desc": "Number of generated responses to return. Currently only 1 is supported",
                "type": AbstractDataProviderTypeMap."*softint",
            },
            "stopSequences": {
                "display_name": "Stop Sequences",
                "short_desc": "Sequences that stop generation",
                "desc": "A list of strings that, if encountered, will stop generation",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "responseMimeType": {
                "display_name": "Response MIME Type",
                "short_desc": "Output MIME type",
                "desc": "The MIME type of the generated candidate text. Supported values: `text/plain`, "
                    "`application/json`, `text/x.enum`",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "responseSchema": {
                "display_name": "Response Schema",
                "short_desc": "Schema for structured output",
                "desc": "Output schema for the generated candidate text when `responseMimeType` is "
                    "`application/json`",
                "type": AbstractDataProviderTypeMap."*hash",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Safety setting data type
public class GeminiSafetySettingDataType inherits HashDataType {
    public {
        const Fields = {
            "category": {
                "display_name": "Category",
                "short_desc": "The safety category",
                "desc": "The safety category to configure a threshold for",
                "type": AbstractDataProviderTypeMap."string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "HARM_CATEGORY_HARASSMENT",
                        "desc": "Harassment content",
                    },
                    <AllowedValueInfo>{
                        "value": "HARM_CATEGORY_HATE_SPEECH",
                        "desc": "Hate speech content",
                    },
                    <AllowedValueInfo>{
                        "value": "HARM_CATEGORY_SEXUALLY_EXPLICIT",
                        "desc": "Sexually explicit content",
                    },
                    <AllowedValueInfo>{
                        "value": "HARM_CATEGORY_DANGEROUS_CONTENT",
                        "desc": "Dangerous content",
                    },
                    <AllowedValueInfo>{
                        "value": "HARM_CATEGORY_CIVIC_INTEGRITY",
                        "desc": "Civic integrity content",
                    },
                ),
            },
            "threshold": {
                "display_name": "Threshold",
                "short_desc": "The blocking threshold",
                "desc": "The threshold at which this safety category is blocked",
                "type": AbstractDataProviderTypeMap."string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "BLOCK_NONE",
                        "desc": "Always show regardless of probability of unsafe content",
                    },
                    <AllowedValueInfo>{
                        "value": "BLOCK_LOW_AND_ABOVE",
                        "desc": "Block when low probability of unsafe content or higher",
                    },
                    <AllowedValueInfo>{
                        "value": "BLOCK_MEDIUM_AND_ABOVE",
                        "desc": "Block when medium probability of unsafe content or higher",
                    },
                    <AllowedValueInfo>{
                        "value": "BLOCK_ONLY_HIGH",
                        "desc": "Block when high probability of unsafe content",
                    },
                ),
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Candidate data type (a single response candidate)
public class GeminiCandidateDataType inherits HashDataType {
    public {
        const Fields = {
            "finishReason": {
                "display_name": "Finish Reason",
                "short_desc": "The reason the model stopped generating",
                "desc": "The reason the model stopped generating tokens: `STOP`, `MAX_TOKENS`, `SAFETY`, "
                    "`RECITATION`, or `OTHER`",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "index": {
                "display_name": "Index",
                "short_desc": "The candidate index",
                "desc": "The index of the candidate in the list of candidates",
                "type": AbstractDataProviderTypeMap."*softint",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
        addField(new QoreDataField({
            "name": "content",
            "display_name": "Content",
            "short_desc": "The generated content",
            "desc": "The generated content returned from the model",
            "type": new GeminiContentDataType(),
        }));
        setDefaultOtherFieldType(AbstractDataProviderTypeMap."any");
    }
}

#! Usage metadata data type
public class GeminiUsageMetadataDataType inherits HashDataType {
    public {
        const Fields = {
            "promptTokenCount": {
                "display_name": "Prompt Token Count",
                "short_desc": "Number of tokens in the prompt",
                "desc": "Number of tokens in the prompt",
                "type": AbstractDataProviderTypeMap."*softint",
            },
            "candidatesTokenCount": {
                "display_name": "Candidates Token Count",
                "short_desc": "Number of tokens in the generated candidates",
                "desc": "Total number of tokens across the generated candidates",
                "type": AbstractDataProviderTypeMap."*softint",
            },
            "totalTokenCount": {
                "display_name": "Total Token Count",
                "short_desc": "Total number of tokens",
                "desc": "Total token count for the generation request (prompt + candidates)",
                "type": AbstractDataProviderTypeMap."*softint",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Generate content request data type
public class GeminiGenerateContentRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "model": {
                "display_name": "Model",
                "short_desc": "The model to use for generation",
                "desc": "The model name to use for generation (e.g., `gemini-2.5-flash`)",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "ref_data": "models",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
        addField(new QoreDataField({
            "name": "contents",
            "display_name": "Contents",
            "short_desc": "The conversation contents",
            "desc": "The content of the current conversation with the model. For single-turn queries, this is a "
                "single message. For multi-turn queries, this is a repeated field that contains conversation "
                "history and the latest request",
            "type": new ListDataType("ContentList", new GeminiContentDataType()),
            "required": True,
        }));
        addField(new QoreDataField({
            "name": "systemInstruction",
            "display_name": "System Instruction",
            "short_desc": "System instruction for the model",
            "desc": "Optional system instruction that sets the behavior and context for the model",
            "type": new GeminiContentDataType(),
        }));
        addField(new QoreDataField({
            "name": "generationConfig",
            "display_name": "Generation Config",
            "short_desc": "Configuration for generation",
            "desc": "Configuration options for model generation and outputs",
            "type": new GeminiGenerationConfigDataType(),
        }));
        addField(new QoreDataField({
            "name": "safetySettings",
            "display_name": "Safety Settings",
            "short_desc": "Safety settings for content filtering",
            "desc": "A list of unique safety settings for blocking unsafe content",
            "type": new ListDataType("SafetySettingList", new GeminiSafetySettingDataType()),
        }));
    }
}

#! Generate content response data type
public class GeminiGenerateContentResponseDataType inherits HashDataType {
    public {
        const Fields = {
            "modelVersion": {
                "display_name": "Model Version",
                "short_desc": "The model version used",
                "desc": "The model version used for generation",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
        addField(new QoreDataField({
            "name": "candidates",
            "display_name": "Candidates",
            "short_desc": "Generated response candidates",
            "desc": "Candidate responses from the model",
            "type": new ListDataType("CandidateList", new GeminiCandidateDataType()),
        }));
        addField(new QoreDataField({
            "name": "usageMetadata",
            "display_name": "Usage Metadata",
            "short_desc": "Token usage metadata",
            "desc": "Metadata on the generation request's token usage",
            "type": new GeminiUsageMetadataDataType(),
        }));
        setDefaultOtherFieldType(AbstractDataProviderTypeMap."any");
    }
}

#! Simple chat request data type
public class GeminiSimpleChatRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "model": {
                "display_name": "Model",
                "short_desc": "The model to use for generation",
                "desc": "The model name to use for generation (e.g., `gemini-2.5-flash`)",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "ref_data": "models",
            },
            "role": {
                "display_name": "Role",
                "short_desc": "The role of the message author",
                "desc": "The role of the message author (default: `user`)",
                "type": AbstractDataProviderTypeMap."*string",
                "default_value": "user",
            },
            "content": {
                "display_name": "Content",
                "short_desc": "The message text",
                "desc": "The text content of the message to send to the model",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "temperature": {
                "display_name": "Temperature",
                "short_desc": "Controls randomness in generation",
                "desc": "Controls the randomness of the output. Values range from `0.0` to `2.0`",
                "type": AbstractDataProviderTypeMap."*float",
            },
            "topP": {
                "display_name": "Top P",
                "short_desc": "Nucleus sampling parameter",
                "desc": "The maximum cumulative probability of tokens to consider when sampling",
                "type": AbstractDataProviderTypeMap."*float",
            },
            "topK": {
                "display_name": "Top K",
                "short_desc": "Top-K sampling parameter",
                "desc": "The maximum number of tokens to consider when sampling",
                "type": AbstractDataProviderTypeMap."*softint",
            },
            "maxOutputTokens": {
                "display_name": "Max Output Tokens",
                "short_desc": "Maximum number of tokens to generate",
                "desc": "The maximum number of tokens to generate in the response",
                "type": AbstractDataProviderTypeMap."*softint",
            },
            "systemInstruction": {
                "display_name": "System Instruction",
                "short_desc": "System instruction text",
                "desc": "Optional system instruction text that sets the behavior and context for the model",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Simple chat response data type
public class GeminiSimpleChatResponseDataType inherits HashDataType {
    public {
        const Fields = {
            "text": {
                "display_name": "Text",
                "short_desc": "The generated text response",
                "desc": "The generated text response from the model",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "finishReason": {
                "display_name": "Finish Reason",
                "short_desc": "The reason generation stopped",
                "desc": "The reason the model stopped generating",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "promptTokenCount": {
                "display_name": "Prompt Token Count",
                "short_desc": "Number of tokens in the prompt",
                "desc": "Number of tokens in the prompt",
                "type": AbstractDataProviderTypeMap."*softint",
            },
            "candidatesTokenCount": {
                "display_name": "Candidates Token Count",
                "short_desc": "Number of tokens in the response",
                "desc": "Number of tokens in the generated response",
                "type": AbstractDataProviderTypeMap."*softint",
            },
            "totalTokenCount": {
                "display_name": "Total Token Count",
                "short_desc": "Total number of tokens",
                "desc": "Total token count (prompt + response)",
                "type": AbstractDataProviderTypeMap."*softint",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Embed content request data type
public class GeminiEmbedContentRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "model": {
                "display_name": "Model",
                "short_desc": "The embedding model to use",
                "desc": "The model name to use for embedding (e.g., `text-embedding-004`)",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "ref_data": "models",
            },
            "taskType": {
                "display_name": "Task Type",
                "short_desc": "The intended task type for the embedding",
                "desc": "Optional task type for which the embeddings will be used",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "RETRIEVAL_QUERY",
                        "desc": "Specifies the given text is a query in a search/retrieval setting",
                    },
                    <AllowedValueInfo>{
                        "value": "RETRIEVAL_DOCUMENT",
                        "desc": "Specifies the given text is a document from the corpus being searched",
                    },
                    <AllowedValueInfo>{
                        "value": "SEMANTIC_SIMILARITY",
                        "desc": "Specifies the given text will be used for semantic text similarity",
                    },
                    <AllowedValueInfo>{
                        "value": "CLASSIFICATION",
                        "desc": "Specifies that the given text will be classified",
                    },
                    <AllowedValueInfo>{
                        "value": "CLUSTERING",
                        "desc": "Specifies that the embeddings will be used for clustering",
                    },
                ),
            },
            "title": {
                "display_name": "Title",
                "short_desc": "An optional title for the text",
                "desc": "An optional title for the text. Only applicable when task_type is RETRIEVAL_DOCUMENT",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "outputDimensionality": {
                "display_name": "Output Dimensionality",
                "short_desc": "Optional reduced dimension for the output embedding",
                "desc": "Optional reduced dimension for the output embedding. If set, excessive values in the "
                    "output embedding are truncated from the end",
                "type": AbstractDataProviderTypeMap."*softint",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
        addField(new QoreDataField({
            "name": "content",
            "display_name": "Content",
            "short_desc": "The content to embed",
            "desc": "The content to embed. Only the `parts.text` fields will be counted",
            "type": new GeminiContentDataType(),
            "required": True,
        }));
    }
}

#! Embed content response data type
public class GeminiEmbedContentResponseDataType inherits HashDataType {
    public {
        const Fields = {
            "values": {
                "display_name": "Values",
                "short_desc": "The embedding values",
                "desc": "The embedding values as a list of floats",
                "type": AbstractDataProviderTypeMap."softlist",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Count tokens request data type
public class GeminiCountTokensRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "model": {
                "display_name": "Model",
                "short_desc": "The model to use for token counting",
                "desc": "The model name to use for token counting",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "ref_data": "models",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
        addField(new QoreDataField({
            "name": "contents",
            "display_name": "Contents",
            "short_desc": "The content to count tokens for",
            "desc": "The content to count tokens for",
            "type": new ListDataType("CountTokensContentList", new GeminiContentDataType()),
            "required": True,
        }));
    }
}

#! Count tokens response data type
public class GeminiCountTokensResponseDataType inherits HashDataType {
    public {
        const Fields = {
            "totalTokens": {
                "display_name": "Total Tokens",
                "short_desc": "The total number of tokens",
                "desc": "The number of tokens that the model tokenizes the content to",
                "type": AbstractDataProviderTypeMap."softint",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Batch embed request data type
public class GeminiBatchEmbedContentsRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "model": {
                "display_name": "Model",
                "short_desc": "The embedding model to use",
                "desc": "The model name to use for embedding (e.g., `text-embedding-004`)",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "ref_data": "models",
            },
            "requests": {
                "display_name": "Requests",
                "short_desc": "Embed requests to batch",
                "desc": "List of individual embed content requests. The `model` field in each request is ignored "
                    "in favor of the top-level model",
                "type": AbstractDataProviderTypeMap."softlist",
                "required": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Batch embed response data type
public class GeminiBatchEmbedContentsResponseDataType inherits HashDataType {
    public {
        const Fields = {
            "embeddings": {
                "display_name": "Embeddings",
                "short_desc": "The generated embeddings",
                "desc": "The generated embeddings for each request, in the same order as provided in the batch "
                    "request",
                "type": AbstractDataProviderTypeMap."softlist",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Model data type
public class GeminiModelInfoDataType inherits HashDataType {
    public {
        const Fields = {
            "name": {
                "display_name": "Name",
                "short_desc": "The resource name of the model",
                "desc": "The resource name of the model (e.g., `models/gemini-2.5-flash`)",
                "type": AbstractDataProviderTypeMap."string",
            },
            "displayName": {
                "display_name": "Display Name",
                "short_desc": "The human-readable name",
                "desc": "The human-readable name of the model",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "description": {
                "display_name": "Description",
                "short_desc": "A short description of the model",
                "desc": "A short description of the model",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "version": {
                "display_name": "Version",
                "short_desc": "The version of the model",
                "desc": "The version number of the model",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "inputTokenLimit": {
                "display_name": "Input Token Limit",
                "short_desc": "Maximum number of input tokens",
                "desc": "Maximum number of input tokens allowed for this model",
                "type": AbstractDataProviderTypeMap."*softint",
            },
            "outputTokenLimit": {
                "display_name": "Output Token Limit",
                "short_desc": "Maximum number of output tokens",
                "desc": "Maximum number of output tokens available for this model",
                "type": AbstractDataProviderTypeMap."*softint",
            },
            "supportedGenerationMethods": {
                "display_name": "Supported Generation Methods",
                "short_desc": "The model's supported generation methods",
                "desc": "The model's supported generation methods (e.g., `generateContent`, "
                    "`streamGenerateContent`)",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
        setDefaultOtherFieldType(AbstractDataProviderTypeMap."any");
    }
}

#! File info data type
public class GeminiFileInfoDataType inherits HashDataType {
    public {
        const Fields = {
            "name": {
                "display_name": "Name",
                "short_desc": "The resource name of the file",
                "desc": "The resource name of the file (e.g., `files/abc-123`)",
                "type": AbstractDataProviderTypeMap."string",
            },
            "displayName": {
                "display_name": "Display Name",
                "short_desc": "The human-readable display name",
                "desc": "The human-readable display name for the file",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "mimeType": {
                "display_name": "MIME Type",
                "short_desc": "MIME type of the file",
                "desc": "MIME type of the file",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "sizeBytes": {
                "display_name": "Size (bytes)",
                "short_desc": "Size of the file in bytes",
                "desc": "Size of the file in bytes",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "createTime": {
                "display_name": "Create Time",
                "short_desc": "The timestamp when the file was created",
                "desc": "The timestamp when the file was created",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "updateTime": {
                "display_name": "Update Time",
                "short_desc": "The timestamp when the file was last updated",
                "desc": "The timestamp when the file was last updated",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "expirationTime": {
                "display_name": "Expiration Time",
                "short_desc": "The timestamp when the file will be deleted",
                "desc": "The timestamp when the file will be automatically deleted",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "sha256Hash": {
                "display_name": "SHA-256 Hash",
                "short_desc": "SHA-256 hash of the file",
                "desc": "SHA-256 hash of the uploaded bytes",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "uri": {
                "display_name": "URI",
                "short_desc": "The URI of the file",
                "desc": "The URI of the file",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "state": {
                "display_name": "State",
                "short_desc": "Processing state of the file",
                "desc": "Processing state of the file: `PROCESSING`, `ACTIVE`, or `FAILED`",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
        setDefaultOtherFieldType(AbstractDataProviderTypeMap."any");
    }
}

#! Cached content data type
public class GeminiCachedContentInfoDataType inherits HashDataType {
    public {
        const Fields = {
            "name": {
                "display_name": "Name",
                "short_desc": "The resource name of the cached content",
                "desc": "The resource name of the cached content (e.g., `cachedContents/abc-123`)",
                "type": AbstractDataProviderTypeMap."string",
            },
            "displayName": {
                "display_name": "Display Name",
                "short_desc": "The human-readable display name",
                "desc": "The human-readable display name for the cached content",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "model": {
                "display_name": "Model",
                "short_desc": "The model associated with the cached content",
                "desc": "The name of the model to use for cached content",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "createTime": {
                "display_name": "Create Time",
                "short_desc": "Creation timestamp",
                "desc": "Creation time of the cache entry",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "updateTime": {
                "display_name": "Update Time",
                "short_desc": "Last update timestamp",
                "desc": "When the cache entry was last updated",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "expireTime": {
                "display_name": "Expire Time",
                "short_desc": "Expiration timestamp",
                "desc": "Timestamp when this resource is considered expired",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
        setDefaultOtherFieldType(AbstractDataProviderTypeMap."any");
    }
}

#! File upload request data type
public class GeminiFileUploadRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "display_name": {
                "display_name": "Display Name",
                "short_desc": "Display name for the uploaded file",
                "desc": "Optional display name for the uploaded file",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "mime_type": {
                "display_name": "MIME Type",
                "short_desc": "The MIME type of the file",
                "desc": "The MIME type of the file being uploaded",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "file_data": {
                "display_name": "File Data",
                "short_desc": "The file data to upload",
                "desc": "Base64-encoded file data to upload",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Cached content create request data type
public class GeminiCachedContentCreateRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "model": {
                "display_name": "Model",
                "short_desc": "The model to use",
                "desc": "The name of the model to use for cached content (e.g., `models/gemini-2.5-flash`)",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "ref_data": "models",
            },
            "displayName": {
                "display_name": "Display Name",
                "short_desc": "Display name for the cached content",
                "desc": "Optional human-readable display name",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "expireTime": {
                "display_name": "Expire Time",
                "short_desc": "Expiration timestamp",
                "desc": "Timestamp in UTC when the cached content should expire (RFC 3339 format)",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "ttl": {
                "display_name": "TTL",
                "short_desc": "Time-to-live duration",
                "desc": "Time-to-live for the cached content (e.g., `3600s` for one hour)",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
        addField(new QoreDataField({
            "name": "contents",
            "display_name": "Contents",
            "short_desc": "The content to cache",
            "desc": "The content to cache for future use",
            "type": new ListDataType("CachedContentList", new GeminiContentDataType()),
            "required": True,
        }));
        addField(new QoreDataField({
            "name": "systemInstruction",
            "display_name": "System Instruction",
            "short_desc": "System instruction to cache",
            "desc": "Optional system instruction to include in the cache",
            "type": new GeminiContentDataType(),
        }));
    }
}

#! Cached content update request data type
public class GeminiCachedContentUpdateRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "expireTime": {
                "display_name": "Expire Time",
                "short_desc": "New expiration timestamp",
                "desc": "New expiration timestamp in UTC (RFC 3339 format)",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "ttl": {
                "display_name": "TTL",
                "short_desc": "New time-to-live duration",
                "desc": "New time-to-live for the cached content (e.g., `3600s` for one hour)",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! File search store data type
public class GeminiFileSearchStoreInfoDataType inherits HashDataType {
    public {
        const Fields = {
            "name": {
                "display_name": "Name",
                "short_desc": "The resource name of the file search store",
                "desc": "The resource name of the file search store",
                "type": AbstractDataProviderTypeMap."string",
            },
            "displayName": {
                "display_name": "Display Name",
                "short_desc": "The human-readable display name",
                "desc": "The human-readable display name for the store",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
        setDefaultOtherFieldType(AbstractDataProviderTypeMap."any");
    }
}

#! File search store create request data type
public class GeminiFileSearchStoreCreateRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "displayName": {
                "display_name": "Display Name",
                "short_desc": "Display name for the store",
                "desc": "The human-readable display name for the new file search store",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! File search store import file request data type
public class GeminiFileSearchStoreImportFileRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "file_name": {
                "display_name": "File Name",
                "short_desc": "The resource name of the file to import",
                "desc": "The resource name of the file to import (e.g., `files/abc-123`)",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Stream event data type
public class GeminiStreamEventDataType inherits HashDataType {
    public {
        const Fields = {
            "type": {
                "display_name": "Event Type",
                "short_desc": "The stream event type",
                "desc": "The stream event type",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "id": {
                "display_name": "Event ID",
                "short_desc": "SSE event ID",
                "desc": "SSE event ID",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "event": {
                "display_name": "Event Name",
                "short_desc": "SSE event name",
                "desc": "SSE event name",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "raw_data": {
                "display_name": "Raw Data",
                "short_desc": "Raw event data",
                "desc": "Raw event data when JSON parsing is not possible",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "error": {
                "display_name": "Error",
                "short_desc": "Error payload for stream failures",
                "desc": "Exception info when a stream error occurs",
                "type": AbstractDataProviderTypeMap."*hash",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
        setDefaultOtherFieldType(AbstractDataProviderTypeMap."any");
    }
}

public const GeminiStreamEventDataType = new GeminiStreamEventDataType();
}
