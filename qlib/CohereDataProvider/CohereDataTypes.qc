# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore Cohere data type definitions

/** CohereDataTypes.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace CohereDataProvider {
public class CohereChatMessageDataType inherits HashDataType {
    public {
        const Fields = {
            "role": {
                "display_name": "Role",
                "short_desc": "Role of the message author",
                "desc": "Role: `system`, `user`, `assistant`, or `tool`.",
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
                "short_desc": "Text content (or list of typed content parts)",
                "desc": "Text content of the message.  For assistant messages with "
                    "`tool_calls`, may be empty.",
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
                "type": AbstractDataProviderTypeMap."*softlist",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public class CohereChatRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "model": {
                "display_name": "Model",
                "short_desc": "Cohere chat model",
                "desc": "Cohere chat model identifier, e.g. `command-r-plus`, `command-r`, "
                    "`command-r7b`.",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "command-r-plus",
            },
            "messages": {
                "display_name": "Messages",
                "short_desc": "List of chat messages forming the conversation",
                "desc": "The conversation history as a list of message objects.",
                "type": new ListDataType("CohereChatMessageList",
                    new CohereChatMessageDataType(), False),
                "required": True,
                "example_value": (
                    {"role": "user", "content": "Hello!"},
                ),
            },
            "temperature": {
                "display_name": "Temperature",
                "short_desc": "Sampling temperature (0-1)",
                "desc": "Controls randomness of the output.  Cohere recommends `0.3` as default.",
                "type": AbstractDataProviderTypeMap."*softfloat",
                "example_value": 0.3,
            },
            "max_tokens": {
                "display_name": "Max Tokens",
                "short_desc": "Maximum tokens to emit",
                "desc": "Maximum number of tokens to generate.",
                "type": AbstractDataProviderTypeMap."*softint",
            },
            "p": {
                "display_name": "Top-P",
                "short_desc": "Nucleus sampling cutoff (0-1)",
                "desc": "Nucleus sampling cutoff.",
                "type": AbstractDataProviderTypeMap."*softfloat",
            },
            "k": {
                "display_name": "Top-K",
                "short_desc": "Sample only from the K most likely next tokens",
                "desc": "Restrict sampling to the K most likely next tokens.",
                "type": AbstractDataProviderTypeMap."*softint",
            },
            "stop_sequences": {
                "display_name": "Stop Sequences",
                "short_desc": "List of stop sequences",
                "desc": "List of stop sequences; generation halts when any is emitted.",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "tools": {
                "display_name": "Tools",
                "short_desc": "Tool definitions the model may call",
                "desc": "List of tool definitions.  Each entry has `type: function` and a "
                    "`function` hash with `name`, `description`, and JSON-schema "
                    "`parameters`.",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "tool_choice": {
                "display_name": "Tool Choice",
                "short_desc": "Tool invocation mode",
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
                "short_desc": "Force structured output (e.g. JSON schema)",
                "desc": "Hash that forces a structured response shape, e.g. "
                    "`{type: \"json_object\"}` or "
                    "`{type: \"json_object\", schema: {...}}`.",
                "type": AbstractDataProviderTypeMap."*hash",
            },
            "seed": {
                "display_name": "Seed",
                "short_desc": "Deterministic sampling seed",
                "desc": "Integer seed for deterministic sampling.",
                "type": AbstractDataProviderTypeMap."*softint",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public class CohereChatResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {
                "display_name": "Response ID",
                "desc": "Cohere-assigned unique identifier for this response.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "finish_reason": {
                "display_name": "Finish Reason",
                "desc": "Why generation stopped: `COMPLETE`, `MAX_TOKENS`, `TOOL_CALL`, "
                    "`STOP_SEQUENCE`, etc.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "message": {
                "display_name": "Message",
                "desc": "The assistant message — hash with `role`, `content` (list of "
                    "typed parts), optional `tool_calls`.",
                "type": AbstractDataProviderTypeMap."*hash",
            },
            "usage": {
                "display_name": "Usage",
                "desc": "Token usage information for this request (Cohere shape: "
                    "`tokens.input_tokens` + `tokens.output_tokens`).",
                "type": AbstractDataProviderTypeMap."*hash",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public class CohereEmbedRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "model": {
                "display_name": "Model",
                "short_desc": "Cohere embedding model",
                "desc": "Cohere embedding model identifier.  Examples:\n\n"
                    "- `embed-multilingual-v3.0` — 100+ languages (recommended default)\n"
                    "- `embed-english-v3.0` — English-only, slightly faster\n"
                    "- `embed-multilingual-light-v3.0` — smaller, lower-cost variant",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "embed-multilingual-v3.0",
            },
            "texts": {
                "display_name": "Texts",
                "short_desc": "List of texts to embed",
                "desc": "List of input texts to convert to embedding vectors.",
                "type": AbstractDataProviderTypeMap."softlist",
                "required": True,
                "example_value": ("Hello, world!",),
            },
            "input_type": {
                "display_name": "Input Type",
                "short_desc": "Task hint (required for v3 models)",
                "desc": "Task hint required by Cohere v3 embedding models.  Choosing the "
                    "right input type improves retrieval relevance — query and document "
                    "embeddings are optimized differently.",
                "type": AbstractDataProviderTypeMap."*softstring",
                "example_value": "search_document",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "search_document",
                        "display_name": "Search Document - text to be indexed"},
                    <AllowedValueInfo>{"value": "search_query",
                        "display_name": "Search Query - user query"},
                    <AllowedValueInfo>{"value": "classification",
                        "display_name": "Classification"},
                    <AllowedValueInfo>{"value": "clustering",
                        "display_name": "Clustering"},
                ),
            },
            "truncate": {
                "display_name": "Truncate",
                "short_desc": "How to truncate inputs that exceed context",
                "desc": "How to handle inputs that exceed the embedding model's context "
                    "window.  Default `END`.",
                "type": AbstractDataProviderTypeMap."*softstring",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "NONE",
                        "display_name": "None - error on overflow"},
                    <AllowedValueInfo>{"value": "START",
                        "display_name": "Start - truncate beginning"},
                    <AllowedValueInfo>{"value": "END",
                        "display_name": "End - truncate end (default)"},
                ),
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public class CohereEmbedResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {
                "display_name": "Response ID",
                "desc": "Cohere-assigned response ID.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "embeddings": {
                "display_name": "Embeddings",
                "desc": "Hash with `float`, `int8`, `binary` etc. keys depending on "
                    "requested `embedding_types`; each holds the list of vectors.",
                "type": AbstractDataProviderTypeMap."*hash",
            },
            "texts": {
                "display_name": "Texts",
                "desc": "Echoed input texts.",
                "type": AbstractDataProviderTypeMap."*list",
            },
            "meta": {
                "display_name": "Meta",
                "desc": "Cohere metadata (api_version, billed_units, etc.).",
                "type": AbstractDataProviderTypeMap."*hash",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public class CohereRerankRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "model": {
                "display_name": "Model",
                "short_desc": "Cohere reranker model",
                "desc": "Cohere reranker model.  Examples:\n\n"
                    "- `rerank-v3.5` — latest cross-encoder (recommended default)\n"
                    "- `rerank-english-v3.0` — English-only v3\n"
                    "- `rerank-multilingual-v3.0` — multilingual v3",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "rerank-v3.5",
            },
            "query": {
                "display_name": "Query",
                "short_desc": "The user query to rerank documents against",
                "desc": "Free-form user query.",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "documents": {
                "display_name": "Documents",
                "short_desc": "Candidate documents to rerank",
                "desc": "List of candidate documents to rerank against the query.",
                "type": AbstractDataProviderTypeMap."softlist",
                "required": True,
            },
            "top_n": {
                "display_name": "Top N",
                "short_desc": "How many top-scoring results to return",
                "desc": "Number of top-ranked documents to return.",
                "type": AbstractDataProviderTypeMap."*softint",
                "example_value": 5,
            },
            "return_documents": {
                "display_name": "Return Documents",
                "short_desc": "Whether to echo the document text in the response",
                "desc": "When `True` (default), each result includes the document.",
                "type": AbstractDataProviderTypeMap."*softbool",
            },
            "max_chunks_per_doc": {
                "display_name": "Max Chunks Per Doc",
                "short_desc": "Maximum chunks per document for long-document rerank",
                "desc": "When documents exceed the rerank context window, Cohere chunks "
                    "them internally.  This caps the number of chunks per document.",
                "type": AbstractDataProviderTypeMap."*softint",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public class CohereRerankResultDataType inherits HashDataType {
    private {
        const Fields = {
            "index": {
                "display_name": "Index",
                "desc": "Original index of this document in the input `documents` list.",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "relevance_score": {
                "display_name": "Relevance Score",
                "desc": "Cohere relevance score (0..1 log-scaled probability).  Higher = "
                    "more relevant.  Scale differs from Voyage's relevance scores — don't "
                    "compare across backends.",
                "type": AbstractDataProviderTypeMap."*float",
            },
            "document": {
                "display_name": "Document",
                "desc": "Echoed document (only when `return_documents` is True).",
                "type": AbstractDataProviderTypeMap."*hash",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public class CohereRerankResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {
                "display_name": "Response ID",
                "desc": "Cohere-assigned response ID.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "results": {
                "display_name": "Results",
                "desc": "List of reranked results, sorted by descending relevance score.",
                "type": new ListDataType("CohereRerankResultList",
                    new CohereRerankResultDataType(), True),
            },
            "meta": {
                "display_name": "Meta",
                "desc": "Cohere metadata (billed_units, api_version, etc.).",
                "type": AbstractDataProviderTypeMap."*hash",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public class CohereClassifyExampleDataType inherits HashDataType {
    public {
        const Fields = {
            "text": {
                "display_name": "Text",
                "desc": "The example input text.",
                "type": AbstractDataProviderTypeMap."string",
            },
            "label": {
                "display_name": "Label",
                "desc": "The label assigned to this example.",
                "type": AbstractDataProviderTypeMap."string",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public class CohereClassifyRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "inputs": {
                "display_name": "Inputs",
                "short_desc": "List of input texts to classify",
                "desc": "List of input texts to classify into one of the labels supplied "
                    "via `examples` (few-shot) or the fine-tuned model's vocabulary.",
                "type": AbstractDataProviderTypeMap."softlist",
                "required": True,
                "example_value": ("This product is amazing!",),
            },
            "examples": {
                "display_name": "Examples",
                "short_desc": "Few-shot labeled examples (required without fine-tuned model)",
                "desc": "List of `{text, label}` hashes providing few-shot examples.  "
                    "Required when no fine-tuned model is supplied; minimum 2 examples "
                    "per label and at least 2 labels.",
                "type": new ListDataType("CohereClassifyExampleList",
                    new CohereClassifyExampleDataType(), True),
            },
            "model": {
                "display_name": "Model",
                "short_desc": "Fine-tuned classification model (optional with examples)",
                "desc": "Fine-tuned classification model identifier.  Optional when "
                    "`examples` is supplied for few-shot classification.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "preset": {
                "display_name": "Preset",
                "short_desc": "Preset name (alternative to inline examples)",
                "desc": "Cohere classification preset name (alternative to inline "
                    "`examples`).",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "truncate": {
                "display_name": "Truncate",
                "short_desc": "How to truncate inputs that exceed context",
                "desc": "Truncation strategy: `NONE`, `START`, or `END` (default).",
                "type": AbstractDataProviderTypeMap."*softstring",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public class CohereClassifyResultDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {
                "display_name": "Result ID",
                "desc": "Cohere-assigned ID for this classification.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "input": {
                "display_name": "Input",
                "desc": "Echoed input text.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "prediction": {
                "display_name": "Prediction",
                "desc": "Predicted label.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "confidence": {
                "display_name": "Confidence",
                "desc": "Confidence score (0..1) for the predicted label.",
                "type": AbstractDataProviderTypeMap."*float",
            },
            "labels": {
                "display_name": "Labels",
                "desc": "Hash of all label confidence scores.",
                "type": AbstractDataProviderTypeMap."*hash",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public class CohereClassifyResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {
                "display_name": "Response ID",
                "desc": "Cohere-assigned response ID.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "classifications": {
                "display_name": "Classifications",
                "desc": "List of classification results, one per input.",
                "type": new ListDataType("CohereClassifyResultList",
                    new CohereClassifyResultDataType(), True),
            },
            "meta": {
                "display_name": "Meta",
                "desc": "Cohere metadata.",
                "type": AbstractDataProviderTypeMap."*hash",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}
}
