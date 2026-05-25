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
    #! ONNX mode: in-memory .onnx model data from a registry or database artifact
    *binary model_data;
    #! ONNX mode: tokenizer config for input preparation
    *hash<auto> tokenizer_config;
    #! ONNX mode: optional parsed tokenizer_config.json companion settings
    *hash<auto> tokenizer_runtime_config;
    #! ONNX mode: optional session configuration for providers, threading, and graph optimization
    *hash<ML::OnnxSessionConfig> session_config;
    #! ONNX mode: optional output tensor names to request from ONNX Runtime
    *list<string> output_names;
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
        #! Input tensors accepted by the ONNX model
        hash<string, bool> onnx_input_names = {};
        #! Optional output tensor names requested from the ONNX model
        *list<string> output_names;
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
            if (!config.model_path.val() && !exists config.model_data) {
                throw "EMBEDDING-CONFIG-ERROR",
                    "model_path or model_data is required for ONNX mode";
            }
            if (!config.tokenizer_config) {
                throw "EMBEDDING-CONFIG-ERROR",
                    "tokenizer_config is required for ONNX mode";
            }
            max_tokens = config.max_tokens;
            pooling_strategy = config.pooling_strategy;
            if (exists config.output_names) {
                output_names = config.output_names;
                if (!output_names) {
                    throw "EMBEDDING-CONFIG-ERROR",
                        "output_names must contain at least one ONNX output tensor name; "
                        "omit output_names to request all model outputs";
                }
            }
            onnx_model = exists config.session_config
                ? (exists config.model_data
                    ? new ML::OnnxModel(config.model_data, config.session_config)
                    : new ML::OnnxModel(config.model_path, config.session_config))
                : (exists config.model_data
                    ? new ML::OnnxModel(config.model_data)
                    : new ML::OnnxModel(config.model_path));
            onnx_input_names = EmbeddingModel::buildOnnxInputNameMap(onnx_model);
            onnx_tokenizer = new Tokenizer::HFTokenizer(config.tokenizer_config);
            if (config.tokenizer_runtime_config) {
                onnx_tokenizer.loadConfig(config.tokenizer_runtime_config);
            }
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

    #! Embeds a batch of texts and returns one dense tensor
    /** This method is the lowest-copy local ONNX embedding API. In ONNX mode,
        tokenization, pooling, and normalization stay in dense tensor form and
        the returned tensor has shape \c (texts.size(), dimension). In HTTP mode,
        the endpoint response is converted to a \c float32 tensor.

        @param texts list of texts to embed
        @return an @ref Qore::ML::Tensor with one embedding row per text, or
            \c NOTHING for an empty input batch

        @throw EMBEDDING-ERROR if the embedding request fails

        @par Example:
        @code{.q}
        QoreTokenizerUtils::EmbeddingModel emb(config);
        *ML::Tensor embeddings = emb.embedBatchTensor(("invoice text", "support ticket"));
        if (embeddings) {
            list<auto> dimension_columns = embeddings.toColumnBuffers();
        }
        @endcode

        @since QoreTokenizerUtils 1.2
    */
    *ML::Tensor embedBatchTensor(list<string> texts) {
        if (!texts) {
            return;
        }

        if (mode == "http") {
            return new ML::Tensor(embedBatchHttp(texts), NOTHING, "float32");
        }
        if (mode == "onnx") {
            return embedBatchOnnxTensor(texts);
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
        *ML::Tensor tensor = embedBatchOnnxTensor(texts);
        return tensor ? tensor.toList() : ();
    }

    #! Embeds texts via local ONNX model and keeps the result as a dense tensor
    /** Tokenizes texts, pads to uniform length, runs ONNX inference in
        sub-batches, mean-pools the last hidden state (weighted by attention
        mask), and L2-normalizes the output vectors.

        @param texts list of texts to embed
        @return a dense tensor with one embedding row per input text, or
            \c NOTHING for an empty input batch
    */
    private *ML::Tensor embedBatchOnnxTensor(list<string> texts) {
        if (!texts) {
            return;
        }

        int batch_size = 32;
        list<ML::Tensor> tensor_batches = ();
        int total = texts.size();
        int offset = 0;

        while (offset < total) {
            int end = min(offset + batch_size, total);
            list<string> batch_texts = texts[offset .. end - 1];

            # Batch tokenize directly into dense int64 buffers.
            hash<Tokenizer::TokenizerModelInputBatch> token_batch = onnx_tokenizer.encodeBatchForModel(batch_texts, {
                "max_length": max_tokens,
                "truncation": "longest_first",
                "padding": "batch_longest",
            });

            ML::Tensor attention_mask = ML::Tensor::fromBuffer(token_batch.attention_mask, token_batch.shape);
            hash<auto> inputs = makeOnnxTokenTensorInputs(token_batch);

            hash<auto> result = onnx_model.runTensors(inputs, output_names);

            # Get the last hidden state (typically the first output)
            ML::Tensor hidden_states = result.last_hidden_state ?? result{result.firstKey()};
            ML::Tensor normalized = hidden_states.poolTokenEmbeddings(
                attention_mask: attention_mask, strategy: pooling_strategy).l2NormalizeRows();
            list<int> normalized_shape = normalized.getShape();
            if (!dimension && normalized_shape.size() > 1) {
                dimension = normalized_shape[1];
            }

            push tensor_batches, normalized;

            offset = end;
        }

        return tensor_batches.size() == 1 ? tensor_batches[0] : ML::Tensor::concatRows(tensor_batches);
    }

    #! Builds ONNX Runtime tensor inputs from dense tokenizer buffers
    private hash<auto> makeOnnxTokenTensorInputs(hash<Tokenizer::TokenizerModelInputBatch> token_batch) {
        hash<auto> inputs = {};

        if (onnx_input_names.hasKey("input_ids")) {
            inputs{"input_ids"} = ML::Tensor::fromBuffer(token_batch.input_ids, token_batch.shape);
        }
        if (onnx_input_names.hasKey("attention_mask")) {
            inputs{"attention_mask"} = ML::Tensor::fromBuffer(token_batch.attention_mask, token_batch.shape);
        }
        if (onnx_input_names.hasKey("token_type_ids")) {
            inputs{"token_type_ids"} = ML::Tensor::fromBuffer(token_batch.token_type_ids, token_batch.shape);
        }

        if (!inputs.hasKey("input_ids")) {
            throw "EMBEDDING-ERROR",
                "ONNX embedding model must expose a standard 'input_ids' tensor input";
        }
        return inputs;
    }

    #! Builds a hash map of declared ONNX model input names
    private static hash<string, bool> buildOnnxInputNameMap(auto model) {
        hash<string, bool> rv = {};
        foreach hash<ML::OnnxTensorInfo> input in (model.getInputInfo()) {
            rv{input.name} = True;
        }
        return rv;
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
