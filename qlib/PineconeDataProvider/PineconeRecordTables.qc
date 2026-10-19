# -*- mode: qore; indent-tabs-mode: nil -*-
/** PineconeRecordTables.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace PineconeDataProvider {
#! Complete record collections exposed by the Pinecone application
/** ID listings are hydrated with a batch fetch. Metadata predicates use paginated full-record fetches;
    ranked search endpoints cannot enumerate a collection. A vector's user fields live under metadata,
    while a document's user fields live under fields, so both have a stable record type.
*/
public class PineconeRecordTables {
    public {
        #! Unpaged project catalogues
        const Unpaged = <RestSchemaRecordPagingInfo>{"mode": RSRP_NONE};

        #! Control-plane listings use a continuation token and the service's default page size
        const ControlPaging = <RestSchemaRecordPagingInfo>{
            "mode": RSRP_CURSOR, "page_option": "paginationToken", "next_path": ("pagination", "next"),
        };

        #! Data-plane ID listings are capped at 100 identities per request
        const DataPaging = <RestSchemaRecordPagingInfo>{
            "mode": RSRP_CURSOR, "page_option": "paginationToken", "size_option": "limit",
            "max_size": 100, "next_path": ("pagination", "next"),
        };

        #! Document endpoints spell the pagination option with an underscore
        const DocumentPaging = cast<hash<RestSchemaRecordPagingInfo>>(DataPaging
            + {"page_option": "pagination_token"});

        #! Pinecone metadata filter operators; logical NOT remains on the client
        const Operators = {
            DP_SEARCH_OP_EQ: "$eq", DP_SEARCH_OP_NE: "$ne", DP_SEARCH_OP_GT: "$gt",
            DP_SEARCH_OP_GE: "$gte", DP_SEARCH_OP_LT: "$lt", DP_SEARCH_OP_LE: "$lte",
            DP_SEARCH_OP_IN: "$in", DP_OP_NOT_IN: "$nin",
            DP_OP_IS_NULL: "$exists", DP_OP_IS_NOT_NULL: "$exists",
        };

        #! JSON tree syntax; dynamic field names are validated by the table's predicate hook
        const Syntax = <RestSchemaQuerySyntaxInfo>{
            "name": "Pinecone metadata", "binary_ops": Operators,
            "and_op": "$and", "or_op": "$or", "not_op": NOTHING,
            "render_query": \PineconeRecordTables::renderQuery(),
        };

        #! The index catalogue; create uses the explicit schema/deployment operation
        const Indexes = <RestSchemaRecordTableInfo>{
            "name": "indexes", "display_name": "Indexes", "short_desc": "Pinecone indexes as records",
            "desc": "Manage indexes and their deployment settings. Create uses `create-index`; use the "
                "**Create Index for Model** action for integrated embedding.",
            "list_action": "list-indexes", "get_action": "describe-index", "create_action": "create-index",
            "update_action": "configure-index", "delete_action": "delete-index",
            "id_field": "name", "id_option": "index_name", "records_path": "indexes", "paging": Unpaged,
        };

        #! Legacy pod-index snapshots
        const Collections = <RestSchemaRecordTableInfo>{
            "name": "collections", "display_name": "Collections", "short_desc": "Legacy Pinecone collections",
            "desc": "Manage snapshots of legacy pod-based indexes. Collections are unavailable for serverless "
                "indexes; use backups for those indexes.",
            "list_action": "list-collections", "get_action": "describe-collection",
            "create_action": "create-collection", "delete_action": "delete-collection",
            "id_field": "name", "id_option": "collection_name", "records_path": "collections", "paging": Unpaged,
        };

        #! Project-wide backups; only creation needs an index
        const Backups = <RestSchemaRecordTableInfo>{
            "name": "backups", "display_name": "Backups", "short_desc": "Pinecone index backups",
            "desc": "Read backups across the project, or create a backup of a selected index. "
                "A backup is addressed by `backup_id` even after its source index is removed.",
            "list_action": "list-backups", "get_action": "describe-backup", "create_action": "create-backup",
            "delete_action": "delete-backup", "id_field": "backup_id", "records_path": "data",
            "create_scope_options": "index_name", "paging": ControlPaging,
        };

        #! Index-scoped schedule listings with project-wide identities
        const BackupSchedules = <RestSchemaRecordTableInfo>{
            "name": "backup-schedules", "display_name": "Backup Schedules",
            "short_desc": "Pinecone scheduled backups",
            "desc": "Manage an index's scheduled backups, including frequency and retention. "
                "Reads and writes by `schedule_id` use its project-wide identity.",
            "list_action": "list-backup-schedules", "get_action": "describe-backup-schedule",
            "create_action": "create-backup-schedule", "update_action": "update-backup-schedule",
            "delete_action": "delete-backup-schedule", "id_field": "schedule_id", "records_path": "data",
            "scope_options": "index_name", "paging": ControlPaging,
            "search_option_overrides": {"index_name": {"required": False}},
        };

        #! Read-only restore job status
        const RestoreJobs = <RestSchemaRecordTableInfo>{
            "name": "restore-jobs", "display_name": "Restore Jobs", "short_desc": "Pinecone restore jobs",
            "desc": "Read the progress and outcome of restores. Start a restore with **Create Index from Backup**.",
            "list_action": "list-restore-jobs", "get_action": "describe-restore-job", "id_field": "restore_job_id",
            "id_option": "job_id",
            "records_path": "data", "paging": ControlPaging,
        };

        #! Read-only hosted model catalogue
        const Models = <RestSchemaRecordTableInfo>{
            "name": "models", "display_name": "Models", "short_desc": "Pinecone hosted inference models",
            "desc": "Discover hosted embedding and reranking models before configuring an inference workflow.",
            "list_action": "list-models", "get_action": "describe-model", "id_field": "model",
            "id_option": "model_name", "records_path": "models", "paging": Unpaged,
            "search_options": ("type", "vector_type"),
            "filters": (<RestSchemaRecordFilterInfo>{"field": "type", "operator": DP_SEARCH_OP_EQ,
                "option": "type", "exact": True}, <RestSchemaRecordFilterInfo>{"field": "vector_type",
                "operator": DP_SEARCH_OP_EQ, "option": "vector_type", "exact": True}),
        };

        #! Namespaces on the selected index's host
        const Namespaces = <RestSchemaRecordTableInfo>{
            "name": "namespaces", "display_name": "Namespaces", "short_desc": "Pinecone index namespaces",
            "desc": "Manage partitions of the selected index. Deleting a namespace removes all records in it.",
            "list_action": "list-namespaces", "get_action": "describe-namespace",
            "create_action": "create-namespace", "delete_action": "delete-namespace",
            "id_field": "name", "id_option": "namespace", "records_path": "namespaces",
            "scope_options": "index", "search_options": "prefix", "paging": DataPaging,
        };

        #! Bulk import jobs on the selected index
        const Imports = <RestSchemaRecordTableInfo>{
            "name": "imports", "display_name": "Imports", "short_desc": "Pinecone bulk imports",
            "desc": "Start and inspect imports from object storage. Deleting an import record cancels its job; "
                "it does not undo records already imported.",
            "list_action": "list-imports", "get_action": "describe-import", "create_action": "start-import",
            "delete_action": "cancel-import", "records_path": "data", "scope_options": "index", "paging": DataPaging,
        };

        #! Complete vector records, with open user metadata
        const Vectors = <RestSchemaRecordTableInfo>{
            "name": "vectors", "display_name": "Vectors", "short_desc": "Pinecone vectors and metadata",
            "desc": "Read and write complete vectors in one index and namespace. Filter user metadata with "
                "paths such as `metadata.category`. Creation upserts the supplied ID; updates merge metadata.",
            "list_action": "list-vector-ids", "get_action": "fetch-vectors", "create_action": "upsert-vectors",
            "update_action": "update-vector", "delete_action": "delete-vectors",
            "records_path": "vectors", "record_path": "vectors", "record_is_map": True,
            "hydrate_list": True, "id_list_actions": ("fetch-vectors", "delete-vectors"),
            "action_id_options": {"fetch-vectors": "ids", "delete-vectors": "ids"},
            "scope_options": ("index", "namespace"), "paging": DataPaging,
            "search_option_overrides": {"namespace": {"required": False, "default_value": ""}},
            "write_list_options": {"upsert-vectors": "vectors"},
            "write_field_map": {"metadata": "setMetadata"}, "write_returns_record": False,
            "extra_columns": PineconeVectorFieldsType.getFields(),
            "check_write_identity": True,
            "query_listing": <RestSchemaRecordListingInfo>{"list_action": "fetch-vectors-by-metadata",
                "records_path": "vectors", "records_are_map": True, "paging": DataPaging},
            "query": <RestSchemaRecordQueryInfo>{"option": "filter", "syntax": Syntax,
                "can_render": bool sub (hash<DataProviderExpression> exp) {
                    return PineconeRecordTables::canRenderField(exp, "metadata.");
                }},
        };

        #! Document records retain arbitrary user fields in a typed open hash
        const Documents = <RestSchemaRecordTableInfo>{
            "name": "documents", "display_name": "Documents", "short_desc": "Pinecone documents as records",
            "desc": "Read and write complete documents in a document index. User fields live under `fields`, "
                "for example `fields.title`. Creation upserts the ID; updates patch only supplied fields.",
            "list_action": "list-documents", "get_action": "fetch-documents",
            "create_action": "upsert-documents", "update_action": "update-documents",
            "delete_action": "delete-documents", "id_field": "_id", "records_path": "documents",
            "record_path": "documents", "record_is_map": True, "hydrate_list": True,
            "id_list_actions": ("fetch-documents", "delete-documents"),
            "action_id_options": {"fetch-documents": "ids", "delete-documents": "ids"},
            "scope_options": ("index", "namespace"), "paging": DocumentPaging,
            "write_list_option": "documents", "write_returns_record": False, "check_write_identity": True,
            "to_record": \PineconeRecordTables::documentRecord(), "to_wire": \PineconeRecordTables::documentWire(),
            "query_listing": <RestSchemaRecordListingInfo>{"list_action": "fetch-documents",
                "records_path": "documents", "records_are_map": True, "paging": DocumentPaging},
            "query": <RestSchemaRecordQueryInfo>{"option": "filter", "syntax": Syntax,
                "can_render": bool sub (hash<DataProviderExpression> exp) {
                    return PineconeRecordTables::canRenderField(exp, "fields.");
                }},
            "extra_columns": PineconeDocumentFieldsType.getFields(),
        };

        #! Every table in presentation order
        const Tables = (Indexes, Collections, Backups, BackupSchedules, RestoreJobs, Models,
            Namespaces, Imports, Vectors, Documents);
    }

