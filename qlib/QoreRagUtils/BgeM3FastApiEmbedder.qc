# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreRagUtils module — BgeM3FastApiEmbedder definition

/** BgeM3FastApiEmbedder.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! BGE-M3 FastAPI sidecar embedder — calls the native \c /embed endpoint
/** Wraps @ref BgeM3RestClient::BgeM3RestClientIo "BgeM3RestClientIo" and
    posts directly to \c POST /embed with the BGE-M3 native request shape.

    The sidecar can also produce sparse vectors for hybrid retrieval, but
    @ref AbstractEmbedder::encodeBatch() returns only dense vectors.  Sparse
    embeddings are surfaced through the
    @ref BgeM3FastApiDataProvider::BgeM3FastApiEmbeddingsDataProvider
    "BgeM3FastApiEmbeddingsDataProvider" data-provider action instead.

    @par Configuration

    - \c url (optional): sidecar URL, default \c "http://localhost:30680"
    - \c model (optional): reported back via @ref getModel(); the sidecar
      always serves \c "bge-m3" so the default matches reality
    - \c api_key (optional): bearer token for sidecars with authentication
      enabled
    - \c dimension (optional): cached dimension; when unset the dimension is
      auto-detected on first call
    - \c batch_size (optional): forwarded as the \c batch_size field in the
      sidecar request when set; when unset the sidecar's own default is used
    - \c options.return_sparse (optional): when @ref True, asks the sidecar
      to return sparse vectors as well (the dense ones are still what is
      returned to callers); default @ref False
    - \c timeout (optional): default request timeout (per-call \c opts override
      takes precedence)
    - \c connect_timeout (optional): default connection timeout (per-call
      \c opts override takes precedence)
*/
public class BgeM3FastApiEmbedder inherits AbstractEmbedder {
    public {
        #! Default sidecar URL
        const DefaultUrl = "http://localhost:30680";

        #! Default model name reported by @ref getModel()
        const DefaultModel = "bge-m3";

        #! Backend identifier reported in @c EMBEDDER-MODEL-CONFIG-ERROR
        const Backend = "bge-m3-fastapi";
    }

    private {
        #! Captured configuration
        hash<EmbedderConfig> config;
        #! Resolved model name (echoed back via @ref getModel())
        string model_name;
        #! Lazily-created REST client
        *BgeM3RestClient::BgeM3RestClientIo rest_client;
        #! Cached embedding dimension (from config or auto-detected)
        *int cached_dimension;
        #! Whether to ask the sidecar to also return sparse vectors
        bool return_sparse = False;
        #! Guard for lazy initialization
        Mutex init_mutex();
    }

    #! Creates the embedder
    /** @param config embedder configuration; all fields optional
    */
    constructor(hash<EmbedderConfig> config) {
        self.config = config;
        model_name = config.model.val() ? config.model : DefaultModel;
        if (exists config.dimension) {
            cached_dimension = config.dimension;
        }
        if (config.options.return_sparse.typeCode() == NT_BOOLEAN) {
            return_sparse = config.options.return_sparse;
        }
    }

    #! Encodes a single piece of text into a vector
    list<float> encode(string text, *hash<auto> opts) {
        list<list<float>> rv = encodeBatch((text,), opts);
        return rv[0];
    }

    #! Encodes a batch of texts into vectors via \c POST /embed
    /** Empty input returns an empty list without contacting the sidecar.

        @param texts input texts
        @param opts optional per-call overrides; recognized keys:
        - \c timeout (\c *timeout) — request timeout
        - \c connect_timeout (\c *timeout) — connection timeout

        @throw EMBEDDER-RESPONSE-ERROR if the response is missing \c dense or
        the returned vector count does not match the input text count, or
        any vector is empty or non-numeric
        @throw EMBEDDER-MODEL-CONFIG-ERROR if the sidecar reports a permanent
        model/configuration failure
        @throw REST-RESPONSE-ERROR for transient infrastructure failures
    */
    list<list<float>> encodeBatch(list<string> texts, *hash<auto> opts) {
        if (!texts) {
            return ();
        }

        hash<auto> body = {
            "texts": texts,
            "return_dense": True,
            "return_sparse": return_sparse,
        };
        if (exists config.batch_size && config.batch_size > 0) {
            body.batch_size = config.batch_size;
        }

        hash<auto> resp;
        try {
            resp = pickRestClient(opts).restPost("/embed", body);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "REST-RESPONSE-ERROR" && isPermanentEmbedderConfigError(ex)) {
                throwEmbedderModelConfigError(ex, Backend, model_name);
            }
            rethrow;
        }

