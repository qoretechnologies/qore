# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QdrantVectorStore class definition

/** QdrantVectorStore.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Concrete @ref QoreRagUtils::AbstractVectorStore "AbstractVectorStore" implementation backed by Qdrant
/** Wraps a @ref QdrantRestConnection (or any compatible \c RestClientIo) and
    translates @ref QoreRagUtils::AbstractVectorStore "AbstractVectorStore"
    calls into Qdrant REST API requests:

    | AbstractVectorStore call | Qdrant endpoint |
    |---|---|
    | @ref upsert() | <tt>PUT /collections/{collection}/points</tt> |
    | @ref deleteByIds() | <tt>POST /collections/{collection}/points/delete</tt> |
    | @ref search() | <tt>POST /collections/{collection}/points/query</tt> |

    @par Configuration

    Pass a @ref QoreRagUtils::VectorStoreConfig "VectorStoreConfig":
    - \c backend (required): must be \c "qdrant"
    - \c collection (required): the Qdrant collection name
    - \c url (optional): the Qdrant REST URL; one of \c url or
      \c options.restclient must be set
    - \c options.restclient (optional): a pre-built \c RestClientIo; takes
      precedence over \c url
    - \c options.restclient_options (optional): options forwarded to the
      \c RestClientIo constructor (e.g. \c headers for the \c api-key
      header)
    - \c options.dimension (optional): the vector dimension; when omitted,
      @ref getDimension() probes the collection metadata on first call

    @par Filter syntax

    The optional \c filter argument to @ref search() accepts either Qdrant's
    native filter dialect with \c must / \c should / \c must_not clauses or
    the provider-neutral \c payload_eq form:
    @code{.py}
{"payload_eq": {"group_namespace": 123, "index_id": "log-errors"}}
    @endcode

    When \c payload_eq is present, it is translated to Qdrant \c must /
    \c match.value predicates.  Native filters can still be passed directly
    or under \c native_filter and are merged with the portable predicates.

    @par Point IDs

    Qdrant accepts either unsigned 64-bit integers or UUID strings as point
    IDs.  @ref QoreRagUtils::VectorRecord "VectorRecord.id" is typed as
    string, so on upsert and delete this class converts all-digit strings to
    integer IDs on the wire (and converts integer IDs in search responses
    back to strings) so that callers can use either representation
    interchangeably.

    @par Thread safety

    All instance methods are thread-safe; the dimension probe is guarded
    by an internal lock so concurrent first-time @ref getDimension() calls
    cannot duplicate the probe request.
*/
public class QdrantVectorStore inherits QoreRagUtils::AbstractVectorStore {
    private {
        #! Captured configuration
        hash<QoreRagUtils::VectorStoreConfig> config;
        #! The Qdrant REST client (async)
        RestClientIo::RestClientIo rest;
        #! Cached vector dimension (lazily probed when not provided in options)
        int dim = -1;
        #! Guard for lazy dimension probe
        Mutex dim_mutex();
    }

    #! Creates the vector store
    /** @param config the @ref QoreRagUtils::VectorStoreConfig "VectorStoreConfig"

        @throw VECTOR-STORE-CONFIG-ERROR required configuration is missing
    */
    constructor(hash<QoreRagUtils::VectorStoreConfig> config) {
        if (!config.collection.val()) {
            throw "VECTOR-STORE-CONFIG-ERROR",
                "QdrantVectorStore requires 'collection' in VectorStoreConfig";
        }
        self.config = config;
        rest = QdrantVectorStore::makeRestClient(config);
        if (config.options.dimension.typeCode() == NT_INT && config.options.dimension > 0) {
            dim = config.options.dimension;
        }
    }

    #! Creates the vector store from a config + a pre-built REST client
    /** Provided so tests and other consumers can inject a custom
        \c RestClientIo (e.g. a fixture-driven subclass) without going
        through option parsing.

        @param config the @ref QoreRagUtils::VectorStoreConfig "VectorStoreConfig"
        @param rest the REST client to use
    */
    constructor(hash<QoreRagUtils::VectorStoreConfig> config, RestClientIo::RestClientIo rest) {
        if (!config.collection.val()) {
            throw "VECTOR-STORE-CONFIG-ERROR",
                "QdrantVectorStore requires 'collection' in VectorStoreConfig";
        }
        self.config = config;
        self.rest = rest;
        if (config.options.dimension.typeCode() == NT_INT && config.options.dimension > 0) {
            dim = config.options.dimension;
        }
    }

    #! Upserts a batch of vector records to the Qdrant collection
    /** Calls \c PUT \c /collections/{collection}/points with the points list.

        @param records the records to insert or update; each record's
        \c vector must have the dimension expected by the collection

        @throw QDRANT-VECTOR-STORE-ERROR a record's vector dimension does not
        match the configured / probed dimension (when the dimension is
        known)
    */
    upsert(list<hash<QoreRagUtils::VectorRecord>> records) {
        if (!records) {
            return;
        }
        list<hash<auto>> points = map QdrantVectorStore::pointToWire($1), records;
        if (dim > 0) {
            foreach hash<auto> p in (points) {
                if (p.vector.size() != dim) {
                    throw "QDRANT-VECTOR-STORE-ERROR",
                        sprintf("record id %y vector size %d does not match "
                            "collection dimension %d", p.id, p.vector.size(), dim);
                }
            }
        }
        rest.restPut(pointsPath(), {"points": points});
    }

    #! Deletes records from the Qdrant collection by ID
    /** Calls \c POST \c /collections/{collection}/points/delete.

        @param ids the record ids to remove
    */
    deleteByIds(list<string> ids) {
        if (!ids) {
            return;
        }
        list<auto> wire_ids = map QdrantVectorStore::idToWire($1), ids;
        rest.restPost(pointsPath("/delete"), {"points": wire_ids});
    }

