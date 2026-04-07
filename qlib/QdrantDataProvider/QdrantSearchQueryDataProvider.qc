# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QdrantSearchQueryDataProvider class definition

/** QdrantSearchQueryDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the QdrantDataProvider module
public namespace QdrantDataProvider {
#! The Qdrant universal query API data provider
/** This API performs a universal query on a Qdrant collection using vector similarity search

    @note Corresponds to: POST /collections/{name}/points/query
*/
public class QdrantSearchQueryDataProvider inherits QdrantDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "query",
            "desc": "Qdrant universal query API data provider (`POST /collections/{name}/points/query`)",
            "type": "QdrantSearchQueryDataProvider",
            "constructor_options": QdrantDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new QdrantSearchQueryRequestDataType();

        #! Response type
        const ResponseType = new QdrantSearchQueryResponseDataType();
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        checkOptions("CONSTRUCTOR-ERROR", QdrantDataProvider::ConstructorOptions, options);
        rest = QdrantDataProvider::getRestConnection(options);
    }

    #! Creates the object from a REST connection
    constructor(RestClientIo::RestClientIo rest) : QdrantDataProviderBase(rest) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("Qdrant search %s provider for `%s`", ProviderInfo.name, rest.getSafeUrl());
    }

    #! Makes a request and returns the response
    /** @param req the request to serialize and make according to the request type
        @param request_options the request options; will be processed by validateRequestOptions()

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string collection = remove req.collection;
        return doQdrantPost(collectionPath(collection, "/points/query"), req);
    }

    #! Returns the description of a successful request message, if any
    /** @return the request type for this provider
    */
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the description of a response message, if this object represents a response message
    /** @return the response type for this response message
    */
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns data provider static info
    hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Qdrant search query request data type
public class QdrantSearchQueryRequestDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "collection": {
                "type": StringType,
                "display_name": "Collection",
                "short_desc": "Collection name to query",
                "desc": "The name of the collection to query",
                "example_value": "my-collection",
            },
            "prefetch": {
                "type": AutoHashOrNothingType,
                "display_name": "Prefetch",
                "short_desc": "Prefetch queries for hybrid search",
                "desc": "Prefetch queries for hybrid search; can be a list of query objects or a single query "
                    "object for multi-stage search pipelines",
            },
            "query": {
                "type": AutoListOrNothingType,
                "display_name": "Query",
                "short_desc": "Query vector",
                "desc": "The query vector to use for the search",
                "example_value": (0.2, 0.1, 0.9, 0.7),
            },
            "using": {
                "type": StringOrNothingType,
                "display_name": "Using",
                "short_desc": "Named vector to use",
                "desc": "The name of the named vector to use for the search; if not specified, the default "
                    "vector is used",
                "example_value": "image-vector",
            },
            "filter": {
                "type": AutoHashOrNothingType,
                "display_name": "Filter",
                "short_desc": "Filter conditions for search",
                "desc": "Filter conditions to apply to the search, using Qdrant filter syntax with `must`, "
                    "`should`, and `must_not` clauses",
                "example_value": {"must": ({"key": "city", "match": {"value": "London"}},)},
            },
            "score_threshold": {
                "type": FloatOrNothingType,
                "display_name": "Score Threshold",
                "short_desc": "Minimum score threshold",
                "desc": "The minimum score threshold for results; points with a score below this value will "
                    "be excluded from the results",
                "example_value": 0.5,
            },
            "params": {
                "type": AutoHashOrNothingType,
                "display_name": "Params",
                "short_desc": "Additional search parameters",
                "desc": "Additional search parameters such as `hnsw_ef`, `exact`, `quantization`, and other "
                    "engine-specific options",
                "example_value": {"hnsw_ef": 128, "exact": False},
            },
            "limit": {
                "type": IntOrNothingType,
                "display_name": "Limit",
                "short_desc": "Max results to return",
                "desc": "The maximum number of results to return",
                "example_value": 10,
            },
            "offset": {
                "type": IntOrNothingType,
                "display_name": "Offset",
                "short_desc": "Offset for pagination",
                "desc": "The offset of the first result to return; used for pagination",
                "example_value": 0,
            },
            "with_payload": {
                "type": SoftBoolOrNothingType,
                "display_name": "With Payload",
                "short_desc": "Return payload data",
                "desc": "Whether to return payload data with results",
                "example_value": True,
            },
            "with_vector": {
                "type": SoftBoolOrNothingType,
                "display_name": "With Vector",
                "short_desc": "Return vectors",
                "desc": "Whether to return vectors with results",
                "example_value": False,
            },
            "lookup_from": {
                "type": AutoHashOrNothingType,
                "display_name": "Lookup From",
                "short_desc": "Lookup vectors from another collection",
                "desc": "Configuration to look up vectors from another collection for the search",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Qdrant search query response data type
public class QdrantSearchQueryResponseDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "points": {
                "type": AutoListType,
                "display_name": "Points",
                "short_desc": "Scored point results",
                "desc": "A list of scored point results matching the query, each containing the point ID, "
                    "score, and optionally payload and vector data",
                "example_value": ({"id": 1, "version": 0, "score": 0.95, "payload": {"text": "hello"}},),
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}
}
