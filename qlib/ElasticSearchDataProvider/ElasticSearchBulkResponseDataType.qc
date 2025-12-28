# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ElasticSearchDataProvider module definition

/** ElasticSearchBulkResponseDataType.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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
                "desc": "The type of error that occurred",
            },
            "reason": {
                "type": StringType,
                "desc": "A human-readable explanation of the error",
            },
            "index": {
                "type": StringOrNothingType,
                "desc": "The index associated with the error",
            },
            "shard": {
                "type": IntOrNothingType,
                "desc": "The shard associated with the error",
            },
            "index_uuid": {
                "type": StringOrNothingType,
                "desc": "The UUID of the index",
            },
            "caused_by": {
                "type": AutoHashOrNothingType,
                "desc": "The underlying cause of the error, if any",
            },
        };
    }

    #! Creates the object
    constructor() {
        map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type, $1.value.default_value)),
            Fields.pairIterator();
    }
}

#! Bulk item response shards data type
public class ElasticSearchBulkItemShardsDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "total": {
                "type": IntType,
                "desc": "Total number of shards the operation was executed on",
            },
            "successful": {
                "type": IntType,
                "desc": "Number of shards where the operation succeeded",
            },
            "failed": {
                "type": IntType,
                "desc": "Number of shards where the operation failed",
            },
        };
    }

    #! Creates the object
    constructor() {
        map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type, $1.value.default_value)),
            Fields.pairIterator();
    }
}

#! Bulk item response data type (for each operation in the bulk request)
public class ElasticSearchBulkItemResponseDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "_index": {
                "type": StringType,
                "desc": "The index the document was indexed into",
            },
            "_id": {
                "type": StringType,
                "desc": "The document ID",
            },
            "_version": {
                "type": IntOrNothingType,
                "desc": "The document version",
            },
            "result": {
                "type": StringOrNothingType,
                "desc": "The result of the operation: created, updated, deleted, not_found, or noop",
            },
            "_shards": {
                "type": new ElasticSearchBulkItemShardsDataType(),
                "desc": "Shard information for the operation",
            },
            "_seq_no": {
                "type": IntOrNothingType,
                "desc": "The sequence number for the operation",
            },
            "_primary_term": {
                "type": IntOrNothingType,
                "desc": "The primary term for the operation",
            },
            "status": {
                "type": IntType,
                "desc": "HTTP status code for this operation",
            },
            "error": {
                "type": new ElasticSearchBulkErrorDataType(),
                "desc": "Error information if the operation failed",
            },
            "_type": {
                "type": StringOrNothingType,
                "desc": "The document type (deprecated, always _doc)",
            },
        };
    }

    #! Creates the object
    constructor() {
        map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type, $1.value.default_value)),
            Fields.pairIterator();
    }
}

#! Wrapper for bulk item response with operation type
public class ElasticSearchBulkItemWrapperDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "index": {
                "type": new ElasticSearchBulkItemResponseDataType(),
                "desc": "Response for an index operation",
            },
            "create": {
                "type": new ElasticSearchBulkItemResponseDataType(),
                "desc": "Response for a create operation",
            },
            "update": {
                "type": new ElasticSearchBulkItemResponseDataType(),
                "desc": "Response for an update operation",
            },
            "delete": {
                "type": new ElasticSearchBulkItemResponseDataType(),
                "desc": "Response for a delete operation",
            },
        };
    }

    #! Creates the object
    constructor() {
        map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type, $1.value.default_value)),
            Fields.pairIterator();
    }
}

#! Bulk API response data type
public class ElasticSearchBulkResponseDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "took": {
                "type": IntType,
                "desc": "How long, in milliseconds, it took to process the bulk request",
            },
            "errors": {
                "type": BoolType,
                "desc": "If true, one or more of the operations in the bulk request did not complete successfully",
            },
            "items": {
                "type": new ListDataType("BulkItemList", new ElasticSearchBulkItemWrapperDataType(), True),
                "desc": "Contains the result of each operation in the bulk request, in the order they were submitted",
            },
        };
    }

    #! Creates the object
    constructor() {
        map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type, $1.value.default_value)),
            Fields.pairIterator();
    }
}

#! Type constant for the bulk response
public const ElasticSearchBulkResponseType = new ElasticSearchBulkResponseDataType();
}
