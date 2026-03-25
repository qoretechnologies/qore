# -*- mode: qore; indent-tabs-mode: nil -*-

/*  RestBackend.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Main QoreModelRegistry namespace
public namespace QoreModelRegistry {

#! Default maximum number of cached artifacts
public const DefaultMaxCacheEntries = 100;

#! REST-backed model registry storage backend
/** Stores model artifacts and metadata via a remote REST API.
    Artifacts are cached locally in memory after first download, with a
    configurable maximum cache size (LRU eviction).

    The REST API contract:
    - \c POST {base_path}/{name}/versions — register version (body: metadata + artifact)
    - \c GET {base_path}/{name}/versions/{vid}/artifact — download artifact
    - \c GET {base_path}/{name}/versions — list versions
    - \c GET {base_path} — list models
    - \c PUT {base_path}/{name}/versions/{vid}/tags/{tag} — tag version
    - \c DELETE {base_path}/{name}/versions/{vid}/tags/{tag} — untag version
    - \c POST {base_path}/{name}/prune — prune versions
    - \c DELETE {base_path}/{name} — delete model
    - \c GET {base_path}/{name}/versions/{vid} — get version metadata
    - \c GET {base_path}/tenants — list tenants
    - \c DELETE {base_path}/tenants/{tid} — delete tenant storage

    All path parameters (model name, version ID, tag name, tenant ID) are
    URL-encoded before being used in request paths.

    The caller provides a REST client object and configures authentication
    externally (e.g., via headers, tokens, or connection pools).

    @par Example:
    @code{.py}
    RestClient rc({"url": "https://api.example.com"});
    auto backend = new RestBackend(rc, "/models", "acme-corp", {
        "get_headers": *hash<auto> sub() { return {"Authorization": "Bearer " + token}; },
        "max_cache_entries": 50,
    });
    auto reg = new ModelRegistry({"backend": backend});
    @endcode

    @since QoreModelRegistry 2.0
*/
public class RestBackend inherits AbstractModelRegistryBackend {
    private {
        #! REST client for API calls
        object rest_client;

        #! Base path for REST API endpoints
        string base_path;

        #! Local artifact cache: key "{name}/{version_id}" → binary
        hash<string, binary> artifact_cache = {};

        #! Cache access order for LRU eviction (most recent at end)
        list<string> cache_order = ();

        #! Maximum number of cached artifacts
        int max_cache_entries;

        #! Mutex for cache access
        Mutex cache_mutex();

        #! Optional callback for providing per-request headers (e.g., auth tokens)
        *code<*hash<auto>()> get_headers;
    }

    #! Creates a new REST backend
    /** @param rest_client a REST client object with get/post/put/del methods
        @param base_path the base path for model registry REST endpoints
        @param tenant_id optional tenant identifier for multi-tenant deployments
        @param options optional hash with:
        - \c get_headers: callback code<*hash<auto>()>() returning extra headers
          to include in each request (e.g., for authentication)
        - \c max_cache_entries: maximum number of artifacts to cache locally
          (default @ref DefaultMaxCacheEntries); set to 0 to disable caching
    */
    constructor(object rest_client, string base_path, *string tenant_id,
            *hash<auto> options)
            : AbstractModelRegistryBackend(tenant_id) {
        self.rest_client = rest_client;
        self.base_path = base_path;
        get_headers = options.get_headers;
        max_cache_entries = options.max_cache_entries ?? DefaultMaxCacheEntries;
    }

    string registerVersion(string name, binary artifact,
            hash<ModelVersionInfo> metadata) {
        string version_id = metadata.version_id;

        # POST metadata + base64-encoded artifact
        hash<auto> body = metadata + {
            "artifact_base64": make_base64_string(artifact),
        };

        doPost(getTenantPath() + "/" + encode_url_param(name) + "/versions", body);

        return version_id;
    }

    binary loadArtifact(string name, string version_id) {
        # Check local cache first
        string cache_key = name + "/" + version_id;
        {
            cache_mutex.lock();
            on_exit cache_mutex.unlock();
            if (artifact_cache.hasKey(cache_key)) {
                touchCacheEntry(cache_key);
                return artifact_cache{cache_key};
            }
        }

        # Cache miss — download from REST API
        hash<auto> resp = doGet(getTenantPath() + "/" + encode_url_param(name)
            + "/versions/" + encode_url_param(version_id) + "/artifact");

        binary artifact;
        if (resp.body.artifact_base64) {
            artifact = parse_base64_string(resp.body.artifact_base64);
        } else if (resp.body.typeCode() == NT_BINARY) {
            artifact = resp.body;
        } else {
            throw "ML-REGISTRY-ERROR",
                sprintf("unexpected response format loading artifact for "
                    "model '%s' version '%s'", name, version_id);
        }

        # Cache the artifact (with LRU eviction)
        if (max_cache_entries > 0) {
            cache_mutex.lock();
            on_exit cache_mutex.unlock();
            addCacheEntry(cache_key, artifact);
        }

        return artifact;
    }

