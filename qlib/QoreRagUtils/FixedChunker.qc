# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreRagUtils module — FixedChunker definition

/** FixedChunker.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Fixed-size chunker — splits by character count derived from token count
/** Emits chunks of approximately \c target_tokens worth of characters
    (using a 4-chars-per-token heuristic), with \c overlap_tokens of
    overlap between consecutive chunks for context preservation.

    This chunker respects no natural boundaries — use
    @ref StructuredChunker when paragraph integrity matters.

    @since QoreRagUtils 1.0
*/
public class FixedChunker inherits AbstractChunker {
    public {
        #! Characters-per-token heuristic used to convert token budgets to character counts
        const CharsPerToken = 4;
    }

    private {
        #! Captured configuration
        hash<ChunkerConfig> config;
    }

    #! Creates the chunker
    /** @param config chunker configuration; \c target_tokens controls the
            per-chunk size (default 512) and \c overlap_tokens controls the
            inter-chunk overlap (default 50)
    */
    constructor(hash<ChunkerConfig> config) {
        self.config = config;
    }

    #! Splits content into fixed-size chunks with overlap
    list<hash<ChunkInfo>> chunk(string content, *hash<auto> metadata) {
        if (!content.val()) {
            return ();
        }

        int chunk_chars = config.target_tokens * CharsPerToken;
        int overlap_chars = config.overlap_tokens * CharsPerToken;
        if (chunk_chars < 1) {
            chunk_chars = 1;
        }
        if (overlap_chars < 0) {
            overlap_chars = 0;
        }
        if (overlap_chars >= chunk_chars) {
            # guarantee forward progress
            overlap_chars = chunk_chars - 1;
        }
        int step = chunk_chars - overlap_chars;

        int total = content.length();
        list<hash<ChunkInfo>> chunks = ();
        int index = 0;
        int offset = 0;

        while (offset < total) {
            int end = offset + chunk_chars;
            if (end > total) {
                end = total;
            }
            string piece = content.substr(offset, end - offset);
            hash<auto> chunk_metadata = (metadata ?? {}) + {
                "chunk_index": index,
                "char_start": offset,
                "char_end": end,
            };
            push chunks, <ChunkInfo>{
                "index": index,
                "content": piece,
                "token_count": approximateTokens(piece),
                "metadata": chunk_metadata,
            };
            ++index;
            if (end >= total) {
                break;
            }
            offset += step;
        }

        return chunks;
    }

    #! Returns "fixed"
    string getStrategy() {
        return "fixed";
    }
}
}
