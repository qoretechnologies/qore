# -*- mode: qore; indent-tabs-mode: nil -*-

/*  StructuredDocumentChunker.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! @see StructuredDocumentChunker
public namespace QoreTokenizerUtils {

#! Metadata-carrying text block for structured document input
/** Used by StructuredDocumentChunker to accept text blocks from format-specific
    extractors (PDF pages, DOCX paragraphs, PPTX slides, etc.) along with their
    provenance metadata.
*/
public hashdecl TextBlockInfo {
    #! The text content of this block
    string content;
    #! Optional metadata from the extraction stage (page, slide, heading_path, etc.)
    *hash<auto> metadata;
}

#! Output chunk from StructuredDocumentChunker with text, tokens, and metadata
public hashdecl StructuredChunkInfo {
    #! The text content of this chunk
    string text;
    #! Token IDs for this chunk (for direct embedding model input)
    list<int> input_ids;
    #! Number of tokens in this chunk
    int token_count;
    #! Zero-based sequential chunk index
    int chunk_index;
    #! Metadata inherited from the source text block
    *hash<auto> metadata;
}

#! Structure-aware document chunker with accurate token counting
/** Extends @ref DocumentChunker with paragraph merging, sentence-level splitting,
    and metadata propagation. Accepts structured text blocks (with metadata) from
    format-specific extractors and produces token-bounded chunks that respect
    document structure.

    The chunking algorithm:
    1. Splits input text blocks on double newlines into paragraphs
    2. Accumulates paragraphs until the token budget is reached
    3. For oversized paragraphs, falls back to sentence-level splitting
    4. Applies configurable overlap by backing up through paragraphs
    5. Each chunk inherits metadata from its source block

    Unlike the base @ref DocumentChunker (which uses the tokenizer's sliding
    window on flat text), this class preserves paragraph boundaries and
    document metadata.

    @par Example: Chunk extracted text blocks
    @code{.py}
hash<auto> config = parse_json(ReadOnlyFile::readTextFile("tokenizer.json"));
StructuredDocumentChunker chunker(config, {"chunk_size": 512, "chunk_overlap": 50});

list<hash<TextBlockInfo>> blocks = (
    {"content": "First paragraph.\n\nSecond paragraph.", "metadata": {"page": 1}},
    {"content": "Third paragraph on next page.", "metadata": {"page": 2}},
);

list<hash<StructuredChunkInfo>> chunks = chunker.chunkTextBlocks(blocks);
foreach hash<StructuredChunkInfo> chunk in (chunks) {
    printf("Chunk %d: %d tokens, page %y\n",
        chunk.chunk_index, chunk.token_count, chunk.metadata.page);
}
    @endcode

    @since QoreTokenizerUtils 1.1
*/
public class StructuredDocumentChunker inherits DocumentChunker {
    private {
        #! Token counter for accurate paragraph-level token estimation
        TokenCounter counter;

        #! Regex for sentence-ending punctuation followed by whitespace
        const SentenceSplitRegex = "([.!?]+)\\s+";
    }

    #! Creates a StructuredDocumentChunker with the given tokenizer configuration
    /** @param tokenizer_config a parsed `tokenizer.json` hash (from `parse_json()`)
        @param options optional hash with the same options as @ref DocumentChunker plus:
        - \c chunk_size (int): maximum tokens per chunk (default: `512`)
        - \c chunk_overlap (int): overlapping tokens between chunks (default: `50`)
        - \c add_special_tokens (bool): whether to include special tokens (default: `True`)

        @throw DOCUMENT-CHUNKER-ERROR if chunk_size <= 0 or chunk_overlap >= chunk_size
    */
    constructor(hash<auto> tokenizer_config, *hash<auto> options)
            : DocumentChunker(tokenizer_config, options + {"chunk_overlap": options.chunk_overlap ?? 50}) {
        counter = new TokenCounter(tokenizer_config);
    }

