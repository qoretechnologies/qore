# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreSemanticChunkProcessor class definition

/** QoreSemanticChunkProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Pipeline processor that splits text into semantically coherent chunks
/** For each input record, splits the specified text field into chunks at
    semantic boundaries detected by embedding similarity using
    @ref QoreTokenizerUtils::SemanticDocumentChunker "SemanticDocumentChunker".
    Each chunk becomes a separate output record.

    This is a @ref DPC_AGGREGATE processor because it produces multiple output
    records per input record (one per semantic chunk).

    @par Example:
    @code{.py}
QoreSemanticChunkProcessor proc({
    "text_field": "body",
    "tokenizer_config_path": "/path/to/tokenizer.json",
    "embedding_mode": "http",
    "embedding_url": "http://localhost:30000",
    "embedding_model": "bge-m3",
});
    @endcode

*/
public class QoreSemanticChunkProcessor inherits AbstractDataProviderMLProcessor {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreSemanticChunkProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_AGGREGATE,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = {
            "text_field": <DataProviderOptionInfo>{
                "display_name": "Text Field",
                "short_desc": "Which text field to chunk semantically",
                "desc": "The name of the text field in each record to split into semantic "
                    "chunks. Each output record will contain the original record fields "
                    "plus `chunk_text`, `chunk_index`, and `chunk_token_count`.",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "input_field_names": True,
            },
            "tokenizer_config_path": <DataProviderOptionInfo>{
                "display_name": "Tokenizer Config Path",
                "short_desc": "Path to HuggingFace tokenizer.json",
                "desc": "Filesystem path to a HuggingFace `tokenizer.json` file used for "
                    "token counting and sentence segmentation. Provide either this option "
                    "or `registry_model_name` to load the tokenizer from QoreModelRegistry.",
                "type": AbstractDataProviderTypeMap."string",
                "required": False,
            },
            "chunk_size": <DataProviderOptionInfo>{
                "display_name": "Chunk Size",
                "short_desc": "Maximum tokens per chunk",
                "desc": "Maximum number of tokens per semantic chunk. Semantic boundaries "
                    "are preferred, but chunks will not exceed this limit.",
                "type": AbstractDataProviderTypeMap."int",
                "required": False,
                "default_value": 512,
            },
            "chunk_overlap": <DataProviderOptionInfo>{
                "display_name": "Chunk Overlap",
                "short_desc": "Token overlap between chunks",
                "desc": "Number of overlapping tokens between consecutive chunks.",
                "type": AbstractDataProviderTypeMap."int",
                "required": False,
                "default_value": 50,
            },
            "similarity_threshold": <DataProviderOptionInfo>{
                "display_name": "Similarity Threshold",
                "short_desc": "Cosine similarity threshold for breakpoints",
                "desc": "Cosine similarity threshold below which a semantic boundary is "
                    "detected. Lower values create fewer, larger chunks. Range: 0.0 to 1.0.",
                "type": AbstractDataProviderTypeMap."float",
                "required": False,
                "default_value": 0.5,
            },
            "segment_size": <DataProviderOptionInfo>{
                "display_name": "Segment Size",
                "short_desc": "Sentences per segment for similarity",
                "desc": "Number of sentences grouped into each segment for embedding "
                    "similarity comparison. Larger values smooth out sentence-level noise.",
                "type": AbstractDataProviderTypeMap."int",
                "required": False,
                "default_value": 3,
            },
            "embedding_mode": <DataProviderOptionInfo>{
                "display_name": "Embedding Mode",
                "short_desc": "How to generate embeddings",
                "desc": "The embedding backend mode for semantic similarity:\n"
                    "- `http` — Call an HTTP embedding endpoint\n"
                    "- `onnx` — Run a local ONNX embedding model",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "http",
                        "display_name": "HTTP Endpoint",
                        "desc": "Call an OpenAI-compatible embedding API endpoint",
                    },
                    <AllowedValueInfo>{
                        "value": "onnx",
                        "display_name": "Local ONNX Model",
                        "desc": "Run a local ONNX embedding model",
                    },
                ),
            },
            "embedding_url": <DataProviderOptionInfo>{
                "display_name": "Embedding URL",
                "short_desc": "URL of the embedding endpoint",
                "desc": "Base URL of the embedding service. Required for `http` mode.",
                "type": AbstractDataProviderTypeMap."string",
                "required": False,
            },
            "embedding_model": <DataProviderOptionInfo>{
                "display_name": "Embedding Model",
                "short_desc": "Model name for the embedding endpoint",
                "desc": "Model name to pass in the embedding request (e.g., `bge-m3`).",
                "type": AbstractDataProviderTypeMap."string",
                "required": False,
            },
            "embedding_api_key": <DataProviderOptionInfo>{
                "display_name": "Embedding API Key",
                "short_desc": "API key for embedding authentication",
                "desc": "Optional API key for authenticating with the embedding endpoint.",
                "type": AbstractDataProviderTypeMap."string",
                "required": False,
                "sensitive": True,
            },
            "embedding_model_path": <DataProviderOptionInfo>{
                "display_name": "Embedding ONNX Model Path",
                "short_desc": "Path to the ONNX embedding model",
                "desc": "Filesystem path to the ONNX embedding model file. "
                    "Required for `onnx` mode unless `registry_model_name` is provided.",
                "type": AbstractDataProviderTypeMap."string",
                "required": False,
            },
            "registry_model_name": <DataProviderOptionInfo>{
                "display_name": "Registry Model Name",
                "short_desc": "Registry package containing embedding artifacts",
                "desc": "QoreModelRegistry model/package name containing `tokenizer.json` "
                    "and, for ONNX embedding mode, an ONNX embedding model artifact.",
                "type": AbstractDataProviderTypeMap."string",
            },
            "registry_model_version": <DataProviderOptionInfo>{
                "display_name": "Registry Model Version",
                "short_desc": "Registry model version or latest",
                "desc": "Optional registry model version ID. Omit or use `latest` to load "
                    "the most recent package version.",
                "type": AbstractDataProviderTypeMap."string",
            },
            "registry_model_artifact_name": <DataProviderOptionInfo>{
                "display_name": "Registry ONNX Artifact",
                "short_desc": "Named ONNX artifact in the package",
                "desc": "Optional named ONNX artifact. Defaults to the package execution "
                    "entry artifact.",
                "type": AbstractDataProviderTypeMap."string",
            },
            "registry_tokenizer_artifact_name": <DataProviderOptionInfo>{
                "display_name": "Registry Tokenizer Artifact",
                "short_desc": "Named tokenizer.json artifact in the package",
                "desc": "Optional tokenizer artifact name. Defaults to the package manifest's "
                    "`tokenizer.json` artifact.",
                "type": AbstractDataProviderTypeMap."string",
            },
        } + AbstractDataProviderMLProcessor::ModelRegistryConstructorOptions;
    }

    private {
        #! Configured text field name
        string text_field;

        #! The semantic document chunker
        QoreTokenizerUtils::SemanticDocumentChunker chunker;

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

        if (!copts.tokenizer_config_path.val() && !copts.registry_model_name.val()) {
            throw "CONSTRUCTOR-ERROR",
                "tokenizer_config_path option is required unless registry_model_name is provided";
        }
%ifndef NoJson
        hash<auto> tok_config;
        *QoreModelRegistry::ModelRegistry registry;
        hash<QoreModelRegistry::ModelVersionInfo> manifest;
        if (copts.registry_model_name.val()) {
            registry = new QoreModelRegistry::ModelRegistry(
                AbstractDataProviderMLProcessor::buildModelRegistryOptions(copts));
            manifest = registry.getVersionManifest(copts.registry_model_name,
                copts.registry_model_version);
            tok_config = registry.loadTokenizerConfig(copts.registry_model_name,
                manifest.version_id, copts.registry_tokenizer_artifact_name);
        } else {
            tok_config = parse_json(ReadOnlyFile::readTextFile(copts.tokenizer_config_path));
        }
%else
        throw "JSON-MODULE-MISSING", "json module is required for tokenizer config parsing";
        hash<auto> tok_config = {};
%endif

        # Build embedding model config
        string emb_mode = copts.embedding_mode;
        if (!emb_mode.val()) {
            throw "CONSTRUCTOR-ERROR", "embedding_mode option is required";
        }

        hash<QoreTokenizerUtils::EmbeddingConfig> emb_config(
            <QoreTokenizerUtils::EmbeddingConfig>{
                "mode": emb_mode,
                "max_tokens": copts.chunk_size ?? 512,
            });

        if (emb_mode == "http") {
            if (!copts.embedding_url.val()) {
                throw "CONSTRUCTOR-ERROR",
                    "embedding_url is required for HTTP embedding mode";
            }
            emb_config.url = copts.embedding_url;
            emb_config.model = copts.embedding_model;
            emb_config.api_key = copts.embedding_api_key;
        } else if (emb_mode == "onnx") {
            if (copts.registry_model_name.val()) {
                string artifact_name = copts.registry_model_artifact_name
                    ?? (manifest.execution ? manifest.execution.entry_artifact : NOTHING)
                    ?? "model";
                emb_config.model_data = registry.loadNamedArtifact(copts.registry_model_name,
                    manifest.version_id, artifact_name);
                emb_config.tokenizer_config = tok_config;
            } else if (!copts.embedding_model_path.val()) {
                throw "CONSTRUCTOR-ERROR",
                    "embedding_model_path is required for ONNX embedding mode unless "
                    "registry_model_name is provided";
            } else {
                emb_config.model_path = copts.embedding_model_path;
                emb_config.tokenizer_config = tok_config;
            }
        }

        # Build semantic chunker — constructor creates EmbeddingModel from config
        chunker = new QoreTokenizerUtils::SemanticDocumentChunker(tok_config, emb_config, {
            "chunk_size": copts.chunk_size ?? 512,
            "chunk_overlap": copts.chunk_overlap ?? 50,
            "similarity_threshold": copts.similarity_threshold ?? 0.5,
            "segment_size": copts.segment_size ?? 3,
        });

        configured = True;
    }

    #! Returns the name of this data provider
    string getName() {
        return "semantic-chunk-text";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Splits text into semantically coherent chunks using embedding similarity — "
            "groups related sentences together instead of cutting at arbitrary token boundaries; "
            "produces better search results than fixed-size chunking for narrative text";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Processes a single record: chunks text semantically and emits one record per chunk
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "QoreSemanticChunkProcessor: not configured; provide constructor options";
        }

        string text = DataProvider::AbstractDataProvider::getFieldValue(
            record, text_field).toString();

        if (!text.val()) {
            return;
        }

        list<hash<auto>> chunks = chunker.chunkText(text);

        if (!chunks) {
            return;
        }

        list<auto> results = ();
        int idx = 0;
        foreach hash<auto> chunk in (chunks) {
            hash<auto> result = {} + record;
            result.chunk_text = chunk.text;
            result.chunk_index = idx;
            result.chunk_token_count = chunk.token_count;
            push results, result;
            ++idx;
        }
        return results;
    }

    #! Supports shaped bulk API
    private bool supportsBulkApiImpl() {
        return True;
    }

    #! Returns the processor capability bitfield
    private int getProcessorCapabilitiesImpl() {
        return DPC_AGGREGATE;
    }

    #! Returns NOTHING — output type is dynamic
    private *DataProvider::AbstractDataProviderType getPipelineProcessorOutputTypeImpl() {
        return NOTHING;
    }
}
}
