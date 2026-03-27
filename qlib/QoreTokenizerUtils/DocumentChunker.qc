# -*- mode: qore; indent-tabs-mode: nil -*-

/*  DocumentChunker.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! @see DocumentChunker
public namespace QoreTokenizerUtils {

#! Splits documents into overlapping token-bounded chunks for RAG ingestion
/** Uses the HuggingFace tokenizer's sliding window to split long text into chunks
    that fit within an embedding model's or LLM's context window. Each chunk overlaps
    with its neighbors to preserve context at boundaries.

    Typical use case: preparing documents for vector store ingestion in a RAG pipeline.

    @par Example: Split a document for embedding
    @code{.py}
hash<auto> config = parse_json(ReadOnlyFile::readTextFile("tokenizer.json"));

# 512-token chunks with 64-token overlap (suitable for most embedding models)
DocumentChunker chunker(config, {"chunk_size": 512, "chunk_overlap": 64});

list<hash<auto>> chunks = chunker.chunkText(document_text);
foreach hash<auto> chunk in (chunks) {
    # Send chunk.text to embedding model
    # Or use chunk.input_ids directly for ONNX inference
    vector = embed(chunk.text);
    store_in_vector_db(vector, chunk.text, chunk.start_offset, chunk.end_offset);
}
    @endcode

    @par Example: Process multiple documents
    @code{.py}
DocumentChunker chunker(config, {"chunk_size": 256, "chunk_overlap": 32});

foreach string doc in (documents) {
    list<hash<auto>> chunks = chunker.chunkText(doc);
    printf("Document split into %d chunks\n", chunks.size());
}
    @endcode
*/
public class DocumentChunker {
    private {
        #! The underlying HuggingFace tokenizer
        Tokenizer::HFTokenizer tokenizer;

        #! Maximum tokens per chunk (including special tokens)
        int chunk_size;

        #! Number of overlapping tokens between consecutive chunks
        int chunk_overlap;

        #! Whether to add special tokens (e.g., [CLS], [SEP]) to each chunk
        bool add_special_tokens;
    }

    #! Creates a DocumentChunker with the given tokenizer configuration
    /** @param tokenizer_config a parsed `tokenizer.json` hash (from `parse_json()`)
        @param options optional hash with:
        - \c chunk_size (int): maximum tokens per chunk, including special tokens
          (default: `512`; typical values: `256` for small embeddings, `512` for
          standard models, `8192` for long-context models)
        - \c chunk_overlap (int): number of tokens shared between consecutive chunks
          to preserve context at boundaries (default: `64`; typically 10–20% of
          chunk_size)
        - \c add_special_tokens (bool): whether to include special tokens like `[CLS]`
          and `[SEP]` in each chunk (default: `True`)

        @throw DOCUMENT-CHUNKER-ERROR if chunk_size <= 0 or chunk_overlap >= chunk_size
    */
    constructor(hash<auto> tokenizer_config, *hash<auto> options) {
        tokenizer = new Tokenizer::HFTokenizer(tokenizer_config);
        chunk_size = options.chunk_size ?? 512;
        chunk_overlap = options.chunk_overlap ?? 64;
        add_special_tokens = options.add_special_tokens ?? True;

        if (chunk_size <= 0) {
            throw "DOCUMENT-CHUNKER-ERROR",
                sprintf("chunk_size must be > 0; got %d", chunk_size);
        }
        if (chunk_overlap < 0) {
            throw "DOCUMENT-CHUNKER-ERROR",
                sprintf("chunk_overlap must be >= 0; got %d", chunk_overlap);
        }
        if (chunk_overlap >= chunk_size) {
            throw "DOCUMENT-CHUNKER-ERROR",
                sprintf("chunk_overlap (%d) must be less than chunk_size (%d)",
                    chunk_overlap, chunk_size);
        }
    }

    #! Splits text into overlapping chunks bounded by token count
    /** Each returned chunk contains the original text substring, its token IDs,
        token count, and byte-level offsets into the original document.

        @param text the document text to chunk
        @return list of chunk hashes, each with:
        - \c text (string): the text content of this chunk
        - \c input_ids (list<int>): token IDs for this chunk
        - \c token_count (int): number of tokens in this chunk
        - \c start_offset (int): byte offset of chunk start in the original text
        - \c end_offset (int): byte offset of chunk end in the original text
        - \c chunk_index (int): zero-based index of this chunk

        @par Example:
        @code{.py}
list<hash<auto>> chunks = chunker.chunkText("Very long document text...");
# chunks[0].text = "Very long document..."
# chunks[0].token_count = 512
# chunks[0].start_offset = 0
# chunks[1].start_offset = <overlapping position>
        @endcode
    */
    list<hash<auto>> chunkText(string text) {
        if (!text.size()) {
            return ();
        }

        # Use tokenizer's sliding window
        int stride = chunk_size - chunk_overlap;
        hash<auto> enc = tokenizer.encodeAdvanced(text, {
            "max_length": chunk_size,
            "truncation": "longest_first",
            "stride": stride,
            "return_overflowing_tokens": True,
            "add_special_tokens": add_special_tokens,
        });

        list<hash<auto>> chunks();

        # First chunk
        push chunks, buildChunk(enc, text, 0);

        # Overflow chunks (from sliding window)
        if (exists enc.overflowing) {
            int idx = 1;
            foreach hash<auto> overflow in (enc.overflowing) {
                push chunks, buildChunk(overflow, text, idx);
                ++idx;
            }
        }

        return chunks;
    }

    #! Returns the chunk size setting
    /** @return maximum tokens per chunk
    */
    int getChunkSize() {
        return chunk_size;
    }

    #! Returns the chunk overlap setting
    /** @return number of overlapping tokens between chunks
    */
    int getChunkOverlap() {
        return chunk_overlap;
    }

    #! Builds a chunk hash from an encoding result
    private hash<auto> buildChunk(hash<auto> encoding, string original_text, int idx) {
        list<int> input_ids = encoding.input_ids;
        int token_count = input_ids.size();

        # Determine byte offsets from offset_mapping
        int start_offset = 0;
        int end_offset = original_text.size();
        if (exists encoding.offset_mapping && encoding.offset_mapping.size() > 0) {
            # Find first non-special token offset
            for (int i = 0; i < encoding.offset_mapping.size(); ++i) {
                list<int> offsets = encoding.offset_mapping[i];
                if (offsets[0] != 0 || offsets[1] != 0) {
                    start_offset = offsets[0];
                    break;
                }
            }
            # Find last non-special token offset
            for (int i = encoding.offset_mapping.size() - 1; i >= 0; --i) {
                list<int> offsets = encoding.offset_mapping[i];
                if (offsets[0] != 0 || offsets[1] != 0) {
                    end_offset = offsets[1];
                    break;
                }
            }
        }

        # Extract the text substring
        string chunk_text;
        if (end_offset > start_offset && end_offset <= original_text.size()) {
            chunk_text = original_text.substr(start_offset, end_offset - start_offset);
        } else {
            chunk_text = original_text;
        }

        return {
            "text": chunk_text,
            "input_ids": input_ids,
            "token_count": token_count,
            "start_offset": start_offset,
            "end_offset": end_offset,
            "chunk_index": idx,
        };
    }
}

} # namespace QoreTokenizerUtils
