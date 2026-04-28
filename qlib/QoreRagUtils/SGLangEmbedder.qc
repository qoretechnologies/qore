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

    @par Configuration

    - \c model (required): the model name served by the target SGLang instance;
      this is required by the SGLang connection layer
    - \c url (optional): SGLang server URL, default \c "http://localhost:30000"
    - \c api_key (optional): bearer token for SGLang deployments with
      authentication enabled
    - \c dimension (optional): cached dimension; when unset the dimension is
      auto-detected on first call
*/
public class SGLangEmbedder inherits AbstractEmbedder {
    public {
        #! Default SGLang server URL
        const DefaultUrl = "http://localhost:30000";

        #! Endpoint style: OpenAI-compatible \c /v1/embeddings with \c {"input":[...],"model":...} → \c {"data":[{"embedding":[...]}, ...]}
        const StyleOpenAi = "openai";

        #! Endpoint style: bge-m3 sidecar \c /embed with \c {"texts":[...]} → \c {"dense":[[...], ...]} (or \c {"embeddings":[[...], ...]})
        const StyleNative = "native";
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
    list<float> encode(string text) {
        list<list<float>> rv = encodeBatch((text,));
        return rv[0];
    }

    #! Encodes a batch of texts into vectors
    /** On first call, probes \c /v1/embeddings; if that returns 404, retries
        on the bge-m3 native \c /embed endpoint and pins the style for all
        subsequent calls so we don't pay the probe cost again.
    */
    list<list<float>> encodeBatch(list<string> texts) {
        if (!texts) {
            return ();
        }
        ensureRestClient();

        # Pinned style → straight to the matching encoder.
        switch (endpoint_style) {
            case StyleOpenAi:
                return encodeBatchOpenAi(texts);
            case StyleNative:
                return encodeBatchNative(texts);
        }

        # First call: try OpenAI-compatible, fall back to native on 404.
        try {
            list<list<float>> rv = encodeBatchOpenAi(texts);
            pinEndpointStyle(StyleOpenAi);
            return rv;
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err != "REST-RESPONSE-ERROR" || ex.arg.status_code != 404) {
                rethrow;
            }
        }
        list<list<float>> rv = encodeBatchNative(texts);
        pinEndpointStyle(StyleNative);
        return rv;
    }

    #! Returns the pinned endpoint style, or @ref nothing if no batch has been encoded yet
    *string getEndpointStyle() {
        return endpoint_style;
    }

    #! Encodes a batch via the OpenAI-compatible \c /v1/embeddings endpoint
    private list<list<float>> encodeBatchOpenAi(list<string> texts) {
        hash<auto> body = {
            "model": model_name,
            "input": texts,
        };
        hash<auto> resp = rest_client.restPost("/v1/embeddings", body);

        # sort by index to align with input order
        list<hash<auto>> data = resp.body.data;
        data = sort(data, int sub(hash<auto> a, hash<auto> b) {
            return a."index" <=> b."index";
        });

        list<list<float>> result = ();
        foreach hash<auto> item in (data) {
            list<float> vec = cast<list<float>>(item.embedding);
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
    private list<list<float>> encodeBatchNative(list<string> texts) {
        hash<auto> body = {"texts": texts};
        hash<auto> resp = rest_client.restPost("/embed", body);

        *list<auto> vectors = resp.body.dense ?? resp.body.embeddings;
        if (!vectors) {
            throw "EMBEDDER-RESPONSE-ERROR",
                "bge-m3 /embed response missing 'dense'/'embeddings' vector data",
                {"body": resp.body};
        }
        if (vectors.size() != texts.size()) {
            throw "EMBEDDER-RESPONSE-ERROR",
                sprintf("bge-m3 /embed returned %d vectors for %d input texts",
                    vectors.size(), texts.size()),
                {"body": resp.body};
        }

        list<list<float>> result = ();
        foreach auto vec in (vectors) {
            list<float> typed = cast<list<float>>(vec);
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
                hash<auto> opts = {
                    "url": config.url ?? DefaultUrl,
                    "model": model_name,
                };
                if (config.api_key.val()) {
                    opts.apikey = config.api_key;
                }
                rest_client = new SGLangRestClient::SGLangRestClientIo(opts);
                rest_client.clearConnectionPath();
            }
        }
    }
}
}
