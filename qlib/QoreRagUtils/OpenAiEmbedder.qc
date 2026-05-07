# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreRagUtils module — OpenAiEmbedder definition

/** OpenAiEmbedder.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! OpenAI-backed embedder — calls the OpenAI \c /embeddings endpoint
/** Wraps @ref OpenAiRestClient::OpenAiRestClientIo "OpenAiRestClientIo" for
    async-capable HTTP calls.

    @par Configuration

    - \c api_key (required): OpenAI API key; passed via the \c token option
    - \c url (optional): override the default OpenAI URL
    - \c model (optional): model name, default \c "text-embedding-3-small"
    - \c dimension (optional): forwarded as the \c dimensions request
      parameter; when unset the dimension is auto-detected on first call
    - \c timeout (optional): default request timeout (per-call \c opts override
      takes precedence)
    - \c connect_timeout (optional): default connection timeout (per-call
      \c opts override takes precedence)
*/
public class OpenAiEmbedder inherits AbstractEmbedder {
    public {
        #! Default model name
        const DefaultModel = "text-embedding-3-small";

        #! Backend identifier reported in @c EMBEDDER-MODEL-CONFIG-ERROR
        const Backend = "openai";
    }

    private {
        #! Captured configuration
        hash<EmbedderConfig> config;
        #! Resolved model name (never empty)
        string model_name;
        #! Lazily-created REST client
        *OpenAiRestClient::OpenAiRestClientIo rest_client;
        #! Cached embedding dimension (from config or auto-detected)
        *int cached_dimension;
        #! Guard for lazy initialization
        Mutex init_mutex();
    }

    #! Creates the embedder
    /** @param config embedder configuration; \c api_key must be set

        @throw EMBEDDER-CONFIG-ERROR if required fields are missing
    */
    constructor(hash<EmbedderConfig> config) {
        if (!config.api_key.val()) {
            throw "EMBEDDER-CONFIG-ERROR",
                "OpenAiEmbedder requires 'api_key' in EmbedderConfig";
        }
        self.config = config;
        model_name = config.model ?? DefaultModel;
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
    /** @param texts input texts
        @param opts optional per-call overrides; recognized keys:
        - \c timeout (\c *timeout) — request timeout
        - \c connect_timeout (\c *timeout) — connection timeout

        @throw EMBEDDER-RESPONSE-ERROR if the response is missing data, has
        a vector count mismatch, contains an empty vector, or contains
        non-numeric vector elements
        @throw EMBEDDER-MODEL-CONFIG-ERROR if the backend reports a permanent
        model/configuration failure (see @ref isPermanentEmbedderConfigError())
        @throw REST-RESPONSE-ERROR for transient infrastructure failures
        (5xx, rate limits, network)
    */
    list<list<float>> encodeBatch(list<string> texts, *hash<auto> opts) {
        if (!texts) {
            return ();
        }

        hash<auto> body = {
            "model": model_name,
            "input": texts,
        };
        if (exists cached_dimension) {
            body.dimensions = cached_dimension;
        }

        hash<auto> resp;
        try {
            resp = pickRestClient(opts).restPost("/embeddings", body);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "REST-RESPONSE-ERROR" && isPermanentEmbedderConfigError(ex)) {
                throwEmbedderModelConfigError(ex, Backend, model_name);
            }
            rethrow;
        }

        # sort by index to align with input order
        *list<auto> data = resp.body.data;
        if (!exists data) {
            throw "EMBEDDER-RESPONSE-ERROR",
                sprintf("%s: response missing 'data' field", Backend),
                {"body": resp.body};
        }
        if (data.size() != texts.size()) {
            throw "EMBEDDER-RESPONSE-ERROR",
                sprintf("%s: response returned %d items for %d input texts",
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

    #! Returns the embedding dimension produced by this backend
    /** If the dimension was not provided in the config, performs a one-shot
        probe call and caches the result.
    */
    int getDimension() {
        if (exists cached_dimension) {
            return cached_dimension;
        }
        encode(" ");
        return cached_dimension ?? -1;
    }

    #! Returns the configured model name
    *string getModel() {
        return model_name;
    }

    #! Classifies an OpenAI \c REST-RESPONSE-ERROR as a permanent model/config failure
    /** Recognizes:
        - HTTP 400 / 404 with response body containing OpenAI's
          \c error.code or \c error.type indicating a model issue
          (\c model_not_found, \c invalid_request_error referencing the model)
        - HTTP 400 / 404 with response body containing keywords
          \c "model" + (\c "not found" | \c "does not exist" | \c "invalid" |
          \c "is not an embedding" | \c "does not support embeddings")
    */
    private bool isPermanentEmbedderConfigError(hash<ExceptionInfo> ex) {
        *int status_code = ex.arg.status_code;
        if (status_code != 400 && status_code != 404) {
            return False;
        }
        auto body = ex.arg.body;
        # Structured error field: OpenAI returns {"error": {"code": ..., "type": ..., "message": ...}}
        *string code = body.error.code;
        if (code == "model_not_found" || code == "invalid_model") {
            return True;
        }
        *string error_type = body.error.type;
        *string message = body.error.message ?? body.message ?? body.detail;
        if (!exists message && body.typeCode() == NT_STRING) {
            message = body;
        }
        if (!exists message) {
            return False;
        }
        string lower = message.lwr();
        if (lower.find("model") == -1) {
            return False;
        }
        return lower.find("not found") != -1
            || lower.find("does not exist") != -1
            || lower.find("invalid model") != -1
            || lower.find("unsupported model") != -1
            || lower.find("is not an embedding") != -1
            || lower.find("does not appear to be an embedding") != -1
            || lower.find("does not support embeddings") != -1
            || error_type == "invalid_request_error";
    }

    #! Validates an embedding vector value and casts to \c list<float>
    private list<float> validateAndCastVector(auto raw, auto body) {
        if (!exists raw) {
            throw "EMBEDDER-RESPONSE-ERROR",
                sprintf("%s: response item missing 'embedding' field", Backend),
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
    private OpenAiRestClient::OpenAiRestClientIo pickRestClient(*hash<auto> opts) {
        if (exists opts.timeout || exists opts.connect_timeout) {
            return new OpenAiRestClient::OpenAiRestClientIo(
                buildRestOptions(opts.timeout, opts.connect_timeout));
        }
        ensureRestClient();
        return rest_client;
    }

    #! Builds the constructor option hash for the underlying REST client
    private hash<auto> buildRestOptions(*timeout req_timeout, *timeout conn_timeout) {
        hash<auto!> opts = {
            "token": config.api_key,
        };
        if (config.url.val()) {
            opts.url = config.url;
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
                rest_client = new OpenAiRestClient::OpenAiRestClientIo(buildRestOptions());
            }
        }
    }
}
}