    #! Searches for the K most similar vectors in the collection
    /** Calls \c POST \c /collections/{collection}/points/query.

        @param query_vector the query embedding
        @param k how many results to return
        @param filter optional Qdrant filter (must / should / must_not
        clauses); see the class doxygen for the dialect

        @return list of @ref QoreRagUtils::VectorSearchResult "VectorSearchResult"
        hashes ordered by descending similarity
    */
    list<hash<QoreRagUtils::VectorSearchResult>> search(list<float> query_vector,
            int k, *hash<auto> filter) {
        hash<auto> body = {
            "query": query_vector,
            "limit": k,
            "with_payload": True,
        };
        if (filter) {
            body.filter = QdrantVectorStore::normalizeSearchFilter(filter);
        }
        hash<auto> resp = rest.restPost(pointsPath("/query"), body).body.result;
        list<hash<auto>> raw_points = resp.points ?? ();
        return map QdrantVectorStore::pointFromWire($1), raw_points;
    }

    #! Returns the configured collection name
    string getCollection() {
        return config.collection;
    }

    #! Returns the vector dimension this store expects
    /** When the dimension is not provided in \c VectorStoreConfig.options.dimension,
        this method probes the collection metadata on first call and caches
        the result.

        @throw QDRANT-VECTOR-STORE-ERROR the collection does not exist or
        the dimension cannot be determined from the collection metadata
        (e.g. an unconfigured named-vectors collection)
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
        hash<auto> info;
        try {
            info = rest.restGet(sprintf("/collections/%s", encode_url(config.collection, True))).body.result;
        } catch (hash<ExceptionInfo> ex) {
            throw "QDRANT-VECTOR-STORE-ERROR",
                sprintf("cannot probe dimension for collection %y: %s",
                    config.collection, ex.desc), ex;
        }
        # Qdrant returns either {config.params.vectors.size} for an unnamed
        # default vector, or {config.params.vectors.{name: {size}, ...}} when
        # the collection uses named vectors.
        auto vectors = info.config.params.vectors;
        if (!vectors) {
            throw "QDRANT-VECTOR-STORE-ERROR",
                sprintf("cannot determine dimension for collection %y: no "
                    "vectors block in collection info", config.collection);
        }
        if (vectors.typeCode() == NT_HASH && exists vectors.size) {
            dim = vectors.size;
        } else if (vectors.typeCode() == NT_HASH) {
            # named vectors — pick the first
            *string first_name = (keys vectors)[0];
            if (first_name && vectors{first_name}.size) {
                dim = vectors{first_name}.size;
            }
        }
        if (dim <= 0) {
            throw "QDRANT-VECTOR-STORE-ERROR",
                sprintf("collection %y uses named vectors but the dimension "
                    "could not be determined; pass options.dimension "
                    "explicitly", config.collection);
        }
        return dim;
    }

    #! Builds a \c RestClientIo for the given config
    /** Honors \c options.restclient (passed through unchanged),
        \c options.restclient_options (forwarded to the constructor),
        and \c url.
    */
    private:internal static RestClientIo::RestClientIo makeRestClient(hash<QoreRagUtils::VectorStoreConfig> config) {
        if (config.options.restclient) {
            return config.options.restclient;
        }
        if (!config.url.val()) {
            throw "VECTOR-STORE-CONFIG-ERROR",
                "QdrantVectorStore requires either 'url' or "
                "'options.restclient' in VectorStoreConfig";
        }
        hash<auto> opts = config.options.restclient_options ?? {};
        opts.url = config.url;
        return new RestClientIo::RestClientIo(opts);
    }

    #! Returns the points endpoint for the configured collection with an optional suffix
    private string pointsPath(*string suffix) {
        return sprintf("/collections/%s/points%s",
            encode_url(config.collection, True), suffix ?? "");
    }

    #! Converts a @ref QoreRagUtils::VectorRecord "VectorRecord" to the Qdrant points-upsert wire shape
    private:internal static hash<auto> pointToWire(hash<QoreRagUtils::VectorRecord> rec) {
        hash<auto> p = {
            "id": QdrantVectorStore::idToWire(rec.id),
            "vector": rec.vector,
        };
        if (rec.payload) {
            p.payload = rec.payload;
        }
        return p;
    }

    #! Converts a string id to the Qdrant wire shape (int when all-digits, string otherwise)
    private:internal static auto idToWire(string id) {
        if (id =~ /^[0-9]+$/) {
            return int(id);
        }
        return id;
    }

    #! Converts a Qdrant scored point to a @ref QoreRagUtils::VectorSearchResult "VectorSearchResult"
    private:internal static hash<QoreRagUtils::VectorSearchResult> pointFromWire(hash<auto> p) {
        return <QoreRagUtils::VectorSearchResult>{
            "id": string(p.id),
            "score": p.score,
            "payload": p.payload,
        };
    }

    #! Converts provider-neutral payload filters to Qdrant filter syntax
    private:internal static hash<auto> normalizeSearchFilter(hash<auto> filter) {
        auto payload_eq = filter.payload_eq;
        if (!(payload_eq instanceof hash) || payload_eq.empty()) {
            return filter.native_filter ?? (exists filter{"payload_eq"} ? filter - ("payload_eq",) : filter);
        }

        hash<auto> qfilter = filter.native_filter ?? (filter - ("payload_eq",));
        list<auto> must = ();
        if (qfilter.must) {
            must = qfilter.must instanceof list ? qfilter.must : (qfilter.must,);
        }

        foreach string key in (keys payload_eq) {
            push must, {
                "key": key,
                "match": {"value": payload_eq{key}},
            };
        }
        qfilter.must = must;
        return qfilter;
    }
}
}