    #! Tests whether a predicate addresses a scalar user metadata field
    /** @param exp a predicate whose operands have passed the generic syntax checks
        @param prefix the table's container for user fields
        @return whether Pinecone can evaluate this predicate
    */
    static bool canRenderField(hash<DataProviderExpression> exp, string prefix) {
        # one key in the container: metadata.year, or metadata.release\.year for the key release.year
        list<string> path = AbstractDataProvider::getFieldPath(exp.args[0].field);
        if (path.size() != 2 || path[0] + "." != prefix || path[1] == "" || path[1].startsWith("$")) {
            return False;
        }
        if (inlist(exp.exp, (DP_OP_IS_NULL, DP_OP_IS_NOT_NULL))) {
            return True;
        }
        bool membership = inlist(exp.exp, (DP_SEARCH_OP_IN, DP_OP_NOT_IN));
        list<auto> values = membership
            ? RestSchemaRecordFilter::getInValues(exp) : (exp.args[1],);
        if (!values || (membership && values.size() > 10000)) {
            return False;
        }
        foreach auto v in (values) {
            if (!inlist(v.typeCode(), (NT_STRING, NT_INT, NT_FLOAT, NT_BOOLEAN))) {
                return False;
            }
            if (membership && (v.typeCode() == NT_BOOLEAN
                    || (v.typeCode() == NT_STRING) != (values[0].typeCode() == NT_STRING))) {
                return False;
            }
            if (inlist(exp.exp, (DP_SEARCH_OP_LT, DP_SEARCH_OP_LE, DP_SEARCH_OP_GT, DP_SEARCH_OP_GE))
                    && !inlist(v.typeCode(), (NT_INT, NT_FLOAT))) {
                return False;
            }
        }
        return True;
    }

