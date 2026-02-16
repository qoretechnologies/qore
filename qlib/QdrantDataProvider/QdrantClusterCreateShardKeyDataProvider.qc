# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QdrantClusterCreateShardKeyDataProvider class definition

/** QdrantClusterCreateShardKeyDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! The Qdrant create shard key API data provider
/** This API creates a new shard key for a collection

    @note Corresponds to: PUT /collections/{name}/shards
*/
public class QdrantClusterCreateShardKeyDataProvider inherits QdrantDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-shard-key",
            "desc": "Qdrant create shard key API data provider (`PUT /collections/{name}/shards`)",
            "type": "QdrantClusterCreateShardKeyDataProvider",
            "constructor_options": QdrantDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new QdrantClusterCreateShardKeyRequestDataType();

        #! Response type
        const ResponseType = new QdrantClusterCreateShardKeyResponseDataType();

        #! Query args
        const QueryArgs = ("timeout",);
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
        return sprintf("Qdrant create shard key provider for `%s`", rest.getSafeURL());
    }

    #! Makes a request and returns the response
    /** @param req the request to serialize and make according to the request type
        @param request_options the request options; will be processed by validateRequestOptions()

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string collection = remove req.collection;
        string uri_path = collectionPath(collection, "/shards");
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
        return doQdrantPut(uri_path, req);
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

#! Qdrant create shard key request data type
public class QdrantClusterCreateShardKeyRequestDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "collection": {
                "type": StringType,
                "display_name": "Collection",
                "short_desc": "Collection name",
                "desc": "The name of the collection to create a shard key for",
                "example_value": "my_collection",
            },
            "shard_key": {
                "type": StringType,
                "display_name": "Shard Key",
                "short_desc": "Shard key value",
                "desc": "The shard key to create; a string value that identifies the shard key",
                "example_value": "my_shard_key",
            },
            "shards_number": {
                "type": IntOrNothingType,
                "display_name": "Shards Number",
                "short_desc": "Number of shards",
                "desc": "The number of shards to create for this shard key",
                "example_value": 1,
            },
            "replication_factor": {
                "type": IntOrNothingType,
                "display_name": "Replication Factor",
                "short_desc": "Replication factor",
                "desc": "The number of replicas for each shard created with this shard key",
                "example_value": 1,
            },
            "placement": {
                "type": AutoListOrNothingType,
                "display_name": "Placement",
                "short_desc": "Peer placement list",
                "desc": "List of peer IDs where the shards should be placed",
                "example_value": (1, 2),
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

#! Qdrant create shard key response data type
public class QdrantClusterCreateShardKeyResponseDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "result": {
                "type": SoftBoolOrNothingType,
                "display_name": "Result",
                "short_desc": "Operation result",
                "desc": "Returns `true` if the shard key was created successfully",
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
