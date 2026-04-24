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
    Voyage HTTP API, SGLang, etc.) and translate text to fixed-dimension
    dense vectors.

    Implementations must be thread-safe — multiple threads may call
    @ref encode() and @ref encodeBatch() concurrently.
*/
public class AbstractEmbedder {
    #! Encodes a single piece of text into a vector
    /** @param text the input text

        @return a dense vector of floats with length equal to @ref getDimension()
    */
    abstract list<float> encode(string text);

    #! Encodes a batch of texts into vectors
    /** @param texts the input texts

        @return a list of vectors in the same order as the input
    */
    abstract list<list<float>> encodeBatch(list<string> texts);

    #! Returns the embedding dimension produced by this backend
    /** Used by callers to size vector store columns / Qdrant collections.

        @return the vector dimension
    */
    abstract int getDimension();

    #! Returns the model name in use (informational)
    *string getModel() {
        return NOTHING;
    }
}
}