    list<hash<ModelVersionInfo>> listVersions(string name) {
        hash<auto> resp = doGet(getTenantPath() + "/" + encode_url_param(name)
            + "/versions");
        auto result = resp.body.versions;
        if (!exists result) {
            # Fall back to resp.body only if it is a list
            if (resp.body.typeCode() == NT_LIST) {
                result = resp.body;
            }
        }
        if (result.typeCode() == NT_LIST) {
            return cast<list<hash<ModelVersionInfo>>>(result);
        }
        if (!exists result) {
            return ();
        }
        throw "ML-REGISTRY-ERROR",
            sprintf("unexpected response type '%s' listing versions for model '%s'; "
                "expected a list of version info hashes", result.fullType(), name);
    }

    list<string> listModels() {
        hash<auto> resp = doGet(getTenantPath());
        return resp.body.models ?? resp.body ?? ();
    }

    nothing tag(string name, string version_id, string tag_name) {
        doPut(getTenantPath() + "/" + encode_url_param(name) + "/versions/"
            + encode_url_param(version_id) + "/tags/"
            + encode_url_param(tag_name));
    }

    nothing untag(string name, string version_id, string tag_name) {
        doDel(getTenantPath() + "/" + encode_url_param(name) + "/versions/"
            + encode_url_param(version_id) + "/tags/"
            + encode_url_param(tag_name));
    }

    int prune(string name, *hash<PruneOptions> options) {
        hash<auto> resp = doPost(getTenantPath() + "/" + encode_url_param(name)
            + "/prune", options);
        return resp.body.deleted ?? 0;
    }

    nothing deleteModel(string name) {
        doDel(getTenantPath() + "/" + encode_url_param(name));
        evictModelFromCache(name);
    }

    hash<ModelVersionInfo> getVersionMetadata(string name, string version_id) {
        hash<auto> resp = doGet(getTenantPath() + "/" + encode_url_param(name)
            + "/versions/" + encode_url_param(version_id));
        return cast<hash<ModelVersionInfo>>(resp.body);
    }

    list<string> listTenants() {
        # listTenants is an admin operation — always uses base_path, not tenant-scoped
        hash<auto> resp = doGet(base_path + "/tenants");
        return resp.body.tenants ?? resp.body ?? ();
    }

    nothing deleteTenantStorage(string tid) {
        if (tid !~ /^[a-zA-Z0-9_-]+$/) {
            throw "ML-REGISTRY-ERROR",
                sprintf("invalid tenant_id '%s' for deletion", tid);
        }
        # deleteTenantStorage is an admin operation — always uses base_path
        doDel(base_path + "/tenants/" + encode_url_param(tid));
    }

    #! Invalidates the local cache for a specific model/version
    /** Called when an external event (e.g., WebSocket notification) indicates
        that a model has been updated.

        @param name the model name
        @param version_id optional version ID; if not provided, all versions
        of the model are invalidated
    */
    nothing invalidateCache(string name, *string version_id) {
        cache_mutex.lock();
        on_exit cache_mutex.unlock();

        if (version_id) {
            string cache_key = name + "/" + version_id;
            remove artifact_cache{cache_key};
            cache_order = (map $1, cache_order, $1 != cache_key);
        } else {
            evictModelFromCacheUnlocked(name);
        }
    }

    #! Clears the entire local artifact cache
    nothing clearCache() {
        cache_mutex.lock();
        on_exit cache_mutex.unlock();
        artifact_cache = {};
        cache_order = ();
    }

    #! Private: returns the tenant-scoped base path
    /** When tenant_id is set, returns \c {base_path}/tenants/{tenant_id};
        otherwise returns \c base_path.
    */
    private string getTenantPath() {
        if (tenant_id) {
            return base_path + "/tenants/" + encode_url_param(tenant_id);
        }
        return base_path;
    }

    #! Private: URL-encodes a path segment parameter
    /** Percent-encodes all characters that are not unreserved (RFC 3986),
        including '/' which encode_uri_request() leaves unencoded.
    */
    private static string encode_url_param(string param) {
        # encode_uri_request() doesn't encode '/', which is unsafe for path segments
        string encoded = encode_uri_request(param);
        encoded = replace(encoded, "/", "%2F");
        return encoded;
    }

