# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore PgVectorRagProvider class definition

/*  PgVectorRagProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! pgvector-backed vector store — uses PostgreSQL with the pgvector extension
/** Wraps any @ref Qore::SQL::AbstractDatasource pointing at a PostgreSQL
    database with the [pgvector](https://github.com/pgvector/pgvector)
    extension installed.  Operates on a single table with at least three
    columns: `id` (text PK), `embedding` (`vector(N)`), and `payload`
    (`jsonb`).

    @par Configuration

    Pass a @ref VectorStoreConfig with:

    - \c collection (required) — the table name
    - \c options.datasource (required) — an already-constructed
      @ref Qore::SQL::AbstractDatasource bound to a PostgreSQL database
      with pgvector installed
    - \c options.dimension (optional) — vector dimension; auto-probed from
      the table's `vector_dims(embedding)` on first call if omitted
    - \c options.distance (optional) — distance operator: \c "cosine"
      (default, uses `<=>`), \c "l2" (uses `<->`), or \c "inner_product"
      (uses `<#>`)

    @par Filter syntax

    @ref search() accepts the provider-neutral \c payload_eq filter and
    translates it to a bound JSONB containment predicate:
    @code{.py}
{"payload_eq": {"group_namespace": 123, "index_id": "log-errors"}}
    @endcode

    Advanced callers can still pass raw SQL with \c where_sql and
    \c where_args; portable and raw predicates are combined with \c AND.

    @par Example:
    @code{.py}
%requires Pgsql
%requires QoreRagUtils

Datasource ds("pgsql:user/pass@dbname%localhost");
PgVectorRagProvider store(<VectorStoreConfig>{
    "backend": "pgvector",
    "collection": "embeddings",
    "options": {"datasource": ds, "dimension": 1536},
});
store.upsert((
    <VectorRecord>{"id": "doc1", "vector": (...), "payload": {"title": "..."}},
));
list<hash<VectorSearchResult>> hits = store.search(query_vec, 10);
    @endcode

    @par Schema bootstrap

    This class does NOT create the table or extension — those are
    operator concerns.  The table must already exist with a schema like:

    @verbatim
CREATE EXTENSION IF NOT EXISTS vector;
CREATE TABLE embeddings (
    id        text PRIMARY KEY,
    embedding vector(1536) NOT NULL,
    payload   jsonb
);
CREATE INDEX ON embeddings USING hnsw (embedding vector_cosine_ops);
    @endverbatim

    The @ref qlib/PgsqlSqlUtilBase.qm "PgsqlSqlUtilBase" module's
    `vector`, `halfvec`, `sparsevec` column types and the `PgsqlIndex`
    HNSW/IVFFlat method support are available for programmatic schema
    management.
*/
public class PgVectorRagProvider inherits AbstractVectorStore {
    public {
        #! Supported distance operators
        const DistanceOps = {
            "cosine":        "<=>",
            "l2":            "<->",
            "inner_product": "<#>",
        };
    }

    private {
        hash<VectorStoreConfig> config;
        Qore::SQL::AbstractDatasource ds;
        string table_name;
        string distance;
        string distance_op;
        *int cached_dimension;
        Mutex dim_mutex();
    }

    constructor(hash<VectorStoreConfig> config) {
        self.config = config;

        if (!config.collection.val()) {
            throw "VECTOR-STORE-CONFIG-ERROR",
                "PgVectorRagProvider requires 'collection' (table name) in VectorStoreConfig";
        }
        table_name = config.collection;

        # Require a pre-constructed Datasource; opening one here would
        # need a hard SqlUtil dependency that QoreRagUtils does not carry
        if (!config.options.datasource) {
            throw "VECTOR-STORE-CONFIG-ERROR",
                "PgVectorRagProvider requires 'options.datasource' "
                "(an AbstractDatasource bound to a PostgreSQL database "
                "with pgvector installed)";
        }
        ds = config.options.datasource;

        distance = config.options.distance ?? "cosine";
        *string op = DistanceOps{distance};
        if (!op) {
            throw "VECTOR-STORE-CONFIG-ERROR",
                sprintf("PgVectorRagProvider 'options.distance' must be one of %y, got %y",
                    keys DistanceOps, distance);
        }
        distance_op = op;

        if (exists config.options.dimension) {
            cached_dimension = config.options.dimension;
        }
    }

    upsert(list<hash<VectorRecord>> records) {
        if (!records) {
            return;
        }
        # ON CONFLICT (id) DO UPDATE for each record; pgvector vector literal
        # is '[a,b,c]'::vector
        string upsert_sql = sprintf(
            "INSERT INTO %s (id, embedding, payload) VALUES (%%v, %%v::vector, %%v::jsonb) "
            "ON CONFLICT (id) DO UPDATE SET "
            "embedding = EXCLUDED.embedding, payload = EXCLUDED.payload",
            quoteIdent(table_name));

        on_error ds.rollback();
        on_success ds.commit();

        foreach hash<VectorRecord> rec in (records) {
            string vec_lit = formatVectorLiteral(rec.vector);
            string payload_json = rec.payload
                ? call_function_args("make_json", (rec.payload,))
                : "{}";
            ds.exec(upsert_sql, rec.id, vec_lit, payload_json);
            if (!exists cached_dimension) {
                cached_dimension = rec.vector.size();
            }
        }
    }

