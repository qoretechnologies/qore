# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreRerankerProcessor class definition

/** QoreRerankerProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Pipeline processor that re-ranks accumulated records by query relevance
/** Accumulates records, then re-ranks the text field against a query using
    a cross-encoder model and emits the top-k records with relevance scores.

    This is a @ref DPC_AGGREGATE processor because it needs to see all
    candidate records before it can produce ranked output.

    @par Example:
    @code{.py}
QoreRerankerProcessor proc({
    "query_field": "search_query",
    "passage_field": "chunk_text",
    "top_k": 5,
    "reranker_mode": "http",
    "reranker_url": "http://localhost:8080",
});
    @endcode

*/
public class QoreRerankerProcessor inherits AbstractDataProviderMLProcessor {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreRerankerProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_AGGREGATE,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = {
            "query_field": <DataProviderOptionInfo>{
                "display_name": "Query Field",
                "short_desc": "Field containing the search query",
                "desc": "The name of the field in each record containing the search query "
                    "to rank against. All records in a batch should have the same query.",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "input_field_names": True,
            },
            "passage_field": <DataProviderOptionInfo>{
                "display_name": "Passage Field",
                "short_desc": "Field containing the text to rank",
                "desc": "The name of the field in each record containing the text passage "
                    "to score against the query.",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "input_field_names": True,
            },
            "top_k": <DataProviderOptionInfo>{
                "display_name": "Top K",
                "short_desc": "Number of top results to return",
                "desc": "The number of highest-scoring records to emit after re-ranking. "
                    "Records below this threshold are dropped.",
                "type": AbstractDataProviderTypeMap."int",
                "required": True,
                "default_value": 5,
            },
            "reranker_mode": <DataProviderOptionInfo>{
                "display_name": "Reranker Mode",
                "short_desc": "How to connect to the reranker",
                "desc": "The reranker backend mode:\n"
                    "- `http` — Call an HTTP reranker endpoint\n"
                    "- `onnx` — Run a local ONNX cross-encoder model",
                "type": AbstractDataProviderTypeMap."string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "http",
                        "display_name": "HTTP Endpoint",
                        "desc": "Call an HTTP reranker API endpoint",
                    },
                    <AllowedValueInfo>{
                        "value": "onnx",
                        "display_name": "Local ONNX Model",
                        "desc": "Run a local ONNX cross-encoder model",
                    },
                ),
            },
            "reranker_url": <DataProviderOptionInfo>{
                "display_name": "Reranker URL",
                "short_desc": "URL of the HTTP reranker endpoint",
                "desc": "URL of the reranker endpoint. Required for `http` mode.",
                "type": AbstractDataProviderTypeMap."string",
                "required": False,
            },
            "reranker_api_key": <DataProviderOptionInfo>{
                "display_name": "Reranker API Key",
                "short_desc": "API key for reranker authentication",
                "desc": "Optional API key for authenticating with the reranker endpoint.",
                "type": AbstractDataProviderTypeMap."string",
                "required": False,
                "sensitive": True,
            },
            "reranker_model_path": <DataProviderOptionInfo>{
                "display_name": "ONNX Model Path",
                "short_desc": "Path to the ONNX cross-encoder model",
                "desc": "Filesystem path to the ONNX cross-encoder model file. "
                    "Required for `onnx` mode.",
                "type": AbstractDataProviderTypeMap."string",
                "required": False,
            },
            "tokenizer_config_path": <DataProviderOptionInfo>{
                "display_name": "Tokenizer Config Path",
                "short_desc": "Path to tokenizer.json for ONNX mode",
                "desc": "Filesystem path to a HuggingFace `tokenizer.json` file. "
                    "Required for `onnx` mode unless `registry_model_name` is provided.",
                "type": AbstractDataProviderTypeMap."string",
                "required": False,
            },
            "onnx_session_config": <DataProviderOptionInfo>{
                "display_name": "ONNX Session Config",
                "short_desc": "Provider and threading config for ONNX Runtime",
                "desc": "Optional `ML::OnnxSessionConfig` hash passed to the local ONNX "
                    "cross-encoder model. Use this to select execution providers, disable "
                    "GPU auto-detection with `{\"providers\": ()}`, or tune "
                    "`intra_op_threads`, `inter_op_threads`, and graph optimization. "
                    "Registry package execution config is used by default when present; "
                    "this option overrides it.",
                "type": AbstractDataProviderTypeMap."*hash",
                "example_value": {"providers": ()},
            },
            "registry_model_name": <DataProviderOptionInfo>{
                "display_name": "Registry Model Name",
                "short_desc": "Registry package containing reranker artifacts",
                "desc": "QoreModelRegistry model/package name containing an ONNX cross-encoder "
                    "model and a `tokenizer.json` artifact.",
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
            "max_length": <DataProviderOptionInfo>{
                "display_name": "Max Sequence Length",
                "short_desc": "Maximum tokens for query+passage pairs",
                "desc": "Maximum sequence length for query+passage tokenization. "
                    "Longer pairs are truncated. Default is 512.",
                "type": AbstractDataProviderTypeMap."int",
                "required": False,
                "default_value": 512,
            },
        } + AbstractDataProviderMLProcessor::ModelRegistryConstructorOptions;
    }

    private {
        #! Query field name
        string query_field;

        #! Passage field name
        string passage_field;

        #! Number of top results to return
        int top_k;

        #! The reranker instance
        QoreTokenizerUtils::CrossEncoderReranker reranker;

        #! Optional ONNX Runtime session configuration
        *hash<ML::OnnxSessionConfig> onnx_session_config;

        #! Registry backend options
        hash<auto> registry_options;

        #! True when ONNX mode was inferred from registry execution metadata
        bool reranker_mode_inferred = False;

        #! Accumulated records
        list<auto> buffer = ();

        #! Whether configured
        bool configured = False;
    }

    #! Creates the object with no options (for data provider navigation)
    constructor() {
    }

    #! Creates the object with the given options
    constructor(hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        query_field = copts.query_field;
        passage_field = copts.passage_field;
        top_k = copts.top_k ?? 5;

        if (!query_field.val()) {
            throw "CONSTRUCTOR-ERROR", "query_field option is required";
        }
        if (!passage_field.val()) {
            throw "CONSTRUCTOR-ERROR", "passage_field option is required";
        }

        string reranker_mode;
        *string requested_reranker_mode = copts.reranker_mode;
        if (requested_reranker_mode.val()) {
            reranker_mode = requested_reranker_mode;
        }
        if (!reranker_mode.val() && copts.registry_model_name.val()) {
            reranker_mode = "onnx";
            reranker_mode_inferred = True;
        }
        if (!reranker_mode.val()) {
            throw "CONSTRUCTOR-ERROR",
                "reranker_mode is required unless registry_model_name identifies a "
                "registry package with an onnx/reranker execution plan";
        }

        hash<QoreTokenizerUtils::RerankerConfig> rconfig(<QoreTokenizerUtils::RerankerConfig>{
            "mode": reranker_mode,
            "max_length": copts.max_length ?? 512,
        });
        if (exists copts.onnx_session_config) {
            onnx_session_config = cast<hash<ML::OnnxSessionConfig>>(copts.onnx_session_config);
        }

        if (reranker_mode == "http") {
            if (!copts.reranker_url.val()) {
                throw "CONSTRUCTOR-ERROR",
                    "reranker_url is required for HTTP mode";
            }
            rconfig.url = copts.reranker_url;
            rconfig.api_key = copts.reranker_api_key;
        } else if (reranker_mode == "onnx") {
            registry_options = AbstractDataProviderMLProcessor::buildModelRegistryOptions(copts);
            if (copts.registry_model_name.val()) {
%ifndef NoJson
                QoreModelRegistry::ModelRegistry registry(registry_options);
                hash<QoreModelRegistry::ModelVersionInfo> manifest =
                    registry.getVersionManifest(copts.registry_model_name,
                        copts.registry_model_version);
                AbstractDataProviderMLProcessor::validateRegistryExecutionTask(
                    manifest, "reranker", "rerank-text", reranker_mode_inferred);
                string artifact_name = copts.registry_model_artifact_name
                    ?? AbstractDataProviderMLProcessor::getRegistryExecutionPlanArtifactName(
                        manifest)
                    ?? (manifest.execution ? manifest.execution.entry_artifact : NOTHING)
                    ?? "model";
                if (manifest.execution && manifest.execution.config) {
                    rconfig.session_config =
                        cast<hash<ML::OnnxSessionConfig>>(manifest.execution.config);
                }
                rconfig.model_data = registry.loadNamedArtifact(copts.registry_model_name,
                    manifest.version_id, artifact_name);
                hash<auto> tokenizer_package = registry.loadTokenizerPackageConfig(
                    copts.registry_model_name, manifest.version_id,
                    copts.registry_tokenizer_artifact_name);
                rconfig.tokenizer_config = tokenizer_package.tokenizer_json;
                rconfig.tokenizer_runtime_config = tokenizer_package.tokenizer_config;
%else
                throw "JSON-MODULE-MISSING", "json module is required for tokenizer config parsing";
%endif
            } else {
                if (!copts.reranker_model_path.val()) {
                    throw "CONSTRUCTOR-ERROR",
                        "reranker_model_path is required for ONNX mode unless "
                        "registry_model_name is provided";
                }
                if (!copts.tokenizer_config_path.val()) {
                    throw "CONSTRUCTOR-ERROR",
                        "tokenizer_config_path is required for ONNX mode unless "
                        "registry_model_name is provided";
                }
                rconfig.model_path = copts.reranker_model_path;
%ifndef NoJson
                rconfig.tokenizer_config =
                    parse_json(ReadOnlyFile::readTextFile(copts.tokenizer_config_path));
%else
                throw "CONSTRUCTOR-ERROR",
                    "json module is required for tokenizer config parsing";
%endif
            }
            if (onnx_session_config) {
                rconfig.session_config = onnx_session_config;
            }
        }

        reranker = new QoreTokenizerUtils::CrossEncoderReranker(rconfig);
        configured = True;
    }

    #! Returns the name of this data provider
    string getName() {
        return "rerank-text";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Re-ranks accumulated text records by relevance to a search query using a "
            "cross-encoder model — improves search precision by scoring each candidate "
            "against the query; emits the top-k most relevant records with scores";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Accumulates records for re-ranking
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "QoreRerankerProcessor: not configured; provide constructor options";
        }
        push buffer, record;
        return;
    }

    #! Flushes accumulated records: re-ranks and emits top-k
    private *list<auto> flushRecordsImpl() {
        if (!configured || !buffer) {
            return;
        }

        # Extract query (use first record's query) and passages
        string query = DataProvider::AbstractDataProvider::getFieldValue(
            buffer[0], query_field).toString();
        list<string> passages = ();
        foreach auto record in (buffer) {
            push passages, DataProvider::AbstractDataProvider::getFieldValue(
                record, passage_field).toString();
        }

        # Score all passages
        list<float> scores = reranker.score(query, passages);

        # Create scored pairs and sort by score descending
        list<hash<auto>> scored = ();
        for (int i = 0; i < scores.size(); ++i) {
            push scored, {"index": i, "score": scores[i]};
        }
        scored = sort(scored, int sub(hash<auto> a, hash<auto> b) {
            return b.score <=> a.score;
        });

        # Emit top-k records enriched with reranker_score and reranker_rank
        int limit = min(top_k, scored.size());
        list<auto> results = ();
        for (int i = 0; i < limit; ++i) {
            hash<auto> result = {} + buffer[scored[i]."index"];
            result.reranker_score = scored[i].score;
            result.reranker_rank = i + 1;
            push results, result;
        }

        buffer = ();
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
                "QoreRerankerProcessor: not configured; provide constructor options";
        }

        updatePendingOutputShape(shape);
        if (shape == DataProvider::DataProviderShape::RowRecords) {
            return processRecordImpl(records);
        }

        if (buffer || query_field.find(".") >= 0 || passage_field.find(".") >= 0) {
            return processShapeRowsFallback(records, shape);
        }

        int count = DataProvider::AbstractDataProcessor::shapeRecordCount(records, shape);
        if (!count) {
            return;
        }

        *list<string> queries = stringColumnFromShape(records, shape, count, query_field);
        *list<string> passages = stringColumnFromShape(records, shape, count, passage_field);
        if (!exists queries || !exists passages) {
            return processShapeRowsFallback(records, shape);
        }

        list<float> scores = reranker.score(queries[0], passages);
        if (scores.lsize() != count) {
            throw "PROCESSOR-ERROR",
                sprintf("reranker returned %d scores for %d input rows", scores.lsize(), count);
        }

        list<hash<auto>> scored;
        for (int i = 0; i < scores.lsize(); ++i) {
            push scored, {"index": i, "score": scores[i]};
        }
        scored = sort(scored, int sub(hash<auto> a, hash<auto> b) {
            return b.score <=> a.score;
        });

        int limit = min(top_k, scored.lsize());
        list<int> selected_indices;
        list<float> selected_scores;
        list<int> ranks;
        for (int i = 0; i < limit; ++i) {
            push selected_indices, scored[i]."index";
            push selected_scores, scored[i].score;
            push ranks, i + 1;
        }

        return selectRerankedRows(records, shape, selected_indices, selected_scores, ranks);
    }

    #! Uses the row-oriented implementation when a shaped column path is not safe
    private *auto processShapeRowsFallback(auto records, DataProvider::DataProviderShape shape) {
        list<auto> rows = DataProvider::AbstractDataProcessor::convertShape(
            records, shape, DataProvider::DataProviderShape::RowRecords);
        foreach auto row in (rows) {
            processRecordImpl(row);
        }
        return;
    }

    #! Reads a top-level string column from a shaped block
    private *list<string> stringColumnFromShape(auto records, DataProvider::DataProviderShape shape,
            int count, string field) {
        *auto column;
        switch (shape) {
            case DataProvider::DataProviderShape::DataFrameBlock:
                if (records.columnNames().contains(field)) {
                    column = records.getColumn(field);
                }
                break;
            case DataProvider::DataProviderShape::BufferColumns:
                if (DataProvider::AbstractDataProcessor::isColumnarResult(records)) {
                    if (records.columnNames().contains(field)) {
                        column = records.getColumn(field);
                    }
                } else if (records.hasKey(field)) {
                    column = records{field};
                }
                break;
            case DataProvider::DataProviderShape::HashOfLists:
                if (records.hasKey(field)) {
                    column = records{field};
                }
                break;
        }
        if (!exists column) {
            return;
        }
        return QoreRerankerProcessor::columnToStrings(column, count, field);
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

    #! Builds a shaped top-k output block with reranker score and rank columns
    private auto selectRerankedRows(auto records, DataProvider::DataProviderShape shape,
            list<int> selected_indices, list<float> scores, list<int> ranks) {
        hash<auto> columns;
        switch (shape) {
            case DataProvider::DataProviderShape::DataFrameBlock:
                foreach string name in (records.columnNames()) {
                    columns{name} = QoreRerankerProcessor::selectColumnValues(
                        records.getColumn(name), selected_indices);
                }
                columns.reranker_score = scores;
                columns.reranker_rank = ranks;
                return DataProvider::AbstractDataProcessor::getDataFrameClass().newObject(columns);
            case DataProvider::DataProviderShape::BufferColumns:
                columns = DataProvider::AbstractDataProcessor::isColumnarResult(records)
                    ? records.toColumnHash()
                    : records + {};
                foreach string name in (keys columns) {
                    columns{name} = QoreRerankerProcessor::selectColumnValues(
                        columns{name}, selected_indices);
                }
                columns.reranker_score = scores;
                columns.reranker_rank = ranks;
                return ColumnarResult::fromColumnHash(columns);
            case DataProvider::DataProviderShape::HashOfLists:
                foreach string name in (keys records) {
                    columns{name} = QoreRerankerProcessor::selectColumnValues(
                        records{name}, selected_indices);
                }
                columns.reranker_score = scores;
                columns.reranker_rank = ranks;
                return columns;
        }
        return records;
    }

    #! Selects a ranked subset from a list or dense buffer column
    private static list<auto> selectColumnValues(auto column, list<int> selected_indices) {
        list<auto> values = column.typeCode() == NT_BUFFER ? column.toList() : column;
        list<auto> result;
        if (values.typeCode() != NT_LIST) {
            return DataProvider::AbstractDataProcessor::repeatValue(column, selected_indices.lsize());
        }
        foreach int index in (selected_indices) {
            push result, values[index];
        }
        return result;
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
