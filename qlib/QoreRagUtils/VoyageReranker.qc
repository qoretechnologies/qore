# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreRagUtils module — VoyageReranker definition

/** VoyageReranker.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Voyage AI-backed reranker — calls the Voyage \c /rerank endpoint
/** Wraps @ref VoyageRestClient::VoyageRestClientIo "VoyageRestClientIo".

    @par Configuration

    - \c api_key (required): Voyage API key; passed via the \c apikey option
    - \c url (optional): override the default Voyage URL
    - \c model (optional): model name, default \c "rerank-2"
*/
public class VoyageReranker inherits AbstractReranker {
    public {
        #! Default model name
        const DefaultModel = "rerank-2";
    }

    private {
        #! Captured configuration
        hash<RerankerConfig> config;
        #! Resolved model name
        string model_name;
        #! Lazily-created REST client
        *VoyageRestClient::VoyageRestClientIo rest_client;
        #! Guard for lazy initialization
        Mutex init_mutex();
    }

    #! Creates the reranker
    /** @param config reranker configuration; \c api_key must be set

        @throw RERANKER-CONFIG-ERROR if required fields are missing
    */
    constructor(hash<RerankerConfig> config) {
        if (!config.api_key.val()) {
            throw "RERANKER-CONFIG-ERROR",
                "VoyageReranker requires 'api_key' in RerankerConfig";
        }
        self.config = config;
        model_name = config.model ?? DefaultModel;
    }

    #! Re-ranks a list of documents against a query and returns the top-K
    list<hash<RerankResult>> rerank(string query, list<string> documents, int top_k) {
        if (!documents) {
            return ();
        }
        ensureRestClient();

        hash<auto> body = {
            "model": model_name,
            "query": query,
            "documents": documents,
            "top_k": top_k,
            "return_documents": True,
        };

        hash<auto> resp = rest_client.restPost("/rerank", body);

        list<hash<auto>> data = resp.body.data ?? ();
        list<hash<RerankResult>> result = ();
        foreach hash<auto> item in (data) {
            int idx = item."index";
            string doc = item.document.typeCode() == NT_STRING
                ? item.document
                : (exists item.document.text ? item.document.text : documents[idx]);
            push result, <RerankResult>{
                "index": idx,
                "score": item.relevance_score.toFloat(),
                "document": doc,
            };
        }
        return result;
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
