# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ElasticSearchDataProvider module definition

/** ElasticSearchBulkDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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
#! The ElasticSearch bulk API data provider
/** This API allows the caller to perform bulk operations on ElasticSearch documents

    The bulk API supports the following operation types:
    - @b index: Index a document (create or replace)
    - @b create: Create a document (fail if exists)
    - @b update: Update an existing document
    - @b delete: Delete a document

    @see https://www.elastic.co/guide/en/elasticsearch/reference/current/docs-bulk.html
*/
public class ElasticSearchBulkDataProvider inherits ElasticSearchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "bulk",
            "desc": "ElasticSearch bulk API data provider for batch document operations",
            "type": "ElasticSearchBulkDataProvider",
            "constructor_options": ElasticSearchDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new ElasticSearchBulkRequestDataType();

        #! Response type
        const ResponseType = ElasticSearchBulkResponseType;

        #! Query args
        const QueryArgs = ("pipeline", "refresh", "routing", "timeout", "wait_for_active_shards", "require_alias");
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        checkOptions("CONSTRUCTOR-ERROR", ElasticSearchDataProvider::ConstructorOptions, options);
        rest = ElasticSearchDataProvider::getRestConnection(options);
    }

    #! Creates the object from a REST connection
    constructor(RestClient::RestClient rest) : ElasticSearchDataProviderBase(rest) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("ElasticSearch bulk API provider for `%s`", rest.getSafeURL());
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

        return rest.post(uri, ndjson, {"Content-Type": "application/x-ndjson"}).body;
    }

    #! Formats the operations list as NDJSON
    /** @param operations list of operation hashes
        @return NDJSON string for the bulk request
    */
    private string formatOperations(list<hash<auto>> operations) {
        string result = "";

        foreach hash<auto> op in (operations) {
            string action = op.action;
            if (action !~ /^(index|create|update|delete)$/) {
                throw "ELASTICSEARCH-BULK-ERROR", sprintf("Invalid action type '%s'; must be one of: index, create, "
                    "update, delete", action);
            }

            # Build action metadata
            hash<auto> action_meta = {};
            if (op._index) {
                action_meta._index = op._index;
            }
            if (op._id) {
                action_meta._id = op._id;
            }
            if (op.routing) {
                action_meta.routing = op.routing;
            }
            if (op.pipeline) {
                action_meta.pipeline = op.pipeline;
            }
            if (op.if_seq_no.typeCode() != NT_NOTHING) {
                action_meta.if_seq_no = op.if_seq_no;
            }
            if (op.if_primary_term.typeCode() != NT_NOTHING) {
                action_meta.if_primary_term = op.if_primary_term;
            }

            # Append action line
            result += make_json({action: action_meta}) + "\n";

            # Append document line (except for delete operations)
            if (action != "delete" && op.doc) {
                if (action == "update") {
                    # Update operations support doc, script, upsert, etc.
                    hash<auto> update_body = {};
                    if (op.doc) {
                        update_body.doc = op.doc;
                    }
                    if (op.doc_as_upsert) {
                        update_body.doc_as_upsert = op.doc_as_upsert;
                    }
                    if (op.script) {
                        update_body.script = op.script;
                    }
                    if (op.upsert) {
                        update_body.upsert = op.upsert;
                    }
                    result += make_json(update_body) + "\n";
                } else {
                    result += make_json(op.doc) + "\n";
                }
            }
        }

        return result;
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
public class ElasticSearchBulkOperationItemDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
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
                "display_name": "Doc As Upsert",
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
public class ElasticSearchBulkRequestDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "operations": {
                "type": new ListDataType("BulkOperationsList", new ElasticSearchBulkOperationItemDataType(), True),
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
                "display_name": "Require Alias",
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
