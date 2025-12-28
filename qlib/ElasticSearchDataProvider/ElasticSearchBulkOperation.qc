# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ElasticSearchDataProvider module definition

/** ElasticSearchBulkOperation.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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
#! Bulk insert/upsert operation for ElasticSearch
/** This class implements the AbstractDataProviderBulkOperation interface for ElasticSearch bulk operations
*/
public class ElasticSearchBulkOperation inherits DataProvider::AbstractDataProviderBulkOperation {
    private {
        #! The REST client for ElasticSearch
        RestClient rest;

        #! The index name
        string index;

        #! The operation type: "index", "create", "update", or "delete"
        string optype;

        #! Optional pipeline for document processing
        *string pipeline;

        #! Optional routing value
        *string routing;

        #! Refresh setting: "true", "false", or "wait_for"
        *string refresh;
    }

    #! Creates the bulk operation object
    /** @param provider the data provider for the operation
        @param rest the REST client for ElasticSearch
        @param index the index name for the bulk operations
        @param optype the operation type: "index", "create", "update", or "delete"
        @param block_size the block size for bulk operations (default: 1000)
        @param options additional options: pipeline, routing, refresh
    */
    constructor(DataProvider::AbstractDataProvider provider, RestClient rest, string index, string optype,
            *int block_size, *hash<auto> options)
            : AbstractDataProviderBulkOperation(provider, optype, block_size ?? DefaultBlockSize) {
        self.rest = rest;
        self.index = index;
        self.optype = optype;
        self.pipeline = options.pipeline;
        self.routing = options.routing;
        self.refresh = options.refresh;
    }

    #! Flushes queued data to ElasticSearch using the bulk API
    private flushImpl() {
        # Build NDJSON payload
        string ndjson = formatNDJSON();
        if (!ndjson) {
            return;
        }

        # Build URI with optional parameters
        string uri = "_bulk";
        list<string> params = ();
        if (pipeline) {
            push params, sprintf("pipeline=%s", pipeline);
        }
        if (routing) {
            push params, sprintf("routing=%s", routing);
        }
        if (refresh) {
            push params, sprintf("refresh=%s", refresh);
        }
        if (params) {
            uri += "?" + params.join("&");
        }

        # Send bulk request
        hash<auto> response = rest.post(uri, ndjson, {"Content-Type": "application/x-ndjson"});

        # Handle partial failures
        if (response.body.errors) {
            handleBulkErrors(response.body);
        }
    }

    #! Formats the buffered records as NDJSON for the bulk API
    /** @return the NDJSON string for the bulk request
    */
    private string formatNDJSON() {
        string result = "";
        int count = buf.firstValue().lsize();

        for (int i = 0; i < count; ++i) {
            # Build the record from column lists
            hash<auto> record = map {$1: buf{$1}[i]}, keys buf;

            # Extract document ID if present
            *string doc_id = remove record._id;

            # Build action metadata
            hash<auto> action_meta = {
                "_index": index,
            };
            if (doc_id) {
                action_meta._id = doc_id;
            }

            hash<auto> action = {
                optype: action_meta,
            };

            # Append action line
            result += make_json(action) + "\n";

            # Append document line (except for delete operations)
            if (optype != "delete") {
                if (optype == "update") {
                    # Update operations need the document wrapped in "doc"
                    result += make_json({"doc": record}) + "\n";
                } else {
                    result += make_json(record) + "\n";
                }
            }
        }

        return result;
    }

    #! Handles errors from a bulk response
    /** @param response the bulk API response body
    */
    private handleBulkErrors(hash<auto> response) {
        list<hash<auto>> failures = ();
        int i = 0;
        foreach hash<auto> item in (response.items) {
            # Get the operation result (index, create, update, or delete)
            hash<auto> op_result = item{optype} ?? item.firstValue();
            if (op_result.error) {
                push failures, {
                    "index": i,
                    "id": op_result._id,
                    "error_type": op_result.error.type,
                    "reason": op_result.error.reason,
                };
            }
            ++i;
        }

        if (failures) {
            throw "ELASTICSEARCH-BULK-ERROR", sprintf("%d of %d operations failed in bulk request",
                failures.size(), response.items.size()), failures;
        }
    }
}
}
