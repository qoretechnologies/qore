# -*- mode: qore; indent-tabs-mode: nil -*-

/*  CrossEncoderReranker.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! @see CrossEncoderReranker
public namespace QoreTokenizerUtils {

#! A scored reranker hit: an index into the caller's passage list plus the model score
/** Returned by @ref CrossEncoderReranker::rerankWithScores().  The list of hits is
    ordered by descending \c score and truncated to the caller's \c top_k.

    @since QoreTokenizerUtils 1.1
*/
public hashdecl RerankerScoredHit {
    #! Position in the input passage list this hit refers to
    int index;
    #! Cross-encoder relevance score — the model's raw output (or sigmoid(raw),
    #! depending on backend configuration); higher means more relevant
    float score;
}

#! Configuration for CrossEncoderReranker
public hashdecl RerankerConfig {
    #! Mode: "http" for HTTP endpoint, "onnx" for local ONNX model
    string mode;
    #! HTTP mode: URL of the reranker endpoint
    *string url;
    #! HTTP mode: optional Bearer token
    *string api_key;
    #! ONNX mode: path to the ONNX cross-encoder model
    *string model_path;
    #! ONNX mode: in-memory .onnx model data from a registry or database artifact
    *binary model_data;
    #! ONNX mode: tokenizer config for input preparation
    *hash<auto> tokenizer_config;
    #! ONNX mode: optional parsed tokenizer_config.json companion settings
    *hash<auto> tokenizer_runtime_config;
    #! ONNX mode: optional session configuration for providers, threading, and graph optimization
    *hash<ML::OnnxSessionConfig> session_config;
    #! Maximum sequence length for query+passage pairs; default 512
    int max_length = 512;
    #! Activation function for ONNX logits: "sigmoid" (default, binary relevance),
    #! "softmax" (multi-class ranking), or "none" (raw logits)
    string activation = "sigmoid";
    #! HTTP connect timeout in milliseconds; default 10000
    int connect_timeout_ms = 10000;
    #! HTTP request timeout in milliseconds; default 60000
    int timeout_ms = 60000;
}

#! Cross-encoder re-ranker for two-stage retrieval
/** Provides precision-oriented re-ranking of retrieval candidates using a
    cross-encoder model. In two-stage retrieval, the first stage uses fast
    approximate nearest neighbor (ANN) search for high recall, and the second
    stage uses a cross-encoder to re-rank the candidates for high precision.

    Supports two backends:
    - \b HTTP: Calls a reranker API endpoint that accepts query/passages pairs
    - \b ONNX: Runs a local ONNX cross-encoder model through dense tensor inputs

    @par Example: Re-rank retrieval candidates
    @code{.py}
CrossEncoderReranker reranker({"mode": "http", "url": "http://localhost:8080/rerank"});

string query = "What is machine learning?";
list<string> passages = (
    "Machine learning is a subset of artificial intelligence.",
    "The weather today is sunny and warm.",
    "Deep learning uses neural networks with many layers.",
);

list<int> top_indices = reranker.rerank(query, passages, 2);
# top_indices might be (0, 2) — the most relevant passages
    @endcode

    @since QoreTokenizerUtils 1.1
*/
public class CrossEncoderReranker {
    private {
        #! Backend mode
        string mode;
        #! HTTP endpoint URL
        *string url;
        #! Authentication token
        *string api_key;
        #! HTTP timeouts
        int connect_timeout_ms;
        int timeout_ms;
        #! Maximum sequence length for ONNX mode
        int max_length;
        #! Activation function for ONNX scoring
        string activation;
        #! REST client (lazy-initialized)
        *RestClientIo::RestClientIo rest_client;
        #! Mutex for thread-safe lazy initialization of rest_client
        Mutex rest_mutex();
        #! ONNX model for local inference
        *ML::OnnxModel onnx_model;
        #! HuggingFace tokenizer for ONNX mode
        *Tokenizer::HFTokenizer onnx_tokenizer;
        #! Input tensors accepted by the ONNX model
        hash<string, bool> onnx_input_names = {};
    }

    #! Creates a CrossEncoderReranker with the given configuration
    /** @param config a @ref RerankerConfig hash specifying the backend

        @throw RERANKER-CONFIG-ERROR if the mode is invalid or required parameters missing
        @throw RERANKER-DEPENDENCY-ERROR if required modules are not available
    */
    constructor(hash<RerankerConfig> config) {
        mode = config.mode;
        if (mode != "http" && mode != "onnx") {
            throw "RERANKER-CONFIG-ERROR",
                sprintf("invalid mode %y; expected \"http\" or \"onnx\"", mode);
        }

        if (mode == "http") {
            if (!config.url.val()) {
                throw "RERANKER-CONFIG-ERROR", "url is required for HTTP mode";
            }
            url = config.url;
            api_key = config.api_key;
            connect_timeout_ms = config.connect_timeout_ms;
            timeout_ms = config.timeout_ms;
        } else if (mode == "onnx") {
            if (!config.model_path.val() && !exists config.model_data) {
                throw "RERANKER-CONFIG-ERROR",
                    "model_path or model_data is required for ONNX mode";
            }
            if (!config.tokenizer_config) {
                throw "RERANKER-CONFIG-ERROR",
                    "tokenizer_config is required for ONNX mode";
            }
            max_length = config.max_length;
            activation = config.activation;
            onnx_model = exists config.session_config
                ? (exists config.model_data
                    ? new ML::OnnxModel(config.model_data, config.session_config)
                    : new ML::OnnxModel(config.model_path, config.session_config))
                : (exists config.model_data
                    ? new ML::OnnxModel(config.model_data)
                    : new ML::OnnxModel(config.model_path));
            onnx_input_names = CrossEncoderReranker::buildOnnxInputNameMap(onnx_model);
            onnx_tokenizer = new Tokenizer::HFTokenizer(config.tokenizer_config);
            if (config.tokenizer_runtime_config) {
                onnx_tokenizer.loadConfig(config.tokenizer_runtime_config);
            }
        }
    }

