# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreRagUtils module — AbstractEmbedder definition

/** AbstractEmbedder.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Abstract base class for text embedders
/** Concrete subclasses wrap a specific backend (local ONNX, OpenAI HTTP API,
    Voyage HTTP API, SGLang, BGE-M3 FastAPI, etc.) and translate text to
    fixed-dimension dense vectors.

    Implementations must be thread-safe — multiple threads may call
    @ref encode() and @ref encodeBatch() concurrently.

    @par Per-call options

    @ref encode() and @ref encodeBatch() accept an optional \c opts hash that
    can override per-call defaults; HTTP-backed embedders recognize:

    - \c timeout (\c *timeout) — request timeout for this call only
    - \c connect_timeout (\c *timeout) — connection timeout for this call only

    Precedence: per-call \c opts > @ref EmbedderConfig defaults > REST client
    / connection default.

    @par Permanent vs transient backend errors

    HTTP backends classify their own \c REST-RESPONSE-ERROR exceptions via the
    private @ref isPermanentEmbedderConfigError() override.  When that returns
    @ref True (e.g. unknown model, model is not an embedding model), the
    embedder rethrows as @c EMBEDDER-MODEL-CONFIG-ERROR via
    @ref throwEmbedderModelConfigError() so callers can distinguish permanent
    misconfiguration from transient infrastructure failures (5xx, timeouts,
    rate limits) without parsing backend-specific JSON shapes.
*/
public class AbstractEmbedder {
    #! Encodes a single piece of text into a vector
    /** @param text the input text
        @param opts optional per-call options; recognized HTTP keys are
        \c timeout and \c connect_timeout (both \c *timeout).  Non-HTTP
        backends ignore \c opts.

        @return a dense vector of floats with length equal to @ref getDimension()
    */
    abstract list<float> encode(string text, *hash<auto> opts);

    #! Encodes a batch of texts into vectors
    /** @param texts the input texts
        @param opts optional per-call options; recognized HTTP keys are
        \c timeout and \c connect_timeout (both \c *timeout).  Non-HTTP
        backends ignore \c opts.

        @return a list of vectors in the same order as the input
    */
    abstract list<list<float>> encodeBatch(list<string> texts, *hash<auto> opts);

    #! Returns the embedding dimension produced by this backend
    /** Used by callers to size vector store columns / Qdrant collections.

        @return the vector dimension
    */
    abstract int getDimension();

    #! Returns the model name in use (informational)
    *string getModel() {
        return NOTHING;
    }

    #! Classifies a backend exception as a permanent model/configuration failure
    /** Concrete subclasses for HTTP backends override this to recognize
        backend-specific permanent-failure shapes (e.g. unknown model,
        model is not an embedding model, body type \c BadRequestError or
        \c InvalidModelError).  The default implementation returns @ref False
        so that any exception propagates as a transient REST error unless the
        subclass classifies it.

        Called from each subclass's @ref encodeBatch() catch block; when this
        returns @ref True, the subclass calls
        @ref throwEmbedderModelConfigError() to rethrow as
        @c EMBEDDER-MODEL-CONFIG-ERROR with a normalized \c arg shape.

        @param ex the original exception (typically \c REST-RESPONSE-ERROR)

        @return @ref True if the error is permanent (block create / surface
        misconfiguration); @ref False if transient (warn and continue)
    */
    private bool isPermanentEmbedderConfigError(hash<ExceptionInfo> ex) {
        return False;
    }

    #! Rethrows a backend exception as a normalized \c EMBEDDER-MODEL-CONFIG-ERROR
    /** Subclasses call this from their @ref encodeBatch() catch block once
        @ref isPermanentEmbedderConfigError() has returned @ref True.

        The thrown exception preserves the original error metadata so callers
        can construct a usable error message:

        - \c arg.backend — backend identifier (e.g. \c "openai", \c "voyage")
        - \c arg.model — model name (or @ref NOTHING)
        - \c arg.status_code — HTTP status code from the original error
        - \c arg.body — response body from the original error
        - \c arg.original_err — \c ex.err of the original exception
        - \c arg.original_desc — \c ex.desc of the original exception

        @param orig the original \c REST-RESPONSE-ERROR exception
        @param backend backend identifier
        @param model model name (or @ref NOTHING)

        @throw EMBEDDER-MODEL-CONFIG-ERROR always
    */
    private throwEmbedderModelConfigError(hash<ExceptionInfo> orig, string backend, *string model) {
        *int status_code = orig.arg.status_code;
        auto body = orig.arg.body;
        throw "EMBEDDER-MODEL-CONFIG-ERROR",
            sprintf("%s: model %y rejected by backend (HTTP %s): %s",
                backend, model ?? "<unset>",
                exists status_code ? string(status_code) : "?",
                orig.desc),
            {
                "backend": backend,
                "model": model,
                "status_code": status_code,
                "body": body,
                "original_err": orig.err,
                "original_desc": orig.desc,
            };
    }
}
}
