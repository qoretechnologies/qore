# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore VoyageDataProvider module definition

/** VoyageEmbeddingsRequestDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the VoyageDataProvider module
public namespace VoyageDataProvider {
#! Data type for Voyage AI embeddings request
public class VoyageEmbeddingsRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "model": {
                "display_name": "Model",
                "short_desc": "The AI model to use for generating embeddings",
                "desc": "Choose which Voyage AI model to use for converting your text into embeddings.\n\n"
                    "**Recommended models**:\n"
                    "- `voyage-4-large` — highest quality, best for multilingual content and complex documents\n"
                    "- `voyage-4` — good balance of quality and speed for most use cases\n"
                    "- `voyage-4-lite` — fastest and cheapest, good for high-volume or latency-sensitive workloads\n"
                    "- `voyage-code-3` — specialized for source code search and retrieval\n\n"
                    "**Domain-specific models** (trained on specialized data):\n"
                    "- `voyage-finance-2` — financial documents, SEC filings, earnings reports\n"
                    "- `voyage-law-2` — legal contracts, case law, regulations",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "voyage-4-large",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "voyage-4-large",
                        "display_name": "Voyage 4 Large - Best Quality (general-purpose, multilingual)"},
                    <AllowedValueInfo>{"value": "voyage-4",
                        "display_name": "Voyage 4 - Balanced (good quality and speed)"},
                    <AllowedValueInfo>{"value": "voyage-4-lite",
                        "display_name": "Voyage 4 Lite - Fastest (high-volume workloads)"},
                    <AllowedValueInfo>{"value": "voyage-code-3",
                        "display_name": "Voyage Code 3 - Code Search (source code retrieval)"},
                    <AllowedValueInfo>{"value": "voyage-finance-2",
                        "display_name": "Voyage Finance 2 - Finance (SEC filings, earnings)"},
                    <AllowedValueInfo>{"value": "voyage-law-2",
                        "display_name": "Voyage Law 2 - Legal (contracts, case law)"},
                    <AllowedValueInfo>{"value": "voyage-3-large",
                        "display_name": "Voyage 3 Large - Previous Gen (general-purpose)"},
                    <AllowedValueInfo>{"value": "voyage-3",
                        "display_name": "Voyage 3 - Previous Gen (balanced)"},
                    <AllowedValueInfo>{"value": "voyage-multilingual-2",
                        "display_name": "Voyage Multilingual 2 - Previous Gen (multilingual)"},
                ),
            },
            "input": {
                "display_name": "Input Text",
                "short_desc": "The text or list of texts to convert into embeddings",
                "desc": "The text content to convert into embeddings. Can be a single string or a list of "
                    "strings for batch processing.\n\n"
                    "**Examples**:\n"
                    "- Single text: `\"How do I reset my password?\"`\n"
                    "- Batch: `[\"Product A description\", \"Product B description\", \"Product C description\"]`\n\n"
                    "Maximum **1,000** items per request. Total token limits vary by model",
                "type": AbstractDataProviderTypeMap."softlist",
                "required": True,
                "example_value": ("The quick brown fox jumps over the lazy dog",),
            },
            "input_type": {
                "display_name": "Input Type",
                "short_desc": "Whether the text is a search query or a document to be searched",
                "desc": "Tells the model whether the input text is a search query or a document to be "
                    "searched. This optimizes the embeddings for better search accuracy.\n\n"
                    "- Use **Query** when embedding a user's search question (e.g., "
                    "`\"How do I reset my password?\"`)\n"
                    "- Use **Document** when embedding content that will be searched through (e.g., "
                    "knowledge base articles, product descriptions)\n\n"
                    "Leave empty if you're not using the embeddings for search/retrieval",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "document",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "query",
                        "display_name": "Query - Search question (e.g., user's search input)"},
                    <AllowedValueInfo>{"value": "document",
                        "display_name": "Document - Content to be searched (e.g., articles, products)"},
                ),
            },
            "truncation": {
                "display_name": "Auto-Truncate Long Text",
                "short_desc": "Automatically shorten text that exceeds the model's limit",
                "desc": "When enabled (the default), text that is too long for the model is automatically "
                    "shortened to fit. When disabled, an error is returned instead, which is useful if you "
                    "need to ensure the complete text is processed",
                "type": AbstractDataProviderTypeMap."*softbool",
            },
            "output_dimension": {
                "display_name": "Output Dimensions",
                "short_desc": "Size of the embedding vector (affects accuracy vs. storage)",
                "desc": "Controls the size of the output embedding vectors. Higher dimensions give more "
                    "accurate results but use more storage and memory.\n\n"
                    "- **256** — smallest, ~4x less storage, good for large-scale approximate search\n"
                    "- **512** — compact, good balance for most applications\n"
                    "- **1024** — default, recommended for high-accuracy search\n"
                    "- **2048** — highest accuracy, best when storage is not a concern\n\n"
                    "Not all models support all dimensions. Default is `1024`",
                "type": AbstractDataProviderTypeMap."*int",
                "example_value": 1024,
                "allowed_values": (
                    <AllowedValueInfo>{"value": 256, "display_name": "256 - Smallest (least storage)"},
                    <AllowedValueInfo>{"value": 512, "display_name": "512 - Compact (good balance)"},
                    <AllowedValueInfo>{"value": 1024, "display_name": "1024 - Default (recommended)"},
                    <AllowedValueInfo>{"value": 2048, "display_name": "2048 - Largest (highest accuracy)"},
                ),
            },
            "output_dtype": {
                "display_name": "Output Precision",
                "short_desc": "Numeric precision of embedding values (affects storage size)",
                "desc": "Controls the numeric precision of embedding values. Lower precision uses "
                    "less storage with a small accuracy trade-off.\n\n"
                    "- **Float** — full precision (default), best accuracy\n"
                    "- **Int8/Uint8** — 4x less storage, minimal accuracy loss\n"
                    "- **Binary/Unsigned Binary** — 32x less storage, suitable for large-scale approximate "
                    "search\n\n"
                    "Use the default (`float`) unless you need to optimize storage for millions of embeddings",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "float",
                        "display_name": "Float - Full Precision (default, best accuracy)"},
                    <AllowedValueInfo>{"value": "int8",
                        "display_name": "Int8 - Compact (4x less storage, minimal accuracy loss)"},
                    <AllowedValueInfo>{"value": "uint8",
                        "display_name": "Uint8 - Compact Unsigned (4x less storage)"},
                    <AllowedValueInfo>{"value": "binary",
                        "display_name": "Binary - Minimal Storage (32x less, for large-scale search)"},
                    <AllowedValueInfo>{"value": "ubinary",
                        "display_name": "Unsigned Binary - Minimal Storage (32x less)"},
                ),
            },
            "encoding_format": {
                "display_name": "Encoding Format",
                "short_desc": "How embedding values are encoded in the response",
                "desc": "Controls how embedding vectors are formatted in the API response.\n\n"
                    "By default, embeddings are returned as plain number arrays. Set to `base64` to "
                    "receive base64-encoded binary data, which can be more efficient for network transfer "
                    "when using compact output types like `int8` or `binary`",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "base64",
                        "display_name": "Base64 - Encoded Binary (efficient for compact types)"},
                ),
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data type for a single embedding item in the response
public class VoyageEmbeddingItemDataType inherits HashDataType {
    public {
        const Fields = {
            "object": {
                "display_name": "Object Type",
                "desc": "Always `embedding`",
                "type": AbstractDataProviderTypeMap."string",
            },
            "embedding": {
                "display_name": "Embedding",
                "desc": "The embedding vector",
                "type": AbstractDataProviderTypeMap."softlist",
            },
            "index": {
                "display_name": "Index",
                "desc": "The index of this embedding in the input list",
                "type": AbstractDataProviderTypeMap."int",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data type for token usage information
public class VoyageUsageDataType inherits HashDataType {
    public {
        const Fields = {
            "total_tokens": {
                "display_name": "Total Tokens",
                "desc": "Total number of tokens processed",
                "type": AbstractDataProviderTypeMap."int",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}
}
