# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreRagUtils module — LocalOnnxReranker definition

/** LocalOnnxReranker.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Local ONNX reranker — wraps @ref QoreTokenizerUtils::CrossEncoderReranker "CrossEncoderReranker"
/** Runs a local ONNX cross-encoder reranker via the underlying
    @ref QoreTokenizerUtils::CrossEncoderReranker "CrossEncoderReranker"
    class in \c "onnx" mode.

    @par Configuration

    - \c model_path (required): filesystem path to the ONNX cross-encoder model file
    - \c options.tokenizer_config (required): tokenizer JSON hash
    - \c options.max_length (optional): maximum query+passage tokens; default 512
    - \c options.activation (optional): \c "sigmoid" (default), \c "softmax", or \c "none"
*/
public class LocalOnnxReranker inherits AbstractReranker {
    private {
        #! Captured configuration
        hash<RerankerConfig> config;
        #! Lazily-created reranker
        *QoreTokenizerUtils::CrossEncoderReranker reranker;
        #! Guard for lazy initialization
        Mutex init_mutex();
    }

    #! Creates the reranker
    /** @param config reranker configuration; \c model_path must be set

        @throw RERANKER-CONFIG-ERROR if required fields are missing
    */
    constructor(hash<RerankerConfig> config) {
        if (!config.model_path.val()) {
            throw "RERANKER-CONFIG-ERROR",
                "LocalOnnxReranker requires 'model_path' in RerankerConfig";
        }
        self.config = config;
    }

    #! Re-ranks a list of documents against a query and returns the top-K
    list<hash<RerankResult>> rerank(string query, list<string> documents, int top_k) {
        if (!documents) {
            return ();
        }
        ensureReranker();

        # CrossEncoderReranker::rerank() returns a list of top indices;
        # use score() instead so we have scores to include in the result
        list<float> scores = reranker.score(query, documents);

        list<hash<auto>> scored = ();
        for (int i = 0; i < scores.size(); ++i) {
            push scored, {"index": i, "score": scores[i]};
        }
        scored = sort(scored, int sub(hash<auto> a, hash<auto> b) {
            return b.score <=> a.score;
        });

        int limit = min(top_k, scored.size());
        list<hash<RerankResult>> result = ();
        for (int i = 0; i < limit; ++i) {
            int idx = scored[i]."index";
            push result, <RerankResult>{
                "index": idx,
                "score": scored[i].score,
                "document": documents[idx],
            };
        }
        return result;
    }

    #! Returns the model name (basename of \c model_path)
    *string getModel() {
        return basename(config.model_path);
    }

    #! Ensures the underlying CrossEncoderReranker is created
    private ensureReranker() {
        if (!reranker) {
            init_mutex.lock();
            on_exit init_mutex.unlock();
            if (!reranker) {
                hash<QoreTokenizerUtils::RerankerConfig> cfg = <QoreTokenizerUtils::RerankerConfig>{
                    "mode": "onnx",
                    "model_path": config.model_path,
                    "tokenizer_config": config.options.tokenizer_config,
                };
                if (config.options.max_length) {
                    cfg.max_length = config.options.max_length;
                }
                if (config.options.activation.val()) {
                    cfg.activation = config.options.activation;
                }
                reranker = new QoreTokenizerUtils::CrossEncoderReranker(cfg);
            }
        }
    }
}
}
