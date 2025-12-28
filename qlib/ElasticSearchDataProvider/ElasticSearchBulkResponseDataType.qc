# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ElasticSearchDataProvider module definition

/** ElasticSearchBulkResponseDataType.qc Copyright 2025 Qore Technologies, s.r.o.

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
#! Bulk operation error data type
public class ElasticSearchBulkErrorDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "type": {
                "type": StringType,
                "display_name": "Error Type",
                "short_desc": "Type of error",
                "desc": "The type of error that occurred",
                "example_value": "mapper_parsing_exception",
            },
            "reason": {
                "type": StringType,
                "display_name": "Reason",
                "short_desc": "Error explanation",
                "desc": "A human-readable explanation of the error",
                "example_value": "failed to parse field [timestamp]",
            },
            "index": {
                "type": StringOrNothingType,
                "display_name": "Index",
                "short_desc": "Associated index",
                "desc": "The index associated with the error",
                "example_value": "my-index",
            },
            "shard": {
                "type": IntOrNothingType,
                "display_name": "Shard",
                "short_desc": "Associated shard",
                "desc": "The shard associated with the error",
                "example_value": 0,
            },
            "index_uuid": {
                "type": StringOrNothingType,
                "display_name": "Index UUID",
                "short_desc": "Index unique identifier",
                "desc": "The UUID of the index",
                "example_value": "aXNkZXhfMQ",
            },
            "caused_by": {
                "type": AutoHashOrNothingType,
                "display_name": "Caused By",
                "short_desc": "Underlying cause",
                "desc": "The underlying cause of the error, if any",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Bulk item response shards data type
public class ElasticSearchBulkItemShardsDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "total": {
                "type": IntType,
                "display_name": "Total Shards",
                "short_desc": "Total shards for operation",
                "desc": "Total number of shards the operation was executed on",
                "example_value": 2,
            },
            "successful": {
                "type": IntType,
                "display_name": "Successful Shards",
                "short_desc": "Shards that succeeded",
                "desc": "Number of shards where the operation succeeded",
                "example_value": 2,
            },
            "failed": {
                "type": IntType,
                "display_name": "Failed Shards",
                "short_desc": "Shards that failed",
                "desc": "Number of shards where the operation failed",
                "example_value": 0,
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Bulk item response data type (for each operation in the bulk request)
public class ElasticSearchBulkItemResponseDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "_index": {
                "type": StringType,
                "display_name": "Index",
                "short_desc": "Target index name",
                "desc": "The index the document was indexed into",
                "example_value": "my-index",
            },
            "_id": {
                "type": StringType,
                "display_name": "Document ID",
                "short_desc": "Document identifier",
                "desc": "The document ID",
                "example_value": "doc-123",
            },
            "_version": {
                "type": IntOrNothingType,
                "display_name": "Version",
                "short_desc": "Document version",
                "desc": "The document version",
                "example_value": 1,
            },
            "result": {
                "type": StringOrNothingType,
                "display_name": "Result",
                "short_desc": "Operation result",
                "desc": "The result of the operation: created, updated, deleted, not_found, or noop",
                "example_value": "created",
            },
            "_shards": {
                "type": new ElasticSearchBulkItemShardsDataType(),
                "display_name": "Shards",
                "short_desc": "Shard information",
                "desc": "Shard information for the operation",
            },
            "_seq_no": {
                "type": IntOrNothingType,
                "display_name": "Sequence Number",
                "short_desc": "Operation sequence number",
                "desc": "The sequence number for the operation",
                "example_value": 1,
            },
            "_primary_term": {
                "type": IntOrNothingType,
                "display_name": "Primary Term",
                "short_desc": "Operation primary term",
                "desc": "The primary term for the operation",
                "example_value": 1,
            },
            "status": {
                "type": IntType,
                "display_name": "Status",
                "short_desc": "HTTP status code",
                "desc": "HTTP status code for this operation",
                "example_value": 201,
            },
            "error": {
                "type": new ElasticSearchBulkErrorDataType(),
                "display_name": "Error",
                "short_desc": "Error information",
                "desc": "Error information if the operation failed",
            },
            "_type": {
                "type": StringOrNothingType,
                "display_name": "Type",
                "short_desc": "Document type (deprecated)",
                "desc": "The document type (deprecated, always _doc)",
                "example_value": "_doc",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Wrapper for bulk item response with operation type
public class ElasticSearchBulkItemWrapperDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "index": {
                "type": new ElasticSearchBulkItemResponseDataType(),
                "display_name": "Index Response",
                "short_desc": "Index operation result",
                "desc": "Response for an index operation",
            },
            "create": {
                "type": new ElasticSearchBulkItemResponseDataType(),
                "display_name": "Create Response",
                "short_desc": "Create operation result",
                "desc": "Response for a create operation",
            },
            "update": {
                "type": new ElasticSearchBulkItemResponseDataType(),
                "display_name": "Update Response",
                "short_desc": "Update operation result",
                "desc": "Response for an update operation",
            },
            "delete": {
                "type": new ElasticSearchBulkItemResponseDataType(),
                "display_name": "Delete Response",
                "short_desc": "Delete operation result",
                "desc": "Response for a delete operation",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Bulk API response data type
public class ElasticSearchBulkResponseDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "took": {
                "type": IntType,
                "display_name": "Time Taken",
                "short_desc": "Processing time in ms",
                "desc": "How long, in milliseconds, it took to process the bulk request",
                "example_value": 30,
            },
            "errors": {
                "type": BoolType,
                "display_name": "Errors?",
                "short_desc": "Whether any operations failed",
                "desc": "If true, one or more of the operations in the bulk request did not complete successfully",
                "example_value": False,
            },
            "items": {
                "type": new ListDataType("BulkItemList", new ElasticSearchBulkItemWrapperDataType(), True),
                "display_name": "Items",
                "short_desc": "Operation results",
                "desc": "Contains the result of each operation in the bulk request, in the order they were submitted",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Type constant for the bulk response
public const ElasticSearchBulkResponseType = new ElasticSearchBulkResponseDataType();
}
