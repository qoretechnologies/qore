# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MongoAtlasVectorStore class definition

/** MongoAtlasVectorStore.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace QoreRagUtils {
#! MongoDB Atlas Vector Search-backed vector store
/** Adapter that turns a MongoDB collection with an Atlas Vector Search
    index into a @ref QoreRagUtils::AbstractVectorStore "AbstractVectorStore"
    implementation usable from @ref QoreRagUtils::RagPipeline "RagPipeline".

    @par Configuration

    @ref VectorStoreConfig fields:

    - \c collection (required): the MongoDB collection name
    - \c url (required): the MongoDB connection URI (<tt>mongodb://...</tt> or
      <tt>mongodb+srv://...</tt>)
    - \c options.database (required): the MongoDB database name
    - \c options.index_name (required): the Atlas Vector Search index name
      on the collection
    - \c options.embedding_path (optional): document field path that holds
      the embedding vector; defaults to \c "embedding"
    - \c options.id_field (optional): document field used as the primary
      key on upsert/delete; defaults to \c "_id"
    - \c options.payload_field (optional): document field that holds the
      RAG payload; defaults to \c "payload"
    - \c options.dimension (optional): vector dimension; auto-probed on
      first call if omitted

    @par Atlas index setup (out-of-band)

    The target collection MUST have an Atlas Vector Search index created
    before this adapter is used.  Example via <tt>mongosh</tt>:

    @verbatim
db.embeddings.createSearchIndex(
    "embeddings_vector_idx",
    "vectorSearch",
    {fields: [{type: "vector", path: "embedding", numDimensions: 1536, similarity: "cosine"}]}
)
    @endverbatim
*/
public class MongoAtlasVectorStore inherits AbstractVectorStore {
    public {
        const Backend = "mongo-atlas";
    }

    private {
        hash<VectorStoreConfig> config;
        MongoClient client;
        MongoDatabase db;
        MongoCollection coll;
        string coll_name;
        string database_name;
        string index_name;
        string embedding_path;
        string id_field;
        string payload_field;
        *int cached_dimension;
        Mutex init_mutex();
    }

    constructor(hash<VectorStoreConfig> config) {
        self.config = config;
        if (!config.collection.val()) {
            throw "VECTOR-STORE-CONFIG-ERROR",
                "MongoAtlasVectorStore requires 'collection' (MongoDB collection name)";
        }
        if (!config.url.val()) {
            throw "VECTOR-STORE-CONFIG-ERROR",
                "MongoAtlasVectorStore requires 'url' (MongoDB connection URI)";
        }
        if (!config.options.database) {
            throw "VECTOR-STORE-CONFIG-ERROR",
                "MongoAtlasVectorStore requires 'options.database'";
        }
        if (!config.options.index_name) {
            throw "VECTOR-STORE-CONFIG-ERROR",
                "MongoAtlasVectorStore requires 'options.index_name' "
                "(Atlas Vector Search index name)";
        }
        coll_name = config.collection;
        database_name = config.options.database;
        index_name = config.options.index_name;
        embedding_path = config.options.embedding_path ?? "embedding";
        id_field = config.options.id_field ?? "_id";
        payload_field = config.options.payload_field ?? "payload";
        if (exists config.options.dimension) {
            cached_dimension = config.options.dimension;
        }
    }

    upsert(list<hash<VectorRecord>> records) {
        if (!records) {
            return;
        }
        ensureClient();
        foreach hash<VectorRecord> rec in (records) {
            hash<auto> filter = {(id_field): rec.id};
            hash<auto> doc = filter + {
                (embedding_path): rec.vector,
                (payload_field): rec.payload ?? {},
            };
            coll.updateOne(filter, {"$set": doc}, {"upsert": True});
            if (!exists cached_dimension && rec.vector) {
                cached_dimension = rec.vector.size();
            }
        }
    }

    deleteByIds(list<string> ids) {
        if (!ids) {
            return;
        }
        ensureClient();
        coll.deleteMany({(id_field): {"$in": ids}});
    }

    list<hash<VectorSearchResult>> search(list<float> query_vector, int k,
            *hash<auto> filter) {
        ensureClient();
        hash<auto> vs_stage = {
            "$vectorSearch": {
                "index":         index_name,
                "path":          embedding_path,
                "queryVector":   query_vector,
                "limit":         k,
                "numCandidates": k * 10,
            },
        };
        if (filter && filter.typeCode() == NT_HASH && exists filter.mql) {
            vs_stage."$vectorSearch".filter = filter.mql;
        }
        list<auto> pipeline = (
            vs_stage,
            {"$addFields": {"_vs_score": {"$meta": "vectorSearchScore"}}},
        );
        MongoCursor cursor = coll.aggregate(pipeline);
        list<hash<auto>> docs = cursor.toList();

        list<hash<VectorSearchResult>> result = ();
        foreach hash<auto> doc in (docs) {
            *hash<auto> payload = doc{payload_field};
            push result, <VectorSearchResult>{
                "id": (doc{id_field} ?? "").toString(),
                "score": (doc._vs_score ?? 0.0).toFloat(),
                "payload": payload,
            };
        }
        return result;
    }

