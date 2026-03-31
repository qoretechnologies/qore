# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreEmbedTextProcessor class definition

/** QoreEmbedTextProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Pipeline processor that enriches text records with dense embedding vectors
/** Accumulates records in a window and batch-embeds the specified text field using
    @ref QoreTokenizerUtils::EmbeddingModel "EmbeddingModel". Each record is enriched
    with a dense embedding vector in a new `embedding` field.

    Supports both HTTP (OpenAI-compatible endpoint) and ONNX backends for embedding
    generation. Records are batched for efficiency; the batch is flushed when
    `window_size` is reached or at pipeline end.

    This is a @ref DPC_AGGREGATE processor because it buffers records and emits
    them in batches.

    @par Example:
    @code{.py}
QoreEmbedTextProcessor proc({
    "text_field": "body",
    "window_size": 32,
    "embedding_mode": "http",
    "embedding_url": "http://localhost:30000",
    "embedding_model": "bge-m3",
});
    @endcode

*/
public class QoreEmbedTextProcessor inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreEmbedTextProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_AGGREGATE,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = {
            "text_field": <DataProviderOptionInfo>{
                "display_name": "Text Field",
                "short_desc": "Which text field to embed",
                "desc": "The name of the text field in each record to generate embeddings for. "
                    "The embedding vector is stored in a new `embedding` field on each record.",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "input_field_names": True,
            },
            "window_size": <DataProviderOptionInfo>{
                "display_name": "Batch Size",
                "short_desc": "Batch size for embedding calls",
                "desc": "How many records to accumulate before sending a batch embedding request. "
                    "Larger batches improve throughput but use more memory.\n\n"
                    "**Tip**: `32` is a good default; increase for high-throughput pipelines",
                "type": AbstractDataProviderTypeMap."int",
                "default_value": 32,
            },
            "embedding_mode": <DataProviderOptionInfo>{
                "display_name": "Embedding Mode",
                "short_desc": "How to generate embeddings",
                "desc": "The backend to use for generating embeddings:\n\n"
                    "- `http` — call an OpenAI-compatible `/v1/embeddings` HTTP endpoint\n"
                    "- `onnx` — run a local ONNX embedding model (requires the ml module)",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "http",
                        "display_name": "HTTP Endpoint",
                        "desc": "Call an OpenAI-compatible embedding endpoint (SGLang, vLLM, "
                            "OpenAI, etc.)",
                    },
                    <AllowedValueInfo>{
                        "value": "onnx",
                        "display_name": "Local ONNX Model",
                        "desc": "Run a local ONNX embedding model via the ml module",
                    },
                ),
            },
            "embedding_url": <DataProviderOptionInfo>{
                "display_name": "Embedding URL",
                "short_desc": "URL of embedding endpoint (for HTTP mode)",
                "desc": "The base URL of the embedding service endpoint. Required when "
                    "`embedding_mode` is `http`.\n\n"
                    "Example: `http://localhost:30000`",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "embedding_model": <DataProviderOptionInfo>{
                "display_name": "Embedding Model Name",
                "short_desc": "Model name (for HTTP mode)",
                "desc": "The model name to pass in HTTP embedding requests. Some endpoints "
                    "require this to select the model.\n\n"
                    "Example: `bge-m3`, `text-embedding-3-small`",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "embedding_api_key": <DataProviderOptionInfo>{
                "display_name": "API Key",
                "short_desc": "API key for embedding endpoint",
                "desc": "Optional Bearer token for authenticating with the embedding endpoint. "
                    "Required for cloud-hosted services like OpenAI.",
                "type": AbstractDataProviderTypeMap."*string",
                "sensitive": True,
            },
        };
    }

    private {
        #! Configured text field name
        string text_field;

        #! Batch size for embedding calls
        int window_size;

        #! Embedding mode ("http" or "onnx")
        string embedding_mode;

        #! Embedding URL (for HTTP mode)
        *string embedding_url;

        #! Model name (for HTTP mode)
        *string embedding_model;

        #! API key (for HTTP mode)
        *string embedding_api_key;

        #! Lazy-initialized EmbeddingModel
        *QoreTokenizerUtils::EmbeddingModel emb_model;

        #! Accumulated records
        list<hash<auto>> buffer;

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
        window_size = copts.window_size ?? 32;
        if (window_size < 1) {
            throw "CONSTRUCTOR-ERROR",
                sprintf("window_size must be >= 1; got %d", window_size);
        }
        embedding_mode = copts.embedding_mode;
        if (embedding_mode != "http" && embedding_mode != "onnx") {
            throw "CONSTRUCTOR-ERROR",
                sprintf("embedding_mode must be \"http\" or \"onnx\"; got %y", embedding_mode);
        }
        if (embedding_mode == "http" && !copts.embedding_url.val()) {
            throw "CONSTRUCTOR-ERROR",
                "embedding_url is required when embedding_mode is \"http\"";
        }
        embedding_url = copts.embedding_url;
        embedding_model = copts.embedding_model;
        embedding_api_key = copts.embedding_api_key;
        buffer = ();
        configured = True;
    }

    #! Returns the name of this data provider
    string getName() {
        return "embed-text";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Turns text into numeric vectors that capture its meaning, so similar texts "
            "end up close together — use after chunking to make documents searchable by "
            "storing vectors in Qdrant, pgvector, or another vector database";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Accumulates records; emits embedded records when window is full
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "QoreEmbedTextProcessor: not configured; provide constructor options";
        }
        push buffer, record;
        if (buffer.lsize() >= window_size) {
            list<auto> results = processWindow(buffer);
            buffer = ();
            return results;
        }
        return;
    }

    #! Flushes remaining buffered data
    private *list<auto> flushRecordsImpl() {
        if (!configured || !buffer) {
            return;
        }
        list<auto> results = processWindow(buffer);
        buffer = ();
        return results;
    }

    #! Processes a window of records: batch-embeds and enriches each record
    private list<auto> processWindow(list<hash<auto>> records) {
        ensureEmbeddingModel();

        # extract texts from each record
        list<string> texts = ();
        foreach hash<auto> rec in (records) {
            string text =
                DataProvider::AbstractDataProvider::getFieldValue(rec, text_field).toString();
            push texts, text;
        }

        # batch embed
        list<list<float>> embeddings = emb_model.embedBatch(texts);

        # emit one enriched record per input record
        list<auto> results = ();
        for (int i = 0; i < records.lsize(); ++i) {
            hash<auto> result = {} + records[i];
            result.embedding = embeddings[i];
            push results, result;
        }
        return results;
    }

    #! Ensures the EmbeddingModel is initialized (lazy creation to avoid connecting in constructor)
    private ensureEmbeddingModel() {
        if (!emb_model) {
            emb_model = new QoreTokenizerUtils::EmbeddingModel(<QoreTokenizerUtils::EmbeddingConfig>{
                "mode": embedding_mode,
                "url": embedding_url,
                "model": embedding_model,
                "api_key": embedding_api_key,
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

    #! Returns NOTHING — output type is dynamic (input record enriched with embedding)
    private *DataProvider::AbstractDataProviderType getPipelineProcessorOutputTypeImpl() {
        return NOTHING;
    }
}
}
