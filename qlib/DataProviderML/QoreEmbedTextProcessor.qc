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
public class QoreEmbedTextProcessor inherits AbstractDataProviderMLProcessor {
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
            "embedding_model_path": <DataProviderOptionInfo>{
                "display_name": "ONNX Model Path",
                "short_desc": "Path to a local ONNX embedding model",
                "desc": "Filesystem path to a local ONNX embedding model. Required for "
                    "`onnx` mode unless `registry_model_name` is provided.",
                "type": AbstractDataProviderTypeMap."string",
            },
            "tokenizer_config_path": <DataProviderOptionInfo>{
                "display_name": "Tokenizer Config Path",
                "short_desc": "Path to tokenizer.json for ONNX mode",
                "desc": "Filesystem path to a HuggingFace `tokenizer.json` file. Required "
                    "for `onnx` mode unless the tokenizer is loaded from QoreModelRegistry.",
                "type": AbstractDataProviderTypeMap."string",
            },
            "onnx_session_config": <DataProviderOptionInfo>{
                "display_name": "ONNX Session Config",
                "short_desc": "Provider and threading config for ONNX Runtime",
                "desc": "Optional `ML::OnnxSessionConfig` hash passed to the local ONNX "
                    "embedding model. Use this to select execution providers, disable "
                    "GPU auto-detection with `{\"providers\": ()}`, or tune "
                    "`intra_op_threads`, `inter_op_threads`, and graph optimization. "
                    "Registry package execution config is used by default when present; "
                    "this option overrides it.",
                "type": AbstractDataProviderTypeMap."*hash",
                "example_value": {"providers": ()},
            },
            "max_tokens": <DataProviderOptionInfo>{
                "display_name": "Max Tokens",
                "short_desc": "Maximum tokens for ONNX embedding inputs",
                "desc": "Maximum number of tokens per text in ONNX mode. Longer inputs are "
                    "truncated before inference.",
                "type": AbstractDataProviderTypeMap."int",
                "default_value": 512,
            },
            "pooling_strategy": <DataProviderOptionInfo>{
                "display_name": "Pooling Strategy",
                "short_desc": "How token embeddings are pooled",
                "desc": "Pooling strategy for ONNX embedding outputs: `mean`, `cls`, or `max`.",
                "type": AbstractDataProviderTypeMap."string",
                "default_value": "mean",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "mean", "display_name": "Mean"},
                    <AllowedValueInfo>{"value": "cls", "display_name": "CLS Token"},
                    <AllowedValueInfo>{"value": "max", "display_name": "Max"},
                ),
            },
            "output_mode": <DataProviderOptionInfo>{
                "display_name": "Output Mode",
                "short_desc": "How embeddings are added to shaped output",
                "desc": "Controls how embedding vectors are represented in shaped bulk output:\n\n"
                    "- `vector` — keep the existing `embedding` field containing one vector per row\n"
                    "- `columns` — split dimensions into flat dense `embedding_0`, `embedding_1`, "
                    "... columns for analytical pipelines and DataFrame processing\n\n"
                    "`columns` mode avoids nested vector-list materialization for local ONNX embeddings "
                    "when the input shape is `DataFrameBlock` or `BufferColumns`.",
                "type": AbstractDataProviderTypeMap."string",
                "default_value": "vector",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "vector",
                        "display_name": "Vector Field",
                        "desc": "Store each embedding as a vector in the `embedding` field",
                    },
                    <AllowedValueInfo>{
                        "value": "columns",
                        "display_name": "Dense Columns",
                        "desc": "Split embedding dimensions into flat dense columns",
                    },
                ),
            },
            "registry_model_name": <DataProviderOptionInfo>{
                "display_name": "Registry Model Name",
                "short_desc": "Registry package containing ONNX and tokenizer artifacts",
                "desc": "QoreModelRegistry model/package name to load in ONNX mode. The "
                    "package must contain an ONNX model artifact and a `tokenizer.json` "
                    "artifact.",
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

        #! ONNX model path
        *string embedding_model_path;

        #! ONNX tokenizer path
        *string tokenizer_config_path;

        #! Optional ONNX Runtime session configuration
        *hash<ML::OnnxSessionConfig> onnx_session_config;

        #! ONNX maximum input tokens
        int max_tokens = 512;

        #! ONNX pooling strategy
        string pooling_strategy = "mean";

        #! Embedding output mode ("vector" or "columns")
        string output_mode = "vector";

        #! Registry model/package name for ONNX mode
        *string registry_model_name;

        #! Registry model version
        *string registry_model_version;

        #! Registry ONNX artifact name
        *string registry_model_artifact_name;

        #! Registry tokenizer artifact name
        *string registry_tokenizer_artifact_name;

        #! True when ONNX mode was inferred from registry execution metadata
        bool embedding_mode_inferred = False;

        #! Registry backend options
        hash<auto> registry_options;

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
        registry_model_name = copts.registry_model_name;
        registry_model_version = copts.registry_model_version;
        registry_model_artifact_name = copts.registry_model_artifact_name;
        registry_tokenizer_artifact_name = copts.registry_tokenizer_artifact_name;
        registry_options = AbstractDataProviderMLProcessor::buildModelRegistryOptions(copts);
        *string requested_embedding_mode = copts.embedding_mode;
        if (requested_embedding_mode.val()) {
            embedding_mode = requested_embedding_mode;
        }
        if (!embedding_mode.val() && registry_model_name.val()) {
            embedding_mode = "onnx";
            embedding_mode_inferred = True;
        }
        if (!embedding_mode.val()) {
            throw "CONSTRUCTOR-ERROR",
                "embedding_mode is required unless registry_model_name identifies a "
                "registry package with an onnx/embedding execution plan";
        }
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
        embedding_model_path = copts.embedding_model_path;
        tokenizer_config_path = copts.tokenizer_config_path;
        if (exists copts.onnx_session_config) {
            onnx_session_config = cast<hash<ML::OnnxSessionConfig>>(copts.onnx_session_config);
        }
        max_tokens = copts.max_tokens ?? 512;
        pooling_strategy = copts.pooling_strategy ?? "mean";
        output_mode = copts.output_mode ?? "vector";
        if (output_mode != "vector" && output_mode != "columns") {
            throw "CONSTRUCTOR-ERROR",
                sprintf("output_mode must be \"vector\" or \"columns\"; got %y", output_mode);
        }
        if (embedding_mode == "onnx" && !registry_model_name.val()) {
            if (!embedding_model_path.val()) {
                throw "CONSTRUCTOR-ERROR",
                    "embedding_model_path is required for ONNX mode unless "
                    "registry_model_name is provided";
            }
            if (!tokenizer_config_path.val()) {
                throw "CONSTRUCTOR-ERROR",
                    "tokenizer_config_path is required for ONNX mode unless "
                    "registry_model_name is provided";
            }
        }
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
        if (output_mode == "columns") {
            *ML::Tensor embedding_tensor = emb_model.embedBatchTensor(texts);
            if (!embedding_tensor) {
                return ();
            }
            return addEmbeddingColumnsToRows(records, cast<ML::Tensor>(embedding_tensor));
        }

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

    #! Processes records in bulk hash-of-lists format without row materialization when possible
    private *hash<auto> processRecordsBulkImpl(hash<auto> records) {
        updatePendingOutputShape(DataProvider::DataProviderShape::HashOfLists);
        *auto result = processRecordsShapeImpl(records, DataProvider::DataProviderShape::HashOfLists);
        return exists result ? cast<hash<auto>>(result) : NOTHING;
    }

    #! Processes an explicitly negotiated pipeline shape without row materialization when possible
    private *auto processRecordsShapeImpl(auto records, DataProvider::DataProviderShape shape) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "QoreEmbedTextProcessor: not configured; provide constructor options";
        }

        updatePendingOutputShape(shape);
        if (shape == DataProvider::DataProviderShape::RowRecords) {
            return processRecordImpl(records);
        }

        if (buffer || text_field.find(".") >= 0) {
            return processShapeRowsFallback(records, shape);
        }

        int count = DataProvider::AbstractDataProcessor::shapeRecordCount(records, shape);
        if (!count) {
            return;
        }

        *list<string> texts = textColumnFromShape(records, shape, count);
        if (!exists texts) {
            return processShapeRowsFallback(records, shape);
        }

        ensureEmbeddingModel();
        if (output_mode == "columns") {
            *ML::Tensor embedding_tensor = embedTextsInWindowsTensor(texts);
            if (!embedding_tensor) {
                return;
            }
            return addEmbeddingColumns(records, shape, count, cast<ML::Tensor>(embedding_tensor));
        }

        list<list<float>> embeddings = embedTextsInWindows(texts);
        if (embeddings.lsize() != count) {
            throw "PROCESSOR-ERROR",
                sprintf("embedding model returned %d vectors for %d input rows",
                    embeddings.lsize(), count);
        }

        return addEmbeddingColumn(records, shape, embeddings);
    }

    #! Embeds a text column in configured batch windows
    private list<list<float>> embedTextsInWindows(list<string> texts) {
        int count = texts.lsize();
        list<list<float>> embeddings;
        for (int start = 0; start < count; start += window_size) {
            int end = start + window_size;
            if (end > count) {
                end = count;
            }
            list<string> window = start == 0 && end == count
                ? texts
                : texts[start..(end - 1)];
            embeddings += emb_model.embedBatch(window);
        }
        return embeddings;
    }

    #! Embeds a text column in configured batch windows and keeps dense tensor output
    private *ML::Tensor embedTextsInWindowsTensor(list<string> texts) {
        int count = texts.lsize();
        list<ML::Tensor> tensors;
        for (int start = 0; start < count; start += window_size) {
            int end = start + window_size;
            if (end > count) {
                end = count;
            }
            list<string> window = start == 0 && end == count
                ? texts
                : texts[start..(end - 1)];
            *ML::Tensor tensor = emb_model.embedBatchTensor(window);
            if (tensor) {
                push tensors, cast<ML::Tensor>(tensor);
            }
        }
        if (!tensors) {
            return;
        }
        return tensors.lsize() == 1 ? tensors[0] : ML::Tensor::concatRows(tensors);
    }

    #! Uses the row-oriented implementation when a column fast path is not safe
    private *auto processShapeRowsFallback(auto records, DataProvider::DataProviderShape shape) {
        list<auto> rows = DataProvider::AbstractDataProcessor::convertShape(
            records, shape, DataProvider::DataProviderShape::RowRecords);
        *list<auto> results;
        foreach auto row in (rows) {
            *auto row_result = processRecordImpl(row);
            if (exists row_result) {
                if (row_result.typeCode() == NT_LIST) {
                    results += row_result;
                } else {
                    push results, row_result;
                }
            }
        }
        if (!results) {
            return;
        }
        hash<auto> bulk = DataProvider::AbstractDataProcessor::recordsToHashList(results);
        return AbstractDataProviderMLProcessor::convertBulkOutputShape(bulk, shape);
    }

    #! Reads the configured text field directly from a shaped column block
    private *list<string> textColumnFromShape(auto records, DataProvider::DataProviderShape shape, int count) {
        *auto column;
        switch (shape) {
            case DataProvider::DataProviderShape::DataFrameBlock:
                if (records.columnNames().contains(text_field)) {
                    column = records.getColumn(text_field);
                }
                break;
            case DataProvider::DataProviderShape::BufferColumns:
                if (DataProvider::AbstractDataProcessor::isColumnarResult(records)) {
                    if (records.columnNames().contains(text_field)) {
                        column = records.getColumn(text_field);
                    }
                } else if (records.hasKey(text_field)) {
                    column = records{text_field};
                }
                break;
            case DataProvider::DataProviderShape::HashOfLists:
                if (records.hasKey(text_field)) {
                    column = records{text_field};
                }
                break;
        }
        if (!exists column) {
            return;
        }
        return QoreEmbedTextProcessor::columnToStrings(column, count, text_field);
    }

    #! Converts a shaped text column to string values using row-mode value semantics
    private static list<string> columnToStrings(auto column, int count, string field) {
        list<auto> values = column.typeCode() == NT_BUFFER ? column.toList() : column;
        if (values.lsize() != count) {
            throw "PROCESSOR-SHAPE-ERROR",
                sprintf("text field %y has %d values but the shaped block has %d rows",
                    field, values.lsize(), count);
        }

        list<string> texts;
        foreach auto value in (values) {
            push texts, value.toString();
        }
        return texts;
    }

    #! Appends the embedding vectors to a shaped block while preserving the input shape
    private auto addEmbeddingColumn(auto records, DataProvider::DataProviderShape shape,
            list<list<float>> embeddings) {
        switch (shape) {
            case DataProvider::DataProviderShape::DataFrameBlock:
                return records.withColumn("embedding", embeddings);
            case DataProvider::DataProviderShape::BufferColumns: {
                hash<auto> columns = DataProvider::AbstractDataProcessor::isColumnarResult(records)
                    ? records.toColumnHash()
                    : records + {};
                columns.embedding = embeddings;
                return ColumnarResult::fromColumnHash(columns);
            }
            case DataProvider::DataProviderShape::HashOfLists: {
                hash<auto> columns = records + {};
                columns.embedding = embeddings;
                return columns;
            }
        }
        return records;
    }

    #! Appends embedding dimensions as scalar fields to row records
    private list<auto> addEmbeddingColumnsToRows(list<hash<auto>> records, ML::Tensor embeddings) {
        list<int> shape = embeddings.getShape();
        if (shape.lsize() != 2 || shape[0] != records.lsize()) {
            throw "PROCESSOR-ERROR",
                sprintf("embedding tensor shape %y does not match %d input rows", shape, records.lsize());
        }

        list<auto> buffers = embeddings.toColumnBuffers();
        list<list<auto>> values;
        foreach auto buffer in (buffers) {
            push values, buffer.toList();
        }

        list<auto> results = ();
        for (int i = 0; i < records.lsize(); ++i) {
            hash<auto> result = {} + records[i];
            for (int j = 0; j < shape[1]; ++j) {
                result{sprintf("embedding_%d", j)} = values[j][i];
            }
            push results, result;
        }
        return results;
    }

    #! Appends embedding dimensions as dense columns while preserving the negotiated shape
    private auto addEmbeddingColumns(auto records, DataProvider::DataProviderShape shape, int count,
            ML::Tensor embeddings) {
        list<int> tensor_shape = embeddings.getShape();
        if (tensor_shape.lsize() != 2 || tensor_shape[0] != count) {
            throw "PROCESSOR-ERROR",
                sprintf("embedding tensor shape %y does not match %d input rows", tensor_shape, count);
        }

        list<auto> columns = embeddings.toColumnBuffers();
        switch (shape) {
            case DataProvider::DataProviderShape::DataFrameBlock: {
                hash<auto> data = records.toColumnHash();
                for (int j = 0; j < tensor_shape[1]; ++j) {
                    data{sprintf("embedding_%d", j)} = columns[j];
                }
                return DataProvider::AbstractDataProcessor::getDataFrameClass().newObject(data);
            }
            case DataProvider::DataProviderShape::BufferColumns: {
                hash<auto> data = DataProvider::AbstractDataProcessor::isColumnarResult(records)
                    ? records.toColumnHash()
                    : records + {};
                for (int j = 0; j < tensor_shape[1]; ++j) {
                    data{sprintf("embedding_%d", j)} = columns[j];
                }
                return ColumnarResult::fromColumnHash(data);
            }
            case DataProvider::DataProviderShape::HashOfLists: {
                hash<auto> data = records + {};
                for (int j = 0; j < tensor_shape[1]; ++j) {
                    data{sprintf("embedding_%d", j)} = columns[j].toList();
                }
                return data;
            }
        }
        return records;
    }

    #! Ensures the EmbeddingModel is initialized (lazy creation to avoid connecting in constructor)
    private ensureEmbeddingModel() {
        if (!emb_model) {
            hash<QoreTokenizerUtils::EmbeddingConfig> config(<QoreTokenizerUtils::EmbeddingConfig>{
                "mode": embedding_mode,
                "url": embedding_url,
                "model": embedding_model,
                "api_key": embedding_api_key,
                "model_path": embedding_model_path,
                "max_tokens": max_tokens,
                "pooling_strategy": pooling_strategy,
            });
            if (embedding_mode == "onnx") {
%ifndef NoJson
                if (registry_model_name.val()) {
                    QoreModelRegistry::ModelRegistry registry(registry_options);
                    hash<QoreModelRegistry::ModelVersionInfo> manifest =
                        registry.getVersionManifest(registry_model_name, registry_model_version);
                    AbstractDataProviderMLProcessor::validateRegistryExecutionTask(
                        manifest, "embedding", "embed-text", embedding_mode_inferred);
                    string artifact_name = registry_model_artifact_name
                        ?? AbstractDataProviderMLProcessor::getRegistryExecutionPlanArtifactName(
                            manifest)
                        ?? (manifest.execution ? manifest.execution.entry_artifact : NOTHING)
                        ?? "model";
                    if (manifest.execution && manifest.execution.config) {
                        config.session_config =
                            cast<hash<ML::OnnxSessionConfig>>(manifest.execution.config);
                    }
                    config.model_data = registry.loadNamedArtifact(registry_model_name,
                        manifest.version_id, artifact_name);
                    hash<auto> tokenizer_package = registry.loadTokenizerPackageConfig(
                        registry_model_name, manifest.version_id,
                        registry_tokenizer_artifact_name);
                    config.tokenizer_config = tokenizer_package.tokenizer_json;
                    config.tokenizer_runtime_config = tokenizer_package.tokenizer_config;
                } else {
                    config.tokenizer_config =
                        parse_json(ReadOnlyFile::readTextFile(tokenizer_config_path));
                }
                if (onnx_session_config) {
                    config.session_config = onnx_session_config;
                }
%else
                throw "JSON-MODULE-MISSING", "json module is required for tokenizer config parsing";
%endif
            }
            emb_model = new QoreTokenizerUtils::EmbeddingModel(config);
        }
    }

    #! Supports shaped bulk API
    private bool supportsBulkApiImpl() {
        return True;
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
