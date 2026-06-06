# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore OpenSearchDataProvider module definition

/** OpenSearchBulkDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the OpenSearchDataProvider module
public namespace OpenSearchDataProvider {
#! The OpenSearch bulk API data provider
/** This API allows the caller to perform bulk operations on OpenSearch documents

    The bulk API supports the following operation types:
    - @b index: Index a document (create or replace)
    - @b create: Create a document (fail if exists)
    - @b update: Update an existing document
    - @b delete: Delete a document

    @see https://www.elastic.co/guide/en/opensearch/reference/current/docs-bulk.html
*/
public class OpenSearchBulkDataProvider inherits OpenSearchDataProviderBase {
    public {
        #! Provider info
        const hash<DataProviderInfo> ProviderInfo = <DataProviderInfo>{
            "name": "bulk",
            "desc": "OpenSearch bulk API data provider for batch document operations",
            "type": "OpenSearchBulkDataProvider",
            "constructor_options": OpenSearchDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const hash<DataProviderSummaryInfo> ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const AbstractDataProviderType RequestType = new OpenSearchBulkRequestDataType();

        #! Response type
        const AbstractDataProviderType ResponseType = OpenSearchBulkResponseType;

        #! Query args
        const list<string> QueryArgs = ("pipeline", "refresh", "routing", "timeout", "wait_for_active_shards", "require_alias");
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        checkOptions("CONSTRUCTOR-ERROR", OpenSearchDataProvider::ConstructorOptions, options);
        rest = OpenSearchDataProvider::getRestConnection(options);
    }

    #! Creates the object from a REST connection
    constructor(RestClient::AbstractRestClient rest) : OpenSearchDataProviderBase(rest) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("OpenSearch bulk API provider for `%s`", rest.getSafeUrl());
    }

