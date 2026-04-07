# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QdrantSearchQueryBatchDataProvider class definition

/** QdrantSearchQueryBatchDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! The Qdrant batch query API data provider
/** This API performs multiple queries in a single batch request on a Qdrant collection

    @note Corresponds to: POST /collections/{name}/points/query/batch
*/
public class QdrantSearchQueryBatchDataProvider inherits QdrantDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "query-batch",
            "desc": "Qdrant batch query API data provider (`POST /collections/{name}/points/query/batch`)",
            "type": "QdrantSearchQueryBatchDataProvider",
            "constructor_options": QdrantDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new QdrantSearchQueryBatchRequestDataType();

        #! Response type - list of query result sets (Qdrant returns result as a list directly)
        const ResponseType = new ListDataType("QdrantSearchQueryBatchResponse",
            new QdrantSearchQueryBatchResponseElementDataType(), True);
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
        return doQdrantPost(collectionPath(collection, "/points/query/batch"), req);
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

#! Qdrant search query batch request data type
public class QdrantSearchQueryBatchRequestDataType inherits DataProvider::HashDataType {
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
            "searches": {
                "type": AutoListType,
                "display_name": "Searches",
                "short_desc": "List of query request objects",
                "desc": "A list of query request objects to execute in a single batch; each object follows "
                    "the same format as the universal query API request",
                "example_value": ({"query": (0.2, 0.1, 0.9, 0.7), "limit": 10},),
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Qdrant search query batch response element data type
/** Each element represents the result of a single query in the batch, containing a list of scored points
*/
public class QdrantSearchQueryBatchResponseElementDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "points": {
                "type": AutoListType,
                "display_name": "Points",
                "short_desc": "Scored point results for this query",
                "desc": "A list of scored point results for this query in the batch",
                "example_value": ({"id": 1, "version": 0, "score": 0.95},),
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
        setDefaultOtherFieldType(AbstractDataProviderTypeMap."auto");
    }
}
}
