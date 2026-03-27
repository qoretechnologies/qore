# -*- mode: qore; indent-tabs-mode: nil -*-

/*  SemanticDocumentChunker.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! @see SemanticDocumentChunker
public namespace QoreTokenizerUtils {

#! Chunks documents at semantic boundaries detected by embedding similarity
/** Extends @ref StructuredDocumentChunker by using an embedding model to detect
    topic boundaries. Instead of splitting purely on token count, it:

    1. Splits the document into sentences
    2. Groups sentences into small segments (configurable size)
    3. Embeds each segment using the provided @ref EmbeddingModel
    4. Computes cosine similarity between adjacent segments
    5. Identifies breakpoints where similarity drops below a threshold
    6. Merges segments between breakpoints into chunks, respecting the token budget
    7. Falls back to token-bounded splitting for oversized groups

    This produces chunks that are more semantically coherent than fixed-size splitting,
    improving retrieval quality in RAG pipelines.

    @par Example: Semantic chunking with HTTP embeddings
    @code{.py}
hash<auto> tok_config = parse_json(ReadOnlyFile::readTextFile("tokenizer.json"));
hash<EmbeddingConfig> emb_config = {
    "mode": "http",
    "url": "http://localhost:30000",
    "model": "bge-m3",
};
SemanticDocumentChunker chunker(tok_config, emb_config, {
    "chunk_size": 512,
    "similarity_threshold": 0.5,
});

list<hash<StructuredChunkInfo>> chunks = chunker.chunkTextSemantic(long_document);
    @endcode

    @since QoreTokenizerUtils 1.1
*/
public class SemanticDocumentChunker inherits StructuredDocumentChunker {
    private {
        #! Embedding model for segment similarity computation
        EmbeddingModel embedder;

        #! Cosine similarity threshold for breakpoint detection
        float similarity_threshold;

        #! Number of sentences per segment for embedding
        int segment_size;

        #! Regex for sentence splitting
        const SentenceRegex = "([.!?]+)\\s+";
    }

    #! Creates a SemanticDocumentChunker
    /** @param tokenizer_config a parsed `tokenizer.json` hash
        @param embedding_config an @ref EmbeddingConfig hash for the embedding model
        @param options optional hash with:
        - \c chunk_size (int): maximum tokens per chunk (default: `512`)
        - \c chunk_overlap (int): overlapping tokens between chunks (default: `50`)
        - \c segment_size (int): sentences per segment for embedding (default: `3`)
        - \c similarity_threshold (float): breakpoint threshold; lower values produce
          more breakpoints and smaller chunks (default: `0.5`)
        - \c add_special_tokens (bool): whether to include special tokens (default: `True`)

        @throw DOCUMENT-CHUNKER-ERROR if chunk parameters are invalid
        @throw EMBEDDING-CONFIG-ERROR if embedding config is invalid
    */
    constructor(hash<auto> tokenizer_config, hash<EmbeddingConfig> embedding_config,
            *hash<auto> options)
            : StructuredDocumentChunker(tokenizer_config, options) {
        embedder = new EmbeddingModel(embedding_config);
        similarity_threshold = options.similarity_threshold ?? 0.5;
        segment_size = options.segment_size ?? 3;
        if (segment_size < 1) {
            throw "DOCUMENT-CHUNKER-ERROR",
                sprintf("segment_size must be >= 1; got %d", segment_size);
        }
    }

    #! Chunks text using semantic boundary detection
    /** @param text the document text to chunk
        @return list of @ref StructuredChunkInfo hashes with semantically coherent chunks
    */
    list<hash<StructuredChunkInfo>> chunkTextSemantic(string text) {
        if (!text.val()) {
            return ();
        }

        # 1. Split into sentences
        list<string> sentences = splitSentences(text);
        if (sentences.size() <= 1) {
            # too few sentences — fall back to parent's flat chunking
            return chunkTextBlocks((
                <TextBlockInfo>{"content": text},
            ));
        }

        # 2. Group into segments
        list<string> segments = groupSegments(sentences);
        if (segments.size() <= 1) {
            return chunkTextBlocks((
                <TextBlockInfo>{"content": text},
            ));
        }

        # 3. Embed all segments
        list<list<float>> embeddings = embedder.embedBatch(segments);

        # 4. Compute cosine similarity between adjacent segments
        list<float> similarities = ();
        for (int i = 0; i < embeddings.size() - 1; ++i) {
            push similarities, cosineSimilarity(embeddings[i], embeddings[i + 1]);
        }

        # 5. Identify breakpoints (similarity below threshold or local minima)
        list<int> breakpoints = findBreakpoints(similarities);

        # 6. Merge segments between breakpoints into chunks
        return buildChunksFromBreakpoints(segments, breakpoints);
    }