    #! Scores a query against a list of passages
    /** @param query the search query
        @param passages list of candidate passages to score
        @return list of float relevance scores in the same order as passages
    */
    list<float> score(string query, list<string> passages) {
        if (!passages) {
            return ();
        }

        if (mode == "http") {
            return scoreHttp(query, passages);
        }
        if (mode == "onnx") {
            return scoreOnnx(query, passages);
        }

        throw "RERANKER-ERROR", sprintf("unsupported mode %y", mode);
    }

    #! Re-ranks passages by relevance and returns the top-k indices
    /** @param query the search query
        @param passages list of candidate passages
        @param top_k number of top results to return
        @return list of indices into the original passages list, ordered by descending relevance

        @note Thin wrapper around @ref rerankWithScores() — returned indices are
        the \c index column of that method's result.
    */
    list<int> rerank(string query, list<string> passages, int top_k) {
        list<hash<RerankerScoredHit>> hits = rerankWithScores(query, passages, top_k);
        return map $1.index, hits;
    }

    #! Re-ranks passages by relevance and returns the top-k hits with real model scores
    /** Exposes the per-passage relevance scores the model already computes internally,
        instead of discarding them like @ref rerank() does.  Use this when downstream
        consumers (logging, debugging, hit-level observability) need the actual score —
        for example when adapting this class to a generic reranker interface whose
        result type carries a \c score field.

        @param query the search query
        @param passages list of candidate passages
        @param top_k number of top results to return
        @return list of @ref RerankerScoredHit in descending score order, truncated to
        \a top_k entries.  For HTTP backends using the `{"scores": [...]}` response
        format and for ONNX backends, the \c score is the model's raw output (subject
        to the configured activation function); for HTTP backends using the
        `{"results": [{"relevance_score": ...}]}` format it is the provider's
        \c relevance_score.
    */
    list<hash<RerankerScoredHit>> rerankWithScores(string query, list<string> passages,
            int top_k) {
        if (!passages) {
            return ();
        }

        list<float> scores = score(query, passages);

        # create index-score pairs and sort by score descending
        list<hash<RerankerScoredHit>> scored = ();
        for (int i = 0; i < scores.size(); ++i) {
            push scored, <RerankerScoredHit>{"index": i, "score": scores[i]};
        }
        scored = sort(scored,
            int sub(hash<RerankerScoredHit> a, hash<RerankerScoredHit> b) {
                return b.score <=> a.score;
            });

        # truncate to top_k
        int limit = min(top_k, scored.size());
        list<hash<RerankerScoredHit>> result = ();
        for (int i = 0; i < limit; ++i) {
            push result, scored[i];
        }
        return result;
    }