    deleteByIds(list<string> ids) {
        if (!ids) {
            return;
        }
        string sql = sprintf("DELETE FROM %s WHERE id = ANY(%%v::text[])",
            quoteIdent(table_name));
        on_error ds.rollback();
        on_success ds.commit();
        ds.exec(sql, ids);
    }

    list<hash<VectorSearchResult>> search(list<float> query_vector, int k,
            *hash<auto> filter) {
        string vec_lit = formatVectorLiteral(query_vector);
        # SELECT id, payload, embedding <distance_op> $1::vector AS dist
        # FROM <table> [WHERE filter] ORDER BY dist LIMIT $2
        string sql = sprintf(
            "SELECT id, payload, embedding %s %%v::vector AS dist FROM %s",
            distance_op, quoteIdent(table_name));
        list<auto> bind_args = (vec_lit,);
        if (filter) {
            list<string> where_clauses = ();
            if (filter.payload_eq && filter.payload_eq.typeCode() == NT_HASH && filter.payload_eq.size()) {
                push where_clauses, "payload @> %v::jsonb";
                push bind_args, call_function_args("make_json", (filter.payload_eq,));
            }
            if (exists filter.where_sql && filter.where_sql.typeCode() == NT_STRING) {
                # Pass-through raw WHERE fragment for users who want full SQL
                # filter expressivity; bind args appended to `where_args` list
                push where_clauses, "(" + filter.where_sql + ")";
                if (filter.where_args && filter.where_args.typeCode() == NT_LIST) {
                    bind_args += filter.where_args;
                }
            }
            if (where_clauses) {
                sql += " WHERE " + where_clauses.join(" AND ");
            }
        }
        sql += sprintf(" ORDER BY dist LIMIT %d", k);

        *hash<auto> rows = ds.vselect(sql, bind_args);
        list<hash<VectorSearchResult>> result = ();
        if (!rows) {
            return result;
        }
        int n = rows.id.size();
        for (int i = 0; i < n; ++i) {
            auto payload = rows.payload[i];
            *hash<auto> typed_payload;
            if (payload.typeCode() == NT_STRING) {
                try {
                    typed_payload = call_function_args("parse_json", (payload,));
                } catch (hash<ExceptionInfo> ex) {
                    typed_payload = {"raw": payload};
                }
            } else if (payload.typeCode() == NT_HASH) {
                typed_payload = payload;
            }
            push result, <VectorSearchResult>{
                "id": rows.id[i].toString(),
                "score": distanceToScore(rows.dist[i].toFloat()),
                "payload": typed_payload,
            };
        }
        return result;
    }

    string getCollection() {
        return table_name;
    }

    int getDimension() {
        if (exists cached_dimension) {
            return cached_dimension;
        }
        dim_mutex.lock();
        on_exit dim_mutex.unlock();
        if (exists cached_dimension) {
            return cached_dimension;
        }
        # probe a single row
        string sql = sprintf(
            "SELECT vector_dims(embedding) AS dim FROM %s LIMIT 1",
            quoteIdent(table_name));
        *hash<auto> row = ds.selectRow(sql);
        if (exists row.dim) {
            cached_dimension = row.dim.toInt();
            return cached_dimension;
        }
        return -1;
    }

    #! Returns the underlying datasource — useful for cleanup or schema operations
    Qore::SQL::AbstractDatasource getDatasource() {
        return ds;
    }

    #! Converts pgvector distance to a similarity-style score (higher = better)
    /** Cosine distance: 1 - dist; L2 distance: -dist (lower = closer);
        inner product: -dist (pgvector returns negative inner product so flipping
        gives the actual dot product).
    */
    private float distanceToScore(float dist) {
        switch (distance) {
            case "cosine":
                return 1.0 - dist;
            case "inner_product":
                return -dist;
            default:
                # l2 — return negative distance so larger = closer
                return -dist;
        }
    }

    #! Formats a vector as pgvector's '[a,b,c]' literal
    private string formatVectorLiteral(list<float> vec) {
        list<string> parts = ();
        foreach float v in (vec) {
            push parts, sprintf("%g", v);
        }
        return "[" + (foldl $1 + "," + $2, parts) + "]";
    }

    #! Wraps an identifier in double quotes, escaping any embedded quotes
    private string quoteIdent(string ident) {
        string esc = ident;
        esc =~ s/"/""/g;
        return "\"" + esc + "\"";
    }
}

#! Factory for @ref PgVectorRagProvider
public class PgVectorRagProviderFactory inherits AbstractVectorStoreFactory {
    string getType() {
        return "pgvector";
    }

    string getDisplayName() {
        return "PostgreSQL pgvector vector store";
    }

    AbstractVectorStore create(hash<VectorStoreConfig> config) {
        return new PgVectorRagProvider(config);
    }
}
}