    #! Splits text into sentences
    private list<string> splitSentences(string text) {
        string delimited = regex_subst(text, SentenceRegex, "$1\x00", RE_Global);
        list<string> raw = delimited.split("\x00");
        list<string> result = ();
        foreach string s in (raw) {
            string trimmed = trim(s);
            if (trimmed.val()) {
                push result, trimmed;
            }
        }
        return result;
    }

    #! Groups sentences into segments of segment_size
    private list<string> groupSegments(list<string> sentences) {
        list<string> segments = ();
        for (int i = 0; i < sentences.size(); i += segment_size) {
            int end = min(i + segment_size, sentences.size());
            string segment = "";
            for (int j = i; j < end; ++j) {
                if (segment.val()) {
                    segment += " ";
                }
                segment += sentences[j];
            }
            push segments, segment;
        }
        return segments;
    }

    #! Finds breakpoints where similarity drops below threshold
    private list<int> findBreakpoints(list<float> similarities) {
        list<int> breaks = ();
        for (int i = 0; i < similarities.size(); ++i) {
            if (similarities[i] < similarity_threshold) {
                push breaks, i + 1;  # breakpoint is AFTER segment i
            }
        }
        return breaks;
    }

    #! Builds chunks from segments using breakpoints
    private list<hash<StructuredChunkInfo>> buildChunksFromBreakpoints(
            list<string> segments, list<int> breakpoints) {
        # add start and end sentinels
        list<auto> boundaries = (0,) + breakpoints + (segments.size(),);

        list<hash<StructuredChunkInfo>> result = ();
        int chunk_index = 0;
        int target_tokens = getChunkSize();

        for (int i = 0; i < boundaries.size() - 1; ++i) {
            int start = boundaries[i];
            int end = boundaries[i + 1];

            # merge segments in this group
            string group_text = "";
            for (int j = start; j < end; ++j) {
                if (group_text.val()) {
                    group_text += " ";
                }
                group_text += segments[j];
            }

            if (!group_text.val()) {
                continue;
            }

            # if group fits in budget, emit as one chunk
            hash<auto> enc = getEncoding(group_text);
            if (enc.input_ids.size() <= target_tokens) {
                push result, <StructuredChunkInfo>{
                    "text": group_text,
                    "input_ids": enc.input_ids,
                    "token_count": enc.input_ids.size(),
                    "chunk_index": chunk_index++,
                };
            } else {
                # oversized group — fall back to token-bounded splitting
                list<hash<auto>> sub_chunks = chunkText(group_text);
                foreach hash<auto> sc in (sub_chunks) {
                    push result, <StructuredChunkInfo>{
                        "text": sc.text,
                        "input_ids": sc.input_ids,
                        "token_count": sc.token_count,
                        "chunk_index": chunk_index++,
                    };
                }
            }
        }

        return result;
    }

    #! Computes cosine similarity between two vectors
    private static float cosineSimilarity(list<float> a, list<float> b) {
        float dot = 0.0;
        float norm_a = 0.0;
        float norm_b = 0.0;

        for (int i = 0; i < a.size(); ++i) {
            dot += a[i] * b[i];
            norm_a += a[i] * a[i];
            norm_b += b[i] * b[i];
        }

        float denom = sqrt(norm_a) * sqrt(norm_b);
        if (denom == 0.0) {
            return 0.0;
        }
        return dot / denom;
    }
}

} # namespace QoreTokenizerUtils
