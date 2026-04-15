# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreRagUtils module — LruEmbeddingCache definition

/** LruEmbeddingCache.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! A bounded, thread-safe LRU embedding cache
/** The default @ref AbstractEmbeddingCache implementation shipped with the
    module.  Evicts the least-recently-used entry when the configured
    capacity is exceeded.  A \c get() that hits moves its entry to the
    most-recently-used position.

    @par Example
    @code{.py}
LruEmbeddingCache cache(1024);
RagPipeline pipe(embedder, vector_store, reranker, chunker, 10, 50,
    <RagPipelineOptions>{"embedding_cache": cache});
    @endcode

    @since QoreRagUtils 1.0
*/
public class LruEmbeddingCache inherits AbstractEmbeddingCache {
    private {
        #! Maximum number of entries; 0 = unbounded
        int capacity;

        #! Cached vectors keyed by query string
        hash<string, list<float>> entries;

        #! Insertion-/access-ordered list of keys (head = least-recently-used,
        #! tail = most-recently-used)
        list<string> order = ();

        #! Guards \c entries and \c order for concurrent callers
        Mutex m();
    }

    #! Creates an LruEmbeddingCache with the given capacity
    /** @param capacity the maximum number of entries; pass 0 for an
            unbounded cache (not recommended for long-running services)

        @throw LRU-EMBEDDING-CACHE-ERROR if \a capacity is negative
    */
    constructor(int capacity = 1024) {
        if (capacity < 0) {
            throw "LRU-EMBEDDING-CACHE-ERROR",
                sprintf("capacity must be >= 0; got %d", capacity);
        }
        self.capacity = capacity;
    }

    *list<float> get(string key) {
        m.lock();
        on_exit m.unlock();

        if (!entries.hasKey(key)) {
            return NOTHING;
        }
        moveToMru(key);
        return entries{key};
    }

    put(string key, list<float> vector) {
        m.lock();
        on_exit m.unlock();

        if (entries.hasKey(key)) {
            entries{key} = vector;
            moveToMru(key);
            return;
        }
        entries{key} = vector;
        push order, key;
        if (capacity > 0 && order.size() > capacity) {
            string victim = shift order;
            remove entries{victim};
        }
    }

    int size() {
        m.lock();
        on_exit m.unlock();
        return entries.size();
    }

    #! Returns the configured capacity (0 = unbounded)
    int getCapacity() {
        return capacity;
    }

    #! Empties the cache
    clear() {
        m.lock();
        on_exit m.unlock();
        entries = {};
        order = ();
    }

    private moveToMru(string key) {
        # Remove the key from its current position in \c order and push it
        # to the tail; linear in the order list, which is fine for typical
        # cache sizes (hundreds to low thousands).
        int idx = -1;
        for (int i = 0; i < order.size(); ++i) {
            if (order[i] == key) {
                idx = i;
                break;
            }
        }
        if (idx >= 0) {
            splice order, idx, 1;
        }
        push order, key;
    }
}
}
