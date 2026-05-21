# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore Jina data type definitions

/** JinaDataTypes.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace JinaDataProvider {
public class JinaEmbeddingRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "model": {
                "display_name": "Model",
                "short_desc": "Jina embedding model",
                "desc": "Jina embedding model identifier.  Examples:\n\n"
                    "- `jina-embeddings-v3` — multilingual, task-aware\n"
                    "- `jina-embeddings-v2-base-en` — English-only v2\n"
                    "- `jina-embeddings-v2-base-code` — code-specialized",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "jina-embeddings-v3",
            },
            "input": {
                "display_name": "Input",
                "short_desc": "Text(s) to embed",
                "desc": "Text input(s) to convert to embedding vectors.  Single string or "
                    "list of strings.",
                "type": AbstractDataProviderTypeMap."softlist",
                "required": True,
                "example_value": ("Hello, world!",),
            },
            "task": {
                "display_name": "Task Hint",
                "short_desc": "Task hint for jina-embeddings-v3",
                "desc": "Task-aware embedding hint for `jina-embeddings-v3`.  Choosing the "
                    "right task improves retrieval relevance — queries and passages get "
                    "different optimal embeddings.",
                "type": AbstractDataProviderTypeMap."*softstring",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "retrieval.query",
                        "display_name": "Retrieval Query"},
                    <AllowedValueInfo>{"value": "retrieval.passage",
                        "display_name": "Retrieval Passage"},
                    <AllowedValueInfo>{"value": "separation",
                        "display_name": "Separation"},
                    <AllowedValueInfo>{"value": "classification",
                        "display_name": "Classification"},
                    <AllowedValueInfo>{"value": "text-matching",
                        "display_name": "Text Matching"},
                ),
            },
            "dimensions": {
                "display_name": "Dimensions",
                "short_desc": "Output dimensionality (for models that support truncation)",
                "desc": "Optional truncated output dimensionality.  `jina-embeddings-v3` "
                    "supports Matryoshka representation learning and can be truncated to "
                    "smaller dimensions with minimal quality loss.",
                "type": AbstractDataProviderTypeMap."*softint",
            },
            "encoding_format": {
                "display_name": "Encoding Format",
                "short_desc": "Wire format for the returned vectors",
                "desc": "`float` (default) or `base64`.",
                "type": AbstractDataProviderTypeMap."*softstring",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "float",
                        "display_name": "Float - JSON list of floats"},
                    <AllowedValueInfo>{"value": "base64",
                        "display_name": "Base64 - encoded float32 buffer"},
                ),
            },
            "late_chunking": {
                "display_name": "Late Chunking",
                "short_desc": "Enable Jina's late-chunking algorithm",
                "desc": "When `True`, applies Jina's late-chunking algorithm — embeds the "
                    "full passage first, then chunks afterward.  Improves cross-chunk "
                    "context retention for RAG.",
                "type": AbstractDataProviderTypeMap."*softbool",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public class JinaEmbeddingItemDataType inherits HashDataType {
    private {
        const Fields = {
            "index": {
                "display_name": "Index",
                "desc": "Position of this embedding in the input list.",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "object": {
                "display_name": "Object Type",
                "desc": "Object type discriminator (typically 'embedding').",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "embedding": {
                "display_name": "Embedding Vector",
                "desc": "The embedding vector — a list of floats.",
                "type": AbstractDataProviderTypeMap."*list",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public class JinaUsageDataType inherits HashDataType {
    private {
        const Fields = {
            "total_tokens": {
                "display_name": "Total Tokens",
                "desc": "Total tokens consumed.",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "prompt_tokens": {
                "display_name": "Prompt Tokens",
                "desc": "Prompt tokens consumed.",
                "type": AbstractDataProviderTypeMap."*int",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public class JinaEmbeddingResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "model": {
                "display_name": "Model",
                "desc": "The embedding model that produced these vectors.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "object": {
                "display_name": "Object Type",
                "desc": "Object type discriminator (typically 'list').",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "data": {
                "display_name": "Embeddings",
                "desc": "List of embedding results, one per input text, in request order.",
                "type": new ListDataType("JinaEmbeddingList",
                    new JinaEmbeddingItemDataType(), True),
            },
            "usage": {
                "display_name": "Usage",
                "desc": "Token usage information for this embedding request.",
                "type": new JinaUsageDataType(),
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public class JinaRerankRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "model": {
                "display_name": "Model",
                "short_desc": "Jina reranker model",
                "desc": "Jina reranker model identifier.  Examples:\n\n"
                    "- `jina-reranker-v2-base-multilingual` — multilingual cross-encoder\n"
                    "- `jina-colbert-v2` — late-interaction reranker (multi-vector)",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "jina-reranker-v2-base-multilingual",
            },
            "query": {
                "display_name": "Query",
                "short_desc": "The user query to rerank documents against",
                "desc": "Free-form user query.  The model scores each document by how "
                    "relevant it is to this query.",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "what is the population of Tokyo?",
            },
            "documents": {
                "display_name": "Documents",
                "short_desc": "Candidate documents to rerank",
                "desc": "List of candidate documents (strings) to rerank against the query.",
                "type": AbstractDataProviderTypeMap."softlist",
                "required": True,
            },
            "top_n": {
                "display_name": "Top N",
                "short_desc": "How many top-scoring results to return",
                "desc": "Number of top-ranked documents to return.  Defaults to all if "
                    "omitted.",
                "type": AbstractDataProviderTypeMap."*softint",
                "example_value": 5,
            },
            "return_documents": {
                "display_name": "Return Documents",
                "short_desc": "Whether to echo the document text in the response",
                "desc": "When `True` (default), each result includes the document text.",
                "type": AbstractDataProviderTypeMap."*softbool",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public class JinaRerankResultDataType inherits HashDataType {
    private {
        const Fields = {
            "index": {
                "display_name": "Index",
                "desc": "Original index of this document in the input `documents` list.",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "relevance_score": {
                "display_name": "Relevance Score",
                "desc": "Relevance score (higher = more relevant).",
                "type": AbstractDataProviderTypeMap."*float",
            },
            "document": {
                "display_name": "Document",
                "desc": "Echoed document text or hash (only when `return_documents` is True).",
                "type": AbstractDataProviderTypeMap."*hash",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public class JinaRerankResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "model": {
                "display_name": "Model",
                "desc": "The reranker model that produced these scores.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "results": {
                "display_name": "Results",
                "desc": "List of reranked results, sorted by descending relevance score.",
                "type": new ListDataType("JinaRerankResultList",
                    new JinaRerankResultDataType(), True),
            },
            "usage": {
                "display_name": "Usage",
                "desc": "Token usage information for this rerank request.",
                "type": new JinaUsageDataType(),
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}
}
