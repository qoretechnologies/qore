# -*- mode: qore; indent-tabs-mode: nil -*-

/*  RagTelemetry.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Provider-neutral helpers for RAG observability
/** These helpers summarize retrieval, ingest, and rerank activity without
    exposing raw queries, documents, chunks, or vector content.

    @note This class is policy-free: it does not calculate costs, make access
        control decisions, or persist telemetry.

    @par Example
    @code{.py}
hash<auto> request = RagTelemetry::retrieveRequestSummary(
    query, 5, 25, True, NOTHING);
    @endcode
*/
public class RagTelemetry {
    #! Dispatches a RAG telemetry event to an observer
    /** @param observer optional observer callbacks
        @param event event to dispatch

        @note Observer exceptions are caught and ignored.
    */
    static notify(*hash<RagTelemetryObserver> observer,
            hash<RagTelemetryEvent> event) {
        if (!observer) {
            return;
        }
        callObserver(observer.event_callback, event);
        switch (event.type) {
            case "start":
                callObserver(observer.start_callback, event);
                break;
            case "end":
                callObserver(observer.end_callback, event);
                break;
            case "error":
                callObserver(observer.error_callback, event);
                break;
        }
    }

    #! Builds safe ingest request metadata
    /** @param doc_id caller-supplied document id
        @param content document content; only length-derived counts are returned
        @param metadata optional metadata hash

        @return safe ingest request summary with no document content
    */
    static hash<auto> ingestRequestSummary(string doc_id, string content,
            *hash<auto> metadata) {
        return {
            "doc_id": doc_id,
            "content_bytes": content.length(),
            "estimated_tokens": estimateTextTokens(content),
            "has_metadata": metadata ? True : False,
        };
    }

    #! Builds safe retrieval request metadata
    /** @param query retrieval query; only length-derived counts are returned
        @param final_k final result count requested by the caller
        @param search_k vector-search candidate count
        @param rerank_enabled True when a reranker will be used
        @param filter optional backend-specific filter

        @return safe retrieval request summary with no query content
    */
    static hash<auto> retrieveRequestSummary(string query, int final_k,
            int search_k, bool rerank_enabled, *hash<auto> filter) {
        return {
            "query_bytes": query.length(),
            "estimated_query_tokens": estimateTextTokens(query),
            "top_k": final_k,
            "search_k": search_k,
            "rerank_enabled": rerank_enabled,
            "has_filter": filter ? True : False,
        };
    }

    #! Builds safe rerank request metadata
    /** @param query rerank query; only length-derived counts are returned
        @param documents candidate documents; only aggregate counts are returned
        @param top_k requested result count

        @return safe rerank request summary with no query or document content
    */
    static hash<auto> rerankRequestSummary(string query,
            list<string> documents, int top_k) {
        int bytes = 0;
        foreach string doc in (documents) {
            bytes += doc.length();
        }
        return {
            "query_bytes": query.length(),
            "document_count": documents.size(),
            "document_bytes": bytes,
            "estimated_tokens": estimateTextTokens(query)
                + estimateTextTokensByBytes(bytes),
            "top_k": top_k,
        };
    }

    #! Creates an error hash suitable for telemetry
    /** @param ex caught exception info

        @return safe error metadata with selected non-sensitive exception
            argument fields
    */
    static hash<auto> errorInfo(hash<ExceptionInfo> ex) {
        hash<auto!> rv = {
            "err": ex.err,
            "desc": ex.desc,
        };
        if (ex.arg.typeCode() == NT_HASH) {
            hash<auto> arg = cast<hash<auto>>(ex.arg);
            list<string> arg_keys = keys arg;
            foreach string key in (("status_code", "code", "type",
                    "retryable", "backend", "model", "collection")) {
                if (inlist(key, arg_keys)) {
                    rv{key} = arg{key};
                }
            }
        }
        return rv;
    }

    #! Estimates token count using the module-wide 4-byte heuristic
    /** @param text text to estimate

        @return estimated token count
    */
    static int estimateTextTokens(string text) {
        return estimateTextTokensByBytes(text.length());
    }

    #! Estimates token count from a byte count
    /** @param bytes byte count

        @return estimated token count
    */
    static int estimateTextTokensByBytes(int bytes) {
        return (bytes + 3) / 4;
    }

    #! Sums chunk token counts
    /** @param chunks chunk metadata list

        @return sum of chunk token counts
    */
    static int chunkTokenCount(list<hash<ChunkInfo>> chunks) {
        int rv = 0;
        foreach hash<ChunkInfo> chunk in (chunks) {
            rv += chunk.token_count;
        }
        return rv;
    }

    private static callObserver(
            *code<nothing(hash<RagTelemetryEvent>)> cb,
            hash<RagTelemetryEvent> event) {
        if (!cb) {
            return;
        }
        try {
            cb(event);
        } catch () {
        }
    }
}

} # namespace QoreRagUtils
