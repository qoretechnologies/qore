# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreRagUtils module — AbstractChunker definition

/** AbstractChunker.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Abstract base class for document chunkers
/** Concrete subclasses split text content into chunks using a specific
    strategy (structured, semantic, fixed-size).

    Implementations must be thread-safe.

    @since QoreRagUtils 1.0
*/
public class AbstractChunker {
    #! Splits content into chunks
    /** @param content the source document text
        @param metadata optional metadata that should be inherited by every chunk
            (page, heading, slide, source filename, etc.)

        @return list of @ref ChunkInfo hashes; chunk indices are 0-based and
            ascending in source order
    */
    abstract list<hash<ChunkInfo>> chunk(string content, *hash<auto> metadata);

    #! Returns the strategy name (e.g. "structured", "semantic", "fixed")
    abstract string getStrategy();

    #! Approximate token count for a string
    /** Default implementation uses a 4-chars-per-token heuristic; subclasses
        backed by a real tokenizer should override.
    */
    int approximateTokens(string text) {
        return (text.length() / 4) ?? 0;
    }
}
}
