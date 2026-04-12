# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ElasticSearchDataProvider module definition

/** ElasticSearchDocumentDeleteDataProvider.qc Copyright 2023 - 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ElasticSearchDataProvider module
public namespace ElasticSearchDataProvider {
#! The ElasticSearch document delete API data provider
/** This API allows the caller to delete an ElasticSearch document
*/
public class ElasticSearchDocumentDeleteDataProvider inherits ElasticSearchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete",
            "desc": "ElasticSearch document delete API data provider",
            "type": "ElasticSearchDocumentDeleteDataProvider",
            "constructor_options": ElasticSearchDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new ElasticSearchDocumentDeleteRequestDataType();

        #! Response type
        const ResponseType = new ElasticSearchDocumentDeleteResponseDataType();

        #! Query args
        const QueryArgs = ("if_seq_no", "if_primary_term", "refresh", "routing", "timeout",
            "version", "version_type", "wait_for_active_shards");
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        checkOptions("CONSTRUCTOR-ERROR", ElasticSearchDataProvider::ConstructorOptions, options);
        rest = ElasticSearchDataProvider::getRestConnection(options);
    }

    #! Creates the object from a REST connection
    constructor(RestClient::AbstractRestClient rest) : ElasticSearchDataProviderBase(rest) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("ElasticSearch data document %s provider for `%s`", ProviderInfo.name, rest.getSafeUrl());
    }

    #! Makes a request and returns the response
    /** @param req the request to serialize and make according to the request type
        @param request_options the request options; will be processed by validateRequestOptions()

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string uri_path = sprintf("%s/_doc/%s", remove req.index, remove req.id);
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
            uri_path += sprintf("%s=%s", arg, remove req{arg});
        }
        return rest.restDel(uri_path).body;
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

#! Document delete API request data type
class ElasticSearchDocumentDeleteRequestDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            # query parameters
            "index": {
                "type": StringType,
                "display_name": "Index",
                "short_desc": "Index name",
                "desc": "The name of the index holding the document to delete",
                "example_value": "my-index",
            },
            "id": {
                "type": StringType,
                "display_name": "Document ID",
                "short_desc": "Document identifier",
                "desc": "The ID of the document to delete",
                "example_value": "doc-123",
            },
            "if_seq_no": {
                "type": IntOrNothingType,
                "display_name": "If Seq No",
                "short_desc": "Required sequence number",
                "desc": "Only perform the operation if the document has this sequence number",
                "example_value": 1,
            },
            "if_primary_term": {
                "type": IntOrNothingType,
                "display_name": "If Primary Term",
                "short_desc": "Required primary term",
                "desc": "Only perform the operation if the document has this primary term",
                "example_value": 1,
            },
            "refresh": {
                "type": StringOrNothingType,
                "display_name": "Refresh",
                "short_desc": "Refresh setting",
                "desc": "If `true` (default `false`), Elasticsearch refreshes the affected shards to make this "
                    "operation visible to search, if `wait_for` then wait for a refresh to make this operation "
                    "visible to search, if `false` do nothing with refreshes. Valid values: `true`, `false`, "
                    "`wait_for`",
                "example_value": "wait_for",
            },
            "routing": {
                "type": StringOrNothingType,
                "display_name": "Routing",
                "short_desc": "Custom routing value",
                "desc": "Custom value used to route operations to a specific shard",
                "example_value": "user123",
            },
            "timeout": {
                "type": StringOrNothingType,
                "display_name": "Timeout",
                "short_desc": "Response wait period",
                "desc": "Period to wait for a response (default `1m`). If no response is received before the "
                    "timeout expires, the request fails and returns an error",
                "example_value": "1m",
            },
            "version": {
                "type": IntOrNothingType,
                "display_name": "Version",
                "short_desc": "Required version",
                "desc": "Explicit version number for concurrency control. The specified version must match the "
                    "current version of the document for the request to succeed",
                "example_value": 1,
            },
            "version_type": {
                "type": StringOrNothingType,
                "display_name": "Version Type",
                "short_desc": "Version type",
                "desc": "Specific version type: `external`, `external_gte`",
                "example_value": "external",
            },
            "wait_for_active_shards": {
                "type": StringOrNothingType,
                "display_name": "Wait For Active Shards",
                "short_desc": "Required active shards",
                "desc": "The number of shard copies that must be active before proceeding with the operation "
                    "(default `1`). Set to `all` or any positive integer up to the total number of shards in the "
                    "index (number_of_replicas+1)",
                "example_value": "1",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Document delete API response
public class ElasticSearchDocumentDeleteResponseDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "_index": {
                "type": StringType,
                "display_name": "Index",
                "short_desc": "Index name",
                "desc": "The name of the index the document was added to",
                "example_value": "my-index",
            },
            "_id": {
                "type": StringType,
                "display_name": "Document ID",
                "short_desc": "Document identifier",
                "desc": "The unique identifier for the added document",
                "example_value": "doc-123",
            },
            "_version": {
                "type": IntType,
                "display_name": "Version",
                "short_desc": "Document version",
                "desc": "The document version. Incremented each time the document is updated",
                "example_value": 1,
            },
            "result": {
                "type": StringType,
                "display_name": "Result",
                "short_desc": "Operation result",
                "desc": "The result of the indexing operation: `deleted`",
                "example_value": "deleted",
            },
            "_shards": {
                "type": new ElasticSearchDocumentCreateShardsDataType(),
                "display_name": "Shards",
                "short_desc": "Replication info",
                "desc": "Provides information about the replication process of the index operation",
            },
            "_seq_no": {
                "type": IntType,
                "display_name": "Sequence Number",
                "short_desc": "Sequence number",
                "desc": "The sequence number assigned to the document for the indexing operation. Sequence numbers "
                    "are used to ensure an older version of a document doesn't overwrite a newer version",
                "example_value": 1,
            },
            "_primary_term": {
                "type": IntType,
                "display_name": "Primary Term",
                "short_desc": "Primary term",
                "desc": "The primary term assigned to the document for the indexing operation",
                "example_value": 1,
            },
            "_type": {
                "type": StringOrNothingType,
                "display_name": "Type",
                "short_desc": "Document type (deprecated)",
                "desc": "The document type. Elasticsearch indices now support a single document type, `_doc`",
                "example_value": "_doc",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}
}
