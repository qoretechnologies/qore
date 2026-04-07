# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QdrantCollectionsGetDataProvider class definition

/** QdrantCollectionsGetDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! The Qdrant collection get API data provider
/** This API retrieves information about a specific collection

    @note Corresponds to: GET /collections/{name}
*/
public class QdrantCollectionsGetDataProvider inherits QdrantDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "get",
            "desc": "Qdrant collection get data provider (`GET /collections/{name}`)",
            "type": "QdrantCollectionsGetDataProvider",
            "constructor_options": QdrantDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new QdrantCollectionsGetRequestDataType();

        #! Response type
        const ResponseType = new QdrantCollectionsGetResponseDataType();
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
        return sprintf("Qdrant collections %s provider for `%s`", ProviderInfo.name, rest.getSafeUrl());
    }

    #! Makes a request and returns the response
    /** @param req the request to serialize and make according to the request type
        @param request_options the request options; will be processed by validateRequestOptions()

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doQdrantGet(collectionPath(remove req.collection));
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

#! Qdrant collection get request data type
public class QdrantCollectionsGetRequestDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "collection": {
                "type": StringType,
                "display_name": "Collection",
                "short_desc": "Collection name",
                "desc": "The name of the collection to retrieve information for",
                "example_value": "my_collection",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Qdrant collection get response data type
public class QdrantCollectionsGetResponseDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "Collection status",
                "desc": "The current status of the collection (e.g. `green`, `yellow`, `red`)",
                "example_value": "green",
            },
            "optimizer_status": {
                "type": StringOrNothingType,
                "display_name": "Optimizer Status",
                "short_desc": "Optimizer status",
                "desc": "The current status of the collection optimizer; returns `ok` when the optimizer is "
                    "idle, or a status string when the optimizer is running",
                "example_value": "ok",
            },
            "vectors_count": {
                "type": IntOrNothingType,
                "display_name": "Vectors Count",
                "short_desc": "Total number of vectors",
                "desc": "The approximate total number of vectors in the collection",
                "example_value": 1000,
            },
            "indexed_vectors_count": {
                "type": IntOrNothingType,
                "display_name": "Indexed Vectors Count",
                "short_desc": "Number of indexed vectors",
                "desc": "The approximate number of indexed vectors in the collection",
                "example_value": 1000,
            },
            "points_count": {
                "type": IntOrNothingType,
                "display_name": "Points Count",
                "short_desc": "Total number of points",
                "desc": "The approximate total number of points in the collection",
                "example_value": 1000,
            },
            "segments_count": {
                "type": IntOrNothingType,
                "display_name": "Segments Count",
                "short_desc": "Number of segments",
                "desc": "The number of segments in the collection",
                "example_value": 2,
            },
            "config": {
                "type": AutoHashOrNothingType,
                "display_name": "Config",
                "short_desc": "Collection configuration",
                "desc": "The collection configuration including vector parameters, HNSW, WAL, and optimizer settings",
            },
            "payload_schema": {
                "type": AutoHashOrNothingType,
                "display_name": "Payload Schema",
                "short_desc": "Payload schema",
                "desc": "The payload field schema with field types and index information",
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
