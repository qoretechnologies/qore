# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QdrantCollectionsUpdateDataProvider class definition

/** QdrantCollectionsUpdateDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! The Qdrant collection update API data provider
/** This API updates parameters of an existing collection

    @note Corresponds to: PATCH /collections/{name}
*/
public class QdrantCollectionsUpdateDataProvider inherits QdrantDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "desc": "Qdrant collection update data provider (`PATCH /collections/{name}`)",
            "type": "QdrantCollectionsUpdateDataProvider",
            "constructor_options": QdrantDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new QdrantCollectionsUpdateRequestDataType();

        #! Response type
        const ResponseType = new QdrantCollectionsUpdateResponseDataType();

        #! Query args
        const QueryArgs = ("timeout",);
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
        string collection = remove req.collection;
        string uri_path = collectionPath(collection);
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
        return doQdrantPatch(uri_path, req);
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

#! Qdrant collection update request data type
public class QdrantCollectionsUpdateRequestDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "collection": {
                "type": StringType,
                "display_name": "Collection",
                "short_desc": "Collection name to update",
                "desc": "The name of the collection to update",
                "example_value": "my_collection",
            },
            "optimizers_config": {
                "type": AutoHashOrNothingType,
                "display_name": "Optimizers Config",
                "short_desc": "Optimizer settings",
                "desc": "Updated configuration for the collection optimizers, controlling indexing and merging "
                    "behavior",
            },
            "hnsw_config": {
                "type": AutoHashOrNothingType,
                "display_name": "HNSW Config",
                "short_desc": "HNSW index configuration",
                "desc": "Updated configuration for the HNSW (Hierarchical Navigable Small World) index algorithm",
            },
            "quantization_config": {
                "type": AutoHashOrNothingType,
                "display_name": "Quantization Config",
                "short_desc": "Quantization settings",
                "desc": "Updated configuration for vector quantization to reduce memory usage and improve "
                    "search speed",
            },
            "params": {
                "type": AutoHashOrNothingType,
                "display_name": "Params",
                "short_desc": "Collection parameters",
                "desc": "Additional collection parameters to update (e.g. replication factor, write consistency "
                    "factor)",
            },
            "timeout": {
                "type": IntOrNothingType,
                "display_name": "Timeout",
                "short_desc": "Operation timeout in seconds",
                "desc": "Wait for operation commit timeout in seconds. If the timeout is reached, the request "
                    "will return with a service error",
                "example_value": 30,
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Qdrant collection update response data type
public class QdrantCollectionsUpdateResponseDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "result": {
                "type": SoftBoolOrNothingType,
                "display_name": "Result",
                "short_desc": "Operation result",
                "desc": "Returns `true` if the collection was updated successfully",
                "example_value": True,
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}
}
