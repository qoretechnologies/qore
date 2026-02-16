# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QdrantPointsGetDataProvider class definition

/** QdrantPointsGetDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! The Qdrant points get API data provider
/** This API retrieves points by IDs from a Qdrant collection

    @note Corresponds to: POST /collections/{name}/points
*/
public class QdrantPointsGetDataProvider inherits QdrantDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "get",
            "desc": "Qdrant points get data provider (`POST /collections/{name}/points`)",
            "type": "QdrantPointsGetDataProvider",
            "constructor_options": QdrantDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new QdrantPointsGetRequestDataType();

        #! Response type - list of point objects (Qdrant returns result as a list directly)
        const ResponseType = new ListDataType("QdrantPointsGetResponse",
            new QdrantPointsGetResponseElementDataType(), True);
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        checkOptions("CONSTRUCTOR-ERROR", QdrantDataProvider::ConstructorOptions, options);
        rest = QdrantDataProvider::getRestConnection(options);
    }

    #! Creates the object from a REST connection
    constructor(RestClient::RestClient rest) : QdrantDataProviderBase(rest) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("Qdrant points %s provider for `%s`", ProviderInfo.name, rest.getSafeURL());
    }

    #! Makes a request and returns the response
    /** @param req the request to serialize and make according to the request type
        @param request_options the request options; will be processed by validateRequestOptions()

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string collection = remove req.collection;
        return doQdrantPost(collectionPath(collection, "/points"), req);
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

#! Qdrant points get request data type
public class QdrantPointsGetRequestDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "collection": {
                "type": StringType,
                "display_name": "Collection",
                "short_desc": "Collection name",
                "desc": "The name of the collection to retrieve points from",
                "example_value": "my_collection",
            },
            "ids": {
                "type": AutoListType,
                "display_name": "IDs",
                "short_desc": "List of point IDs to retrieve",
                "desc": "A list of point IDs to retrieve; IDs can be integers or UUIDs",
                "example_value": (1, 2, 3),
            },
            "with_payload": {
                "type": SoftBoolOrNothingType,
                "display_name": "With Payload",
                "short_desc": "Payload selector",
                "desc": "Select which payload fields to return; `true` for all, `false` for none",
                "example_value": True,
            },
            "with_vector": {
                "type": SoftBoolOrNothingType,
                "display_name": "With Vector",
                "short_desc": "Vector selector",
                "desc": "Select which vectors to return; `true` for all, `false` for none",
                "example_value": True,
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Qdrant point element data type for points get response
public class QdrantPointsGetResponseElementDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "id": {
                "type": AutoType,
                "display_name": "ID",
                "short_desc": "Point ID",
                "desc": "The point ID; can be an integer or a UUID string",
                "example_value": 1,
            },
            "payload": {
                "type": AutoHashOrNothingType,
                "display_name": "Payload",
                "short_desc": "Point payload",
                "desc": "The payload data stored with the point",
                "example_value": {"text": "hello"},
            },
            "vector": {
                "type": AutoType,
                "display_name": "Vector",
                "short_desc": "Point vector",
                "desc": "The vector data stored with the point; can be a list of floats or a hash of named vectors",
                "example_value": (0.1, 0.2, 0.3),
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
