# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QdrantClusterUpdateCollectionDataProvider class definition

/** QdrantClusterUpdateCollectionDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! The Qdrant update collection cluster setup API data provider
/** This API updates the cluster setup for a collection

    @note Corresponds to: POST /collections/{name}/cluster
*/
public class QdrantClusterUpdateCollectionDataProvider inherits QdrantDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "update-collection",
            "desc": "Qdrant update collection cluster setup API data provider "
                "(`POST /collections/{name}/cluster`)",
            "type": "QdrantClusterUpdateCollectionDataProvider",
            "constructor_options": QdrantDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new QdrantClusterUpdateCollectionRequestDataType();

        #! Response type
        const ResponseType = new QdrantClusterUpdateCollectionResponseDataType();

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
        return sprintf("Qdrant cluster update collection provider for `%s`", rest.getSafeURL());
    }

    #! Makes a request and returns the response
    /** @param req the request to serialize and make according to the request type
        @param request_options the request options; will be processed by validateRequestOptions()

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string collection = remove req.collection;
        string uri_path = collectionPath(collection, "/cluster");
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

#! Qdrant cluster update collection request data type
public class QdrantClusterUpdateCollectionRequestDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "collection": {
                "type": StringType,
                "display_name": "Collection",
                "short_desc": "Collection name",
                "desc": "The name of the collection to update cluster setup for",
                "example_value": "my_collection",
            },
            "move_shard": {
                "type": AutoHashOrNothingType,
                "display_name": "Move Shard",
                "short_desc": "Move shard operation",
                "desc": "Configuration for moving a shard from one peer to another (includes `shard_id`, "
                    "`from_peer_id`, `to_peer_id`)",
            },
            "replicate_shard": {
                "type": AutoHashOrNothingType,
                "display_name": "Replicate Shard",
                "short_desc": "Replicate shard operation",
                "desc": "Configuration for replicating a shard to another peer (includes `shard_id`, "
                    "`from_peer_id`, `to_peer_id`)",
            },
            "abort_transfer": {
                "type": AutoHashOrNothingType,
                "display_name": "Abort Transfer",
                "short_desc": "Abort shard transfer",
                "desc": "Configuration for aborting an ongoing shard transfer (includes `shard_id`, "
                    "`from_peer_id`, `to_peer_id`)",
            },
            "drop_replica": {
                "type": AutoHashOrNothingType,
                "display_name": "Drop Replica",
                "short_desc": "Drop shard replica",
                "desc": "Configuration for dropping a shard replica from a peer (includes `shard_id`, `peer_id`)",
            },
            "create_sharding_key": {
                "type": AutoHashOrNothingType,
                "display_name": "Create Sharding Key",
                "short_desc": "Create sharding key",
                "desc": "Configuration for creating a new sharding key (includes `shard_key`)",
            },
            "drop_sharding_key": {
                "type": AutoHashOrNothingType,
                "display_name": "Drop Sharding Key",
                "short_desc": "Drop sharding key",
                "desc": "Configuration for dropping an existing sharding key (includes `shard_key`)",
            },
            "restart_transfer": {
                "type": AutoHashOrNothingType,
                "display_name": "Restart Transfer",
                "short_desc": "Restart shard transfer",
                "desc": "Configuration for restarting a shard transfer (includes `shard_id`, "
                    "`from_peer_id`, `to_peer_id`)",
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

#! Qdrant cluster update collection response data type
public class QdrantClusterUpdateCollectionResponseDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "result": {
                "type": SoftBoolOrNothingType,
                "display_name": "Result",
                "short_desc": "Operation result",
                "desc": "Returns `true` if the collection cluster setup was updated successfully",
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
