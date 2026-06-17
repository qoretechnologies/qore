# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreRagUtils module — SGLangReranker definition

/** SGLangReranker.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! SGLang reranker — wraps @ref QoreTokenizerUtils::CrossEncoderReranker "CrossEncoderReranker" in \c "sglang" mode
/** Calls an SGLang reranker server's \c /v1/rerank endpoint through the
    underlying @ref QoreTokenizerUtils::CrossEncoderReranker "CrossEncoderReranker"
    class in \c "sglang" mode.  The SGLang server must be launched with a
    cross-encoder reranker model and \c --is-embedding; it serves exactly one
    model per process, so the GPU does the scoring and no model is loaded
    in-process.

    @par Configuration

    - \c url (required): base URL of the SGLang reranker server (e.g.
      \c "http://host:30800"); the \c /v1/rerank path is appended automatically
    - \c model (optional): served reranker model name, sent in the request body
    - \c api_key (optional): bearer token for SGLang deployments with
      authentication enabled

    @note Reuses the same \c /v1/rerank wire contract and response parsing as the
    @ref QoreTokenizerUtils::CrossEncoderReranker "CrossEncoderReranker" engine,
    so the in-process gateway path and this RAG-backend path stay in sync.
*/
public class SGLangReranker inherits AbstractReranker {
    private {
        #! Captured configuration
        hash<RerankerConfig> config;
        #! Lazily-created reranker engine
        *QoreTokenizerUtils::CrossEncoderReranker reranker;
        #! Guard for lazy initialization
        Mutex init_mutex();
    }

    #! Creates the reranker
    /** @param config reranker configuration; \c url must be set

        @throw RERANKER-CONFIG-ERROR if required fields are missing
    */
    constructor(hash<RerankerConfig> config) {
        if (!config.url.val()) {
            throw "RERANKER-CONFIG-ERROR",
                "SGLangReranker requires 'url' in RerankerConfig";
        }
        self.config = config;
    }

    #! Re-ranks a list of documents against a query and returns the top-K
    /** @param opts ignored by the SGLang backend — the served model and
        endpoint are fixed at construction time
    */
    list<hash<RerankResult>> rerank(string query, list<string> documents, int top_k,
            *hash<auto> opts) {
        if (!documents) {
            return ();
        }
        ensureReranker();

        # rerankWithScores() already sorts by descending score and truncates to
        # top_k; map its hits to RerankResult, echoing the source document text
        list<hash<QoreTokenizerUtils::RerankerScoredHit>> hits =
            reranker.rerankWithScores(query, documents, top_k);

        list<hash<RerankResult>> result = ();
        foreach hash<QoreTokenizerUtils::RerankerScoredHit> hit in (hits) {
            push result, <RerankResult>{
                "index": hit.index,
                "score": hit.score,
                "document": documents[hit.index],
            };
        }
        return result;
    }

    #! Returns the configured served model name (if any)
    *string getModel() {
        return config.model;
    }

    #! Ensures the underlying CrossEncoderReranker is created
    private ensureReranker() {
        if (!reranker) {
            init_mutex.lock();
            on_exit init_mutex.unlock();
            if (!reranker) {
                reranker = new QoreTokenizerUtils::CrossEncoderReranker(
                    <QoreTokenizerUtils::RerankerConfig>{
                        "mode": "sglang",
                        "url": config.url,
                        "model": config.model,
                        "api_key": config.api_key,
                    });
            }
        }
    }
}
}