    #! Chunks structured text blocks into token-bounded chunks with metadata
    /** Accepts text blocks from format-specific extractors (PDF, DOCX, PPTX, etc.)
        and produces token-bounded chunks that preserve paragraph boundaries and
        propagate metadata from source blocks.

        @param text_blocks list of @ref TextBlockInfo hashes with content and metadata
        @return list of @ref StructuredChunkInfo hashes

        @par Algorithm:
        - Splits each block on double newlines into paragraphs
        - Accumulates paragraphs until chunk_size is reached
        - Oversized paragraphs are split by sentence boundaries
        - Overlap is applied by backing up through paragraphs
        - Metadata is inherited from the first paragraph in each chunk
    */
    list<hash<StructuredChunkInfo>> chunkTextBlocks(list<hash<TextBlockInfo>> text_blocks) {
        # merge all text blocks into paragraphs, carrying metadata
        list<hash<auto>> paragraphs = ();
        foreach hash<TextBlockInfo> block in (text_blocks) {
            list<string> parts = block.content.split("\n\n");
            foreach string part in (parts) {
                string trimmed = trim(part);
                if (trimmed.val()) {
                    push paragraphs, {
                        "text": trimmed,
                        "tokens": counter.countTokens(trimmed),
                        "metadata": block.metadata,
                    };
                }
            }
        }

        if (!paragraphs) {
            return ();
        }

        int target_tokens = getChunkSize();
        int overlap_tokens = getChunkOverlap();

        list<hash<StructuredChunkInfo>> result = ();
        int chunk_index = 0;
        int i = 0;

        while (i < paragraphs.size()) {
            string chunk_text = "";
            int chunk_tokens = 0;
            *hash<auto> chunk_metadata = paragraphs[i].metadata;
            int start_i = i;

            # accumulate paragraphs until we reach target
            while (i < paragraphs.size() && chunk_tokens < target_tokens) {
                # if a single paragraph exceeds target, split by sentences
                if (paragraphs[i].tokens > target_tokens && !chunk_text.val()) {
                    list<hash<StructuredChunkInfo>> sentence_chunks = splitBySentences(
                        paragraphs[i].text, paragraphs[i].metadata, target_tokens, \chunk_index);
                    result += sentence_chunks;
                    ++i;
                    break;
                }

                if (chunk_text.val()) {
                    chunk_text += "\n\n";
                }

                chunk_text += paragraphs[i].text;
                chunk_tokens += paragraphs[i].tokens;
                ++i;
            }

            if (chunk_text.val()) {
                # get accurate token count and IDs for the assembled chunk
                hash<auto> enc = getEncoding(chunk_text);
                push result, <StructuredChunkInfo>{
                    "text": chunk_text,
                    "input_ids": enc.input_ids,
                    "token_count": enc.input_ids.size(),
                    "chunk_index": chunk_index++,
                    "metadata": chunk_metadata,
                };
            }

            # apply overlap: back up by overlap_tokens worth of paragraphs
            if (i < paragraphs.size() && overlap_tokens > 0) {
                int overlap_used = 0;
                int backup = i - 1;
                while (backup > start_i && overlap_used < overlap_tokens) {
                    overlap_used += paragraphs[backup].tokens;
                    --backup;
                }
                if (backup + 1 < i) {
                    i = backup + 1;
                }
            }
        }

        return result;
    }

    #! Splits a large paragraph into sentence-bounded chunks
    private list<hash<StructuredChunkInfo>> splitBySentences(string text, *hash<auto> metadata,
            int target_tokens, reference<int> chunk_index) {
        # split on sentence-ending punctuation followed by whitespace
        string delimited = regex_subst(text, SentenceSplitRegex, "$1\x01", RE_Global);
        list<string> sentences = delimited.split("\x01");
        if (!sentences || sentences.size() < 2) {
            # single sentence or unsplittable — use parent's flat chunking as last resort
            list<hash<auto>> flat_chunks = chunkText(text);
            list<hash<StructuredChunkInfo>> result = ();
            foreach hash<auto> fc in (flat_chunks) {
                push result, <StructuredChunkInfo>{
                    "text": fc.text,
                    "input_ids": fc.input_ids,
                    "token_count": fc.token_count,
                    "chunk_index": chunk_index++,
                    "metadata": metadata,
                };
            }
            return result;
        }

        list<hash<StructuredChunkInfo>> result = ();
        string current = "";
        int current_tokens = 0;

        foreach string sent in (sentences) {
            string trimmed = trim(sent);
            if (!trimmed.val()) {
                continue;
            }
            int sent_tokens = counter.countTokens(trimmed);

            if (current_tokens + sent_tokens > target_tokens && current.val()) {
                hash<auto> enc = getEncoding(current);
                push result, <StructuredChunkInfo>{
                    "text": current,
                    "input_ids": enc.input_ids,
                    "token_count": enc.input_ids.size(),
                    "chunk_index": chunk_index++,
                    "metadata": metadata,
                };
                current = "";
                current_tokens = 0;
            }

            if (current.val()) {
                current += " ";
            }
            current += trimmed;
            current_tokens += sent_tokens;
        }

        if (current.val()) {
            hash<auto> enc = getEncoding(current);
            push result, <StructuredChunkInfo>{
                "text": current,
                "input_ids": enc.input_ids,
                "token_count": enc.input_ids.size(),
                "chunk_index": chunk_index++,
                "metadata": metadata,
            };
        }

        return result;
    }

    #! Encodes text and returns the encoding hash (input_ids, etc.)
    /** Uses the parent's HFTokenizer to produce input_ids for the chunk.
    */
    private hash<auto> getEncoding(string text) {
        return tokenizer.encode(text);
    }
}

} # namespace QoreTokenizerUtils
