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

    @par Configuration

    - \c model (required): the model name served by the target SGLang instance;
      this is required by the SGLang connection layer
    - \c url (optional): SGLang server URL, default \c "http://localhost:30000"
    - \c api_key (optional): bearer token for SGLang deployments with
      authentication enabled
    - \c dimension (optional): cached dimension; when unset the dimension is
      auto-detected on first call

    @since QoreRagUtils 1.0
*/
public class SGLangEmbedder inherits AbstractEmbedder {
    public {
        #! Default SGLang server URL
        const DefaultUrl = "http://localhost:30000";
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
    list<list<float>> encodeBatch(list<string> texts) {
        if (!texts) {
            return ();
        }
        ensureRestClient();

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
            }
        }
    }
}
}
