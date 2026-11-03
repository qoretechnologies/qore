# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore OpenSearchDataProvider vector-store adapter

/** OpenSearchVectorStore.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Concrete @ref QoreRagUtils::AbstractVectorStore "AbstractVectorStore" implementation backed by OpenSearch
/** Wraps an OpenSearch REST client and translates
    @ref QoreRagUtils::AbstractVectorStore "AbstractVectorStore" calls into
    OpenSearch index, bulk, delete, and k-NN search requests.

    @par Configuration

    Pass a @ref QoreRagUtils::VectorStoreConfig "VectorStoreConfig":
    - \c backend (required for registry lookup): \c "opensearch"
    - \c collection (required): OpenSearch index name
    - \c url (optional): OpenSearch REST URL; one of \c url or
      \c options.restclient must be set
    - \c options.restclient (optional): a pre-built
      @ref RestClient::AbstractRestClient "AbstractRestClient"
    - \c options.restclient_options (optional): constructor options for a
      created REST client
    - \c options.vector_field (optional): \c knn_vector field name
      (default: \c "embedding")
    - \c options.dimension (optional): vector dimension; when omitted,
      @ref getDimension() probes the index mapping
    - \c options.refresh (optional): refresh query value for upsert/delete

    @par Filter Syntax

    The optional \c filter argument to @ref search() is passed to the
    OpenSearch \c knn query's \c filter field.  If the hash contains
    \c native_filter, that value is used instead, allowing callers to wrap
    backend-specific filters in a portable outer hash.
*/
public class OpenSearchVectorStore inherits QoreRagUtils::AbstractVectorStore {
    public {
        #! Documents fetched per scroll request by summarizeBy()
        const ScrollPageSize = 1000;

        #! How long OpenSearch keeps a scroll context between requests
        const ScrollKeepAlive = "1m";
    }

    private {
        #! Captured configuration
        hash<QoreRagUtils::VectorStoreConfig> config;
        #! OpenSearch REST client
        RestClient::AbstractRestClient rest;
        #! Cached vector dimension
        int dim = -1;
        #! Vector field name
        string vector_field = "embedding";
        #! Guard for lazy dimension probing
        Mutex dim_mutex();
    }

    #! Creates the vector store from config
    /** @param config provider-neutral vector-store configuration

        @throw VECTOR-STORE-CONFIG-ERROR required configuration is missing
    */
    constructor(hash<QoreRagUtils::VectorStoreConfig> config) {
        init(config, OpenSearchVectorStore::makeRestClient(config));
    }

    #! Creates the vector store from config plus a pre-built REST client
    /** @param config provider-neutral vector-store configuration
        @param rest REST client to use
    */
    constructor(hash<QoreRagUtils::VectorStoreConfig> config, RestClient::AbstractRestClient rest) {
        init(config, rest);
    }

    #! Upserts vector records into the OpenSearch index
    /** @param records vector records to insert or replace */
    upsert(list<hash<QoreRagUtils::VectorRecord>> records) {
        if (!records) {
            return;
        }

        int expected_dim = dim > 0 ? dim : -1;
        list<hash<auto>> operations = ();
        foreach hash<QoreRagUtils::VectorRecord> rec in (records) {
            if (expected_dim > 0 && rec.vector.size() != expected_dim) {
                throw "OPENSEARCH-VECTOR-STORE-ERROR",
                    sprintf("record id %y vector size %d does not match index dimension %d",
                        rec.id, rec.vector.size(), expected_dim);
            }

            hash<auto!> doc = cast<hash<auto!>>(rec.payload ?? {});
            doc{vector_field} = rec.vector;
            push operations, {
                "action": "index",
                "_index": config.collection,
                "_id": rec.id,
                "doc": doc,
            };
        }

        string uri = addRefreshQuery("_bulk");
        string body = SearchIndexDataProvider::SearchIndexBulkUtil::formatOperations(operations,
            "OPENSEARCH-VECTOR-STORE-ERROR");
        hash<auto> response = rest.restDoRawRequest("POST", uri, body, {"Content-Type": "application/x-ndjson"}).body;
        checkBulkResponse(response);
    }

    #! Deletes vector records by id
    /** @param ids record ids to delete */
    deleteByIds(list<string> ids) {
        if (!ids) {
            return;
        }

        list<hash<auto>> operations = ();
        foreach string id in (ids) {
            push operations, {
                "action": "delete",
                "_index": config.collection,
                "_id": id,
            };
        }

        string uri = addRefreshQuery("_bulk");
        string body = SearchIndexDataProvider::SearchIndexBulkUtil::formatOperations(operations,
            "OPENSEARCH-VECTOR-STORE-ERROR");
        hash<auto> response = rest.restDoRawRequest("POST", uri, body, {"Content-Type": "application/x-ndjson"}).body;
        checkBulkResponse(response);
    }

