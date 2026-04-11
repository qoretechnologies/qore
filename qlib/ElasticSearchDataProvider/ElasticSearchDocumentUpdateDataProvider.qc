# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ElasticSearchDataProvider module definition

/** ElasticSearchDocumentUpdateDataProvider.qc Copyright 2023 - 2026 Qore Technologies, s.r.o.

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
#! The ElasticSearch document update API data provider
/** This API allows the caller to update an ElasticSearch document
*/
public class ElasticSearchDocumentUpdateDataProvider inherits ElasticSearchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "desc": "ElasticSearch document update API data provider",
            "type": "ElasticSearchDocumentUpdateDataProvider",
            "constructor_options": ElasticSearchDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new ElasticSearchDocumentUpdateRequestDataType();

        #! Response type
        const ResponseType = new ElasticSearchDocumentUpdateResponseDataType();

        #! Query args
        const QueryArgs = ("if_seq_no", "if_primary_term", "lang", "require_alias", "refresh", "retry_on_conflict",
            "routing", "_source", "_source_excludes", "_source_includes", "timeout", "wait_for_active_shards");
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
        string uri_path = sprintf("%s/_update/%s", remove req.index, remove req.id);
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
        return rest.restPost(uri_path, req).body;
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

#! Update document request data type
public class ElasticSearchDocumentUpdateRequestDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            # query parameters
            "index": {
                "type": StringType,
                "display_name": "Index",
                "short_desc": "Index name",
                "desc": "The name of the index to use",
                "example_value": "my-index",
            },
            "id": {
                "type": StringType,
                "display_name": "Document ID",
                "short_desc": "Document identifier",
                "desc": "The ID of the document to update",
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
            "lang": {
                "type": StringOrNothingType,
                "display_name": "Language",
                "short_desc": "Script language",
                "desc": "The script language (default `painless`)",
                "example_value": "painless",
            },
            "require_alias": {
                "type": SoftBoolStringType,
                "display_name": "Require Alias?",
                "short_desc": "Destination must be alias",
                "desc": "If `true` (default `false`), the destination must be an index alias",
                "example_value": False,
            },
            "refresh": {
                "type": StringOrNothingType,
                "display_name": "Refresh",
                "short_desc": "Refresh setting",
                "desc": "If `true` (default `false`), Elasticsearch refreshes the affected shards to make this "
                    "operation visible to search, if `wait_for` then wait for a refresh to make this operation "
                    "visible to search, if `false` do nothing with refreshes. Valid values: `true`, `false`, `wait_for`",
                "example_value": "wait_for",
            },
            "retry_on_conflict": {
                "type": IntOrNothingType,
                "display_name": "Retry On Conflict",
                "short_desc": "Conflict retry count",
                "desc": "Specify how many times should the operation be retried when a conflict occurs (default `0`)",
                "example_value": 3,
            },
            "routing": {
                "type": StringOrNothingType,
                "display_name": "Routing",
                "short_desc": "Custom routing value",
                "desc": "Custom value used to route operations to a specific shard",
                "example_value": "user123",
            },
            "_source": {
                "type": StringOrNothingType,
                "display_name": "Source",
                "short_desc": "Source retrieval control",
                "desc": "Set to `false` to disable source retrieval (default: `true`). You can also specify a "
                    "comma-separated list of the fields you want to retrieve",
                "example_value": "true",
            },
            "_source_excludes": {
                "type": StringOrNothingType,
                "display_name": "Source Excludes",
                "short_desc": "Fields to exclude",
                "desc": "Specify the source fields you want to exclude",
                "example_value": "meta.*",
            },
            "_source_includes": {
                "type": StringOrNothingType,
                "display_name": "Source Includes",
                "short_desc": "Fields to include",
                "desc": "Specify the source fields you want to include",
                "example_value": "title,content",
            },
            "timeout": {
                "type": StringOrNothingType,
                "display_name": "Timeout",
                "short_desc": "Operation timeout",
                "desc": "Period to wait for the following operations:\n"
                    "- Dynamic mapping updates\n"
                    "- Waiting for active shards\n\n"
                    "This guarantees Elasticsearch waits for at least the timeout before failing. The actual wait "
                    "time could be longer, particularly when multiple waits occur (default `1m)",
                "example_value": "1m",
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

            # body parameters
            "script": {
                "type": AutoType,
                "display_name": "Script",
                "short_desc": "Update script",
                "desc": "the script to run",
                "example_value": {"source": "ctx._source.counter++"},
            },
            "doc": {
                "type": AutoHashOrNothingType,
                "display_name": "Document",
                "short_desc": "Partial document update",
                "desc": "Updates the document with the given hash; ignored if `script` is also provided",
                "example_value": {"title": "Updated Title"},
            },
            "detect_noop": {
                "type": BoolOrNothingType,
                "display_name": "Detect Noop?",
                "short_desc": "Ignore no-op updates",
                "desc": "Ignore noops",
                "example_value": True,
            },
            "doc_as_upsert": {
                "type": BoolOrNothingType,
                "display_name": "Doc As Upsert?",
                "short_desc": "Use doc as upsert",
                "desc": "Instead of sending a partial doc plus an upsert doc, you can set `doc_as_upsert` to `true` "
                    "to use the contents of `doc` as the upsert value",
                "example_value": True,
            },
            "scripted_upsert": {
                "type": BoolOrNothingType,
                "display_name": "Scripted Upsert?",
                "short_desc": "Run script on upsert",
                "desc": "To run the script whether or not the document exists",
                "example_value": False,
            },
            "upsert": {
                "type": AutoHashOrNothingType,
                "display_name": "Upsert",
                "short_desc": "Document to insert if missing",
                "desc": "Hash to insert as a new document if the document did not exist previously",
                "example_value": {"title": "New Document", "counter": 0},
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);

        # body field values must be JSON strings
        setDefaultOtherFieldType(AbstractDataProviderTypeMap."string");
    }
}

#! Document update API response
public class ElasticSearchDocumentUpdateResponseDataType inherits DataProvider::HashDataType {
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
                "example_value": 2,
            },
            "result": {
                "type": StringType,
                "display_name": "Result",
                "short_desc": "Operation result",
                "desc": "The result of the indexing operation, `noop` or `updated`",
                "example_value": "updated",
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
                "example_value": 2,
            },
            "_primary_term": {
                "type": IntType,
                "display_name": "Primary Term",
                "short_desc": "Primary term",
                "desc": "The primary term assigned to the document for the indexing operation",
                "example_value": 1,
            },
            "get": {
                "type": AutoHashOrNothingType,
                "display_name": "Get",
                "short_desc": "Updated document info",
                "desc": "Information about the document after the update",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}
}
