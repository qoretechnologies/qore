# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QdrantClusterCollectionInfoDataProvider class definition

/** QdrantClusterCollectionInfoDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! The Qdrant cluster collection info API data provider
/** This API retrieves cluster information for a specific collection

    @note Corresponds to: GET /collections/{name}/cluster
*/
public class QdrantClusterCollectionInfoDataProvider inherits QdrantDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "collection-info",
            "desc": "Qdrant cluster collection info API data provider (`GET /collections/{name}/cluster`)",
            "type": "QdrantClusterCollectionInfoDataProvider",
            "constructor_options": QdrantDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new QdrantClusterCollectionInfoRequestDataType();

        #! Response type
        const ResponseType = new QdrantClusterCollectionInfoResponseDataType();
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
        return sprintf("Qdrant cluster collection info provider for `%s`", rest.getSafeURL());
    }

    #! Makes a request and returns the response
    /** @param req the request to serialize and make according to the request type
        @param request_options the request options; will be processed by validateRequestOptions()

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doQdrantGet(collectionPath(remove req.collection, "/cluster"));
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

#! Qdrant cluster collection info request data type
public class QdrantClusterCollectionInfoRequestDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "collection": {
                "type": StringType,
                "display_name": "Collection",
                "short_desc": "Collection name",
                "desc": "The name of the collection to retrieve cluster information for",
                "example_value": "my_collection",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Qdrant cluster collection info response data type
public class QdrantClusterCollectionInfoResponseDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "peer_id": {
                "type": IntOrNothingType,
                "display_name": "Peer ID",
                "short_desc": "Current peer ID",
                "desc": "The ID of the current peer in the cluster",
                "example_value": 1,
            },
            "shard_count": {
                "type": IntOrNothingType,
                "display_name": "Shard Count",
                "short_desc": "Number of shards",
                "desc": "The total number of shards for the collection",
                "example_value": 2,
            },
            "local_shards": {
                "type": AutoListOrNothingType,
                "display_name": "Local Shards",
                "short_desc": "Local shard information",
                "desc": "Information about shards located on the current peer",
            },
            "remote_shards": {
                "type": AutoListOrNothingType,
                "display_name": "Remote Shards",
                "short_desc": "Remote shard information",
                "desc": "Information about shards located on remote peers",
            },
            "shard_transfers": {
                "type": AutoListOrNothingType,
                "display_name": "Shard Transfers",
                "short_desc": "Active shard transfers",
                "desc": "Information about any active shard transfer operations",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}
}
