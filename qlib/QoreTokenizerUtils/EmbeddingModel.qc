# -*- mode: qore; indent-tabs-mode: nil -*-

/*  EmbeddingModel.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! @see EmbeddingModel
public namespace QoreTokenizerUtils {

#! Configuration for EmbeddingModel
/** Specifies how the embedding model should be accessed — either via HTTP
    (OpenAI-compatible endpoint) or via a local ONNX model.
*/
public hashdecl EmbeddingConfig {
    #! Mode: "http" for HTTP endpoint, "onnx" for local ONNX model
    string mode;
    #! HTTP mode: base URL of the embedding service (e.g., "http://localhost:30000")
    *string url;
    #! HTTP mode: model name to pass in the request (e.g., "bge-m3")
    *string model;
    #! HTTP mode: optional Bearer token for authentication
    *string api_key;
    #! ONNX mode: filesystem path to the .onnx model file
    *string model_path;
    #! ONNX mode: tokenizer config for input preparation
    *hash<auto> tokenizer_config;
    #! Maximum input tokens (longer inputs are truncated); default 512
    int max_tokens = 512;
    #! HTTP endpoint format: "auto" (try OpenAI then bge-m3 fallback),
    #! "openai" (only /v1/embeddings), "bge" (only /embed); default "auto"
    string endpoint_format = "auto";
    #! ONNX pooling strategy: "mean" (default), "cls" (first token), "max" (element-wise max)
    string pooling_strategy = "mean";
    #! HTTP connect timeout in milliseconds; default 10000
    int connect_timeout_ms = 10000;
    #! HTTP request timeout in milliseconds; default 120000
    int timeout_ms = 120000;
}

#! Dense embedding model client supporting HTTP and ONNX backends
/** Provides a unified interface for generating dense vector embeddings from text.
    Supports two backends:
    - \b HTTP: Calls an OpenAI-compatible `/v1/embeddings` endpoint (e.g., SGLang,
      vLLM, OpenAI) with automatic fallback to a `/embed` endpoint (bge-m3 native).
    - \b ONNX: Runs a local ONNX embedding model via the `ml` module.

    @par Example: HTTP mode (SGLang/OpenAI compatible)
    @code{.py}
hash<auto> tok_config = parse_json(ReadOnlyFile::readTextFile("tokenizer.json"));
EmbeddingModel emb({"mode": "http", "url": "http://localhost:30000", "model": "bge-m3"});
list<float> vec = emb.embed("Hello world");
printf("Embedding dimension: %d\n", vec.size());
    @endcode

    @since QoreTokenizerUtils 1.1
*/
public class EmbeddingModel {
    private {
        #! The embedding mode
        string mode;
        #! HTTP configuration
        *string url;
        *string model_name;
        *string api_key;
        int connect_timeout_ms;
        int timeout_ms;
        #! Maximum input tokens for ONNX mode
        int max_tokens;
        #! HTTP endpoint format
        string endpoint_format;
        #! ONNX pooling strategy
        string pooling_strategy;
        #! Cached embedding dimension (detected on first call)
        *int dimension;
        #! REST client for HTTP mode (lazy-initialized)
        *RestClientIo::RestClientIo rest_client;
        #! Mutex for thread-safe lazy initialization of rest_client
        Mutex rest_mutex();
        #! ONNX model for local inference
        *ML::OnnxModel onnx_model;
        #! HuggingFace tokenizer for ONNX mode
        *Tokenizer::HFTokenizer onnx_tokenizer;
    }

