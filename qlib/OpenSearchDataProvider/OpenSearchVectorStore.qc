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
    @ref OpenSearchDataProvider module's init block.
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
