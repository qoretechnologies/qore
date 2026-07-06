# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreRagUtils module — HttpGatewayReranker definition

/** HttpGatewayReranker.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Gateway-backed reranker — calls an HTTP AI gateway \c /ai/rerank endpoint
/** Uses a generic @ref RestClientIo::RestClientIo "RestClientIo" to call the
    gateway-defined \c /ai/rerank route.

    @par Configuration

    - \c url (required): gateway base URL (the \c /ai/rerank path is appended)
    - \c api_key (optional): sent as \c Authorization: \c Bearer
    - \c model (optional): forwarded in the request body
    - \c batch_size (optional, default 50): maximum documents sent per request

    @par Expected response shape
    @code{.unparsed}
{
    "results": [
        {"index": 0, "score": 0.95, "document": "..."},
        ...
    ],
    "model": "...",
    "usage": {...}
}
    @endcode
*/
public class HttpGatewayReranker inherits AbstractReranker {
    private {
        #! Captured configuration
        hash<RerankerConfig> config;
        #! Lazily-created REST client
        *RestClientIo::RestClientIo rest_client;
        #! Guard for lazy initialization
        Mutex init_mutex();
        #! Maximum documents sent per gateway rerank request
        int batch_size;
    }

    #! Creates the reranker
    /** @param config reranker configuration; \c url must be set

        @throw RERANKER-CONFIG-ERROR if required fields are missing
    */
    constructor(hash<RerankerConfig> config) {
        if (!config.url.val()) {
            throw "RERANKER-CONFIG-ERROR",
                "HttpGatewayReranker requires 'url' in RerankerConfig";
        }
        if (config.batch_size < 1) {
            throw "RERANKER-CONFIG-ERROR",
                sprintf("batch_size must be at least 1, got %d", config.batch_size);
        }
        self.config = config;
        batch_size = config.batch_size;
    }

    #! Re-ranks a list of documents against a query and returns the top-K
    /** @param opts when non-empty, every key/value is forwarded into the
        request body so the upstream gateway sees vendor-specific options
        (e.g. \c max_chunks_per_doc, \c rank_fields, \c truncation) without
        the gateway client having to know about them; reserved keys
        (\c query, \c documents, \c top_k, \c model) cannot be overridden
        from \c opts
    */
    list<hash<RerankResult>> rerank(string query, list<string> documents, int top_k,
            *hash<auto> opts) {
        return doRerank(query, documents, top_k, opts).results;
    }

    hash<RerankOutcome> rerankEx(string query, list<string> documents, int top_k,
            *hash<auto> opts) {
        return doRerank(query, documents, top_k, opts);
    }

    #! Re-ranks all documents (batching as needed) returning results and accumulated usage
    private hash<RerankOutcome> doRerank(string query, list<string> documents,
            int top_k, *hash<auto> opts) {
        if (!documents || top_k <= 0) {
            return <RerankOutcome>{
                "results": (),
                "usage": AbstractReranker::estimateRerankUsage(query, documents),
            };
        }
        if (documents.size() <= batch_size) {
            return rerankBatch(query, documents, top_k, opts, 0);
        }

        list<hash<RerankResult>> rv = ();
        # accumulate provider-reported usage across batches; if any batch lacks
        # a usage block the whole call falls back to a single length estimate
        int provider_tokens = 0;
        bool all_provider = True;
        for (int offset = 0; offset < documents.size(); offset += batch_size) {
            int end = min(offset + batch_size, documents.size());
            hash<RerankOutcome> batch = rerankBatch(query,
                documents[offset .. end - 1], min(top_k, end - offset), opts,
                offset);
            foreach hash<RerankResult> hit in (batch.results) {
                push rv, hit;
            }
            if (batch.usage.token_source == "provider") {
                provider_tokens += batch.usage.total_tokens;
            } else {
                all_provider = False;
            }
        }

        rv = sort(rv, int sub(hash<RerankResult> a, hash<RerankResult> b) {
            return b.score <=> a.score;
        });
        if (rv) {
            rv = rv[0 .. min(top_k, rv.size()) - 1];
        }
        hash<RerankUsage> usage = all_provider
            ? <RerankUsage>{"total_tokens": provider_tokens, "token_source": "provider"}
            : AbstractReranker::estimateRerankUsage(query, documents);
        return <RerankOutcome>{
            "results": rv,
            "usage": usage,
        };
    }

    #! Re-ranks one gateway request batch and translates indices to the caller's document list
    private hash<RerankOutcome> rerankBatch(string query, list<string> documents, int top_k,
            *hash<auto> opts, int offset) {
        ensureRestClient();

        hash<auto> body = {};
        if (opts) {
            foreach hash<auto> opt in (opts.pairIterator()) {
                body{opt.key} = opt.value;
            }
        }
        body."query" = query;
        body."documents" = documents;
        body."top_k" = top_k;
        if (config.model.val()) {
            body."model" = config.model;
        }

        *hash<auto> hdr;
        if (config.api_key.val()) {
            hdr."Authorization" = "Bearer " + config.api_key;
        }

        hash<auto> resp = rest_client.restPost("/ai/rerank", body, hdr);

        list<hash<auto>> results = resp.body.results ?? ();
        list<hash<RerankResult>> rv = ();
        foreach hash<auto> item in (results) {
            int idx = item."index";
            if (idx < 0 || idx >= documents.size()) {
                throw "RERANKER-ERROR",
                    sprintf("gateway reranker returned out-of-range index %d for %d "
                        "documents", idx, documents.size());
            }
            string doc = item.document.typeCode() == NT_STRING
                ? item.document
                : documents[idx];
            push rv, <RerankResult>{
                "index": idx + offset,
                "score": item.score.toFloat(),
                "document": doc,
            };
        }
        # the documented gateway response shape includes an optional "usage"
        # block; use it when present, otherwise fall back to a length estimate
        *int total = resp.body.usage."total_tokens";
        hash<RerankUsage> usage = total.val()
            ? <RerankUsage>{"total_tokens": total, "token_source": "provider"}
            : AbstractReranker::estimateRerankUsage(query, documents);
        return <RerankOutcome>{
            "results": rv,
            "usage": usage,
        };
    }

    #! Returns the configured model name (if any)
    *string getModel() {
        return config.model;
    }

    #! Ensures the REST client is initialized
    private ensureRestClient() {
        if (!rest_client) {
            init_mutex.lock();
            on_exit init_mutex.unlock();
            if (!rest_client) {
                rest_client = new RestClientIo::RestClientIo({
                    "url": config.url,
                    "data": "json",
                });
            }
        }
    }
}
}
