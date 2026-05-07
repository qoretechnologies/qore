# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreRagUtils module — SGLangEmbedder definition

/** SGLangEmbedder.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! SGLang-backed embedder — calls the SGLang OpenAI-compatible \c /v1/embeddings endpoint
/** Wraps @ref SGLangRestClient::SGLangRestClientIo "SGLangRestClientIo"
    which captures the \c model at construction time (SGLang serves exactly
    one model per instance).

    The configured URL may also point at a dedicated bge-m3 sidecar that
    serves the native \c /embed endpoint instead of \c /v1/embeddings; the
    embedder auto-detects which style the server speaks on the first
    \ref encodeBatch() call (a 404 from \c /v1/embeddings switches to
    \c /embed) and pins the result for the lifetime of the embedder.

    @par Permanent vs transient errors

    A 404 from \c /v1/embeddings is treated as "wrong endpoint, try the native
    fallback" and is not a permanent model error.  A 400 from
    \c /v1/embeddings — typical when a chat model is loaded without embedding
    support — is classified by @ref isPermanentEmbedderConfigError() and
    rethrown as @c EMBEDDER-MODEL-CONFIG-ERROR; the embedder does not fall
    back to \c /embed in that case.

    @par Configuration

    - \c model (required): the model name served by the target SGLang instance
    - \c url (optional): SGLang server URL, default \c "http://localhost:30000"
    - \c api_key (optional): bearer token for SGLang deployments with
      authentication enabled
    - \c dimension (optional): cached dimension; when unset the dimension is
      auto-detected on first call
    - \c timeout (optional): default request timeout (per-call \c opts override
      takes precedence)
    - \c connect_timeout (optional): default connection timeout (per-call
      \c opts override takes precedence)
*/
public class SGLangEmbedder inherits AbstractEmbedder {
    public {
        #! Default SGLang server URL
        const DefaultUrl = "http://localhost:30000";

        #! Endpoint style: OpenAI-compatible \c /v1/embeddings with \c {"input":[...],"model":...} → \c {"data":[{"embedding":[...]}, ...]}
        const StyleOpenAi = "openai";

        #! Endpoint style: bge-m3 sidecar \c /embed with \c {"texts":[...]} → \c {"dense":[[...], ...]} (or \c {"embeddings":[[...], ...]})
        const StyleNative = "native";

        #! Backend identifier reported in @c EMBEDDER-MODEL-CONFIG-ERROR
        const Backend = "sglang";
    }

    private {
        #! Captured configuration
        hash<EmbedderConfig> config;
        #! Resolved model name (echoed back via getModel())
        string model_name;
        #! Lazily-created REST client
        *SGLangRestClient::SGLangRestClientIo rest_client;
        #! Cached embedding dimension (from config or auto-detected)
        *int cached_dimension;
        #! Pinned endpoint style; resolved on first encodeBatch() call (one of @ref StyleOpenAi or @ref StyleNative)
        *string endpoint_style;
        #! Guard for lazy initialization
        Mutex init_mutex();
    }

    #! Creates the embedder
    /** @param config embedder configuration; \c model must be set (required
        by the SGLang connection layer)

        @throw EMBEDDER-CONFIG-ERROR if required fields are missing
    */
    constructor(hash<EmbedderConfig> config) {
        if (!config.model.val()) {
            throw "EMBEDDER-CONFIG-ERROR",
                "SGLangEmbedder requires 'model' in EmbedderConfig "
                "(SGLang serves exactly one model per instance)";
        }
        self.config = config;
        model_name = config.model;
        if (exists config.dimension) {
            cached_dimension = config.dimension;
        }
    }

    #! Encodes a single piece of text into a vector
    list<float> encode(string text, *hash<auto> opts) {
        list<list<float>> rv = encodeBatch((text,), opts);
        return rv[0];
    }

