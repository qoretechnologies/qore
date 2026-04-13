# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreRagUtils module — StructuredChunker definition

/** StructuredChunker.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Paragraph-oriented chunker — groups paragraphs into chunks under a token budget
/** Splits content on blank lines (double newlines) and accumulates
    paragraphs into chunks until approximately \c target_tokens is reached.
    Paragraph boundaries are preserved — a paragraph is never split.

    Paragraphs that individually exceed \c target_tokens are emitted as
    their own chunk (no forced splitting in this strategy; use
    @ref FixedChunker for hard limits).

    @since QoreRagUtils 1.0
*/
public class StructuredChunker inherits AbstractChunker {
    private {
        #! Captured configuration
        hash<ChunkerConfig> config;
    }

    #! Creates the chunker
    /** @param config chunker configuration; \c target_tokens controls the
            target chunk size (default 512)
    */
    constructor(hash<ChunkerConfig> config) {
        self.config = config;
    }

    #! Splits content into paragraph-grouped chunks
    list<hash<ChunkInfo>> chunk(string content, *hash<auto> metadata) {
        if (!content.val()) {
            return ();
        }

        # normalize any blank-line separator to a canonical "\n\n", then split
        string normalized = regex_subst(content, "\n[ \t]*\n(?:[ \t]*\n)*", "\n\n",
            Qore::RE_Global);
        list<string> paragraphs = ();
        foreach string part in (normalized.split("\n\n")) {
            string trimmed = part;
            trimmed =~ s/^\s+//;
            trimmed =~ s/\s+$//;
            if (trimmed.val()) {
                push paragraphs, trimmed;
            }
        }

        if (!paragraphs) {
            return ();
        }

        int target = config.target_tokens;
        list<hash<ChunkInfo>> chunks = ();
        list<string> current = ();
        int current_tokens = 0;
        int chunk_index = 0;

        foreach string para in (paragraphs) {
            int para_tokens = approximateTokens(para);

            # if adding this paragraph would exceed the target and we have
            # accumulated content, flush the current chunk first
            if (current && current_tokens + para_tokens > target) {
                push chunks, buildChunk(chunk_index, current, current_tokens, metadata);
                ++chunk_index;
                current = ();
                current_tokens = 0;
            }

            push current, para;
            current_tokens += para_tokens;
        }

        if (current) {
            push chunks, buildChunk(chunk_index, current, current_tokens, metadata);
        }

        return chunks;
    }

    #! Returns "structured"
    string getStrategy() {
        return "structured";
    }

    #! Builds a single ChunkInfo from accumulated paragraphs
    private hash<ChunkInfo> buildChunk(int index, list<string> paras, int token_count,
            *hash<auto> metadata) {
        string content = paras.join("\n\n");
        hash<auto> chunk_metadata = (metadata ?? {}) + {"chunk_index": index};
        return <ChunkInfo>{
            "index": index,
            "content": content,
            "token_count": token_count,
            "metadata": chunk_metadata,
        };
    }
}
}
