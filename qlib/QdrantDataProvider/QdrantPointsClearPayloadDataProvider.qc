# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QdrantPointsClearPayloadDataProvider class definition

/** QdrantPointsClearPayloadDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! The Qdrant points clear payload API data provider
/** This API clears all payload fields from points in a Qdrant collection

    @note Corresponds to: POST /collections/{name}/points/payload/clear
*/
public class QdrantPointsClearPayloadDataProvider inherits QdrantDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "clear-payload",
            "desc": "Qdrant points clear payload data provider "
                "(`POST /collections/{name}/points/payload/clear`)",
            "type": "QdrantPointsClearPayloadDataProvider",
            "constructor_options": QdrantDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new QdrantPointsClearPayloadRequestDataType();

        #! Response type
        const ResponseType = new QdrantPointsClearPayloadResponseDataType();

        #! Query args
        const QueryArgs = ("wait", "ordering");
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
        return sprintf("Qdrant points %s provider for `%s`", ProviderInfo.name, rest.getSafeUrl());
    }

    #! Makes a request and returns the response
    /** @param req the request to serialize and make according to the request type
        @param request_options the request options; will be processed by validateRequestOptions()

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string collection = remove req.collection;
        string uri_path = collectionPath(collection, "/points/payload/clear");
        bool q;
        foreach string arg in (QueryArgs) {
            if (!exists req{arg}) {
                continue;
            }
            if (!q) {
                uri_path += "?";
                q = True;
            } else {
                uri_path += "&";
            }
            uri_path += sprintf("%s=%s", arg, QdrantDataProviderBase::formatQueryValue(remove req{arg}));
        }
        return doQdrantPost(uri_path, req);
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

#! Qdrant points clear payload request data type
public class QdrantPointsClearPayloadRequestDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "collection": {
                "type": StringType,
                "display_name": "Collection",
                "short_desc": "Collection name",
                "desc": "The name of the collection containing the points to clear payloads from",
                "example_value": "my_collection",
            },
            "points": {
                "type": AutoListOrNothingType,
                "display_name": "Points",
                "short_desc": "List of point IDs",
                "desc": "A list of point IDs to clear all payload fields from; IDs can be integers or UUIDs",
                "example_value": (1, 2, 3),
            },
            "filter": {
                "type": AutoHashOrNothingType,
                "display_name": "Filter",
                "short_desc": "Filter conditions",
                "desc": "Filter conditions to select points for the payload clear operation; alternative to "
                    "specifying point IDs directly",
                "example_value": {"must": ({"key": "city", "match": {"value": "London"}},)},
            },
            "wait": {
                "type": SoftBoolOrNothingType,
                "display_name": "Wait",
                "short_desc": "Wait for operation to complete",
                "desc": "If `true`, wait for changes to actually happen; if `false` (default), the request "
                    "returns immediately after acknowledging",
                "example_value": True,
            },
            "ordering": {
                "type": StringOrNothingType,
                "display_name": "Ordering",
                "short_desc": "Write ordering guarantees",
                "desc": "The write ordering guarantees for the operation",
                "example_value": "weak",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "weak", "desc": "Weak ordering; fastest, no guarantees"},
                    <AllowedValueInfo>{"value": "medium", "desc": "Medium ordering; wait for local persistence"},
                    <AllowedValueInfo>{"value": "strong", "desc": "Strong ordering; wait for all replicas"},
                ),
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Qdrant points clear payload response data type
public class QdrantPointsClearPayloadResponseDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "operation_id": {
                "type": IntOrNothingType,
                "display_name": "Operation ID",
                "short_desc": "The operation ID",
                "desc": "The ID of the operation that was performed",
                "example_value": 0,
            },
            "status": {
                "type": StringType,
                "display_name": "Status",
                "short_desc": "Operation status",
                "desc": "The status of the operation (e.g. `completed`, `acknowledged`)",
                "example_value": "completed",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}
}
