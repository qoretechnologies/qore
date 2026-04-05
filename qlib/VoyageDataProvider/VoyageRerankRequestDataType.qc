# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore VoyageDataProvider module definition

/** VoyageRerankRequestDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Data type for Voyage AI rerank request
public class VoyageRerankRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "model": {
                "display_name": "Model",
                "short_desc": "The AI model to use for reranking documents",
                "desc": "Choose which Voyage AI model to use for reranking your documents.\n\n"
                    "- `rerank-2.5` — best quality, most accurate relevance scoring\n"
                    "- `rerank-2.5-lite` — faster and cheaper, good for high-volume workloads\n"
                    "- `rerank-2` — previous generation model",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "rerank-2.5",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "rerank-2.5",
                        "display_name": "Rerank 2.5 - Best Quality (most accurate relevance scoring)"},
                    <AllowedValueInfo>{"value": "rerank-2.5-lite",
                        "display_name": "Rerank 2.5 Lite - Fastest (high-volume workloads)"},
                    <AllowedValueInfo>{"value": "rerank-2",
                        "display_name": "Rerank 2 - Previous Generation"},
                ),
            },
            "query": {
                "display_name": "Search Query",
                "short_desc": "The search question to rank documents against",
                "desc": "The search query or question that documents will be ranked against. "
                    "Documents most relevant to this query will be ranked highest.\n\n"
                    "**Example**: `\"How do I cancel my subscription?\"`",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "What is machine learning?",
            },
            "documents": {
                "display_name": "Documents",
                "short_desc": "The list of documents or text snippets to rank",
                "desc": "A list of text documents to rank by relevance to the search query. These can be "
                    "full documents, paragraphs, or short text snippets.\n\n"
                    "**Example**: `[\"Guide to canceling subscriptions...\", \"Billing FAQ...\", "
                    "\"How to upgrade your plan...\"]`\n\n"
                    "Maximum **1,000** documents per request",
                "type": AbstractDataProviderTypeMap."softlist",
                "required": True,
                "example_value": (
                    "Machine learning is a subset of artificial intelligence.",
                    "The weather today is sunny.",
                ),
            },
            "top_k": {
                "display_name": "Number of Results",
                "short_desc": "How many top results to return",
                "desc": "Limits the results to only the top most-relevant documents.\n\n"
                    "For example, set to `5` to get only the 5 most relevant documents. "
                    "If not specified, all documents are returned sorted by relevance score",
                "type": AbstractDataProviderTypeMap."*int",
                "example_value": 5,
            },
            "return_documents": {
                "display_name": "Include Document Text",
                "short_desc": "Include the original document text in each result",
                "desc": "When enabled, each result includes the full text of the document alongside its "
                    "relevance score. This is convenient when you want to display results directly without "
                    "looking up documents by their index.\n\n"
                    "Disabled by default to reduce response size",
                "type": AbstractDataProviderTypeMap."*softbool",
            },
            "truncation": {
                "display_name": "Auto-Truncate Long Text",
                "short_desc": "Automatically shorten text that exceeds the model's limit",
                "desc": "When enabled (the default), text that is too long for the model is automatically "
                    "shortened to fit. When disabled, an error is returned instead, which is useful if you "
                    "need to ensure the complete text is processed",
                "type": AbstractDataProviderTypeMap."*softbool",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data type for a single rerank result item
public class VoyageRerankItemDataType inherits HashDataType {
    public {
        const Fields = {
            "index": {
                "display_name": "Index",
                "desc": "The index of this document in the original input list",
                "type": AbstractDataProviderTypeMap."int",
            },
            "relevance_score": {
                "display_name": "Relevance Score",
                "desc": "The relevance score for this document (higher is more relevant)",
                "type": AbstractDataProviderTypeMap."float",
            },
            "document": {
                "display_name": "Document",
                "desc": "The original document text (only present if `return_documents` was `true`)",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}
}