    #! Private: adds an entry to the cache with LRU eviction
    /** Must be called with cache_mutex held */
    private nothing addCacheEntry(string key, binary data) {
        # Remove existing entry if present (will re-add at end)
        if (artifact_cache.hasKey(key)) {
            cache_order = (map $1, cache_order, $1 != key);
        }
        # Evict oldest entries if at capacity
        while (cache_order.size() >= max_cache_entries && cache_order.size() > 0) {
            string oldest = cache_order[0];
            splice cache_order, 0, 1;
            remove artifact_cache{oldest};
        }
        artifact_cache{key} = data;
        push cache_order, key;
    }

    #! Private: moves a cache entry to the end (most recently used)
    /** Must be called with cache_mutex held */
    private nothing touchCacheEntry(string key) {
        cache_order = (map $1, cache_order, $1 != key);
        push cache_order, key;
    }

    #! Private: evicts all cache entries for a model
    private nothing evictModelFromCache(string name) {
        cache_mutex.lock();
        on_exit cache_mutex.unlock();
        evictModelFromCacheUnlocked(name);
    }

    #! Private: evicts all cache entries for a model (unlocked)
    /** Must be called with cache_mutex held */
    private nothing evictModelFromCacheUnlocked(string name) {
        string prefix = name + "/";
        list<string> keys_to_remove();
        foreach string key in (keys artifact_cache) {
            if (key.find(prefix) == 0) {
                push keys_to_remove, key;
            }
        }
        foreach string key in (keys_to_remove) {
            remove artifact_cache{key};
        }
        cache_order = (map $1, cache_order, $1.find(prefix) != 0);
    }

    #! Private: performs a GET request with error checking
    private hash<auto> doGet(string path) {
        *hash<auto> hdrs = get_headers ? get_headers() : NOTHING;
        try {
            hash<auto> resp = rest_client.get(path, NOTHING, hdrs);
            checkResponse(resp, "GET", path);
            return resp;
        } catch (hash<ExceptionInfo> ex) {
            rethrowRestError(ex, "GET", path);
        }
    }

    #! Private: performs a POST request with error checking
    private hash<auto> doPost(string path, *hash<auto> body) {
        *hash<auto> hdrs = get_headers ? get_headers() : NOTHING;
        try {
            hash<auto> resp = rest_client.post(path, body, NOTHING, hdrs);
            checkResponse(resp, "POST", path);
            return resp;
        } catch (hash<ExceptionInfo> ex) {
            rethrowRestError(ex, "POST", path);
        }
    }

    #! Private: performs a PUT request with error checking
    private hash<auto> doPut(string path, *hash<auto> body) {
        *hash<auto> hdrs = get_headers ? get_headers() : NOTHING;
        try {
            hash<auto> resp = rest_client.put(path, body, NOTHING, hdrs);
            checkResponse(resp, "PUT", path);
            return resp;
        } catch (hash<ExceptionInfo> ex) {
            rethrowRestError(ex, "PUT", path);
        }
    }

    #! Private: performs a DELETE request with error checking
    private hash<auto> doDel(string path) {
        *hash<auto> hdrs = get_headers ? get_headers() : NOTHING;
        try {
            hash<auto> resp = rest_client.del(path, NOTHING, hdrs);
            checkResponse(resp, "DELETE", path);
            return resp;
        } catch (hash<ExceptionInfo> ex) {
            rethrowRestError(ex, "DELETE", path);
        }
    }

    #! Private: checks a REST response for HTTP errors
    /** Handles the case where RestClient has error_passthru enabled,
        so HTTP errors come back as response hashes rather than exceptions.
    */
    private static nothing checkResponse(hash<auto> resp, string method,
            string path) {
        int status_code = resp.status_code ?? 0;
        if (status_code >= 400) {
            string msg = resp.body.error ?? resp.body.message
                ?? sprintf("HTTP %d", status_code);
            throw "ML-REGISTRY-REST-ERROR",
                sprintf("%s %s failed: %s (status %d)", method, path,
                    msg, status_code);
        }
    }

    #! Private: translates RestClient exceptions to ML-REGISTRY-REST-ERROR
    /** RestClient raises REST-RESPONSE-ERROR by default for HTTP error status
        codes. This method catches those and normalizes them to
        ML-REGISTRY-REST-ERROR for consistent error handling.

        Non-REST exceptions are re-thrown as-is.
    */
    private static nothing rethrowRestError(hash<ExceptionInfo> ex, string method,
            string path) {
        if (ex.err == "REST-RESPONSE-ERROR") {
            throw "ML-REGISTRY-REST-ERROR",
                sprintf("%s %s failed: %s (status %s)", method, path,
                    ex.desc, ex.arg.status_code ?? "unknown");
        }
        throw ex.err, ex.desc, ex.arg;
    }
}

} # namespace QoreModelRegistry
