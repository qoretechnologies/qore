# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QdrantCollectionsDeleteIndexDataProvider class definition

/** QdrantCollectionsDeleteIndexDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! The Qdrant collection delete index API data provider
/** This API deletes a payload field index from a collection

    @note Corresponds to: DELETE /collections/{name}/index/{field}
*/
public class QdrantCollectionsDeleteIndexDataProvider inherits QdrantDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete-index",
            "desc": "Qdrant collection delete index data provider "
                "(`DELETE /collections/{name}/index/{field}`)",
            "type": "QdrantCollectionsDeleteIndexDataProvider",
            "constructor_options": QdrantDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new QdrantCollectionsDeleteIndexRequestDataType();

        #! Response type
        const ResponseType = new QdrantCollectionsDeleteIndexResponseDataType();

        #! Query args
        const QueryArgs = ("wait", "ordering");
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
        return sprintf("Qdrant collections %s provider for `%s`", ProviderInfo.name, rest.getSafeURL());
    }

    #! Makes a request and returns the response
    /** @param req the request to serialize and make according to the request type
        @param request_options the request options; will be processed by validateRequestOptions()

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string collection = remove req.collection;
        string field_name = remove req.field_name;
        string uri_path = collectionPath(collection, "/index/" + encode_url(field_name, True));
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
        return doQdrantDelete(uri_path);
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

#! Qdrant collection delete index request data type
public class QdrantCollectionsDeleteIndexRequestDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "collection": {
                "type": StringType,
                "display_name": "Collection",
                "short_desc": "Collection name",
                "desc": "The name of the collection to delete the payload field index from",
                "example_value": "my_collection",
            },
            "field_name": {
                "type": StringType,
                "display_name": "Field Name",
                "short_desc": "Payload field name",
                "desc": "The name of the payload field index to delete",
                "example_value": "city",
            },
            "wait": {
                "type": SoftBoolOrNothingType,
                "display_name": "Wait",
                "short_desc": "Wait for operation completion",
                "desc": "If `true`, the request will wait for the operation to complete before returning. "
                    "Default is `false`",
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

#! Qdrant collection delete index response data type
public class QdrantCollectionsDeleteIndexResponseDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "operation_id": {
                "type": IntOrNothingType,
                "display_name": "Operation ID",
                "short_desc": "Operation identifier",
                "desc": "The identifier of the index deletion operation",
                "example_value": 0,
            },
            "status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "Operation status",
                "desc": "The status of the index deletion operation (e.g. `acknowledged`, `completed`)",
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
