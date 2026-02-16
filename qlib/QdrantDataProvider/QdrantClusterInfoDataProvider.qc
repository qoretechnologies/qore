# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QdrantClusterInfoDataProvider class definition

/** QdrantClusterInfoDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! The Qdrant cluster info API data provider
/** This API retrieves cluster status and information

    @note Corresponds to: GET /cluster
*/
public class QdrantClusterInfoDataProvider inherits QdrantDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "info",
            "desc": "Qdrant cluster info API data provider (`GET /cluster`)",
            "type": "QdrantClusterInfoDataProvider",
            "constructor_options": QdrantDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = AbstractDataProviderTypeMap."nothing";

        #! Response type
        const ResponseType = new QdrantClusterInfoResponseDataType();
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
        return sprintf("Qdrant cluster info provider for `%s`", rest.getSafeURL());
    }

    #! Makes a request and returns the response
    /** @param req the request (ignored)
        @param request_options the request options; will be processed by validateRequestOptions()

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doQdrantGet("/cluster");
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

#! Qdrant cluster info response data type
public class QdrantClusterInfoResponseDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "status": {
                "type": StringType,
                "display_name": "Status",
                "short_desc": "Cluster status",
                "desc": "The current status of the cluster (e.g. `enabled` or `disabled`)",
                "example_value": "enabled",
            },
            "peer_id": {
                "type": IntOrNothingType,
                "display_name": "Peer ID",
                "short_desc": "Current peer ID",
                "desc": "The ID of the current peer in the cluster",
                "example_value": 1,
            },
            "peers": {
                "type": AutoHashOrNothingType,
                "display_name": "Peers",
                "short_desc": "Cluster peers",
                "desc": "Information about all peers in the cluster, keyed by peer ID",
            },
            "raft_info": {
                "type": AutoHashOrNothingType,
                "display_name": "Raft Info",
                "short_desc": "Raft consensus info",
                "desc": "Information about the Raft consensus state including term, commit, and leader",
            },
            "consensus_thread_status": {
                "type": AutoHashOrNothingType,
                "display_name": "Consensus Thread Status",
                "short_desc": "Consensus thread status",
                "desc": "Status information about the consensus thread",
            },
            "message_send_failures": {
                "type": AutoHashOrNothingType,
                "display_name": "Message Send Failures",
                "short_desc": "Message send failures",
                "desc": "Information about message send failures between peers",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}
}