        *list<auto> vectors = resp.body.dense;
        if (!exists vectors) {
            throw "EMBEDDER-RESPONSE-ERROR",
                sprintf("%s: /embed response missing 'dense' vector data", Backend),
                {"body": resp.body};
        }
        if (vectors.size() != texts.size()) {
            throw "EMBEDDER-RESPONSE-ERROR",
                sprintf("%s: /embed returned %d vectors for %d input texts",
                    Backend, vectors.size(), texts.size()),
                {"body": resp.body};
        }

        list<list<float>> result = ();
        foreach auto vec in (vectors) {
            list<float> typed = validateAndCastVector(vec, resp.body);
            if (!exists cached_dimension) {
                cached_dimension = typed.size();
            }
            push result, typed;
        }
        return result;
    }

    #! Returns the embedding dimension produced by this backend
    int getDimension() {
        if (exists cached_dimension) {
            return cached_dimension;
        }
        encode(" ");
        return cached_dimension ?? -1;
    }

    #! Returns the configured model name without making any HTTP request
    *string getModel() {
        return model_name;
    }

    #! Classifies a sidecar \c REST-RESPONSE-ERROR as a permanent model/config failure
    /** The BGE-M3 FastAPI sidecar always serves \c bge-m3, so model errors
        are unusual; the classifier recognizes:
        - HTTP 400 / 404 / 422 with body containing model-related keywords
          (\c "not found" / \c "unsupported" / \c "invalid" / \c "is not an
          embedding")

        FastAPI uses 422 for request-validation errors; the body has
        \c {"detail": [{"loc": ..., "msg": ..., "type": ...}]}.  We do NOT
        treat a generic 422 as permanent because it can also indicate a
        client-side request-shape bug.
    */
    private bool isPermanentEmbedderConfigError(hash<ExceptionInfo> ex) {
        *int status_code = ex.arg.status_code;
        if (status_code != 400 && status_code != 404 && status_code != 422) {
            return False;
        }
        auto body = ex.arg.body;
        *string message = body.detail.typeCode() == NT_STRING
            ? body.detail
            : (body.message ?? body.error);
        if (!exists message && body.typeCode() == NT_STRING) {
            message = body;
        }
        if (!exists message) {
            return False;
        }
        string lower = message.lwr();
        if (lower.find("model") == -1 && lower.find("embedding") == -1) {
            return False;
        }
        return lower.find("not found") != -1
            || lower.find("does not exist") != -1
            || lower.find("invalid model") != -1
            || lower.find("unsupported model") != -1
            || lower.find("is not an embedding") != -1
            || lower.find("does not appear to be an embedding") != -1;
    }

    #! Validates an embedding vector value and casts to \c list<float>
    private list<float> validateAndCastVector(auto raw, auto body) {
        if (!exists raw) {
            throw "EMBEDDER-RESPONSE-ERROR",
                sprintf("%s: response missing embedding vector entry", Backend),
                {"body": body};
        }
        if (raw.typeCode() != NT_LIST || !raw.lsize()) {
            throw "EMBEDDER-RESPONSE-ERROR",
                sprintf("%s: response returned an empty or non-list vector", Backend),
                {"body": body};
        }
        try {
            return cast<list<float>>(raw);
        } catch (hash<ExceptionInfo> ex) {
            throw "EMBEDDER-RESPONSE-ERROR",
                sprintf("%s: response vector contains non-numeric elements: %s",
                    Backend, ex.desc),
                {"body": body};
        }
    }

    #! Returns a REST client suitable for this call
    /** When \c opts provides per-call \c timeout or \c connect_timeout, a
        transient REST client is built so the override does not leak into
        other concurrent calls.  Otherwise the cached client is returned.
    */
    private BgeM3RestClient::BgeM3RestClientIo pickRestClient(*hash<auto> opts) {
        if (exists opts.timeout || exists opts.connect_timeout) {
            return new BgeM3RestClient::BgeM3RestClientIo(
                buildRestOptions(opts.timeout, opts.connect_timeout));
        }
        ensureRestClient();
        return rest_client;
    }

    #! Builds the constructor option hash for the underlying REST client
    private hash<auto> buildRestOptions(*timeout req_timeout, *timeout conn_timeout) {
        hash<auto!> opts = {
            "url": config.url ?? DefaultUrl,
            "model": model_name,
        };
        if (config.api_key.val()) {
            opts.apikey = config.api_key;
        }
        *timeout effective_timeout = req_timeout ?? config.timeout;
        if (exists effective_timeout) {
            opts.timeout = effective_timeout;
        }
        *timeout effective_connect = conn_timeout ?? config.connect_timeout;
        if (exists effective_connect) {
            opts.connect_timeout = effective_connect;
        }
        return opts;
    }

    #! Ensures the cached REST client is initialized with config defaults
    private ensureRestClient() {
        if (!rest_client) {
            init_mutex.lock();
            on_exit init_mutex.unlock();
            if (!rest_client) {
                rest_client = new BgeM3RestClient::BgeM3RestClientIo(buildRestOptions());
            }
        }
    }
}
}
