# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore OpenSearchDataProvider neural search provider

/** OpenSearchNeuralSearchDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the OpenSearchDataProvider module
public namespace OpenSearchDataProvider {
#! The OpenSearch neural search API data provider
public class OpenSearchNeuralSearchDataProvider inherits OpenSearchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "neural-search",
            "desc": "OpenSearch neural search API data provider",
            "type": "OpenSearchNeuralSearchDataProvider",
            "constructor_options": OpenSearchDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new OpenSearchNeuralSearchRequestDataType();

        #! Response type
        const ResponseType = OpenSearchIndexSearchDataProvider::ResponseType;

        #! Query args
        const QueryArgs = ("search_pipeline", "preference", "routing");
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        checkOptions("CONSTRUCTOR-ERROR", OpenSearchDataProvider::ConstructorOptions, options);
        rest = OpenSearchDataProvider::getRestConnection(options);
    }

    #! Creates the object from a REST connection
    constructor(RestClient::AbstractRestClient rest) : OpenSearchDataProviderBase(rest) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("OpenSearch neural search provider for `%s`", rest.getSafeUrl());
    }

    #! Makes a request and returns the response
    /** @param req the request to serialize and make according to the request type
        @param request_options the request options; will be processed by validateRequestOptions()

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string index = remove req.index;
        string field = remove req.field;
        hash<auto> neural = {};

        OpenSearchSearchActionUtil::moveIfExists(\neural, \req, "query_text");
        OpenSearchSearchActionUtil::moveIfExists(\neural, \req, "query_image");
        OpenSearchSearchActionUtil::moveIfExists(\neural, \req, "query_tokens");
        OpenSearchSearchActionUtil::moveIfExists(\neural, \req, "model_id");
        OpenSearchSearchActionUtil::moveIfExists(\neural, \req, "filter");
        OpenSearchSearchActionUtil::moveIfExists(\neural, \req, "method_parameters");
        OpenSearchSearchActionUtil::moveIfExists(\neural, \req, "rescore");
        OpenSearchSearchActionUtil::moveIfExists(\neural, \req, "expand_nested_docs");
        OpenSearchSearchActionUtil::moveIfExists(\neural, \req, "semantic_field_search_analyzer");

        if (exists req{"k"}) {
            neural.k = remove req.k;
        } else if (!exists req{"max_distance"} && !exists req{"min_score"}) {
            neural.k = 10;
        }
        OpenSearchSearchActionUtil::moveIfExists(\neural, \req, "max_distance");
        OpenSearchSearchActionUtil::moveIfExists(\neural, \req, "min_score");

        hash<auto> body = {
            "query": {
                "neural": {
                    field: neural,
                },
            },
        };
        OpenSearchSearchActionUtil::addTopLevelSearchOptions(\body, \req);

        string uri = SearchIndexDataProvider::SearchIndexRestUtil::moveQueryOptions(index + "/_search", \req,
            QueryArgs);
        return rest.restPost(uri, body).body;
    }

    #! Returns the request type
    /** @return the request type for this provider */
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the response type
    /** @return the response type for this response message */
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns data provider static info
    hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! OpenSearch neural search request
public class OpenSearchNeuralSearchRequestDataType inherits DataProvider::HashDataType {
    public {
        #! Field descriptions
        const Fields = SearchTopLevelFields + {
            "index": {
                "type": StringType,
                "display_name": "Index",
                "short_desc": "Index name to search",
                "desc": "The OpenSearch index to search",
                "example_value": "my-vector-index",
            },
            "field": {
                "type": StringType,
                "display_name": "Vector Field",
                "short_desc": "Vector or semantic field",
                "desc": "The vector or semantic field to search",
                "example_value": "passage_embedding",
            },
            "query_text": {
                "type": StringOrNothingType,
                "display_name": "Query Text",
                "short_desc": "Text to embed",
                "desc": "Text used by OpenSearch to generate query embeddings",
                "example_value": "errors after payment authorization",
            },
            "query_image": {
                "type": StringOrNothingType,
                "display_name": "Query Image",
                "short_desc": "Base64 image data",
                "desc": "Base64-encoded image data used by OpenSearch to generate query embeddings",
            },
            "query_tokens": {
                "type": AutoHashOrNothingType,
                "display_name": "Query Tokens",
                "short_desc": "Sparse query tokens",
                "desc": "Sparse-vector query tokens and weights",
                "example_value": {"errors": 0.9, "payment": 0.7},
            },
            "model_id": {
                "type": StringOrNothingType,
                "display_name": "Model ID",
                "short_desc": "Embedding model ID",
                "desc": "The OpenSearch model ID used to generate query embeddings; optional for semantic fields",
            },
            "k": {
                "type": SoftIntType,
                "display_name": "Nearest Neighbors",
                "short_desc": "Number of neighbors",
                "desc": "The number of nearest neighbors to retrieve",
                "example_value": 10,
                "default_value": 10,
            },
            "filter": {
                "type": AutoHashOrNothingType,
                "display_name": "Filter",
                "short_desc": "Optional query filter",
                "desc": "Optional OpenSearch query DSL filter applied to candidate documents",
                "example_value": {"term": {"level": "ERROR"}},
            },
            "method_parameters": {
                "type": AutoHashOrNothingType,
                "display_name": "Method Parameters",
                "short_desc": "k-NN method parameters",
                "desc": "Optional method parameters such as `ef_search` for approximate nearest-neighbor search",
                "example_value": {"ef_search": 100},
            },
            "rescore": {
                "type": AutoType,
                "display_name": "Rescore",
                "short_desc": "Vector rescore control",
                "desc": "Optional OpenSearch-compatible rescore setting for neural search",
                "example_value": True,
            },
            "expand_nested_docs": {
                "type": SoftBoolStringType,
                "display_name": "Expand Nested Docs?",
                "short_desc": "Return nested scores",
                "desc": "When true, retrieves scores for nested field documents within each parent document",
                "example_value": False,
            },
            "semantic_field_search_analyzer": {
                "type": StringOrNothingType,
                "display_name": "Semantic Field Analyzer",
                "short_desc": "Semantic field analyzer",
                "desc": "Analyzer for tokenizing `query_text` when using a sparse encoding model",
                "example_value": "standard",
            },
            "max_distance": {
                "type": SoftFloatOrNothingType,
                "display_name": "Max Distance",
                "short_desc": "Maximum vector distance",
                "desc": "Optional maximum vector distance threshold",
                "example_value": 0.8,
            },
            "min_score": {
                "type": SoftFloatOrNothingType,
                "display_name": "Min Score",
                "short_desc": "Minimum score threshold",
                "desc": "Optional minimum score threshold",
                "example_value": 0.2,
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}
}
