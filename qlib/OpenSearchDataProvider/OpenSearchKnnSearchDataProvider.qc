# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore OpenSearchDataProvider k-NN search provider

/** OpenSearchKnnSearchDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! The OpenSearch k-NN search API data provider
public class OpenSearchKnnSearchDataProvider inherits OpenSearchDataProviderBase {
    public {
        #! Provider info
        const hash<DataProviderInfo> ProviderInfo = <DataProviderInfo>{
            "name": "knn-search",
            "desc": "OpenSearch k-NN vector search API data provider",
            "type": "OpenSearchKnnSearchDataProvider",
            "constructor_options": OpenSearchDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const hash<DataProviderSummaryInfo> ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const AbstractDataProviderType RequestType = new OpenSearchKnnSearchRequestDataType();

        #! Response type
        const AbstractDataProviderType ResponseType = OpenSearchIndexSearchDataProvider::ResponseType;

        #! Query args
        const list<string> QueryArgs = ("search_pipeline", "preference", "routing");
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
        return sprintf("OpenSearch k-NN search provider for `%s`", rest.getSafeUrl());
    }

    #! Makes a request and returns the response
    /** @param req the request to serialize and make according to the request type
        @param request_options the request options; will be processed by validateRequestOptions()

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string index = remove req.index;
        string field = remove req.field;
        hash<auto> knn = {
            "vector": remove req.vector,
        };

        if (exists req{"k"}) {
            knn.k = remove req.k;
        } else if (!exists req{"max_distance"} && !exists req{"min_score"}) {
            knn.k = 10;
        }

        OpenSearchSearchActionUtil::moveIfExists(\knn, \req, "filter");
        OpenSearchSearchActionUtil::moveIfExists(\knn, \req, "method_parameters");
        OpenSearchSearchActionUtil::moveIfExists(\knn, \req, "max_distance");
        OpenSearchSearchActionUtil::moveIfExists(\knn, \req, "min_score");

        hash<auto> body = {
            "query": {
                "knn": {
                    field: knn,
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

#! OpenSearch k-NN search request
public class OpenSearchKnnSearchRequestDataType inherits DataProvider::HashDataType {
    public {
        #! Field descriptions
        const hash<string, AbstractDataField> Fields = SearchTopLevelFields + {
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
                "short_desc": "Vector field to search",
                "desc": "The `knn_vector` field to search",
                "example_value": "embedding",
            },
            "vector": {
                "type": AutoListType,
                "display_name": "Query Vector",
                "short_desc": "Vector to compare",
                "desc": "The query vector to compare against vectors stored in `field`",
                "example_value": (0.12, 0.34, 0.56),
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
