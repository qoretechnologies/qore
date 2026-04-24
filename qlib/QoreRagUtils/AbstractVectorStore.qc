# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreRagUtils module — AbstractVectorStore definition

/** AbstractVectorStore.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Abstract base class for vector stores (Qdrant, pgvector, etc.)
/** Concrete subclasses wrap a specific backend and translate calls into
    upserts and similarity searches.

    Implementations must be thread-safe.
*/
public class AbstractVectorStore {
    #! Upserts a batch of vector records
    /** @param records the records to insert or update (keyed by \c id)
    */
    abstract upsert(list<hash<VectorRecord>> records);

    #! Deletes records by id
    /** @param ids the record ids to remove
    */
    abstract deleteByIds(list<string> ids);

    #! Searches for the K most similar vectors to a query vector
    /** @param query_vector the query embedding
        @param k how many results to return
        @param filter optional backend-specific filter (e.g. payload predicates)

        @return list of @ref VectorSearchResult hashes ordered by descending similarity
    */
    abstract list<hash<VectorSearchResult>> search(list<float> query_vector,
        int k, *hash<auto> filter);

    #! Returns the configured collection / table name
    abstract string getCollection();

    #! Returns the vector dimension this store expects
    abstract int getDimension();
}
}