    #! Scores via local ONNX cross-encoder model
    /** Tokenizes each query-passage pair, pads to uniform length, runs ONNX
        inference in sub-batches, and applies sigmoid to the logits.

        @param query the search query
        @param passages candidate passages to score
        @return list of float relevance scores (0.0 to 1.0)
    */
    private list<float> scoreOnnx(string query, list<string> passages) {
        # Tokenize all query-passage pairs
        list<hash<auto>> encodings = ();
        foreach string passage in (passages) {
            hash<auto> enc = onnx_tokenizer.encodeAdvanced(query, {
                "text_pair": passage,
                "max_length": max_length,
                "truncation": "longest_first",
            });
            push encodings, enc;
        }

        # Find max sequence length across all pairs for padding
        int pad_len = 0;
        foreach hash<auto> enc in (encodings) {
            if (enc.input_ids.size() > pad_len) {
                pad_len = enc.input_ids.size();
            }
        }

        int pad_token_id = onnx_tokenizer.getPadTokenId();
        if (pad_token_id < 0) {
            pad_token_id = 0;
        }

        # Pad all sequences to uniform length and build input tensors
        list<list<int>> all_input_ids = ();
        list<list<int>> all_attention_mask = ();
        list<list<int>> all_token_type_ids = ();

        foreach hash<auto> enc in (encodings) {
            list<int> ids = enc.input_ids;
            list<int> mask = enc.attention_mask ?? (map 1, range(ids.size()));
            list<int> type_ids = enc.token_type_ids ?? (map 0, range(ids.size()));

            int pad_count = pad_len - ids.size();
            if (pad_count > 0) {
                ids += (map pad_token_id, range(pad_count));
                mask += (map 0, range(pad_count));
                type_ids += (map 0, range(pad_count));
            }

            push all_input_ids, ids;
            push all_attention_mask, mask;
            push all_token_type_ids, type_ids;
        }

        # Run inference in sub-batches for memory efficiency
        int batch_size = 16;
        list<float> all_scores = ();

        int total = all_input_ids.size();
        int offset = 0;
        while (offset < total) {
            int end = min(offset + batch_size, total);
            list<list<int>> batch_ids = all_input_ids[offset .. end - 1];
            list<list<int>> batch_mask = all_attention_mask[offset .. end - 1];
            list<list<int>> batch_types = all_token_type_ids[offset .. end - 1];

            hash<auto> inputs = makeOnnxTokenTensorInputs(batch_ids, batch_mask,
                batch_types, batch_ids[0].size());

            hash<auto> result = onnx_model.runTensors(inputs);

            # Extract logits and apply sigmoid
            # Cross-encoder models typically output logits in a "logits" key
            # or a single unnamed output; handle both
            auto logits = result.logits ?? result{result.firstKey()};
            logits = logits.toList();

            if (logits.typeCode() == NT_LIST) {
                foreach auto logit_row in (logits) {
                    float raw;
                    if (logit_row.typeCode() == NT_LIST) {
                        # Shape [batch, 1] or [batch, num_labels]
                        raw = logit_row[0].toFloat();
                    } else {
                        raw = logit_row.toFloat();
                    }
                    # Apply configured activation function
                    switch (activation) {
                        case "sigmoid":
                            push all_scores, 1.0 / (1.0 + exp(-raw));
                            break;
                        case "none":
                            push all_scores, raw;
                            break;
                        case "softmax":
                            # For single-output models, softmax degenerates to sigmoid
                            push all_scores, 1.0 / (1.0 + exp(-raw));
                            break;
                        default:
                            push all_scores, 1.0 / (1.0 + exp(-raw));
                    }
                }
            }

            offset = end;
        }

        return all_scores;
    }

    #! Builds ONNX Runtime tensor inputs from padded token IDs and masks
    private hash<auto> makeOnnxTokenTensorInputs(list<list<int>> input_ids,
            list<list<int>> attention_mask, list<list<int>> token_type_ids, int width) {
        hash<auto> inputs = {};

        if (onnx_input_names.hasKey("input_ids")) {
            inputs{"input_ids"} = CrossEncoderReranker::makeInt64Tensor(input_ids, width, "input_ids");
        }
        if (onnx_input_names.hasKey("attention_mask")) {
            inputs{"attention_mask"} = CrossEncoderReranker::makeInt64Tensor(
                attention_mask, width, "attention_mask");
        }
        if (onnx_input_names.hasKey("token_type_ids")) {
            inputs{"token_type_ids"} = CrossEncoderReranker::makeInt64Tensor(
                token_type_ids, width, "token_type_ids");
        }

        if (!inputs.hasKey("input_ids")) {
            throw "RERANKER-ERROR",
                "ONNX reranker model must expose a standard 'input_ids' tensor input";
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

    #! Converts padded token rows to a dense int64 Tensor
    private static auto makeInt64Tensor(list<list<int>> rows, int width, string input_name) {
        int row_count = rows.size();
        buffer<int64> data = buffer<int64>::sized(row_count * width);
        int offset = 0;
        for (int i = 0; i < row_count; ++i) {
            if (rows[i].size() != width) {
                throw "RERANKER-ERROR",
                    sprintf("token tensor '%s' row %d has length %d; expected %d",
                        input_name, i, rows[i].size(), width);
            }
            for (int j = 0; j < width; ++j) {
                data[offset++] = rows[i][j];
            }
        }
        return ML::Tensor::fromBuffer(data, (row_count, width));
    }

    #! Scores via HTTP endpoint
    private list<float> scoreHttp(string query, list<string> passages) {
        ensureRestClient();

        # Standard reranker API format (compatible with most reranker services)
        hash<auto> body = {
            "query": query,
            "passages": passages,
        };

        *hash<auto> hdr;
        if (api_key.val()) {
            hdr."Authorization" = "Bearer " + api_key;
        }

        hash<auto> resp = rest_client.restPost("/rerank", body, hdr);

        # handle response format: {"scores": [float, ...]}
        if (exists resp.body.scores) {
            return resp.body.scores;
        }

        # alternative format: {"results": [{"index": N, "relevance_score": F}, ...]}
        if (exists resp.body.results) {
            # sort results by index to match input order
            list<hash<auto>> results = sort(resp.body.results,
                int sub(hash<auto> a, hash<auto> b) {
                    return a."index" <=> b."index";
                });
            return map $1.relevance_score, results;
        }

        throw "RERANKER-ERROR", "unexpected response format from reranker endpoint";
    }

    #! Ensures the REST client is initialized
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
}

} # namespace QoreTokenizerUtils