    #! Searches the OpenSearch vector index using a k-NN query
    /** @param query_vector query embedding
        @param k maximum result count
        @param filter optional OpenSearch filter or hash containing
            \c native_filter

        @return normalized vector search results
    */
    list<hash<QoreRagUtils::VectorSearchResult>> search(list<float> query_vector,
            int k, *hash<auto> filter) {
        hash<auto> knn = {
            "vector": query_vector,
            "k": k,
        };

        if (filter) {
            knn.filter = exists filter.native_filter ? filter.native_filter : filter;
        }

        if (config.options.method_parameters) {
            knn.method_parameters = config.options.method_parameters;
        }

        hash<auto> body = {
            "size": k,
            "query": {
                "knn": {
                    vector_field: knn,
                },
            },
        };

        if (config.options."_source") {
            body."_source" = config.options."_source";
        }

        hash<auto> response = rest.restPost(indexPath("_search"), body).body;
        list<hash<auto>> hits = response.hits.hits ?? ();
        return map OpenSearchVectorStore::hitFromWire($1, vector_field,
            boolean(config.options.include_vectors ?? False)), hits;
    }

    #! Counts the documents that match a payload filter
    /** Calls \c POST \c /{index}/_count.

        @param filter optional filter: the portable \c payload_eq form (each key is an exact \c term match, so
            string payload fields should be mapped as \c keyword), \c native_filter (an OpenSearch query), or an
            OpenSearch query

        @return the number of matching documents
    */
    int countByFilter(*hash<auto> filter) {
        return rest.restPost(indexPath("_count"), {"query": OpenSearchVectorStore::makeQuery(filter)})
            .body.count.toInt();
    }

    #! Deletes the documents that match a payload filter
    /** Calls \c POST \c /{index}/_delete_by_query, which reports the number of documents it removed.

        @param filter the filter; see countByFilter()

        @return the number of documents removed

        @throw VECTOR-STORE-FILTER-ERROR the filter is empty or would select every document
        @throw OPENSEARCH-VECTOR-STORE-ERROR the delete reported failures
    */
    int deleteByFilter(hash<auto> filter) {
        QoreRagUtils::AbstractVectorStore::requireFilterCriteria(OpenSearchVectorStore::hasCriteria(filter));
        string uri = indexPath("_delete_by_query?conflicts=proceed");
        if (exists config.options.refresh) {
            uri += sprintf("&refresh=%s", encode_url(string(config.options.refresh), True));
        }
        hash<auto> response = rest.restPost(uri, {"query": OpenSearchVectorStore::makeQuery(filter)}).body;
        if (response.failures) {
            throw "OPENSEARCH-VECTOR-STORE-ERROR", sprintf("delete by query failed: %y", response.failures);
        }
        return response.deleted.toInt();
    }

    #! Summarizes the documents by the distinct values of a payload field
    /** Pages through the matching documents with the scroll API, fetching only the requested payload fields, and
        groups them here, so that the result does not depend on how the index maps the fields.

        @param field the top-level payload field to group by
        @param filter optional filter; see countByFilter()
        @param timestamp_field optional top-level payload field holding a date or an ISO 8601 string

        @return the groups ordered by descending count and then by value
    */
    list<hash<QoreRagUtils::VectorFieldSummary>> summarizeBy(string field, *hash<auto> filter,
            *string timestamp_field) {
        hash<auto> body = {
            "size": ScrollPageSize,
            "query": OpenSearchVectorStore::makeQuery(filter),
            "_source": timestamp_field ? (field, timestamp_field) : (field,),
        };
        hash<string, hash<auto>> groups = {};
        hash<auto> response = rest.restPost(indexPath("_search?scroll=" + ScrollKeepAlive), body).body;
        *string scroll_id = response."_scroll_id";
        on_exit {
            if (scroll_id) {
                try {
                    rest.restDoRequest("DELETE", "_search/scroll", {"scroll_id": scroll_id});
                } catch () {
                }
            }
        }
        while (True) {
            list<hash<auto>> hits = response.hits.hits ?? ();
            if (!hits) {
                break;
            }
            foreach hash<auto> hit in (hits) {
                QoreRagUtils::AbstractVectorStore::addToSummary(\groups, hit."_source"{field},
                    timestamp_field ? hit."_source"{timestamp_field} : NOTHING);
            }
            response = rest.restPost("_search/scroll", {"scroll": ScrollKeepAlive, "scroll_id": scroll_id}).body;
            scroll_id = response."_scroll_id" ?? scroll_id;
        }
        return QoreRagUtils::AbstractVectorStore::finishSummary(groups);
    }

    #! Returns the configured index name
    string getCollection() {
        return config.collection;
    }

