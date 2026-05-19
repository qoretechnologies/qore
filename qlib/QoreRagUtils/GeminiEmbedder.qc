# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreRagUtils module — GeminiEmbedder definition

/** GeminiEmbedder.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Google Gemini-backed embedder — calls the Gemini \c batchEmbedContents API
/** Wraps @ref GeminiRestClient::GeminiRestClientIo "GeminiRestClientIo".

    @par Configuration

    - \c api_key (required): Google Gemini API key; passed via the
      \c x-goog-api-key header
    - \c url (optional): override the default Gemini URL
    - \c model (optional): model name, default \c "gemini-embedding-001"
    - \c dimension (optional): forwarded as \c outputDimensionality; when
      unset the dimension is auto-detected on first call
    - \c options.taskType (optional): forwarded as Gemini \c taskType
    - \c timeout (optional): default request timeout (per-call \c opts override
      takes precedence)
    - \c connect_timeout (optional): default connection timeout (per-call
      \c opts override takes precedence)
*/
public class GeminiEmbedder inherits AbstractEmbedder {
    public {
        #! Default Gemini embedding model
        const DefaultModel = "gemini-embedding-001";

        #! Backend identifier reported in @c EMBEDDER-MODEL-CONFIG-ERROR
        const Backend = "gemini";

        #! Maximum texts sent to one batchEmbedContents request
        const MaxBatch = 100;
    }

    private {
        #! Captured configuration
        hash<EmbedderConfig> config;
        #! Resolved model name
        string model_name;
        #! Lazily-created REST client
        *GeminiRestClient::GeminiRestClientIo rest_client;
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
                "GeminiEmbedder requires 'api_key' in EmbedderConfig";
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
    list<list<float>> encodeBatch(list<string> texts, *hash<auto> opts) {
        if (!texts) {
            return ();
        }

        GeminiRestClient::GeminiRestClientIo client = pickRestClient(opts);
        list<list<float>> result = ();
        int offset;
        while (offset < texts.size()) {
            int end = offset + MaxBatch;
            if (end > texts.size()) {
                end = texts.size();
            }
            result += encodeBatchSlice(client, texts[offset .. end - 1]);
            offset = end;
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

    #! Returns the configured model name
    *string getModel() {
        return model_name;
    }

    #! Encodes one API-sized batch
    private list<list<float>> encodeBatchSlice(GeminiRestClient::GeminiRestClientIo client,
            list<string> texts) {
        string resource = getModelResourceName();
        list<hash<auto>> requests = ();
        foreach string text in (texts) {
            hash<auto!> req = {
                "model": resource,
                "content": {
                    "parts": ({"text": text},),
                },
            };
            if (exists cached_dimension) {
                req.outputDimensionality = cached_dimension;
            }
            if (config.options.taskType.val()) {
                req.taskType = config.options.taskType;
            }
            push requests, req;
        }

        hash<auto> resp;
        try {
            resp = client.restPost(resource + ":batchEmbedContents",
                {"requests": requests});
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "REST-RESPONSE-ERROR" && isPermanentEmbedderConfigError(ex)) {
                throwEmbedderModelConfigError(ex, Backend, model_name);
            }
            rethrow;
        }

        *list<auto> embeddings = resp.body.embeddings;
        if (!exists embeddings) {
            throw "EMBEDDER-RESPONSE-ERROR",
                sprintf("%s: response missing 'embeddings' field", Backend),
                {"body": resp.body};
        }
        if (embeddings.size() != texts.size()) {
            throw "EMBEDDER-RESPONSE-ERROR",
                sprintf("%s: response returned %d embeddings for %d input texts",
                    Backend, embeddings.size(), texts.size()),
                {"body": resp.body};
        }

        list<list<float>> result = ();
        foreach hash<auto> item in (embeddings) {
            list<float> vec = validateAndCastVector(item.values, resp.body);
            if (!exists cached_dimension) {
                cached_dimension = vec.size();
            }
            push result, vec;
        }
        return result;
    }

    #! Returns the Gemini resource name for the configured model
    private string getModelResourceName() {
        if (model_name =~ /^models\//) {
            return model_name;
        }
        return "models/" + model_name;
    }

    #! Classifies a Gemini \c REST-RESPONSE-ERROR as a permanent model/config failure
    private bool isPermanentEmbedderConfigError(hash<ExceptionInfo> ex) {
        *int status_code = ex.arg.status_code;
        if (status_code != 400 && status_code != 404) {
            return False;
        }
        auto body = ex.arg.body;
        *string status = body.error.status ?? body.status;
        if (status == "NOT_FOUND") {
            return True;
        }
        *string message = body.error.message ?? body.message ?? body.error;
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
        return status == "INVALID_ARGUMENT"
            || lower.find("not found") != -1
            || lower.find("does not exist") != -1
            || lower.find("invalid model") != -1
            || lower.find("unsupported model") != -1
            || lower.find("does not support embeddings") != -1
            || lower.find("not an embedding") != -1;
    }

    #! Validates an embedding vector value and casts to \c list<float>
    private list<float> validateAndCastVector(auto raw, auto body) {
        if (!exists raw) {
            throw "EMBEDDER-RESPONSE-ERROR",
                sprintf("%s: response item missing 'values' field", Backend),
                {"body": body};
        }
        if (raw.typeCode() != NT_LIST || !raw.lsize()) {
            throw "EMBEDDER-RESPONSE-ERROR",
                sprintf("%s: response returned an empty or non-list vector",
                    Backend),
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
    private GeminiRestClient::GeminiRestClientIo pickRestClient(*hash<auto> opts) {
        if (exists opts.timeout || exists opts.connect_timeout) {
            return new GeminiRestClient::GeminiRestClientIo(
                buildRestOptions(opts.timeout, opts.connect_timeout));
        }
        ensureRestClient();
        return rest_client;
    }

    #! Builds the constructor option hash for the underlying REST client
    private hash<auto> buildRestOptions(*timeout req_timeout, *timeout conn_timeout) {
        hash<auto!> opts = {
            "apikey": config.api_key,
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
                rest_client = new GeminiRestClient::GeminiRestClientIo(
                    buildRestOptions());
            }
        }
    }
}
}