    #! Encodes a batch of texts into vectors
    /** On first call, probes \c /v1/embeddings; if that returns 404, retries
        on the bge-m3 native \c /embed endpoint and pins the style for all
        subsequent calls so we don't pay the probe cost again.

        @param texts input texts
        @param opts optional per-call overrides; recognized keys:
        - \c timeout (\c *timeout) — request timeout
        - \c connect_timeout (\c *timeout) — connection timeout

        @throw EMBEDDER-RESPONSE-ERROR if the response shape is malformed
        @throw EMBEDDER-MODEL-CONFIG-ERROR if the backend reports a permanent
        model/configuration failure (e.g. 400 from \c /v1/embeddings on a
        non-embedding model)
        @throw REST-RESPONSE-ERROR for transient infrastructure failures
    */
    list<list<float>> encodeBatch(list<string> texts, *hash<auto> opts) {
        if (!texts) {
            return ();
        }
        SGLangRestClient::SGLangRestClientIo client = pickRestClient(opts);

        # Pinned style → straight to the matching encoder.
        switch (endpoint_style) {
            case StyleOpenAi:
                return encodeBatchOpenAi(client, texts);
            case StyleNative:
                return encodeBatchNative(client, texts);
        }

        # First call: try OpenAI-compatible, fall back to native ONLY on 404.
        # 400 (non-embedding model) goes through the permanent-error classifier
        # and does NOT fall back.
        try {
            list<list<float>> rv = encodeBatchOpenAi(client, texts);
            pinEndpointStyle(StyleOpenAi);
            return rv;
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "REST-RESPONSE-ERROR" && ex.arg.status_code == 404) {
                # try the native endpoint
            } else if (ex.err == "REST-RESPONSE-ERROR"
                    && isPermanentEmbedderConfigError(ex)) {
                throwEmbedderModelConfigError(ex, Backend, model_name);
            } else {
                rethrow;
            }
        }
        list<list<float>> rv = encodeBatchNative(client, texts);
        pinEndpointStyle(StyleNative);
        return rv;
    }

    #! Returns the pinned endpoint style, or @ref nothing if no batch has been encoded yet
    *string getEndpointStyle() {
        return endpoint_style;
    }

    #! Encodes a batch via the OpenAI-compatible \c /v1/embeddings endpoint
    private list<list<float>> encodeBatchOpenAi(SGLangRestClient::SGLangRestClientIo client,
            list<string> texts) {
        hash<auto> body = {
            "model": model_name,
            "input": texts,
        };
        hash<auto> resp = client.restPost("/v1/embeddings", body);

        # sort by index to align with input order
        *list<auto> data = resp.body.data;
        if (!exists data) {
            throw "EMBEDDER-RESPONSE-ERROR",
                sprintf("%s: /v1/embeddings response missing 'data' field", Backend),
                {"body": resp.body};
        }
        if (data.size() != texts.size()) {
            throw "EMBEDDER-RESPONSE-ERROR",
                sprintf("%s: /v1/embeddings returned %d items for %d input texts",
                    Backend, data.size(), texts.size()),
                {"body": resp.body};
        }
        list<hash<auto>> sorted_data = sort(cast<list<hash<auto>>>(data),
            int sub(hash<auto> a, hash<auto> b) {
                return a."index" <=> b."index";
            });

        list<list<float>> result = ();
        foreach hash<auto> item in (sorted_data) {
            list<float> vec = validateAndCastVector(item.embedding, resp.body);
            if (!exists cached_dimension) {
                cached_dimension = vec.size();
            }
            push result, vec;
        }
        return result;
    }

    #! Encodes a batch via the bge-m3 native \c /embed endpoint
    /** The sidecar response shape is either \c {"dense":[[...], ...]} or
        \c {"embeddings":[[...], ...]}; both are accepted to match the
        wire-format variants in the wild.
    */
    private list<list<float>> encodeBatchNative(SGLangRestClient::SGLangRestClientIo client,
            list<string> texts) {
        hash<auto> body = {"texts": texts};
        hash<auto> resp = client.restPost("/embed", body);

        *list<auto> vectors = resp.body.dense ?? resp.body.embeddings;
        if (!exists vectors) {
            throw "EMBEDDER-RESPONSE-ERROR",
                sprintf("%s: /embed response missing 'dense'/'embeddings' vector data",
                    Backend),
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

    #! Pins the resolved endpoint style under @ref init_mutex
    private pinEndpointStyle(string style) {
        init_mutex.lock();
        on_exit init_mutex.unlock();
        if (!endpoint_style) {
            endpoint_style = style;
        }
    }

    #! Returns the embedding dimension produced by this backend
    int getDimension() {
        if (exists cached_dimension) {
            return cached_dimension;
        }
        encode(" ");
        return cached_dimension ?? -1;
    }

    #! Returns the model name served by the target SGLang instance
    *string getModel() {
        ensureRestClient();
        return rest_client.getModel();
    }

    #! Classifies an SGLang \c REST-RESPONSE-ERROR as a permanent model/config failure
    /** Recognizes 400 / 404 responses where the body indicates the loaded
        model does not support embeddings (the typical SGLang case where the
        server is launched as a chat-completion model and \c /v1/embeddings
        is called against it).  404 from \c /v1/embeddings alone does NOT
        match this classifier — it indicates a native-only sidecar and the
        embedder retries on \c /embed.
    */
    private bool isPermanentEmbedderConfigError(hash<ExceptionInfo> ex) {
        *int status_code = ex.arg.status_code;
        if (status_code != 400 && status_code != 404) {
            return False;
        }
        auto body = ex.arg.body;
        *string error_type = body.object;
        if (error_type == "error") {
            # SGLang OpenAI-compat error envelope: {"object": "error", "message": "..."}
            # treat as permanent only if the message is model-related
        }
        *string message = body.message ?? body.error.message ?? body.detail ?? body.error;
        if (!exists message && body.typeCode() == NT_STRING) {
            message = body;
        }
        if (!exists message) {
            return False;
        }
        string lower = message.lwr();
        return lower.find("is not an embedding") != -1
            || lower.find("does not appear to be an embedding") != -1
            || lower.find("does not support embeddings") != -1
            || lower.find("not an embedding model") != -1
            || lower.find("unsupported model") != -1
            || lower.find("invalid model") != -1
            || lower.find("model not found") != -1;
    }

    #! Validates an embedding vector value and casts to \c list<float>
    private list<float> validateAndCastVector(auto raw, auto body) {
        if (!exists raw) {
            throw "EMBEDDER-RESPONSE-ERROR",
                sprintf("%s: response item missing embedding vector", Backend),
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
    private SGLangRestClient::SGLangRestClientIo pickRestClient(*hash<auto> opts) {
        if (exists opts.timeout || exists opts.connect_timeout) {
            SGLangRestClient::SGLangRestClientIo per_call =
                new SGLangRestClient::SGLangRestClientIo(
                    buildRestOptions(opts.timeout, opts.connect_timeout));
            per_call.clearConnectionPath();
            return per_call;
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

    #! Ensures the REST client is initialized
    /** The native bge-m3 sidecar serves \c /embed at the URL root, not under
        the SGLang \c /v1 connection path; pinning the connection path to
        \c /v1 here would cause \c /embed to be rewritten to \c /v1/embed
        once the OpenAI-compatible probe fails.  We therefore clear the
        connection path and let @ref encodeBatchOpenAi() / @ref encodeBatchNative()
        spell their endpoints out absolutely.
    */
    private ensureRestClient() {
        if (!rest_client) {
            init_mutex.lock();
            on_exit init_mutex.unlock();
            if (!rest_client) {
                rest_client = new SGLangRestClient::SGLangRestClientIo(buildRestOptions());
                rest_client.clearConnectionPath();
            }
        }
    }
}
}
