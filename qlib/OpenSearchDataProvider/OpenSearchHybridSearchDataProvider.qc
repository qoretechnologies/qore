# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore OpenSearchDataProvider hybrid search provider

/** OpenSearchHybridSearchDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! The OpenSearch hybrid search API data provider
public class OpenSearchHybridSearchDataProvider inherits OpenSearchDataProviderBase {
    public {
        #! Provider info
        const hash<DataProviderInfo> ProviderInfo = <DataProviderInfo>{
            "name": "hybrid-search",
            "desc": "OpenSearch hybrid search API data provider",
            "type": "OpenSearchHybridSearchDataProvider",
            "constructor_options": OpenSearchDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const hash<DataProviderSummaryInfo> ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const AbstractDataProviderType RequestType = new OpenSearchHybridSearchRequestDataType();

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
        return sprintf("OpenSearch hybrid search provider for `%s`", rest.getSafeUrl());
    }

    #! Makes a request and returns the response
    /** @param req the request to serialize and make according to the request type
        @param request_options the request options; will be processed by validateRequestOptions()

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string index = remove req.index;
        hash<auto> hybrid = {
            "queries": remove req.queries,
        };
        OpenSearchSearchActionUtil::moveIfExists(\hybrid, \req, "filter");
        OpenSearchSearchActionUtil::moveIfExists(\hybrid, \req, "pagination_depth");

        hash<auto> body = {
            "query": {
                "hybrid": hybrid,
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

#! OpenSearch hybrid search request
public class OpenSearchHybridSearchRequestDataType inherits DataProvider::HashDataType {
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
            "queries": {
                "type": AutoListType,
                "display_name": "Queries",
                "short_desc": "Query clauses to combine",
                "desc": "A list of OpenSearch query clauses to combine in a hybrid query",
                "example_value": (
                    {"match": {"message": "payment authorization failure"}},
                    {
                        "knn": {
                            "embedding": {
                                "vector": (0.12, 0.34, 0.56),
                                "k": 10,
                            },
                        },
                    },
                ),
            },
            "filter": {
                "type": AutoHashOrNothingType,
                "display_name": "Filter",
                "short_desc": "Optional query filter",
                "desc": "Optional OpenSearch query DSL filter applied to all hybrid subqueries",
                "example_value": {"term": {"level": "ERROR"}},
            },
            "pagination_depth": {
                "type": IntOrNothingType,
                "display_name": "Pagination Depth",
                "short_desc": "Hybrid pagination depth",
                "desc": "Maximum number of search results to retrieve from each shard for each hybrid subquery",
                "example_value": 100,
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}
}
