# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreRagUtils module — VoyageEmbedder definition

/** VoyageEmbedder.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Voyage AI-backed embedder — calls the Voyage \c /embeddings endpoint
/** Wraps @ref VoyageRestClient::VoyageRestClientIo "VoyageRestClientIo" for
    async-capable HTTP calls to the Voyage AI API.

    @par Configuration

    - \c api_key (required): Voyage API key; passed via the \c apikey option
    - \c url (optional): override the default Voyage URL
    - \c model (optional): model name, default \c "voyage-3"
    - \c dimension (optional): forwarded as the \c output_dimension request
      parameter; when unset the dimension is auto-detected on first call
*/
public class VoyageEmbedder inherits AbstractEmbedder {
    public {
        #! Default model name
        const DefaultModel = "voyage-3";
    }

    private {
        #! Captured configuration
        hash<EmbedderConfig> config;
        #! Resolved model name
        string model_name;
        #! Lazily-created REST client
        *VoyageRestClient::VoyageRestClientIo rest_client;
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
                "VoyageEmbedder requires 'api_key' in EmbedderConfig";
        }
        self.config = config;
        model_name = config.model ?? DefaultModel;
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
        if (exists cached_dimension) {
            body.output_dimension = cached_dimension;
        }

        hash<auto> resp = rest_client.restPost("/embeddings", body);

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

    #! Returns the configured model name
    *string getModel() {
        return model_name;
    }

    #! Ensures the REST client is initialized
    private ensureRestClient() {
        if (!rest_client) {
            init_mutex.lock();
            on_exit init_mutex.unlock();
            if (!rest_client) {
                hash<auto> opts = {
                    "apikey": config.api_key,
                };
                if (config.url.val()) {
                    opts.url = config.url;
                }
                rest_client = new VoyageRestClient::VoyageRestClientIo(opts);
            }
        }
    }
}
}