    #! Returns the vector dimension this store expects
    /** When the dimension was not supplied as \c options.dimension, the value
        is probed from the index mapping and cached.

        @throw OPENSEARCH-VECTOR-STORE-ERROR the mapping cannot be read or the
            vector field has no dimension
    */
    int getDimension() {
        if (dim > 0) {
            return dim;
        }

        dim_mutex.lock();
        on_exit dim_mutex.unlock();
        if (dim > 0) {
            return dim;
        }

        hash<auto> mappings;
        try {
            mappings = rest.restGet(indexPath("_mapping")).body;
        } catch (hash<ExceptionInfo> ex) {
            throw "OPENSEARCH-VECTOR-STORE-ERROR",
                sprintf("cannot probe dimension for OpenSearch index %y: %s",
                    config.collection, ex.desc), ex;
        }

        hash<auto> index_mapping = mappings{config.collection} ?? mappings.firstValue() ?? {};
        auto field = index_mapping.mappings.properties{vector_field};
        if (!field || !field.dimension) {
            throw "OPENSEARCH-VECTOR-STORE-ERROR",
                sprintf("cannot determine dimension for OpenSearch index %y field %y",
                    config.collection, vector_field);
        }

        dim = int(field.dimension);
        return dim;
    }

    private init(hash<QoreRagUtils::VectorStoreConfig> config, RestClient::AbstractRestClient rest) {
        if (!config.collection.val()) {
            throw "VECTOR-STORE-CONFIG-ERROR",
                "OpenSearchVectorStore requires 'collection' in VectorStoreConfig";
        }

        self.config = config;
        self.rest = rest;

        if (config.options.vector_field) {
            vector_field = string(config.options.vector_field);
        }

        if (config.options.dimension.typeCode() == NT_INT && config.options.dimension > 0) {
            dim = config.options.dimension;
        }
    }

    private static RestClient::AbstractRestClient makeRestClient(hash<QoreRagUtils::VectorStoreConfig> config) {
        if (config.options.restclient) {
            return config.options.restclient;
        }
        if (!config.url.val()) {
            throw "VECTOR-STORE-CONFIG-ERROR",
                "OpenSearchVectorStore requires either 'url' or 'options.restclient' in VectorStoreConfig";
        }
        hash<auto> opts = config.options.restclient_options ?? {};
        opts.url = config.url;
        return new RestClientIo::RestClientIo(opts);
    }

    private string indexPath(string suffix) {
        return sprintf("%s/%s", encode_url(config.collection, True), suffix);
    }

    private string addRefreshQuery(string uri) {
        if (exists config.options.refresh) {
            return sprintf("%s?refresh=%s", uri, encode_url(string(config.options.refresh), True));
        }
        return uri;
    }

    #! Returns the OpenSearch query for a filter: \c match_all, \c native_filter, \c payload_eq or a native query
    private static hash<auto> makeQuery(*hash<auto> filter) {
        if (!filter) {
            return {"match_all": {}};
        }
        if (exists filter.native_filter) {
            return filter.native_filter;
        }
        if (exists filter{"payload_eq"}) {
            if (!(filter.payload_eq instanceof hash) || filter.payload_eq.empty()) {
                return {"match_all": {}};
            }
            list<hash<auto>> terms = ();
            foreach string key in (keys filter.payload_eq) {
                push terms, {"term": {key: filter.payload_eq{key}}};
            }
            return {"bool": {"filter": terms}};
        }
        return filter;
    }

    #! Returns True if the filter constrains the documents
    /** The query is built as it will be sent, so an explicit \c match_all (or an empty query) is no constraint. */
    private static bool hasCriteria(hash<auto> filter) {
        hash<auto> query = OpenSearchVectorStore::makeQuery(filter);
        return query.size() > 0 && !(query.size() == 1 && exists query.match_all);
    }

    private static hash<QoreRagUtils::VectorSearchResult> hitFromWire(hash<auto> hit, string vector_field,
            bool include_vector) {
        hash<auto> payload = hit."_source" ?? {};
        if (!include_vector) {
            remove payload{vector_field};
        }

        return <QoreRagUtils::VectorSearchResult>{
            "id": string(hit."_id"),
            "score": float(hit."_score" ?? 0.0),
            "payload": payload,
        };
    }

    private static checkBulkResponse(hash<auto> response) {
        if (!response.errors) {
            return;
        }

        foreach hash<auto> item in (response.items ?? ()) {
            hash<auto> result = item.index ?? item.create ?? item.update ?? item.delete ?? item.firstValue();
            if (result.error) {
                throw "OPENSEARCH-VECTOR-STORE-ERROR",
                    sprintf("bulk operation failed for document %y: %y",
                        result."_id", result.error);
            }
        }

        throw "OPENSEARCH-VECTOR-STORE-ERROR", "bulk operation failed";
    }
}

#! Factory that builds @ref OpenSearchVectorStore instances for the RAG provider registry
/** Registered under backend identifier \c "opensearch" by the
    @ref opensearchdataproviderintro "OpenSearchDataProvider" module's init block.
*/
public class OpenSearchVectorStoreFactory inherits QoreRagUtils::AbstractVectorStoreFactory {
    #! Returns the backend identifier
    string getType() {
        return "opensearch";
    }

    #! Returns the human-readable display name
    string getDisplayName() {
        return "OpenSearch vector store";
    }

    #! Creates an OpenSearch vector store from configuration
    QoreRagUtils::AbstractVectorStore create(hash<QoreRagUtils::VectorStoreConfig> config) {
        return new OpenSearchVectorStore(config);
    }
}
}
