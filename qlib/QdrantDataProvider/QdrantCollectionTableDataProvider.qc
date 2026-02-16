# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QdrantCollectionTableDataProvider class definition

/** QdrantCollectionTableDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Per-collection data provider implementing record-based CRUD and vector search
/** Exposes a Qdrant collection as a record-based data provider with support for:
    - \b Search (DPAT_FIND): vector similarity search with filtering and pagination
    - \b Create/Upsert: insert or replace points with vectors and payloads
    - \b Update: modify payload and/or vector data on existing points
    - \b Delete: remove points by ID
    - \b Bulk create/upsert: batch point upserts for high-throughput ingestion

    Use the \c filter search option to pass Qdrant filter conditions (\c must, \c should, \c must_not clauses).

    The \c where_cond for update and delete operations must contain the \c id field to identify the point(s) to
    modify.

    @note Corresponds to: POST /collections/{name}/points/query (search),
    PUT /collections/{name}/points (create/upsert),
    POST /collections/{name}/points/payload (update payload),
    PUT /collections/{name}/points/vectors (update vectors),
    POST /collections/{name}/points/delete (delete)
*/
public class QdrantCollectionTableDataProvider inherits QdrantDataProviderBase {
    public {
        #! Write operation query parameter names
        const WriteQueryArgs = ("wait", "ordering");

        #! Write operation options (shared by create, upsert, update, delete)
        const WriteOptions = {
            "wait": <DataProviderOptionInfo>{
                "display_name": "Wait",
                "short_desc": "Wait for operation to complete",
                "type": AbstractDataProviderTypeMap."*softbool",
                "desc": "If true, wait for changes to actually happen; if false (default), the request "
                    "returns immediately after acknowledging",
            },
            "ordering": <DataProviderOptionInfo>{
                "display_name": "Ordering",
                "short_desc": "Write ordering guarantees",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "The write ordering guarantees for the operation",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "weak", "desc": "Weak ordering; fastest, no guarantees"},
                    <AllowedValueInfo>{"value": "medium", "desc": "Medium ordering; wait for local persistence"},
                    <AllowedValueInfo>{"value": "strong", "desc": "Strong ordering; wait for all replicas"},
                ),
            },
        };

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "desc": "Qdrant collection table data provider; provides record-based CRUD operations on a "
                "Qdrant collection including vector similarity search, point create/upsert/update/delete, "
                "and bulk operations. Use the `filter` search option to pass Qdrant filter conditions",
            "type": "QdrantCollectionTableDataProvider",
            "constructor_options": QdrantDataProvider::ConstructorOptions,
            "has_record": True,
            "supports_read": True,
            "supports_native_search": True,
            "supports_create": True,
            "supports_upsert": True,
            "supports_update": True,
            "supports_delete": True,
            "supports_bulk_create": True,
            "supports_bulk_upsert": True,
            "create_options": WriteOptions,
            "upsert_options": WriteOptions,
            "search_options": GenericRecordSearchOptions{"columns", "limit", "offset", "requires_result"} + {
                "query": <DataProviderOptionInfo>{
                    "display_name": "Query",
                    "short_desc": "Query vector for similarity search",
                    "type": AbstractDataProviderTypeMap."*softlist",
                    "desc": "The query vector to use for similarity search",
                    "example_value": (0.2, 0.1, 0.9, 0.7),
                },
                "using": <DataProviderOptionInfo>{
                    "display_name": "Using",
                    "short_desc": "Named vector to query against",
                    "type": AbstractDataProviderTypeMap."*string",
                    "desc": "The name of the named vector to use for the search; if not specified, the default "
                        "vector is used",
                },
                "filter": <DataProviderOptionInfo>{
                    "display_name": "Filter",
                    "short_desc": "Qdrant filter conditions",
                    "type": AbstractDataProviderTypeMap."*hash",
                    "desc": "Qdrant filter conditions with `must`, `should`, and `must_not` clauses",
                },
                "score_threshold": <DataProviderOptionInfo>{
                    "display_name": "Score Threshold",
                    "short_desc": "Minimum score cutoff",
                    "type": AbstractDataProviderTypeMap."*float",
                    "desc": "The minimum score threshold; points with a score below this value will be excluded",
                },
                "prefetch": <DataProviderOptionInfo>{
                    "display_name": "Prefetch",
                    "short_desc": "Prefetch config for hybrid/multi-stage search",
                    "type": AbstractDataProviderTypeMap."auto",
                    "desc": "Prefetch queries for hybrid search; can be a list of query objects or a single query "
                        "object for multi-stage search pipelines",
                },
                "params": <DataProviderOptionInfo>{
                    "display_name": "Params",
                    "short_desc": "Search engine params",
                    "type": AbstractDataProviderTypeMap."*hash",
                    "desc": "Additional search parameters such as `hnsw_ef`, `exact`, `quantization`, and other "
                        "engine-specific options",
                },
                "with_payload": <DataProviderOptionInfo>{
                    "display_name": "With Payload",
                    "short_desc": "Include payload in results",
                    "type": AbstractDataProviderTypeMap."*softbool",
                    "desc": "Whether to return payload data with results; default: True",
                },
                "with_vector": <DataProviderOptionInfo>{
                    "display_name": "With Vector",
                    "short_desc": "Include vectors in results",
                    "type": AbstractDataProviderTypeMap."*softbool",
                    "desc": "Whether to return vectors with results",
                },
                "lookup_from": <DataProviderOptionInfo>{
                    "display_name": "Lookup From",
                    "short_desc": "Lookup vectors from another collection",
                    "type": AbstractDataProviderTypeMap."*hash",
                    "desc": "Configuration to look up vectors from another collection for the search",
                },
            } + WriteOptions,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Record type
        const RecordType = new QdrantScoredPointRecordType();
    }

    private {
        #! The collection name
        string collection_name;
    }

    #! Creates the object from a REST connection and collection name
    constructor(RestClient::RestClient rest, string collection_name) : QdrantDataProviderBase(rest) {
        self.collection_name = collection_name;
    }

    #! Returns the data provider name
    string getName() {
        return collection_name;
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("Qdrant collection %y table data provider for `%s`", collection_name, rest.getSafeURL());
    }

    #! Returns an iterator for zero or more records matching the search options
    /** @param where_cond the search criteria
        @param search_options the search options after processing by validateSearchOptions()
    */
    private DataProvider::AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        return new QdrantSearchRecordIterator(rest, collection_name, where_cond, search_options);
    }

    #! Creates a new point in the collection
    /** @param rec the record to create; must contain `id` and `vector`, optionally `payload`
        @param create_options the create options after processing by validateCreateOptions()

        @return the record as created
    */
    private *hash<auto> createRecordImpl(hash<auto> rec, *hash<auto> create_options) {
        hash<auto> point = {"id": rec.id, "vector": rec.vector};
        if (rec.payload) {
            point.payload = rec.payload;
        }
        string uri = buildWriteUri("/points", create_options);
        rest.put(uri, {"points": (point,)});
        return rec;
    }

    #! Upserts a point in the collection (same as create for Qdrant)
    /** @param rec the record to upsert; must contain `id` and `vector`, optionally `payload`
        @param upsert_options the upsert options after processing by validateUpsertOptions()

        @return the upsert result string
    */
    private string upsertRecordImpl(hash<auto> rec, *hash<auto> upsert_options) {
        createRecordImpl(rec, upsert_options);
        return UpsertResultInserted;
    }

    #! Updates one or more points in the collection
    /** @param set the hash of field data to set; supports `payload` (hash) and/or `vector` (list).
        When both are present, a single batch request is used to update both atomically.
        @param where_cond a hash identifying the point(s) to update; must contain `id`
        @param search_options the search options after processing by validateSearchOptions()

        @return the number of records updated
    */
    private int updateRecordsImpl(hash<auto> set, *hash<auto> where_cond, *hash<auto> search_options) {
        list<auto> id_list = getPointIds(where_cond, "update");

        *hash<auto> payload_data = set.payload;
        *auto vector_data = set.vector;

        if (!exists vector_data && !payload_data) {
            throw "QDRANT-UPDATE-ERROR", sprintf("nothing to update in collection %y; 'set' must contain "
                "'payload' and/or 'vector' fields", collection_name);
        }

        if (exists vector_data && payload_data) {
            # Use batch API to update both vector and payload in one request
            list<hash<auto>> vector_points;
            foreach auto id in (id_list) {
                push vector_points, {"id": id, "vector": vector_data};
            }
            list<hash<auto>> operations = (
                {"update_vectors": {"points": vector_points}},
                {"set_payload": {"payload": payload_data, "points": id_list}},
            );
            string uri = buildWriteUri("/points/batch", search_options);
            rest.post(uri, {"operations": operations});
        } else if (exists vector_data) {
            list<hash<auto>> vector_points;
            foreach auto id in (id_list) {
                push vector_points, {"id": id, "vector": vector_data};
            }
            string uri = buildWriteUri("/points/vectors", search_options);
            rest.put(uri, {"points": vector_points});
        } else {
            string uri = buildWriteUri("/points/payload", search_options);
            rest.post(uri, {"payload": payload_data, "points": id_list});
        }

        return id_list.size();
    }

    #! Deletes one or more points from the collection
    /** @param where_cond a hash identifying the point(s) to delete; must contain `id`
        @param search_options the search options after processing by validateSearchOptions()

        @return the number of records deleted
    */
    private int deleteRecordsImpl(*hash<auto> where_cond, *hash<auto> search_options) {
        list<auto> id_list = getPointIds(where_cond, "delete");
        string uri = buildWriteUri("/points/delete", search_options);
        rest.post(uri, {"points": id_list});
        return id_list.size();
    }

    #! Returns a bulk insert operation object for this data provider
    /** @param block_size the number of records in a block

        @return a bulk insert operation object for this data provider
    */
    DataProvider::AbstractDataProviderBulkOperation getBulkInserter(*int block_size) {
        return new QdrantCollectionBulkOperation(self, rest, collection_name, block_size);
    }

    #! Returns a bulk upsert operation object for this data provider
    /** @param block_size the number of records in a block

        @return a bulk upsert operation object for this data provider

        @note In Qdrant, the upsert endpoint has insert-or-replace semantics, so both bulk insert
        and bulk upsert use the same operation
    */
    DataProvider::AbstractDataProviderBulkOperation getBulkUpserter(*int block_size) {
        return new QdrantCollectionBulkOperation(self, rest, collection_name, block_size);
    }

    #! Returns the record type
    private *hash<string, DataProvider::AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return RecordType.getFields();
    }

    #! Returns data provider static info
    hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Extracts and validates point ID(s) from where_cond
    /** @param where_cond the where condition; must contain `id`
        @param operation the operation name for error messages

        @return a list of point IDs
    */
    private list<auto> getPointIds(*hash<auto> where_cond, string operation) {
        if (!where_cond || !exists where_cond.id) {
            throw "QDRANT-" + operation.upr() + "-ERROR", sprintf("missing 'id' field in where_cond to identify "
                "point(s) to %s in collection %y", operation, collection_name);
        }
        if (where_cond.size() > 1) {
            throw "QDRANT-" + operation.upr() + "-ERROR", sprintf("only 'id' field is supported in where_cond "
                "for %s in collection %y; got fields: %y", operation, collection_name, keys where_cond);
        }
        return where_cond.id.typeCode() == NT_LIST ? where_cond.id : (where_cond.id,);
    }

    #! Builds a collection URI with write query parameters (wait, ordering)
    /** @param suffix the path suffix after the collection path
        @param options the options hash (create_options, upsert_options, or search_options)

        @return the complete URI path with query parameters
    */
    private string buildWriteUri(string suffix, *hash<auto> options) {
        string uri = collectionPath(collection_name, suffix);
        list<string> params;
        if (exists options.wait) {
            push params, "wait=" + QdrantDataProviderBase::formatQueryValue(boolean(options.wait));
        }
        if (options.ordering) {
            push params, "ordering=" + options.ordering;
        }
        if (params) {
            uri += "?" + params.join("&");
        }
        return uri;
    }
}
}
