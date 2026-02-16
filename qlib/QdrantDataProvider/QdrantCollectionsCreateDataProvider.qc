# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QdrantCollectionsCreateDataProvider class definition

/** QdrantCollectionsCreateDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! The Qdrant collection create API data provider
/** This API creates a new collection in the Qdrant instance

    @note Corresponds to: PUT /collections/{name}
*/
public class QdrantCollectionsCreateDataProvider inherits QdrantDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Qdrant collection create data provider (`PUT /collections/{name}`)",
            "type": "QdrantCollectionsCreateDataProvider",
            "constructor_options": QdrantDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new QdrantCollectionsCreateRequestDataType();

        #! Response type
        const ResponseType = new QdrantCollectionsCreateResponseDataType();

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
        return sprintf("Qdrant collections %s provider for `%s`", ProviderInfo.name, rest.getSafeURL());
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

#! Qdrant collection create request data type
public class QdrantCollectionsCreateRequestDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "collection": {
                "type": StringType,
                "display_name": "Collection",
                "short_desc": "Collection name to create",
                "desc": "The name of the collection to create",
                "example_value": "my_collection",
            },
            "vectors": {
                "type": AutoHashOrNothingType,
                "display_name": "Vectors",
                "short_desc": "Vector configuration",
                "desc": "Vector configuration for the collection, including vector size, distance metric, and other "
                    "parameters (e.g. `size`, `distance`)",
                "example_value": {"size": 128, "distance": "Cosine"},
            },
            "sparse_vectors": {
                "type": AutoHashOrNothingType,
                "display_name": "Sparse Vectors",
                "short_desc": "Sparse vector configuration",
                "desc": "Sparse vector configuration for the collection",
            },
            "shard_number": {
                "type": IntOrNothingType,
                "display_name": "Shard Number",
                "short_desc": "Number of shards",
                "desc": "The number of shards for the collection. A higher number of shards can improve "
                    "performance for large collections",
                "example_value": 1,
            },
            "sharding_method": {
                "type": StringOrNothingType,
                "display_name": "Sharding Method",
                "short_desc": "Sharding method",
                "desc": "The sharding method to use for the collection",
                "example_value": "auto",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "auto", "desc": "Automatic sharding managed by Qdrant"},
                    <AllowedValueInfo>{"value": "custom", "desc": "Custom sharding with user-defined shard keys"},
                ),
            },
            "replication_factor": {
                "type": IntOrNothingType,
                "display_name": "Replication Factor",
                "short_desc": "Replication factor",
                "desc": "The number of replicas for each shard in the collection",
                "example_value": 1,
            },
            "write_consistency_factor": {
                "type": IntOrNothingType,
                "display_name": "Write Consistency Factor",
                "short_desc": "Write consistency factor",
                "desc": "The minimum number of replicas that must acknowledge a write operation before it is "
                    "considered successful",
                "example_value": 1,
            },
            "on_disk_payload": {
                "type": SoftBoolOrNothingType,
                "display_name": "On Disk Payload",
                "short_desc": "Store payload on disk",
                "desc": "If `true`, the payload will be stored on disk rather than in memory. This can reduce "
                    "memory usage for large payloads",
                "example_value": False,
            },
            "hnsw_config": {
                "type": AutoHashOrNothingType,
                "display_name": "HNSW Config",
                "short_desc": "HNSW index configuration",
                "desc": "Configuration for the HNSW (Hierarchical Navigable Small World) index algorithm",
            },
            "wal_config": {
                "type": AutoHashOrNothingType,
                "display_name": "WAL Config",
                "short_desc": "WAL configuration",
                "desc": "Configuration for the Write-Ahead Log (WAL)",
            },
            "optimizers_config": {
                "type": AutoHashOrNothingType,
                "display_name": "Optimizers Config",
                "short_desc": "Optimizer settings",
                "desc": "Configuration for the collection optimizers, controlling indexing and merging behavior",
            },
            "init_from": {
                "type": AutoHashOrNothingType,
                "display_name": "Init From",
                "short_desc": "Initialize from another collection",
                "desc": "Initialize the collection from another existing collection by specifying the source "
                    "collection name",
                "example_value": {"collection": "source_collection"},
            },
            "quantization_config": {
                "type": AutoHashOrNothingType,
                "display_name": "Quantization Config",
                "short_desc": "Quantization settings",
                "desc": "Configuration for vector quantization to reduce memory usage and improve search speed",
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

#! Qdrant collection create response data type
public class QdrantCollectionsCreateResponseDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "result": {
                "type": SoftBoolOrNothingType,
                "display_name": "Result",
                "short_desc": "Operation result",
                "desc": "Returns `true` if the collection was created successfully",
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
