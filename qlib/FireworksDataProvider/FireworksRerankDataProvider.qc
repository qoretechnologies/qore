# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore FireworksRerankDataProvider class definition

/** FireworksRerankDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace FireworksDataProvider {
#! Reranks candidate documents for search and retrieval
public class FireworksRerankDataProvider inherits FireworksDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "rerank", "type": "FireworksRerankDataProvider",
            "constructor_options": ConstructorOptions, "supports_request": True,
        };
    }

    #! Creates a reranker using an existing authenticated REST client
    /** @param rest client copied for the inference endpoint */
    constructor(RestClientIo::RestClientIo rest) : FireworksDataProviderCommon(rest) {
    }

    #! Creates a reranker from API-key or REST-client options
    /** @param options connection options
        @throw CONSTRUCTOR-ERROR required credentials are missing or options are invalid
    */
    constructor(*hash<auto> options) : FireworksDataProviderCommon(options) {
    }

    #! Returns the stable action name
    /** @return <tt>rerank</tt> */
    string getName() { return "rerank"; }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private *AbstractDataProviderType getRequestTypeImpl() { return FireworksRerankRequestType; }

    private *AbstractDataProviderType getResponseTypeImpl() { return FireworksRerankResponseType; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        if (!req.documents || !req.query || (exists req.top_n && req.top_n < 1)) {
            throw "FIREWORKS-INPUT-ERROR", "provide a query, non-empty documents, and a positive top_n";
        }
        return doRestCommand("POST", "rerank", req).body;
    }
}

#! Options for selecting the documents most relevant to a query
/** @see https://docs.fireworks.ai/api-reference/rerank-documents */
public class FireworksRerankRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "query": {"display_name": "Query", "type": AbstractDataProviderTypeMap."string",
                "desc": "The search query to match against candidate documents.",
                "example_value": "How do I return an order?", "required": True},
            "documents": {"display_name": "Documents", "type": AbstractDataProviderType::get("list<string>"),
                "desc": "Candidate documents to rank by relevance to `query`.",
                "example_value": ("Returns are accepted within 30 days.", "Shipping takes two days."),
                "required": True},
            "model": {"display_name": "Model", "type": AbstractDataProviderTypeMap."*string",
                "desc": "Reranker model resource name; omit to use the service default.",
                "example_value": "accounts/fireworks/models/qwen3-reranker-8b",
                "ref_data": "inference-models", "supports_custom_values": True},
            "top_n": {"display_name": "Top N", "type": AbstractDataProviderTypeMap."*int",
                "desc": "Maximum number of ranked documents to return.", "example_value": 5},
            "return_documents": {"display_name": "Return Documents", "type": AbstractDataProviderTypeMap."*bool",
                "desc": "Include the original document text in the results.", "default_value": True},
            "task": {"display_name": "Task", "type": AbstractDataProviderTypeMap."*string",
                "desc": "Optional instruction describing the relevance task.",
                "example_value": "Find passages that answer the customer question."},
        };
    }
    #! Creates the typed reranking request fields
    constructor() : HashDataType("FireworksRerankRequest") { addQoreFields(Fields); }
}

#! A document's relevance score and original position
public class FireworksRerankResultDataType inherits HashDataType {
    private {
        const Fields = {
            "index": {"display_name": "Index", "type": AbstractDataProviderTypeMap."int",
                "desc": "Zero-based position of the document in the input list."},
            "relevance_score": {"display_name": "Relevance Score", "type": AbstractDataProviderTypeMap."float",
                "desc": "Relevance score between `0` and `1`; higher values indicate a closer match."},
            "document": {"display_name": "Document", "type": AbstractDataProviderTypeMap."*string",
                "desc": "Original text, included when `return_documents` is enabled."},
        };
    }
    #! Creates the typed reranking result fields
    constructor() : HashDataType("FireworksRerankResult") { addQoreFields(Fields); }
}

#! Ranked documents and the request's token usage
public class FireworksRerankResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "object": {"display_name": "Object", "type": AbstractDataProviderTypeMap."string",
                "desc": "The response object type.",
                "allowed_values": (<AllowedValueInfo>{"value": "list", "display_name": "List"},)},
            "model": {"display_name": "Model", "type": AbstractDataProviderTypeMap."string",
                "desc": "Model used to rank the documents."},
            "data": {"display_name": "Ranked Documents",
                "type": new ListDataType("FireworksRerankResults", new FireworksRerankResultDataType()),
                "desc": "Results sorted from highest to lowest relevance."},
            "usage": {"display_name": "Usage", "type": new FireworksUsageDataType(),
                "desc": "Tokens consumed by the request."},
        };
    }
    #! Creates the typed reranking response fields
    constructor() : HashDataType("FireworksRerankResponse") { addQoreFields(Fields); }
}

public const FireworksRerankRequestType = new FireworksRerankRequestDataType();
public const FireworksRerankResponseType = new FireworksRerankResponseDataType();
}
