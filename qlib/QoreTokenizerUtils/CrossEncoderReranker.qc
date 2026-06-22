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
    #! Mode: "http" for HTTP endpoint, "onnx" for local ONNX model, "sglang" for an SGLang reranker server
    string mode;
    #! HTTP / SGLang mode: URL of the reranker endpoint
    *string url;
    #! HTTP / SGLang mode: optional Bearer token
    *string api_key;
    #! SGLang mode: model name to send in the rerank request (the served reranker model)
    *string model;
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
    #! ONNX mode: optional output tensor names to request from ONNX Runtime
    *list<string> output_names;
    #! Maximum sequence length for query+passage pairs; default 512
    int max_length = 512;
    #! Activation function for ONNX logits: "sigmoid" (default, binary relevance),
    #! "softmax" (multi-class ranking), or "none" (raw logits)
    string activation = "sigmoid";
    #! HTTP connect timeout in milliseconds; default 10000
    int connect_timeout_ms = 10000;
    #! HTTP request timeout in milliseconds; default 60000
    int timeout_ms = 60000;
    #! HTTP / SGLang mode: maximum passages sent per rerank request; default 50
    int batch_size = 50;
}

#! Cross-encoder re-ranker for two-stage retrieval
/** Provides precision-oriented re-ranking of retrieval candidates using a
    cross-encoder model. In two-stage retrieval, the first stage uses fast
    approximate nearest neighbor (ANN) search for high recall, and the second
    stage uses a cross-encoder to re-rank the candidates for high precision.

    Supports three backends:
    - \b HTTP: Calls a reranker API endpoint that accepts query/passages pairs
    - \b ONNX: Runs a local ONNX cross-encoder model through dense tensor inputs
    - \b SGLang: Calls an SGLang reranker server's \c /v1/rerank endpoint (query/documents)

    HTTP and SGLang modes split large candidate lists into request batches
    controlled by \c batch_size (default 50) and return scores in the original
    input order.

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
        #! HTTP / SGLang endpoint URL
        *string url;
        #! Authentication token
        *string api_key;
        #! SGLang mode: served reranker model name
        *string model;
        #! HTTP timeouts
        int connect_timeout_ms;
        int timeout_ms;
        #! Maximum candidate passages sent per HTTP / SGLang request
        int request_batch_size;
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
        #! Optional output tensor names requested from the ONNX model
        *list<string> output_names;
    }

    #! Creates a CrossEncoderReranker with the given configuration
    /** @param config a @ref RerankerConfig hash specifying the backend

        @throw RERANKER-CONFIG-ERROR if the mode is invalid or required parameters missing
        @throw RERANKER-DEPENDENCY-ERROR if required modules are not available
    */
    constructor(hash<RerankerConfig> config) {
        mode = config.mode;
        if (mode != "http" && mode != "onnx" && mode != "sglang") {
            throw "RERANKER-CONFIG-ERROR",
                sprintf("invalid mode %y; expected \"http\", \"onnx\", or \"sglang\"", mode);
        }

        if (mode == "http" || mode == "sglang") {
            if (!config.url.val()) {
                throw "RERANKER-CONFIG-ERROR",
                    sprintf("url is required for %s mode", mode == "sglang" ? "SGLang" : "HTTP");
            }
            url = config.url;
            api_key = config.api_key;
            model = config.model;
            connect_timeout_ms = config.connect_timeout_ms;
            timeout_ms = config.timeout_ms;
            if (config.batch_size < 1) {
                throw "RERANKER-CONFIG-ERROR",
                    sprintf("batch_size must be at least 1, got %d", config.batch_size);
            }
            request_batch_size = config.batch_size;
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
            if (exists config.output_names) {
                output_names = config.output_names;
                if (!output_names) {
                    throw "RERANKER-CONFIG-ERROR",
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
        if (mode == "sglang") {
            return scoreSglang(query, passages);
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
        \c relevance_score; for SGLang backends it is the server's per-document
        \c score.
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
        int pad_token_id = onnx_tokenizer.getPadTokenId();
        if (pad_token_id < 0) {
            pad_token_id = 0;
        }

        # Tokenize and run inference in sub-batches for memory efficiency.
        int batch_size = 16;
        list<float> all_scores = ();

        int total = passages.size();
        int offset = 0;
        while (offset < total) {
            int end = min(offset + batch_size, total);
            list<string> batch_passages = passages[offset .. end - 1];
            list<string> batch_queries = map query, range(batch_passages.size());

            hash<Tokenizer::TokenizerModelInputBatch> token_batch = onnx_tokenizer.encodeBatchPairsForModel(
                batch_queries, batch_passages, {
                    "max_length": max_length,
                    "truncation": "longest_first",
                    "padding": "batch_longest",
                    "pad_token_id": pad_token_id,
                });

            hash<auto> inputs = makeOnnxTokenTensorInputs(token_batch);

            hash<auto> result = onnx_model.runTensors(inputs, output_names);

            # Extract logits and apply the configured activation function.
            # Cross-encoder models typically output logits in a "logits" key
            # or a single unnamed output; handle both.
            auto logits = result.logits ?? result{result.firstKey()};
            foreach float score in (scoresFromLogits(logits)) {
                push all_scores, score;
            }

            offset = end;
        }

        return all_scores;
    }

    #! Converts ONNX logits to relevance scores while avoiding nested list materialization where possible
    private list<float> scoresFromLogits(auto logits) {
        if (logits instanceof ML::Tensor) {
            ML::Tensor tensor = cast<ML::Tensor>(logits);
            list<int> shape = tensor.getShape();
            bool simple = shape.lsize() == 1 || (shape.lsize() == 2 && shape[1] == 1);
            if (simple) {
                list<float> rv = ();
                auto values = tensor.getBuffer().toList();
                foreach auto value in (values) {
                    push rv, applyActivation(value.toFloat());
                }
                return rv;
            }
            return scoresFromListLogits(tensor.toList());
        }
        return scoresFromListLogits(logits);
    }

    #! Converts row/list logits to relevance scores using the established fallback semantics
    private list<float> scoresFromListLogits(auto logits) {
        list<float> rv = ();
        if (logits.typeCode() == NT_LIST) {
            foreach auto logit_row in (logits) {
                float raw;
                if (logit_row.typeCode() == NT_LIST) {
                    # Shape [batch, 1] or [batch, num_labels]
                    raw = logit_row[0].toFloat();
                } else {
                    raw = logit_row.toFloat();
                }
                push rv, applyActivation(raw);
            }
        }
        return rv;
    }

    #! Applies the configured scalar activation function
    private float applyActivation(float raw) {
        switch (activation) {
            case "sigmoid":
                return 1.0 / (1.0 + exp(-raw));
            case "none":
                return raw;
            case "softmax":
                # For single-output models, softmax degenerates to sigmoid.
                return 1.0 / (1.0 + exp(-raw));
            default:
                return 1.0 / (1.0 + exp(-raw));
        }
    }

    #! Builds ONNX Runtime tensor inputs from dense tokenizer buffers
    private hash<auto> makeOnnxTokenTensorInputs(hash<Tokenizer::TokenizerModelInputBatch> token_batch) {
        hash<auto> inputs = {};

        if (onnx_input_names.hasKey("input_ids")) {
            inputs{"input_ids"} = ML::Tensor::fromBuffer(token_batch.input_ids, token_batch.shape, True);
        }
        if (onnx_input_names.hasKey("attention_mask")) {
            inputs{"attention_mask"} = ML::Tensor::fromBuffer(token_batch.attention_mask, token_batch.shape, True);
        }
        if (onnx_input_names.hasKey("token_type_ids")) {
            inputs{"token_type_ids"} = ML::Tensor::fromBuffer(token_batch.token_type_ids, token_batch.shape, True);
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

    #! Scores via HTTP endpoint
    private list<float> scoreHttp(string query, list<string> passages) {
        if (passages.size() <= request_batch_size) {
            return scoreHttpBatch(query, passages);
        }

        list<float> rv = ();
        for (int offset = 0; offset < passages.size(); offset += request_batch_size) {
            int end = min(offset + request_batch_size, passages.size());
            foreach float score in (scoreHttpBatch(query, passages[offset .. end - 1])) {
                push rv, score;
            }
        }
        return rv;
    }

    #! Scores one HTTP request batch
    private list<float> scoreHttpBatch(string query, list<string> passages) {
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
            if (resp.body.scores.size() != passages.size()) {
                throw "RERANKER-ERROR",
                    sprintf("HTTP reranker returned %d scores for %d passages",
                        resp.body.scores.size(), passages.size());
            }
            return resp.body.scores;
        }

        # alternative format: {"results": [{"index": N, "relevance_score": F}, ...]}
        if (exists resp.body.results) {
            list<float> scores = ();
            for (int i = 0; i < passages.size(); ++i) {
                push scores, 0.0;
            }
            foreach hash<auto> hit in (resp.body.results) {
                int idx = hit."index";
                if (idx < 0 || idx >= passages.size()) {
                    throw "RERANKER-ERROR",
                        sprintf("HTTP reranker returned out-of-range index %d for %d "
                            "passages", idx, passages.size());
                }
                scores[idx] = (hit.relevance_score ?? hit.score ?? 0.0).toFloat();
            }
            return scores;
        }

        throw "RERANKER-ERROR", "unexpected response format from reranker endpoint";
    }

    #! Scores via an SGLang reranker server (POST /v1/rerank)
    /** Posts the SGLang/Jina-style rerank request \c {model, query, documents} and
        maps the returned hits — which the server may return in descending-score order —
        back to the input passage order, so the caller (and @ref rerankWithScores())
        sees \c scores[i] aligned to \c passages[i].

        @param query the search query
        @param passages candidate passages (documents) to score
        @return list of float relevance scores in the same order as @a passages

        @throw RERANKER-ERROR if the response shape is unrecognized or a returned
        index is out of range
    */
    private list<float> scoreSglang(string query, list<string> passages) {
        if (passages.size() <= request_batch_size) {
            return scoreSglangBatch(query, passages);
        }

        list<float> rv = ();
        for (int offset = 0; offset < passages.size(); offset += request_batch_size) {
            int end = min(offset + request_batch_size, passages.size());
            foreach float score in (scoreSglangBatch(query, passages[offset .. end - 1])) {
                push rv, score;
            }
        }
        return rv;
    }

    #! Scores one SGLang request batch
    private list<float> scoreSglangBatch(string query, list<string> passages) {
        ensureRestClient();

        hash<auto> body = {
            "query": query,
            "documents": passages,
        };
        if (model.val()) {
            body.model = model;
        }

        *hash<auto> hdr;
        if (api_key.val()) {
            hdr."Authorization" = "Bearer " + api_key;
        }

        hash<auto> resp = rest_client.restPost("/v1/rerank", body, hdr);

        # SGLang/Jina return either a top-level list of hits or {"results": [...]}
        auto hits;
        if (resp.body.typeCode() == NT_LIST) {
            hits = resp.body;
        } else if (exists resp.body.results) {
            hits = resp.body.results;
        } else {
            throw "RERANKER-ERROR",
                "unexpected response format from SGLang reranker endpoint";
        }

        # pre-fill input-ordered scores; the server may omit or reorder entries
        list<float> scores = ();
        for (int i = 0; i < passages.size(); ++i) {
            push scores, 0.0;
        }
        foreach hash<auto> hit in (hits) {
            int idx = hit."index";
            if (idx < 0 || idx >= passages.size()) {
                throw "RERANKER-ERROR",
                    sprintf("SGLang reranker returned out-of-range index %d for %d "
                        "passages", idx, passages.size());
            }
            scores[idx] = (hit.score ?? hit.relevance_score ?? 0.0).toFloat();
        }
        return scores;
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
