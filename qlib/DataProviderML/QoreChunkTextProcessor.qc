# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreChunkTextProcessor class definition

/** QoreChunkTextProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the DataProviderML module
public namespace DataProviderML {
#! Pipeline processor that splits text records into token-bounded chunks
/** For each input record, splits the specified text field into overlapping
    token-bounded chunks using @ref QoreTokenizerUtils::DocumentChunker "DocumentChunker".
    Each chunk becomes a separate output record containing the original record fields
    plus chunk metadata.

    This is a @ref DPC_AGGREGATE processor because it produces multiple output records
    per input record (one per chunk).

    @par Example:
    @code{.py}
QoreChunkTextProcessor proc({
    "text_field": "body",
    "chunk_size": 512,
    "chunk_overlap": 64,
    "tokenizer_config_path": "/path/to/tokenizer.json",
});
    @endcode

*/
public class QoreChunkTextProcessor inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreChunkTextProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_AGGREGATE,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = {
            "text_field": <DataProviderOptionInfo>{
                "display_name": "Text Field",
                "short_desc": "Which text field to chunk",
                "desc": "The name of the text field in each record to split into chunks. "
                    "Each output record will contain the original record fields plus "
                    "`chunk_text`, `chunk_index`, and `chunk_token_count`.",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "input_field_names": True,
            },
            "chunk_size": <DataProviderOptionInfo>{
                "display_name": "Chunk Size",
                "short_desc": "Maximum tokens per chunk",
                "desc": "The maximum number of tokens in each output chunk. Smaller chunks "
                    "improve search precision (finding the exact relevant passage), while larger "
                    "chunks preserve more surrounding context.",
                "type": AbstractDataProviderTypeMap."int",
                "default_value": 512,
                "allowed_values_creatable": True,
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": 256,
                        "display_name": "Small (256)",
                        "desc": "Best for short-answer retrieval and small embedding models — "
                            "high precision, less context per chunk",
                    },
                    <AllowedValueInfo>{
                        "value": 512,
                        "display_name": "Standard (512)",
                        "desc": "Good balance of precision and context — works well with most "
                            "embedding models including bge-m3 and OpenAI",
                    },
                    <AllowedValueInfo>{
                        "value": 1024,
                        "display_name": "Large (1024)",
                        "desc": "Preserves more surrounding context per chunk — useful for "
                            "documents where paragraphs are long and context matters",
                    },
                ),
            },
            "chunk_overlap": <DataProviderOptionInfo>{
                "display_name": "Chunk Overlap",
                "short_desc": "Shared tokens between consecutive chunks",
                "desc": "The number of tokens shared between consecutive chunks to prevent "
                    "information loss at chunk boundaries. A sentence that spans a boundary "
                    "will appear in both chunks.",
                "type": AbstractDataProviderTypeMap."int",
                "default_value": 64,
                "allowed_values_creatable": True,
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": 0,
                        "display_name": "None (0)",
                        "desc": "No overlap — each chunk is independent; fastest but may miss "
                            "information at boundaries",
                    },
                    <AllowedValueInfo>{
                        "value": 32,
                        "display_name": "Small (32)",
                        "desc": "Minimal overlap for sentence continuity at boundaries",
                    },
                    <AllowedValueInfo>{
                        "value": 64,
                        "display_name": "Standard (64)",
                        "desc": "Good default — about 12%% of a 512-token chunk, enough to "
                            "capture most boundary sentences",
                    },
                    <AllowedValueInfo>{
                        "value": 128,
                        "display_name": "Large (128)",
                        "desc": "Generous overlap for maximum context preservation at the cost "
                            "of more chunks and storage",
                    },
                ),
            },
            "tokenizer_config_path": <DataProviderOptionInfo>{
                "display_name": "Tokenizer Config Path",
                "short_desc": "Path to HuggingFace tokenizer.json file",
                "desc": "The filesystem path to a HuggingFace `tokenizer.json` configuration "
                    "file. This file defines the vocabulary and tokenization rules used to "
                    "measure chunk boundaries accurately for a specific model.",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
        };
    }

    private {
        #! Configured text field name
        string text_field;

        #! Maximum tokens per chunk
        int chunk_size;

        #! Overlapping tokens between chunks
        int chunk_overlap;

        #! Path to the tokenizer config
        string tokenizer_config_path;

        #! Lazy-initialized DocumentChunker
        *QoreTokenizerUtils::DocumentChunker chunker;

        #! Whether configured
        bool configured = False;
    }

    #! Creates the object with no options (for data provider navigation)
    constructor() {
    }

    #! Creates the object with the given options
    constructor(hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        text_field = copts.text_field;
        if (!text_field.val()) {
            throw "CONSTRUCTOR-ERROR", "text_field option is required";
        }
        tokenizer_config_path = copts.tokenizer_config_path;
        if (!tokenizer_config_path.val()) {
            throw "CONSTRUCTOR-ERROR", "tokenizer_config_path option is required";
        }
        chunk_size = copts.chunk_size ?? 512;
        if (chunk_size <= 0) {
            throw "CONSTRUCTOR-ERROR",
                sprintf("chunk_size must be > 0; got %d", chunk_size);
        }
        chunk_overlap = copts.chunk_overlap ?? 64;
        if (chunk_overlap < 0) {
            throw "CONSTRUCTOR-ERROR",
                sprintf("chunk_overlap must be >= 0; got %d", chunk_overlap);
        }
        if (chunk_overlap >= chunk_size) {
            throw "CONSTRUCTOR-ERROR",
                sprintf("chunk_overlap (%d) must be less than chunk_size (%d)",
                    chunk_overlap, chunk_size);
        }
        configured = True;
    }

    #! Returns the name of this data provider
    string getName() {
        return "chunk-text";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Breaks long documents into overlapping pieces sized for AI search — each "
            "input record produces one or more output records with chunk text, position, "
            "and token count, ready for embedding and vector store insertion";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Processes a single record: chunks the text field and emits multiple output records
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "QoreChunkTextProcessor: not configured; provide constructor options";
        }

        ensureChunker();

        string text =
            DataProvider::AbstractDataProvider::getFieldValue(record, text_field).toString();

        list<hash<auto>> chunks = chunker.chunkText(text);

        # emit one output record per chunk
        list<auto> results = ();
        foreach hash<auto> chunk in (chunks) {
            hash<auto> result = {} + record;
            result.chunk_text = chunk.text;
            result.chunk_index = chunk.chunk_index;
            result.chunk_token_count = chunk.token_count;
            push results, result;
        }
        return results;
    }

    #! Flushes remaining buffered data — no buffering, so returns NOTHING
    private *list<auto> flushRecordsImpl() {
        return NOTHING;
    }

    #! Ensures the DocumentChunker is initialized (lazy creation to avoid file I/O in constructor)
    private ensureChunker() {
        if (!chunker) {
            hash<auto> tokenizer_config =
                parse_json(ReadOnlyFile::readTextFile(tokenizer_config_path));
            chunker = new QoreTokenizerUtils::DocumentChunker(tokenizer_config, {
                "chunk_size": chunk_size,
                "chunk_overlap": chunk_overlap,
            });
        }
    }

    #! Does not support bulk API
    private bool supportsBulkApiImpl() {
        return False;
    }

    #! Returns the processor capability bitfield
    private int getProcessorCapabilitiesImpl() {
        return DPC_AGGREGATE;
    }

    #! Returns NOTHING — output type is dynamic (input record enriched with chunk fields)
    private *DataProvider::AbstractDataProviderType getPipelineProcessorOutputTypeImpl() {
        return NOTHING;
    }
}
}
