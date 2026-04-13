# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreRagUtils module — AbstractReranker definition

/** AbstractReranker.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Abstract base class for cross-encoder rerankers
/** Concrete subclasses wrap a specific backend (local ONNX cross-encoder,
    Voyage rerank API, gateway HTTP endpoint, etc.) and score a list of
    candidate documents against a query.

    Implementations must be thread-safe.

    @since QoreRagUtils 1.0
*/
public class AbstractReranker {
    #! Re-ranks a list of documents against a query and returns the top-K
    /** @param query the query text
        @param documents the candidate documents (typically from a first-stage vector search)
        @param top_k how many results to return after reranking; if greater than
            \c documents.size(), all documents are returned

        @return list of @ref RerankResult hashes ordered by descending score; the
            \c index field maps each result back to its position in the input
    */
    abstract list<hash<RerankResult>> rerank(string query, list<string> documents, int top_k);

    #! Returns the model name in use (informational)
    *string getModel() {
        return NOTHING;
    }
}
}