    #! Renders a validated metadata predicate tree
    /** @param exp a predicate accepted by the table's filter declaration
        @return Pinecone's JSON metadata filter
        @throw REST-SCHEMA-RECORD-ERROR an unsupported operator was passed directly
    */
    static auto renderQuery(hash<DataProviderExpression> exp) {
        if (exp.exp == DP_OP_AND || exp.exp == DP_OP_OR) {
            return {exp.exp == DP_OP_AND ? "$and" : "$or":
                map PineconeRecordTables::renderQuery(cast<hash<DataProviderExpression>>($1)), exp.args};
        }
        *string op = Operators{exp.exp};
        if (!op.val()) {
            throw "REST-SCHEMA-RECORD-ERROR", sprintf("unsupported Pinecone operator %y", exp.exp);
        }
        # the key in the container (canRenderField())
        string name = AbstractDataProvider::getFieldPath(exp.args[0].field)[1];
        auto value;
        if (exp.exp == DP_OP_IS_NULL || exp.exp == DP_OP_IS_NOT_NULL) {
            value = exp.exp == DP_OP_IS_NOT_NULL;
        } else if (exp.exp == DP_SEARCH_OP_IN || exp.exp == DP_OP_NOT_IN) {
            value = RestSchemaRecordFilter::getInValues(exp);
        } else {
            value = exp.args[1];
        }
        return {name: {op: value}};
    }

