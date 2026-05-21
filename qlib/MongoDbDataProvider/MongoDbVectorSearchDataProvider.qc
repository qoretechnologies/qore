# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MongoDbVectorSearchDataProvider class definition

/** MongoDbVectorSearchDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

%modern

public namespace MongoDbDataProvider {
#! MongoDB Atlas Vector Search data provider — runs a `$vectorSearch` pipeline
/** Wraps a MongoDB collection's aggregation pipeline with a single
    [`$vectorSearch`](https://www.mongodb.com/docs/atlas/atlas-vector-search/vector-search-stage/)
    stage targeting a pre-created Atlas Vector Search index.

    This is a thin specialization of @ref MongoDbAggregateDataProvider — it
    just constructs the `$vectorSearch` stage from named fields, then runs
    it through the same `MongoCollection::aggregate()` path.

    @par Index setup

    The target collection must have an Atlas Vector Search index created
    out-of-band (via Atlas UI, `mongosh`, or the Atlas Admin API).  The
    index name is passed via the `index_name` request field.

    @par Example:
    @code{.py}
hash<auto> req = {
    "index_name": "embeddings_vector_idx",
    "path": "embedding",
    "query_vector": (0.1, 0.2, 0.3, ...),
    "num_candidates": 100,
    "limit": 10,
};
hash<auto> result = provider.doRequest(req);
foreach hash<auto> doc in (result.results) {
    printf("%s (score=%.3f)\n", doc._id, doc.score);
}
    @endcode
*/
public class MongoDbVectorSearchDataProvider inherits DataProvider::AbstractDataProvider {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "type": "MongoDbVectorSearchDataProvider",
            "supports_request": True,
            "supports_children": False,
            "constructor_options": NOTHING,
        };

        const RequestType = new MongoDbVectorSearchRequestDataType();
        const ResponseType = new MongoDbVectorSearchResponseDataType();

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    private {
        MongoClient client;
        MongoDatabase db;
        MongoCollection coll;
    }

    constructor(MongoClient client, MongoDatabase db, MongoCollection coll) {
        self.client = client;
        self.db = db;
        self.coll = coll;
    }

    string getName() {
        return "vector-search";
    }

    *string getDesc() {
        return "MongoDB Atlas Vector Search — $vectorSearch aggregation stage";
    }

    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Runs a $vectorSearch aggregation and returns matching documents + scores
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        if (!req.index_name) {
            throw "MONGODB-VECTOR-SEARCH-ERROR", "index_name is required";
        }
        if (!req.path) {
            throw "MONGODB-VECTOR-SEARCH-ERROR", "path is required";
        }
        if (!req.query_vector) {
            throw "MONGODB-VECTOR-SEARCH-ERROR", "query_vector is required";
        }

        hash<auto> vs_stage = {
            "$vectorSearch": {
                "index":         req.index_name,
                "path":          req.path,
                "queryVector":   req.query_vector,
                "limit":         req.limit ?? 10,
                "numCandidates": req.num_candidates ?? ((req.limit ?? 10) * 10),
            },
        };
        if (exists req.filter) {
            vs_stage."$vectorSearch".filter = req.filter;
        }
        if (req.exact) {
            vs_stage."$vectorSearch".exact = True;
        }

        # Attach `$meta: "vectorSearchScore"` projection so the score field
        # surfaces in each result document
        list<auto> pipeline = (
            vs_stage,
            {"$addFields": {"score": {"$meta": "vectorSearchScore"}}},
        );

        MongoCursor cursor = coll.aggregate(pipeline);
        list<hash<auto>> docs = cursor.toList();

        return {
            "results": docs,
            "count": docs.size(),
        };
    }
}

#! Vector-search request data type
public class MongoDbVectorSearchRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "index_name": {
                "display_name": "Index Name",
                "short_desc": "Atlas Vector Search index name",
                "type": SoftStringType,
                "desc": "Name of the Atlas Vector Search index on this collection.  Must "
                    "be created out-of-band via the Atlas UI, `mongosh`, or the Atlas "
                    "Admin API.",
                "required": True,
                "example_value": "embeddings_vector_idx",
            },
            "path": {
                "display_name": "Path",
                "short_desc": "Document field path holding the embedding vector",
                "type": SoftStringType,
                "desc": "Document field path that holds the embedding vector.  Must match "
                    "the `path` field of the Atlas Vector Search index definition.",
                "required": True,
                "example_value": "embedding",
            },
            "query_vector": {
                "display_name": "Query Vector",
                "short_desc": "Query embedding vector",
                "type": SoftListType,
                "desc": "The query embedding vector (list of floats).",
                "required": True,
            },
            "limit": {
                "display_name": "Limit",
                "short_desc": "Maximum results to return",
                "type": SoftIntType,
                "desc": "Maximum number of nearest-neighbor results to return.  Default: 10.",
                "example_value": 10,
            },
            "num_candidates": {
                "display_name": "Number of Candidates",
                "short_desc": "Number of HNSW candidates to consider",
                "type": SoftIntType,
                "desc": "Number of nearest-neighbor candidates the HNSW index should consider "
                    "before applying any filter and returning `limit` results.  Higher values "
                    "improve recall at the cost of latency.  Atlas recommends 10x-20x `limit`. "
                    "Default: `limit * 10`.",
            },
            "filter": {
                "display_name": "Filter",
                "short_desc": "Pre-filter applied before vector search (MQL filter)",
                "type": AutoHashType,
                "desc": "Optional MongoDB query filter applied BEFORE the vector search "
                    "stage.  Must reference fields indexed as `filter` type in the Atlas "
                    "Vector Search index definition.",
            },
            "exact": {
                "display_name": "Exact",
                "short_desc": "Use exact nearest-neighbor search (ENN) instead of HNSW",
                "type": SoftBoolType,
                "desc": "When `True`, performs exact nearest-neighbor search (no HNSW "
                    "approximation).  Much slower; useful for benchmarking recall.",
            },
        };
    }

    constructor() : HashDataType("MongoDbVectorSearchRequest", Fields) {
    }
}

#! Vector-search response data type
public class MongoDbVectorSearchResponseDataType inherits HashDataType {
    public {
        const Fields = {
            "results": {
                "display_name": "Results",
                "short_desc": "Matching documents with `score` field appended",
                "type": SoftListType,
                "desc": "List of matching documents.  Each document carries an injected "
                    "`score` field with the Atlas vector-search relevance score.",
            },
            "count": {
                "display_name": "Count",
                "short_desc": "Number of results",
                "type": IntType,
                "desc": "Number of documents returned.",
            },
        };
    }

    constructor() : HashDataType("MongoDbVectorSearchResponse", Fields) {
    }
}
}
