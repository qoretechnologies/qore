# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreRagUtils module — AbstractEmbeddingCache definition

/** AbstractEmbeddingCache.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Abstract base class for query-embedding caches
/** A RagPipeline that is wired with an AbstractEmbeddingCache will consult
    the cache for the query vector before calling
    @ref AbstractEmbedder::encode() "AbstractEmbedder::encode()" and will
    populate the cache with every miss.  Cache keys are the raw query strings
    (or transformer outputs, when a \c query_transformer is configured —
    whichever string is about to be encoded).

    Implementations must be thread-safe; the pipeline may call
    @ref get() and @ref put() from multiple threads concurrently.
*/
public class AbstractEmbeddingCache {
    #! Looks up a cached embedding for the given key
    /** @param key the cache key (the exact string that would otherwise be
            passed to @ref AbstractEmbedder::encode() "encode()")

        @return the cached embedding, or @ref NOTHING if not present
    */
    abstract *list<float> get(string key);

    #! Stores an embedding under the given key
    /** @param key the cache key
        @param vector the embedding vector to cache
    */
    abstract put(string key, list<float> vector);

    #! Optional size hint — returns the current number of cached entries
    /** Default returns -1, meaning "unknown".

        @return the entry count or -1 if the implementation does not track it
    */
    int size() {
        return -1;
    }
}
}