    #! Creates an EmbeddingModel with the given configuration
    /** @param config an @ref EmbeddingConfig hash specifying the backend and parameters

        @throw EMBEDDING-CONFIG-ERROR if the mode is invalid or required parameters are missing
        @throw EMBEDDING-DEPENDENCY-ERROR if required modules are not available
    */
    constructor(hash<EmbeddingConfig> config) {
        mode = config.mode;
        if (mode != "http" && mode != "onnx") {
            throw "EMBEDDING-CONFIG-ERROR",
                sprintf("invalid mode %y; expected \"http\" or \"onnx\"", mode);
        }

        if (mode == "http") {
            if (!config.url.val()) {
                throw "EMBEDDING-CONFIG-ERROR", "url is required for HTTP mode";
            }
%ifdef NoJson
            throw "EMBEDDING-DEPENDENCY-ERROR",
                "json module is required for HTTP mode but is not available";
%endif
            url = config.url;
            model_name = config.model;
            api_key = config.api_key;
            endpoint_format = config.endpoint_format;
            connect_timeout_ms = config.connect_timeout_ms;
            timeout_ms = config.timeout_ms;
        } else if (mode == "onnx") {
            if (!config.model_path.val()) {
                throw "EMBEDDING-CONFIG-ERROR", "model_path is required for ONNX mode";
            }
            if (!config.tokenizer_config) {
                throw "EMBEDDING-CONFIG-ERROR",
                    "tokenizer_config is required for ONNX mode";
            }
            max_tokens = config.max_tokens;
            pooling_strategy = config.pooling_strategy;
            onnx_model = new ML::OnnxModel(config.model_path);
            onnx_tokenizer = new Tokenizer::HFTokenizer(config.tokenizer_config);
        }
    }

    #! Embeds a single text and returns a dense vector
    /** @param text the text to embed
        @return a list of floats representing the dense embedding vector

        @throw EMBEDDING-ERROR if the embedding request fails
    */
    list<float> embed(string text) {
        list<list<float>> results = embedBatch((text,));
        return results[0];
    }

    #! Embeds a batch of texts and returns a list of dense vectors
    /** @param texts list of texts to embed
        @return list of float vectors, one per input text, in the same order

        @throw EMBEDDING-ERROR if the embedding request fails
    */
    list<list<float>> embedBatch(list<string> texts) {
        if (!texts) {
            return ();
        }

        if (mode == "http") {
            return embedBatchHttp(texts);
        }
        if (mode == "onnx") {
            return embedBatchOnnx(texts);
        }
        throw "EMBEDDING-ERROR", sprintf("unsupported mode %y", mode);
    }

    #! Returns the embedding vector dimension
    /** Returns the cached dimension after the first successful embedding call.
        @return the embedding dimension, or -1 if no embedding has been computed yet
    */
    int getDimension() {
        return dimension ?? -1;
    }

    #! Ensures the REST client is initialized (lazy creation to avoid connecting in constructor)
    private ensureRestClient() {
        if (!rest_client) {
            rest_mutex.lock();
            on_exit rest_mutex.unlock();
            if (!rest_client) {
                rest_client = new RestClientIo::RestClientIo({
                    "url": url,
                    "connect_timeout": connect_timeout_ms,
                    "timeout": timeout_ms,
                    "data": "json",
                });
            }
        }
    }