    #! Counts the documents whose payload matches a filter
    /** Calls \c countDocuments() with the match document.

        @param filter optional filter: the portable \c payload_eq form (equality on top-level payload keys) or
            \c mql, a MongoDB match document

        @return the number of matching documents

        @throw MONGO-ATLAS-VECTOR-STORE-ERROR a \c payload_eq key is empty, starts with a dollar sign or has a dot
    */
    int countByFilter(*hash<auto> filter) {
        hash<auto> match = makeMatch(filter);
        ensureClient();
        return match ? coll.countDocuments(match) : coll.countDocuments();
    }

    #! Deletes the documents whose payload matches a filter
    /** Calls \c deleteMany() with the match document and returns the \c deletedCount it reports.

        @param filter the filter; see countByFilter()

        @return the number of documents deleted

        @throw VECTOR-STORE-FILTER-ERROR the filter is empty
    */
    int deleteByFilter(hash<auto> filter) {
        hash<auto> match = makeMatch(filter);
        AbstractVectorStore::requireFilterCriteria(match.size() > 0);
        ensureClient();
        return coll.deleteMany(match).deletedCount.toInt();
    }

    #! Summarizes the documents by the distinct values of a payload key
    /** Groups on the server with an aggregation: <tt>$match</tt>, then <tt>$group</tt> on the payload key with a
        <tt>$sum</tt> count and, when a timestamp key is given, a <tt>$max</tt> of it.

        @param field the top-level payload key to group by
        @param filter optional filter; see countByFilter()
        @param timestamp_field optional top-level payload key holding a date or an ISO 8601 string

        @return the groups ordered by descending count and then by value
    */
    list<hash<VectorFieldSummary>> summarizeBy(string field, *hash<auto> filter, *string timestamp_field) {
        list<hash<auto>> pipeline = buildSummaryPipeline(field, makeMatch(filter), timestamp_field);
        ensureClient();
        return MongoAtlasVectorStore::summaryFromGroups(coll.aggregate(pipeline).toList());
    }

    string getCollection() {
        return coll_name;
    }

    #! Returns the MongoDB match document for a filter; empty for no filter
    hash<auto> makeMatch(*hash<auto> filter) {
        if (!filter) {
            return {};
        }
        if (filter.mql instanceof hash && filter.mql) {
            return filter.mql;
        }
        hash<auto!> match = {};
        if (filter.payload_eq instanceof hash) {
            foreach string key in (keys filter.payload_eq) {
                match{payloadPath(key)} = filter.payload_eq{key};
            }
        }
        return match;
    }

    #! Returns the dotted document path of a top-level payload key
    /** @throw MONGO-ATLAS-VECTOR-STORE-ERROR the key is empty, starts with a dollar sign or has a dot */
    string payloadPath(string key) {
        if (!key.val() || key[0] == "$" || key.find(".") >= 0) {
            throw "MONGO-ATLAS-VECTOR-STORE-ERROR", sprintf("payload key %y must be a non-empty top-level key "
                "without a dot and not starting with a dollar sign", key);
        }
        return payload_field + "." + key;
    }

    #! Returns the aggregation pipeline that summarizes the matching documents
    list<hash<auto>> buildSummaryPipeline(string field, hash<auto> match, *string timestamp_field) {
        hash<auto!> group = {"_id": "$" + payloadPath(field), "count": {"$sum": 1}};
        if (timestamp_field) {
            group.last = {"$max": "$" + payloadPath(timestamp_field)};
        }
        list<hash<auto>> pipeline = ();
        if (match) {
            push pipeline, {"$match": match};
        }
        push pipeline, {"$group": group};
        return pipeline;
    }

    #! Converts the documents returned by the summary pipeline to ordered summary groups
    /** A document without the key groups under a null \c _id, which becomes a @ref nothing value. */
    static list<hash<VectorFieldSummary>> summaryFromGroups(list<hash<auto>> docs) {
        list<hash<VectorFieldSummary>> groups = ();
        foreach hash<auto> doc in (docs) {
            *date ts;
            if (doc.last instanceof date) {
                ts = doc.last;
            } else if (doc.last instanceof string) {
                try {
                    ts = date(doc.last);
                } catch () {
                }
            }
            push groups, <VectorFieldSummary>{
                "value": doc."_id" === NULL ? NOTHING : doc."_id",
                "count": doc.count.toInt(),
                "last_write": ts,
            };
        }
        return AbstractVectorStore::sortSummary(groups);
    }

    int getDimension() {
        if (exists cached_dimension) {
            return cached_dimension;
        }
        init_mutex.lock();
        on_exit init_mutex.unlock();
        if (exists cached_dimension) {
            return cached_dimension;
        }
        ensureClient();
        # probe a single document
        *hash<auto> doc = coll.findOne({});
        if (exists doc{embedding_path} && doc{embedding_path}.typeCode() == NT_LIST) {
            cached_dimension = doc{embedding_path}.size();
            return cached_dimension;
        }
        return -1;
    }

    private ensureClient() {
        if (!client) {
            init_mutex.lock();
            on_exit init_mutex.unlock();
            if (!client) {
                client = new MongoClient(config.url);
                db = client.getDatabase(database_name);
                coll = db.getCollection(coll_name);
            }
        }
    }
}

#! Factory for @ref MongoAtlasVectorStore
public class MongoAtlasVectorStoreFactory inherits AbstractVectorStoreFactory {
    string getType() {
        return "mongo-atlas";
    }

    string getDisplayName() {
        return "MongoDB Atlas Vector Search";
    }

    AbstractVectorStore create(hash<VectorStoreConfig> config) {
        return new MongoAtlasVectorStore(config);
    }
}
}