    #! Wraps a fetched document's open fields
    /** @param rec a complete Pinecone document
        @return the document in the table's shape
    */
    static hash<auto> documentRecord(hash<auto> rec) {
        return {"_id": rec._id, "fields": rec - "_id"};
    }

    #! Flattens the document's open fields without allowing them to replace its identity
    /** @param rec the table record or partial update
        @return the document fields Pinecone accepts
        @throw REST-SCHEMA-RECORD-ERROR fields attempts to override the document identity
    */
    static hash<auto> documentWire(hash<auto> rec) {
        hash<auto> fields = rec.fields ?? {};
        if (fields.hasKey("_id")) {
            throw "REST-SCHEMA-RECORD-ERROR", "fields cannot contain the reserved document identity _id";
        }
        return fields + (exists rec._id ? {"_id": rec._id} : {});
    }
}

#! Sparse vector components carried by Pinecone records
public class PineconeSparseVectorDataType inherits HashDataType {
    private {
        const Fields = {
            "indices": {"display_name": "Indices", "desc": "The indices of nonzero vector components.",
                "type": AbstractDataProviderType::get(new Type("list<int>"))},
            "values": {"display_name": "Values", "desc": "The nonzero values, in the same order as `indices`.",
                "type": AbstractDataProviderType::get(new Type("list<float>"))},
        };
    }
    #! Creates the declared record field type
    constructor() {
        addQoreFields(Fields);
    }
}
#! The sparse-vector field type
public const PineconeSparseVectorType = new PineconeSparseVectorDataType();

#! Vector columns that an identity-map response cannot describe as ordinary object properties
public class PineconeVectorFieldsDataType inherits HashDataType {
    private {
        const Fields = {
            "values": {"display_name": "Vector Values", "desc": "The dense vector components.",
                "type": AbstractDataProviderType::get(new Type("*list<float>")), "example_value": (0.1, 0.2)},
            "sparseValues": {"display_name": "Sparse Values", "desc": "Sparse vector indices and values.",
                "type": PineconeSparseVectorType.getOrNothingType()},
            "metadata": {"display_name": "Metadata", "desc": "User metadata for filtering, such as "
                "`metadata.category` or `metadata.year`.", "type": AutoHashOrNothingType,
                "example_value": {"category": "finance", "year": 2026}},
        };
    }
    #! Creates the declared record field type
    constructor() {
        addQoreFields(Fields);
    }
}
#! The additional vector columns
public const PineconeVectorFieldsType = new PineconeVectorFieldsDataType();

#! The document's open field container, shared by read and write records
public class PineconeDocumentFieldsDataType inherits HashDataType {
    public {
        const Fields = {
            "fields": {"display_name": "Document Fields", "short_desc": "User-defined document fields",
                "desc": "The document's schema fields and metadata, keyed by field name. "
                    "Use paths such as `fields.title` in searches.",
                "type": AutoHashOrNothingType, "example_value": {"title": "Quarterly report", "category": "finance"}},
        };
    }
    #! Creates the declared record field type
    constructor() {
        addQoreFields(Fields);
    }
}
#! The shared type for the document's additional column
public const PineconeDocumentFieldsType = new PineconeDocumentFieldsDataType();
}