    #! Makes a request and returns the response
    /** @param req the request to serialize and make according to the request type
        @param request_options the request options; will be processed by validateRequestOptions()

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        # Build NDJSON payload from operations
        string ndjson = formatOperations(req.operations);

        # Build URI with optional parameters
        string uri = "_bulk";
        bool has_query_params;
        foreach string arg in (QueryArgs) {
            if (!exists req{arg}) {
                continue;
            }
            if (!has_query_params) {
                uri += "?";
                has_query_params = True;
            } else {
                uri += "&";
            }
            uri += sprintf("%s=%s", arg, encode_url(string(req{arg}), True));
        }

        # Use restDoRawRequest to bypass the validator's JSON serialization step —
        # OpenSearch's bulk API is strict about newline-delimited JSON and rejects
        # any wrapping (the default data-serialization path would JSON-encode the string,
        # wrapping it in quotes and stripping the trailing \n).
        return rest.restDoRawRequest("POST", uri, ndjson, {"Content-Type": "application/x-ndjson"}).body;
    }

    #! Formats the operations list as NDJSON
    /** @param operations list of operation hashes
        @return NDJSON string for the bulk request

        @note uses @ref NdjsonDataProvider::NdjsonStringWriter "NdjsonStringWriter" — backed
            by a @ref Qore::StringOutputStream "StringOutputStream" with geometric-growth
            buffering — so building the bulk body is O(n) in the number of operations
            rather than the O(n²) cost of repeated `string += make_json(x) + "\n"`
            concatenation
    */
    private string formatOperations(list<hash<auto>> operations) {
        return SearchIndexDataProvider::SearchIndexBulkUtil::formatOperations(operations, "OPENSEARCH-BULK-ERROR");
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

#! Bulk operation item data type
public class OpenSearchBulkOperationItemDataType inherits DataProvider::HashDataType {
    public {
        #! Field descriptions
        const hash<string, AbstractDataField> Fields = {
            "action": {
                "type": StringType,
                "display_name": "Action",
                "short_desc": "Operation type",
                "desc": "The action type: 'index', 'create', 'update', or 'delete'",
                "example_value": "index",
            },
            "_index": {
                "type": StringOrNothingType,
                "display_name": "Index",
                "short_desc": "Target index name",
                "desc": "The target index for this operation (optional if default index is set)",
                "example_value": "my-index",
            },
            "_id": {
                "type": StringOrNothingType,
                "display_name": "Document ID",
                "short_desc": "Document identifier",
                "desc": "The document ID (optional for index/create, required for update/delete)",
                "example_value": "doc-123",
            },
            "doc": {
                "type": AutoHashOrNothingType,
                "display_name": "Document",
                "short_desc": "Document body",
                "desc": "The document body for index/create operations, or the partial document for update operations",
                "example_value": {"title": "Example", "content": "Sample document"},
            },
            "doc_as_upsert": {
                "type": SoftBoolStringType,
                "display_name": "Doc As Upsert?",
                "short_desc": "Use doc as upsert",
                "desc": "For update: if true, use the doc as the upsert document if the document does not exist",
                "example_value": True,
            },
            "script": {
                "type": AutoHashOrNothingType,
                "display_name": "Script",
                "short_desc": "Update script",
                "desc": "For update: a script to run to update the document",
                "example_value": {"source": "ctx._source.counter++"},
            },
            "upsert": {
                "type": AutoHashOrNothingType,
                "display_name": "Upsert",
                "short_desc": "Upsert document",
                "desc": "For update: the document to insert if it does not exist",
                "example_value": {"counter": 1},
            },
            "routing": {
                "type": StringOrNothingType,
                "display_name": "Routing",
                "short_desc": "Custom routing value",
                "desc": "Custom routing value for this operation",
                "example_value": "user123",
            },
            "pipeline": {
                "type": StringOrNothingType,
                "display_name": "Pipeline",
                "short_desc": "Ingest pipeline name",
                "desc": "The ingest pipeline to use for this operation",
                "example_value": "my-pipeline",
            },
            "if_seq_no": {
                "type": IntOrNothingType,
                "display_name": "If Seq No",
                "short_desc": "Optimistic concurrency seq no",
                "desc": "Only perform the operation if the document has this sequence number",
                "example_value": 1,
            },
            "if_primary_term": {
                "type": IntOrNothingType,
                "display_name": "If Primary Term",
                "short_desc": "Optimistic concurrency term",
                "desc": "Only perform the operation if the document has this primary term",
                "example_value": 1,
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Bulk request data type
public class OpenSearchBulkRequestDataType inherits DataProvider::HashDataType {
    public {
        #! Field descriptions
        const hash<string, AbstractDataField> Fields = {
            "operations": {
                "type": new ListDataType("BulkOperationsList", new OpenSearchBulkOperationItemDataType(), True),
                "display_name": "Operations",
                "short_desc": "List of bulk operations",
                "desc": "List of bulk operations to perform",
            },
            "pipeline": {
                "type": StringOrNothingType,
                "display_name": "Pipeline",
                "short_desc": "Default ingest pipeline",
                "desc": "The default ingest pipeline to use for all operations",
                "example_value": "my-pipeline",
            },
            "refresh": {
                "type": StringOrNothingType,
                "display_name": "Refresh",
                "short_desc": "Refresh setting",
                "desc": "Refresh setting: 'true', 'false', or 'wait_for'",
                "example_value": "wait_for",
            },
            "routing": {
                "type": StringOrNothingType,
                "display_name": "Routing",
                "short_desc": "Default routing value",
                "desc": "Default routing value for all operations",
                "example_value": "user123",
            },
            "timeout": {
                "type": StringOrNothingType,
                "display_name": "Timeout",
                "short_desc": "Response wait period",
                "desc": "Period to wait for a response (default: 1m)",
                "example_value": "1m",
            },
            "wait_for_active_shards": {
                "type": StringOrNothingType,
                "display_name": "Wait For Active Shards",
                "short_desc": "Required active shards",
                "desc": "Number of shard copies that must be active before proceeding",
                "example_value": "1",
            },
            "require_alias": {
                "type": SoftBoolStringType,
                "display_name": "Require Alias?",
                "short_desc": "Destination must be alias",
                "desc": "If true, the destination must be an index alias",
                "example_value": False,
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}
}