    #! Embeds texts via local ONNX model
    /** Tokenizes texts, pads to uniform length, runs ONNX inference in
        sub-batches, mean-pools the last hidden state (weighted by attention
        mask), and L2-normalizes the output vectors.

        @param texts list of texts to embed
        @return list of float vectors, one per input text
    */
    private list<list<float>> embedBatchOnnx(list<string> texts) {
        int batch_size = 32;
        list<list<float>> all_embeddings = ();
        int total = texts.size();
        int offset = 0;

        while (offset < total) {
            int end = min(offset + batch_size, total);
            list<string> batch_texts = texts[offset .. end - 1];

            # Batch tokenize with padding to longest in batch
            list<hash<auto>> encodings = onnx_tokenizer.encodeBatch(batch_texts, {
                "max_length": max_tokens,
                "truncation": "longest_first",
                "padding": "true",
            });

            # Build input tensors
            list<list<int>> batch_input_ids = ();
            list<list<int>> batch_attention_mask = ();
            list<list<int>> batch_token_type_ids = ();

            foreach hash<auto> enc in (encodings) {
                push batch_input_ids, enc.input_ids;
                push batch_attention_mask,
                    enc.attention_mask ?? (map 1, range(enc.input_ids.size()));
                push batch_token_type_ids,
                    enc.token_type_ids ?? (map 0, range(enc.input_ids.size()));
            }

            hash<auto> inputs = {
                "input_ids": batch_input_ids,
                "attention_mask": batch_attention_mask,
                "token_type_ids": batch_token_type_ids,
            };

            hash<auto> result = onnx_model.run(inputs);

            # Get the last hidden state (typically the first output)
            auto hidden_states = result.last_hidden_state ?? result{result.firstKey()};

            # Pool and L2-normalize using C++ vector math from ml module
            for (int i = 0; i < batch_input_ids.size(); ++i) {
                list<float> pooled;
                switch (pooling_strategy) {
                    case "cls":
                        # CLS token pooling — use the first token's embedding
                        pooled = hidden_states[i][0];
                        break;
                    case "max":
                        # Max pooling — element-wise max across masked tokens
                        pooled = ML::ml_max_pool(
                            hidden_states[i], batch_attention_mask[i]);
                        break;
                    default:
                        # Mean pooling (default)
                        pooled = ML::ml_mean_pool(
                            hidden_states[i], batch_attention_mask[i]);
                }
                list<float> normalized = ML::ml_l2_normalize(pooled);

                if (!dimension) {
                    dimension = normalized.size();
                }

                push all_embeddings, normalized;
            }

            offset = end;
        }

        return all_embeddings;
    }

    #! Embeds texts via HTTP endpoint
    private list<list<float>> embedBatchHttp(list<string> texts) {
        ensureRestClient();
        switch (endpoint_format) {
            case "openai":
                return callOpenAiEndpoint(texts);
            case "bge":
                return callBgeM3Endpoint(texts);
            default:
                # "auto" — try OpenAI first, fall back to bge-m3 on 404
                try {
                    return callOpenAiEndpoint(texts);
                } catch (hash<ExceptionInfo> ex) {
                    if (ex.arg.typeCode() == NT_HASH && ex.arg.status_code == 404) {
                        return callBgeM3Endpoint(texts);
                    }
                    rethrow;
                }
        }
    }

    #! Calls the OpenAI-compatible /v1/embeddings endpoint
    private list<list<float>> callOpenAiEndpoint(list<string> texts) {
        hash<auto> body = {
            "input": texts,
        };
        if (model_name.val()) {
            body.model = model_name;
        }

        *hash<auto> hdr;
        if (api_key.val()) {
            hdr."Authorization" = "Bearer " + api_key;
        }

        hash<auto> resp = rest_client.restPost("/v1/embeddings", body, hdr);

        # sort by index and extract embeddings
        list<hash<auto>> data = resp.body.data;
        data = sort(data, int sub(hash<auto> a, hash<auto> b) {
            return a."index" <=> b."index";
        });

        list<list<float>> result = ();
        foreach hash<auto> item in (data) {
            list<float> vec = item.embedding;
            if (!dimension) {
                dimension = vec.size();
            }
            push result, vec;
        }
        return result;
    }

    #! Calls the bge-m3 native /embed endpoint (fallback)
    private list<list<float>> callBgeM3Endpoint(list<string> texts) {
        hash<auto> body = {
            "texts": texts,
        };

        *hash<auto> hdr;
        if (api_key.val()) {
            hdr."Authorization" = "Bearer " + api_key;
        }

        hash<auto> resp = rest_client.restPost("/embed", body, hdr);

        list<list<float>> result = ();
        foreach list<float> vec in (resp.body.dense) {
            if (!dimension) {
                dimension = vec.size();
            }
            push result, vec;
        }
        return result;
    }
}

} # namespace QoreTokenizerUtils
