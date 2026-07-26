# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file JsonLdContextCache.qc @brief Thread-safe JSON-LD context cache

/*  JsonLdContextCache.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! the %JsonLd namespace holds all public definitions in the %JsonLd module
public namespace JsonLd {

#! Thread-safe JSON-LD context cache with TTL
/** Provides a thread-safe cache for parsed JSON-LD contexts fetched from remote URLs.
    Supports TTL-based expiration and maximum entry limits.

    @since json 1.12
*/
public class JsonLdContextCache {
    private {
        #! Mutex for thread safety
        Mutex m();

        #! Cache storage: URL -> cache entry
        hash<string, hash<JsonLdCacheEntry>> entries = {};

        #! Access order for LRU eviction (keys in insertion order, most recently used last)
        hash<string, bool> accessOrder = {};

        #! TTL in seconds (0 = no expiration)
        int ttlSeconds;

        #! Maximum entries (0 = unlimited)
        int maxEntries;

        #! Cache hit count
        int hits = 0;

        #! Cache miss count
        int misses = 0;
    }

    #! Creates a new context cache
    /** @param ttl_seconds time-to-live in seconds (default 3600 = 1 hour, 0 = no expiration)
        @param max_entries maximum number of cached entries (default 1000, 0 = unlimited)
    */
    constructor(int ttl_seconds = 3600, int max_entries = 1000) {
        ttlSeconds = ttl_seconds;
        maxEntries = max_entries;
    }

    #! Gets a cached context by URL
    /** @param url the URL to look up
        @return the cached context, or NOTHING if not found or expired

        @since json 1.12
    */
    *auto get(string url) {
        AutoLock al(m);
        *hash<JsonLdCacheEntry> entry = entries{url};
        if (!exists entry) {
            ++misses;
            return NOTHING;
        }

        # Check TTL
        if (ttlSeconds > 0 && (clock_getmicros() / 1000000 - entry.cachedAt) > ttlSeconds) {
            remove entries{url};
            remove accessOrder{url};
            ++misses;
            return NOTHING;
        }

        # Update access order (remove and re-add to move to end)
        remove accessOrder{url};
        accessOrder{url} = True;
        ++hits;
        return entry.contextValue;
    }

    #! Stores a context in the cache
    /** @param url the URL key
        @param ctx the context value to cache
        @param etag optional ETag for conditional requests

        @since json 1.12
    */
    set(string url, auto ctx, *string etag) {
        AutoLock al(m);

        # Evict if at capacity
        if (maxEntries > 0 && entries.size() >= maxEntries && !exists entries{url}) {
            evictLru();
        }

        entries{url} = <JsonLdCacheEntry>{
            "contextValue": ctx,
            "cachedAt": clock_getmicros() / 1000000,
            "etag": etag,
        };

        remove accessOrder{url};
        accessOrder{url} = True;
    }

    #! Returns True if the cache contains a non-expired entry for the URL
    /** @param url the URL to check
        @return True if a valid entry exists

        @since json 1.12
    */
    bool has(string url) {
        AutoLock al(m);
        *hash<JsonLdCacheEntry> entry = entries{url};
        if (!exists entry) {
            return False;
        }
        if (ttlSeconds > 0 && (clock_getmicros() / 1000000 - entry.cachedAt) > ttlSeconds) {
            remove entries{url};
            remove accessOrder{url};
            return False;
        }
        return True;
    }

    #! Clears all cached entries
    /** @since json 1.12
    */
    clear() {
        AutoLock al(m);
        entries = {};
        accessOrder = {};
        hits = 0;
        misses = 0;
    }

    #! Removes expired entries
    /** @since json 1.12
    */
    prune() {
        AutoLock al(m);
        if (ttlSeconds <= 0) {
            return;
        }
        int now = clock_getmicros() / 1000000;
        list<string> to_remove = ();
        foreach string url in (keys entries) {
            if ((now - entries{url}.cachedAt) > ttlSeconds) {
                push to_remove, url;
            }
        }
        foreach string url in (to_remove) {
            remove entries{url};
            remove accessOrder{url};
        }
    }

    #! Returns cache statistics
    /** @return a hash with hits, misses, size, maxEntries, ttlSeconds

        @since json 1.12
    */
    hash<auto> getStats() {
        AutoLock al(m);
        return {
            "hits": hits,
            "misses": misses,
            "size": entries.size(),
            "maxEntries": maxEntries,
            "ttlSeconds": ttlSeconds,
        };
    }

    #! Evicts the least recently used entry
    private evictLru() {
        if (accessOrder.size() > 0) {
            string lru_url = accessOrder.firstKey();
            remove accessOrder{lru_url};
            remove entries{lru_url};
        }
    }
}

} # namespace JsonLd
