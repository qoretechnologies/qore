# -*- mode: qore; indent-tabs-mode: nil -*-

/*  QoreRegistryModelProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Main DataProviderML namespace
public namespace DataProviderML {

#! Pipeline processor that loads a pre-trained model from the model registry and runs inference
/** Loads a trained ML model from a @ref QoreModelRegistry::ModelRegistry "ModelRegistry"
    by name (and optional version/tag), then applies the registry execution adapter to
    each window of records.

    The model is loaded once at construction time and reused for all subsequent windows.
    Supports native Qore ML models and ONNX Runtime model packages loaded through the
    registry.

    @par Example:
    @code{.py}
    auto proc = new QoreRegistryModelProcessor({
        "registry_path": "/var/models",
        "model_name": "fraud-detector",
        "version": "latest",
        "fields": ("amount", "merchant_category", "hour_of_day"),
        "window_size": 100,
    });
    @endcode

    @since DataProviderML 1.2
*/
public class QoreRegistryModelProcessor inherits AbstractDataProviderMLProcessor {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreRegistryModelProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_AGGREGATE,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Supported \c backend_type values
        const BackendTypes = (
            <AllowedValueInfo>{"value": "filesystem", "display_name": "Filesystem"},
            <AllowedValueInfo>{"value": "database", "display_name": "Database (SqlUtil)"},
            <AllowedValueInfo>{"value": "rest", "display_name": "REST API"},
        );

        #! Constructor options
        /** qorus#244: extended with \c backend_type so this processor can
            consume DB- and REST-backed registries (e.g. the live Qorus
            ml_models / ml_versions / ml_blobs registry served by
            \c /api/v9/models), not just filesystem-backed ones.
        */
        const ConstructorOptions = {
            "backend_type": <DataProviderOptionInfo>{
                "display_name": "Backend Type",
                "short_desc": "Storage backend type for the model registry",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The type of storage backend to use: 'filesystem' (default), "
                    "'database' (e.g., the Qorus omq schema's ml_models tables), "
                    "or 'rest' (e.g., the Qorus /api/v9/models REST surface).",
                "allowed_values": BackendTypes,
                "default_value": "filesystem",
            },
            "registry_path": <DataProviderOptionInfo>{
                "display_name": "Registry Path",
                "short_desc": "Filesystem path to the model registry",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Base directory path for the filesystem-backed model "
                    "registry; required when backend_type is 'filesystem'.",
            },
            "registry_datasource": <DataProviderOptionInfo>{
                "display_name": "Registry Datasource",
                "short_desc": "Database datasource for database backend",
                "type": AbstractDataProviderTypeMap."any",
                "desc": "An AbstractDatasource object for database storage; "
                    "required when backend_type is 'database'.",
            },
            "registry_table_prefix": <DataProviderOptionInfo>{
                "display_name": "Registry Table Prefix",
                "short_desc": "Table name prefix for database backend",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Prefix for database table names (default 'ml_'); "
                    "only used with backend_type 'database'.",
            },
            "registry_rest_client": <DataProviderOptionInfo>{
                "display_name": "REST Client",
                "short_desc": "REST client object for REST backend",
                "type": AbstractDataProviderTypeMap."any",
                "desc": "A REST client object with get/post/put/del methods; "
                    "required when backend_type is 'rest'.",
            },
            "registry_rest_base_path": <DataProviderOptionInfo>{
                "display_name": "REST Base Path",
                "short_desc": "Base path for REST API endpoints",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Base URL path for model registry REST endpoints "
                    "(e.g., '/models'); required when backend_type is 'rest'.",
            },
            "model_name": <DataProviderOptionInfo>{
                "display_name": "Model Name",
                "short_desc": "Name of the model in the registry",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The registered model name to load for inference.",
                "required": True,
            },
            "version": <DataProviderOptionInfo>{
                "display_name": "Version",
                "short_desc": "Version ID or 'latest'",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The version to load. Use `latest` or omit for the most "
                    "recent version.",
            },
            "tag": <DataProviderOptionInfo>{
                "display_name": "Tag",
                "short_desc": "Load version by tag (e.g., 'production')",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Load the latest version with this tag instead of "
                    "specifying a version ID.",
            },
            "tenant_id": <DataProviderOptionInfo>{
                "display_name": "Tenant ID",
                "short_desc": "Tenant identifier for multi-tenant registries",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Optional tenant identifier to scope registry access.",
            },
            "fields": <DataProviderOptionInfo>{
                "display_name": "Input Fields",
                "short_desc": "Fields to use as model input features",
                "type": new SoftListDataType(AbstractDataProviderTypeMap."string", True),
                "desc": "List of field names from each record to pass as input "
                    "features to the model. Native Qore ML models receive a record "
                    "hash with these fields. ONNX models receive the fields as a "
                    "row-major feature matrix under `input_tensor`. Use either "
                    "`fields` for a single ONNX input tensor or `input_map` for "
                    "models with multiple input tensors.",
                "input_field_names": True,
            },
            "input_tensor": <DataProviderOptionInfo>{
                "display_name": "Input Tensor Name",
                "short_desc": "ONNX model input tensor name",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Name of the ONNX input tensor to receive the feature matrix. "
                    "When omitted, the first input tensor from the ONNX model metadata "
                    "is used. This option is ignored for native Qore ML models.",
            },
            "input_map": <DataProviderOptionInfo>{
                "display_name": "Input Tensor Field Map",
                "short_desc": "Map ONNX input tensors to record fields",
                "type": AbstractDataProviderTypeMap."*hash",
                "desc": "A hash mapping ONNX input tensor names to field lists. Use this for "
                    "registry ONNX models with multiple inputs, for example "
                    "`{\"ids\": \"token_ids\", \"mask\": \"attention_mask\"}` or "
                    "`{\"features\": (\"x\", \"y\")}`. When `input_map` is set, each "
                    "mapped tensor is built from its configured fields and `input_tensor` "
                    "is ignored.",
                "example_value": {"X": ("x", "y")},
            },
            "prompt_field": <DataProviderOptionInfo>{
                "display_name": "Prompt Field",
                "short_desc": "Record field containing text-generation prompts",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Input record field used as the prompt for registry ONNX packages "
                    "declaring a `causal-lm` task. When omitted, the processor uses "
                    "`prompt_field` from the package execution plan or the first "
                    "`fields` entry.",
                "input_field_names": True,
                "example_value": "prompt",
            },
            "max_new_tokens": <DataProviderOptionInfo>{
                "display_name": "Maximum New Tokens",
                "short_desc": "Maximum generated tokens per prompt",
                "type": AbstractDataProviderTypeMap."*int",
                "desc": "Maximum number of tokens to generate for registry ONNX "
                    "`causal-lm` execution. When omitted, the value is read from "
                    "the registry execution plan or defaults to `32`.",
                "example_value": 32,
            },
            "stop_token_ids": <DataProviderOptionInfo>{
                "display_name": "Stop Token IDs",
                "short_desc": "Token IDs that stop causal-LM generation",
                "type": AbstractDataProviderTypeMap."*list",
                "desc": "Optional list of token IDs that stop registry ONNX `causal-lm` "
                    "generation. When omitted, the execution plan value is used, "
                    "then the tokenizer EOS token when one is configured.",
                "example_value": (3,),
            },
            "registry_tokenizer_artifact_name": <DataProviderOptionInfo>{
                "display_name": "Tokenizer Artifact Name",
                "short_desc": "Tokenizer artifact name in the registry package",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Optional `tokenizer.json` artifact name to load from the model "
                    "registry for `causal-lm` execution. Omit this when the package "
                    "manifest already identifies the tokenizer artifact.",
                "example_value": "tokenizer.json",
            },
            "output_mode": <DataProviderOptionInfo>{
                "display_name": "Output Mode",
                "short_desc": "How to return registry model results",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Controls the output representation. `events` preserves the existing "
                    "one-event-per-record structure with nested `record_values` and "
                    "`prediction` hashes. For ONNX models, `columns` / `dataframe` returns "
                    "flat output columns without per-row event hash materialization, and "
                    "`tensor` returns raw `ML::Tensor` outputs. Default: `events`.",
                "default_value": "events",
                "example_value": "columns",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "events", "display_name": "Event Records"},
                    <AllowedValueInfo>{"value": "columns", "display_name": "Column Output"},
                    <AllowedValueInfo>{"value": "dataframe", "display_name": "DataFrame Columns"},
                    <AllowedValueInfo>{"value": "tensor", "display_name": "Tensor Output"},
                ),
            },
            "include_input_fields": <DataProviderOptionInfo>{
                "display_name": "Include Input Fields",
                "short_desc": "Whether compact output includes original feature columns",
                "type": AbstractDataProviderTypeMap."*bool",
                "desc": "When `output_mode` is `columns` or `dataframe`, include the original "
                    "input feature columns alongside ONNX output columns. Disable this when "
                    "downstream steps only need model outputs to reduce output size and "
                    "improve throughput. Default: `False`.",
                "default_value": False,
                "example_value": False,
            },
            "window_size": <DataProviderOptionInfo>{
                "display_name": "Window Size",
                "short_desc": "Number of records per processing batch",
                "type": AbstractDataProviderTypeMap."int",
                "desc": "Number of records to accumulate before processing. "
                    "Default: 100.",
            },
            "include_diagnostics": <DataProviderOptionInfo>{
                "display_name": "Include Diagnostics",
                "short_desc": "Attach runtime diagnostics to output events",
                "type": AbstractDataProviderTypeMap."*bool",
                "desc": "When output mode is `events`, include current ONNX inference "
                    "counters on each emitted ONNX event. Provider diagnostics are always "
                    "available on registry-backed ONNX events; use `getDiagnostics()` for "
                    "a single compact diagnostics record.",
                "default_value": False,
                "example_value": False,
            },
        };
    }

    private {
        #! The loaded model execution adapter
        QoreModelRegistry::ModelExecutableAdapter model;

        #! Model name (for output events)
        string model_name;

        #! Version ID of the loaded model
        string loaded_version;

        #! Input field names
        list<string> fields;

        #! Optional ONNX input tensor name
        *string input_tensor;

        #! Optional mapping of ONNX input tensor names to field lists
        *hash<string, list<string>> input_map;

        #! Active ONNX execution provider, when available
        *string execution_provider;

        #! Runtime/provider diagnostics for the loaded model
        *hash<auto> provider_diagnostics;

        #! Registry execution task, when declared
        *string execution_task;

        #! Registry execution plan metadata, when declared
        *hash<auto> execution_plan;

        #! ONNX input tensor names exposed by the loaded model
        hash<string, bool> onnx_input_names;

        #! Prompt field for causal language model generation
        *string prompt_field;

        #! Tokenizer loaded from the registry for causal language models
        *Tokenizer::HFTokenizer causal_tokenizer;

        #! Causal language model tensor names and generation options
        string causal_lm_input_ids_name = "input_ids";
        string causal_lm_attention_mask_name = "attention_mask";
        string causal_lm_token_type_ids_name = "token_type_ids";
        string causal_lm_logits_output_name = "logits";
        int causal_lm_max_new_tokens = 32;
        int causal_lm_max_input_tokens = 0;
        bool causal_lm_add_special_tokens = True;
        bool causal_lm_skip_special_tokens = True;
        list<int> causal_lm_stop_token_ids;
        hash<auto> causal_lm_kv_cache_metadata;

        #! Output representation mode
        string output_mode = "events";

        #! Whether compact column output includes original feature columns
        bool include_input_fields = False;

        #! Whether event output includes current inference counters
        bool include_diagnostics = False;

        #! Window size
        int window_size = 100;

        #! Record buffer
        list<hash<auto>> buffer();

        #! Whether the processor is configured
        bool configured = False;
    }

    #! Creates an unconfigured processor for navigation
    constructor() {
    }

    #! Creates a configured processor that loads a model from the registry
    /** @param options constructor options (see @ref ConstructorOptions)
    */
    constructor(hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);

        model_name = copts.model_name;
        fields = ();
        if (exists copts.fields) {
            fields = DataProvider::AbstractDataProvider::parseFieldList(copts.fields);
        }
        if (exists copts.input_map) {
            input_map = QoreRegistryModelProcessor::parseInputMap(copts.input_map);
            fields = QoreRegistryModelProcessor::mergeInputMapFields(fields, input_map);
        }
        input_tensor = copts.input_tensor;
        output_mode = copts.output_mode ?? "events";
        if (exists copts.include_input_fields) {
            include_input_fields = copts.include_input_fields;
        }
        if (exists copts.include_diagnostics) {
            include_diagnostics = copts.include_diagnostics;
        }
        if (copts.window_size) {
            window_size = copts.window_size;
        }

        # qorus#244: build the registry options from the backend-typed
        # constructor options and delegate to ModelRegistry's built-in
        # backend dispatch (see ModelRegistry::createBackend).  This lets
        # the same processor consume a filesystem path, a Qorus omq
        # datasource, or the Qorus /api/v9/models REST surface without
        # subclassing.
        hash<auto> reg_opts = {
            "backend_type": copts.backend_type ?? "filesystem",
            "path": copts.registry_path,
            "datasource": copts.registry_datasource,
            "table_prefix": copts.registry_table_prefix,
            "rest_client": copts.registry_rest_client,
            "rest_base_path": copts.registry_rest_base_path,
            "tenant_id": copts.tenant_id,
        };
        QoreModelRegistry::ModelRegistry registry(reg_opts);

        if (copts.tag) {
            model = registry.loadExecutableAdapterByTag(model_name, copts.tag);
            loaded_version = model.getVersionId();
        } else {
            model = registry.loadExecutableAdapter(model_name, copts.version);
            loaded_version = model.getVersionId();
        }
        *hash<QoreModelRegistry::ModelVersionInfo> manifest = model.getManifest();
        if (manifest) {
            execution_task = AbstractDataProviderMLProcessor::getRegistryExecutionTask(manifest);
            if (manifest.execution && manifest.execution.plan) {
                execution_plan = manifest.execution.plan;
            }
        }
        if (model.isOnnx()) {
            provider_diagnostics = model.getProviderDiagnostics();
            execution_provider = provider_diagnostics.active_provider;
            onnx_input_names = QoreRegistryModelProcessor::buildOnnxInputNameMap(provider_diagnostics);
            if (exists input_map) {
                hash<string, list<string>> imap = input_map;
                foreach string tensor_name in (keys imap) {
                    model.getOnnxInputTensorDtype(tensor_name);
                }
            }
        } else if (exists input_map) {
            throw "CONSTRUCTOR-ERROR",
                "input_map is only supported for ONNX Runtime models; native registry models use 'fields'";
        }

        if (execution_task == "causal-lm") {
            configureCausalLm(copts, registry);
        } else if (!fields) {
            throw "CONSTRUCTOR-ERROR",
                "no valid input field names provided; set 'fields' for a single input tensor "
                "or 'input_map' for one or more ONNX input tensors";
        }

        configured = True;
    }

    #! Returns current diagnostics for the loaded registry model
    /** @param finish_profile if True, ends ONNX Runtime profiling and includes
        a structured profile summary when the registry execution configuration
        enabled profiling

        @return provider diagnostics, manifest metadata, inference counters, and
        optional profile summary

        @since DataProviderML 1.4
    */
    hash<auto> getDiagnostics(bool finish_profile = False) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "registry-model processor not configured; provide constructor options";
        }
        hash<auto> rv = {
            "runtime": model.getRuntime(),
            "model_name": model_name,
            "version_id": loaded_version,
            "provider_diagnostics": provider_diagnostics,
            "manifest": model.getManifest(),
            "inference_stats": model.getInferenceStats(),
        };
        if (finish_profile) {
            *string profile_path = model.endProfiling();
            if (profile_path.val()) {
                rv.profile_path = profile_path;
                try {
                    rv.profile_summary = QoreOnnxTools::OnnxProfileTools::summarizeProfile(profile_path);
                } catch (hash<ExceptionInfo> ex) {
                    rv.profile_summary_error = sprintf("%s: %s", ex.err, ex.desc);
                }
                rv.inference_stats = model.getInferenceStats();
            }
        }
        return rv;
    }

    #! Parses the input tensor to field-list mapping
    private static hash<string, list<string>> parseInputMap(hash<auto> raw_map) {
        hash<string, list<string>> rv;
        foreach auto i in (raw_map.pairIterator()) {
            list<string> tensor_fields = DataProvider::AbstractDataProvider::parseFieldList(i.value);
            if (!tensor_fields) {
                throw "CONSTRUCTOR-ERROR",
                    sprintf("input_map entry %y does not contain any valid field names", i.key);
            }
            rv{i.key} = tensor_fields;
        }
        return rv;
    }

    #! Merges single-input fields with all mapped input fields for record metadata
    private static list<string> mergeInputMapFields(list<string> base, *hash<string, list<string>> mapped) {
        list<string> rv = base;
        if (!exists mapped) {
            return rv;
        }
        foreach auto i in (mapped.pairIterator()) {
            foreach string field in (i.value) {
                if (!rv.contains(field)) {
                    push rv, field;
                }
            }
        }
        return rv;
    }

    string getName() {
        return "registry-model";
    }

    *string getDesc() {
        return "Runs inference using a pre-trained model loaded from the model registry";
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    private bool supportsBulkApiImpl() {
        return True;
    }

    private int getProcessorCapabilitiesImpl() {
        return DPC_AGGREGATE;
    }

    private *DataProvider::AbstractDataProviderType getPipelineProcessorOutputTypeImpl() {
        return new HashDeclDataType(
            TypedHash::forName("DataProviderML::RegistryModelEventInfo"));
    }

    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "registry-model processor not configured; provide constructor options";
        }

        push buffer, record;
        if (buffer.lsize() >= window_size) {
            list<auto> results = processWindow(buffer);
            buffer = ();
            return results;
        }
        return;
    }

    private *list<auto> flushRecordsImpl() {
        if (!configured || !buffer) {
            return;
        }
        if (buffer.lsize() > 0) {
            list<auto> results = processWindow(buffer);
            buffer = ();
            return results;
        }
        return;
    }

    private list<auto> processWindow(list<hash<auto>> records) {
        if (model.isOnnx()) {
            if (execution_task == "causal-lm") {
                return processCausalLmWindow(records);
            }
            return processOnnxWindow(records);
        }

        list<auto> events();

        foreach hash<auto> rec in (records) {
            # Extract input features from the record
            hash<auto> input;
            foreach string f in (fields) {
                input{f} = rec{f};
            }

            # Run prediction
            auto prediction = model.predict(input);

            push events, <RegistryModelEventInfo>{
                "event_type": "registry-model",
                "model_name": model_name,
                "version_id": loaded_version,
                "record_values": input,
                "prediction": prediction,
            };
        }

        return events;
    }

    private list<auto> processOnnxWindow(list<hash<auto>> records) {
        if (exists input_map) {
            return processMappedOnnxRecordWindow(records);
        }

        list<list<float>> matrix;
        foreach hash<auto> rec in (records) {
            list<float> row;
            foreach string f in (fields) {
                push row, DataProvider::AbstractDataProvider::getFieldValue(rec, f).toFloat();
            }
            push matrix, row;
        }

        hash<auto> outputs = model.runFeatureMatrix(matrix, input_tensor);

        list<auto> events;
        for (int i = 0; i < records.lsize(); ++i) {
            hash<auto> record_values;
            foreach string f in (fields) {
                record_values{f} = DataProvider::AbstractDataProvider::getFieldValue(records[i], f);
            }

            hash<auto> model_output;
            foreach string okey in (keys outputs) {
                auto oval = outputs{okey};
                if (oval.typeCode() == NT_LIST && oval.lsize() == records.lsize()) {
                    model_output{okey} = oval[i];
                } else {
                    model_output{okey} = oval;
                }
            }

            hash<RegistryModelEventInfo> event = <RegistryModelEventInfo>{
                "event_type": "registry-model",
                "model_name": model_name,
                "version_id": loaded_version,
                "record_values": record_values,
                "execution_provider": execution_provider,
                "provider_diagnostics": provider_diagnostics,
                "prediction": applyTaskOutputPlan(model_output),
            };
            if (include_diagnostics) {
                event.inference_stats = model.getInferenceStats();
            }
            push events, event;
        }

        return events;
    }

    private list<auto> processMappedOnnxRecordWindow(list<hash<auto>> records) {
        int count = records.lsize();
        hash<auto> outputs = runMappedTensorWindow(matricesFromRecordInputMap(records), 0, count, True);

        hash<auto> output_values;
        foreach string okey in (keys outputs) {
            output_values{okey} = tensorOutputToRows(cast<ML::Tensor>(outputs{okey}), count);
        }

        list<auto> events;
        for (int i = 0; i < count; ++i) {
            hash<auto> record_values;
            foreach string f in (fields) {
                record_values{f} = DataProvider::AbstractDataProvider::getFieldValue(records[i], f);
            }

            hash<auto> model_output;
            foreach string okey in (keys output_values) {
                auto oval = output_values{okey};
                if (oval.typeCode() == NT_LIST && oval.lsize() == count) {
                    model_output{okey} = oval[i];
                } else {
                    model_output{okey} = oval;
                }
            }

            hash<RegistryModelEventInfo> event = <RegistryModelEventInfo>{
                "event_type": "registry-model",
                "model_name": model_name,
                "version_id": loaded_version,
                "record_values": record_values,
                "execution_provider": execution_provider,
                "provider_diagnostics": provider_diagnostics,
                "prediction": applyTaskOutputPlan(model_output),
            };
            if (include_diagnostics) {
                event.inference_stats = model.getInferenceStats();
            }
            push events, event;
        }

        return events;
    }

    #! Processes records in bulk hash-of-lists format
    private *hash<auto> processRecordsBulkImpl(hash<auto> records) {
        updatePendingOutputShape(DataProvider::DataProviderShape::HashOfLists);
        *auto result = processCompleteShapeWindows(records, DataProvider::DataProviderShape::HashOfLists);
        if (exists result) {
            if (isColumnOutputMode()) {
                return cast<hash<auto>>(convertColumnOutputShape(cast<hash<auto>>(result),
                    DataProvider::DataProviderShape::HashOfLists));
            }
            return cast<hash<auto>>(result);
        }
        *auto fallback = processShapeRowsFallback(records, DataProvider::DataProviderShape::HashOfLists);
        if (!exists fallback) {
            return;
        }
        return cast<hash<auto>>(fallback);
    }

    #! Processes records in an explicitly negotiated pipeline shape
    private *auto processRecordsShapeImpl(auto records, DataProvider::DataProviderShape shape) {
        updatePendingOutputShape(shape);
        if (shape == DataProvider::DataProviderShape::RowRecords) {
            return processRecordImpl(records);
        }

        *auto result = processCompleteShapeWindows(records, shape);
        if (exists result) {
            if (isTensorOutputMode()) {
                return result;
            }
            if (isColumnOutputMode()) {
                return convertColumnOutputShape(cast<hash<auto>>(result), shape);
            }
            return AbstractDataProviderMLProcessor::convertBulkOutputShape(result, shape);
        }
        return processShapeRowsFallback(records, shape);
    }

    #! Uses the row-oriented implementation as a semantics-preserving fallback
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

    #! Processes complete shaped ONNX windows with one tensor inference call per window
    private *auto processCompleteShapeWindows(auto records, DataProvider::DataProviderShape shape) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "registry-model processor not configured; provide constructor options";
        }
        if (!model.isOnnx() || buffer) {
            return;
        }
        if (execution_task == "causal-lm") {
            return;
        }

        int count = DataProvider::AbstractDataProcessor::shapeRecordCount(records, shape);
        if (!count) {
            return;
        }

        if (exists input_map) {
            return processMappedShapeWindows(records, shape, count);
        }

        if (count <= window_size && (isTensorOutputMode() || (isColumnOutputMode() && !include_input_fields))) {
            *ML::Tensor direct_input = inputTensorFromDenseColumns(records, shape, count, fields,
                model.getOnnxInputTensorDtype(input_tensor));
            if (exists direct_input) {
                hash<auto> outputs = model.runFeatureTensor(direct_input, input_tensor);
                if (isTensorOutputMode()) {
                    return outputs;
                }

                hash<auto> result;
                foreach string okey in (keys outputs) {
                    appendTensorOutputColumns(\result, okey, cast<ML::Tensor>(outputs{okey}), count, True);
                }
                return result;
            }
        }

        list<list<float>> matrix = matrixFromShape(records, shape);
        if (isTensorOutputMode()) {
            return processTensorShapeWindows(matrix, count);
        }

        if (isColumnOutputMode()) {
            hash<auto> result;
            addInputFieldColumns(\result, records, shape, matrix);
            for (int start = 0; start < count; start += window_size) {
                int end = start + window_size;
                if (end > count) {
                    end = count;
                }
                bool single_window = start == 0 && end == count;
                list<list<float>> window = single_window
                    ? matrix
                    : matrix[start..(end - 1)];
                appendTensorWindowColumnResult(\result, window, single_window);
            }
            return result;
        }

        hash<auto> result;
        for (int start = 0; start < count; start += window_size) {
            int end = start + window_size;
            if (end > count) {
                end = count;
            }
            bool single_window = start == 0 && end == count;
            list<list<float>> window = single_window
                ? matrix
                : matrix[start..(end - 1)];
            appendTensorWindowEventResult(\result, window);
        }
        return result;
    }

    #! Processes complete shaped windows for registry ONNX models with mapped inputs
    private hash<auto> processMappedShapeWindows(auto records, DataProvider::DataProviderShape shape, int count) {
        if (count <= window_size && (isTensorOutputMode() || (isColumnOutputMode() && !include_input_fields))) {
            *hash<auto> direct_inputs = inputTensorsFromMappedDenseColumns(records, shape, count);
            if (exists direct_inputs) {
                hash<auto> outputs = model.runTensors(direct_inputs);
                if (isTensorOutputMode()) {
                    return outputs;
                }

                hash<auto> result;
                foreach string okey in (keys outputs) {
                    appendTensorOutputColumns(\result, okey, cast<ML::Tensor>(outputs{okey}), count, True);
                }
                return result;
            }
        }

        hash<auto> mapped_matrices = matricesFromInputMap(records, shape);
        if (isTensorOutputMode()) {
            return processMappedTensorShapeWindows(mapped_matrices, count);
        }

        list<list<float>> record_matrix = matrixFromShape(records, shape);
        hash<auto> result;
        if (isColumnOutputMode()) {
            addInputFieldColumns(\result, records, shape, record_matrix);
            for (int start = 0; start < count; start += window_size) {
                int end = start + window_size;
                if (end > count) {
                    end = count;
                }
                int window_count = end - start;
                bool single_window = start == 0 && end == count;
                hash<auto> outputs = runMappedTensorWindow(mapped_matrices, start, window_count, single_window);
                foreach string okey in (keys outputs) {
                    appendTensorOutputColumns(\result, okey, cast<ML::Tensor>(outputs{okey}), window_count,
                        single_window);
                }
            }
            return result;
        }

        for (int start = 0; start < count; start += window_size) {
            int end = start + window_size;
            if (end > count) {
                end = count;
            }
            appendMappedTensorWindowEventResult(\result, mapped_matrices, record_matrix, start, end - start);
        }
        return result;
    }

    private bool isColumnOutputMode() {
        return output_mode == "columns" || output_mode == "dataframe";
    }

    private bool isTensorOutputMode() {
        return output_mode == "tensor";
    }

    private auto convertColumnOutputShape(hash<auto> columns, DataProvider::DataProviderShape shape) {
        if (shape == DataProvider::DataProviderShape::BufferColumns) {
            return columns;
        }
        list<auto> converted = DataProvider::AbstractDataProcessor::convertShape(
            columns,
            cast<enum<DataProvider::DataProviderShape>>(DataProvider::DataProviderShape::BufferColumns),
            cast<enum<DataProvider::DataProviderShape>>(shape));
        return shape == DataProvider::DataProviderShape::RowRecords ? converted : converted[0];
    }

    #! Creates a dense input tensor directly from matching top-level dense columns when possible
    private *ML::Tensor inputTensorFromDenseColumns(auto records, DataProvider::DataProviderShape shape,
            int count, list<string> tensor_fields, string dtype) {
        *list<auto> columns = denseColumnsFromShape(records, shape, tensor_fields, dtype);
        if (!exists columns) {
            return;
        }

        return ML::Tensor::fromColumns(columns, dtype);
    }

    #! Creates mapped dense input tensors directly from dense columns when possible
    private *hash<auto> inputTensorsFromMappedDenseColumns(auto records,
            DataProvider::DataProviderShape shape, int count) {
        hash<auto> rv;
        hash<string, list<string>> imap = input_map;
        foreach string tensor_name in (keys imap) {
            *ML::Tensor tensor = inputTensorFromDenseColumns(records, shape, count,
                imap{tensor_name}, model.getOnnxInputTensorDtype(tensor_name));
            if (!exists tensor) {
                return;
            }
            rv{tensor_name} = tensor;
        }
        return rv;
    }

    #! Returns matching dense top-level columns, or NOTHING when the fast path is not safe
    private *list<auto> denseColumnsFromShape(auto records, DataProvider::DataProviderShape shape,
            list<string> tensor_fields, string dtype) {
        if (!tensor_fields) {
            return;
        }

        string required_type = sprintf("buffer<%s>", dtype);
        list<auto> columns;
        foreach string field in (tensor_fields) {
            if (field.find(".") >= 0) {
                return;
            }

            *auto column;
            switch (shape) {
                case DataProvider::DataProviderShape::DataFrameBlock:
                    column = records.getColumnBuffer(field);
                    break;
                case DataProvider::DataProviderShape::BufferColumns:
                    if (DataProvider::AbstractDataProcessor::isColumnarResult(records)) {
                        column = records.getColumn(field);
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

            if (!exists column || column.typeCode() != NT_BUFFER || column.fullType() != required_type) {
                return;
            }
            push columns, column;
        }
        return columns;
    }

    #! Extracts numeric input fields as a model matrix from a shaped block
    private list<list<float>> matrixFromShape(auto records, DataProvider::DataProviderShape shape) {
        return matrixFromShapeFields(records, shape, fields);
    }

    #! Extracts selected numeric input fields as a model matrix from a shaped block
    private list<list<float>> matrixFromShapeFields(auto records, DataProvider::DataProviderShape shape,
            list<string> tensor_fields) {
        if (shape == DataProvider::DataProviderShape::DataFrameBlock) {
            return cast<list<list<float>>>(records.toMatrix(tensor_fields));
        }

        if (shape == DataProvider::DataProviderShape::BufferColumns) {
            auto df = DataProvider::AbstractDataProcessor::convertShape(
                records,
                cast<enum<DataProvider::DataProviderShape>>(shape),
                cast<enum<DataProvider::DataProviderShape>>(DataProvider::DataProviderShape::DataFrameBlock))[0];
            return cast<list<list<float>>>(df.toMatrix(tensor_fields));
        }

        list<list<float>> matrix;
        int count = DataProvider::AbstractDataProcessor::shapeRecordCount(records, shape);
        for (int i = 0; i < count; ++i) {
            list<float> row;
            foreach string f in (tensor_fields) {
                push row, records{f}[i].toFloat();
            }
            push matrix, row;
        }
        return matrix;
    }

    #! Extracts each configured ONNX input tensor matrix from a shaped block
    private hash<auto> matricesFromInputMap(auto records, DataProvider::DataProviderShape shape) {
        hash<auto> rv;
        hash<string, list<string>> imap = input_map;
        foreach string tensor_name in (keys imap) {
            rv{tensor_name} = matrixFromShapeFields(records, shape, imap{tensor_name});
        }
        return rv;
    }

    #! Extracts each configured ONNX input tensor matrix from row records
    private hash<auto> matricesFromRecordInputMap(list<hash<auto>> records) {
        hash<auto> rv;
        hash<string, list<string>> imap = input_map;
        foreach string tensor_name in (keys imap) {
            rv{tensor_name} = matrixFromRecordFields(records, imap{tensor_name});
        }
        return rv;
    }

    #! Extracts selected numeric input fields as a model matrix from row records
    private static list<list<float>> matrixFromRecordFields(list<hash<auto>> records,
            list<string> tensor_fields) {
        list<list<float>> matrix;
        foreach hash<auto> rec in (records) {
            list<float> row;
            foreach string f in (tensor_fields) {
                push row, DataProvider::AbstractDataProvider::getFieldValue(rec, f).toFloat();
            }
            push matrix, row;
        }
        return matrix;
    }

    #! Builds a hash map of declared ONNX input tensor names from diagnostics
    private static hash<string, bool> buildOnnxInputNameMap(*hash<auto> diagnostics) {
        hash<string, bool> rv = {};
        if (!diagnostics || !exists diagnostics.inputs) {
            return rv;
        }
        foreach hash<auto> input in (diagnostics.inputs) {
            if (input.name.val()) {
                rv{input.name} = True;
            }
        }
        return rv;
    }

    #! Configures task-aware causal language model generation from registry metadata
    private configureCausalLm(hash<auto> copts, QoreModelRegistry::ModelRegistry registry) {
        if (!model.isOnnx()) {
            throw "CONSTRUCTOR-ERROR",
                "registry-model task 'causal-lm' requires an ONNX Runtime model artifact";
        }
        if (exists input_map) {
            throw "CONSTRUCTOR-ERROR",
                "registry-model task 'causal-lm' builds tokenizer tensors from 'prompt_field'; "
                "do not set 'input_map'";
        }
        if (output_mode != "events") {
            throw "CONSTRUCTOR-ERROR",
                "registry-model task 'causal-lm' currently supports output_mode 'events' only";
        }

        if (exists copts.prompt_field && copts.prompt_field.val()) {
            prompt_field = string(copts.prompt_field);
        }
        if (!prompt_field.val()) {
            prompt_field = getExecutionPlanString(("prompt_field", "text_field", "input_field"),
                ("input", "generation", "tokenizer"));
        }
        if (!prompt_field.val() && fields) {
            prompt_field = fields[0];
        }
        if (!prompt_field.val()) {
            throw "CONSTRUCTOR-ERROR",
                "registry-model task 'causal-lm' requires 'prompt_field' or an execution plan prompt_field";
        }
        if (!fields.contains(prompt_field)) {
            push fields, prompt_field;
        }

        causal_lm_input_ids_name = getExecutionPlanStringDefault(
            ("input_ids_name", "input_ids"), ("input", "generation"), "input_ids");
        causal_lm_attention_mask_name = getExecutionPlanStringDefault(
            ("attention_mask_name", "attention_mask"), ("input", "generation"), "attention_mask");
        causal_lm_token_type_ids_name = getExecutionPlanStringDefault(
            ("token_type_ids_name", "token_type_ids"), ("input", "generation"), "token_type_ids");
        causal_lm_logits_output_name = getExecutionPlanStringDefault(
            ("logits_output_name", "logits_output", "output_name"), ("output", "generation"), "logits");

        if (exists copts.max_new_tokens) {
            causal_lm_max_new_tokens = int(copts.max_new_tokens);
        } else {
            causal_lm_max_new_tokens = getExecutionPlanInt(
                ("max_new_tokens", "max_tokens"), ("generation",), 32);
        }
        if (causal_lm_max_new_tokens <= 0) {
            throw "CONSTRUCTOR-ERROR",
                sprintf("max_new_tokens must be positive for registry-model task 'causal-lm'; got %d",
                    causal_lm_max_new_tokens);
        }
        causal_lm_max_input_tokens = getExecutionPlanInt(
            ("max_input_tokens", "max_length"), ("tokenizer", "generation"), 0);
        causal_lm_add_special_tokens = getExecutionPlanBool(
            ("add_special_tokens",), ("tokenizer", "generation"), True);
        causal_lm_skip_special_tokens = getExecutionPlanBool(
            ("skip_special_tokens",), ("tokenizer", "generation"), True);

        hash<auto> tokenizer_package = registry.loadTokenizerPackageConfig(
            model_name, loaded_version, copts.registry_tokenizer_artifact_name);
        causal_tokenizer = new Tokenizer::HFTokenizer(tokenizer_package.tokenizer_json);
        if (exists tokenizer_package.tokenizer_config) {
            causal_tokenizer.loadConfig(tokenizer_package.tokenizer_config);
        }
        causal_lm_stop_token_ids = resolveCausalStopTokenIds(copts);
        causal_lm_kv_cache_metadata = buildCausalKvCacheMetadata();
    }

    private addInputFieldColumns(reference<hash<auto>> result, auto records,
            DataProvider::DataProviderShape shape, list<list<float>> matrix) {
        if (!include_input_fields) {
            return;
        }

        foreach string f in (fields) {
            if (f.find(".") >= 0) {
                continue;
            }
            switch (shape) {
                case DataProvider::DataProviderShape::DataFrameBlock:
                    result{f} = records.getColumnBuffer(f);
                    break;
                case DataProvider::DataProviderShape::BufferColumns:
                    if (DataProvider::AbstractDataProcessor::isColumnarResult(records)) {
                        result{f} = records.getColumn(f);
                    } else if (records.hasKey(f)) {
                        result{f} = records{f};
                    }
                    break;
                case DataProvider::DataProviderShape::HashOfLists:
                    if (records.hasKey(f)) {
                        result{f} = records{f};
                    }
                    break;
            }
        }

        for (int j = 0; j < fields.lsize(); ++j) {
            if (result.hasKey(fields[j])) {
                continue;
            }
            list<float> values;
            for (int i = 0; i < matrix.lsize(); ++i) {
                push values, matrix[i][j];
            }
            result{fields[j]} = values;
        }
    }

    #! Appends one registry ONNX tensor inference window to a hash-of-lists result
    private appendTensorWindowEventResult(reference<hash<auto>> result, list<list<float>> matrix) {
        hash<auto> outputs = model.runFeatureMatrix(matrix, input_tensor);
        int count = matrix.lsize();

        for (int i = 0; i < count; ++i) {
            hash<auto> record_values;
            for (int j = 0; j < fields.lsize(); ++j) {
                record_values{fields[j]} = matrix[i][j];
            }

            hash<auto> model_output;
            foreach string okey in (keys outputs) {
                auto oval = outputs{okey};
                if (oval.typeCode() == NT_LIST && oval.lsize() == count) {
                    model_output{okey} = oval[i];
                } else {
                    model_output{okey} = oval;
                }
            }

            push result{"event_type"}, "registry-model";
            push result{"model_name"}, model_name;
            push result{"version_id"}, loaded_version;
            push result{"record_values"}, record_values;
            push result{"execution_provider"}, execution_provider;
            push result{"provider_diagnostics"}, provider_diagnostics;
            if (include_diagnostics) {
                push result{"inference_stats"}, model.getInferenceStats();
            }
            push result{"prediction"}, applyTaskOutputPlan(model_output);
        }
    }

    #! Appends one mapped tensor inference window to a hash-of-lists result
    private appendMappedTensorWindowEventResult(reference<hash<auto>> result, hash<auto> mapped_matrices,
            list<list<float>> record_matrix, int start, int count) {
        hash<auto> outputs = runMappedTensorWindow(mapped_matrices, start, count, False);

        hash<auto> output_values;
        foreach string okey in (keys outputs) {
            output_values{okey} = tensorOutputToRows(cast<ML::Tensor>(outputs{okey}), count);
        }

        for (int i = 0; i < count; ++i) {
            int row = start + i;
            hash<auto> record_values;
            for (int j = 0; j < fields.lsize(); ++j) {
                record_values{fields[j]} = record_matrix[row][j];
            }

            hash<auto> model_output;
            foreach string okey in (keys output_values) {
                auto oval = output_values{okey};
                if (oval.typeCode() == NT_LIST && oval.lsize() == count) {
                    model_output{okey} = oval[i];
                } else {
                    model_output{okey} = oval;
                }
            }

            push result{"event_type"}, "registry-model";
            push result{"model_name"}, model_name;
            push result{"version_id"}, loaded_version;
            push result{"record_values"}, record_values;
            push result{"execution_provider"}, execution_provider;
            push result{"provider_diagnostics"}, provider_diagnostics;
            if (include_diagnostics) {
                push result{"inference_stats"}, model.getInferenceStats();
            }
            push result{"prediction"}, applyTaskOutputPlan(model_output);
        }
    }

    #! Processes one event window through a registry-backed ONNX causal language model
    private list<auto> processCausalLmWindow(list<hash<auto>> records) {
        list<auto> events;
        foreach hash<auto> rec in (records) {
            string prompt = string(DataProvider::AbstractDataProvider::getFieldValue(rec, prompt_field));
            hash<auto> prediction = generateCausalLmText(prompt);

            hash<auto> record_values = {
                prompt_field: prompt,
            };

            hash<RegistryModelEventInfo> event = <RegistryModelEventInfo>{
                "event_type": "registry-model",
                "model_name": model_name,
                "version_id": loaded_version,
                "record_values": record_values,
                "execution_provider": execution_provider,
                "provider_diagnostics": provider_diagnostics,
                "prediction": prediction,
            };
            if (include_diagnostics) {
                event.inference_stats = model.getInferenceStats();
            }
            push events, event;
        }
        return events;
    }

    #! Generates text using greedy decoding for an ONNX causal language model
    private hash<auto> generateCausalLmText(string prompt) {
        hash<auto> encode_options = {
            "add_special_tokens": causal_lm_add_special_tokens,
        };
        if (causal_lm_max_input_tokens > 0) {
            encode_options.max_length = causal_lm_max_input_tokens;
            encode_options.truncation = "longest_first";
        }

        hash<auto> enc = causal_tokenizer.encodeAdvanced(prompt, encode_options);
        list<int> token_ids = QoreRegistryModelProcessor::requireIntList(enc.input_ids,
            "tokenizer input_ids");
        if (!token_ids) {
            throw "PROCESSOR-ERROR",
                "registry-model task 'causal-lm' tokenizer produced no input_ids";
        }

        list<int> attention_mask = exists enc.attention_mask
            ? QoreRegistryModelProcessor::requireIntList(enc.attention_mask, "tokenizer attention_mask")
            : (map 1, range(token_ids.size()));
        list<int> token_type_ids = exists enc.token_type_ids
            ? QoreRegistryModelProcessor::requireIntList(enc.token_type_ids, "tokenizer token_type_ids")
            : (map 0, range(token_ids.size()));

        int input_token_count = token_ids.size();
        list<int> generated_ids;
        string finish_reason = "length";
        hash<auto> last_outputs;

        for (int step = 0; step < causal_lm_max_new_tokens; ++step) {
            hash<auto> outputs = model.runTensors(buildCausalLmInputs(token_ids,
                attention_mask, token_type_ids));
            last_outputs = summarizeCausalLmOutputs(outputs);
            string logits_name = resolveCausalLmLogitsOutputName(outputs);
            int next_token = QoreRegistryModelProcessor::selectGreedyToken(
                outputs{logits_name}, token_ids.size());

            push token_ids, next_token;
            push generated_ids, next_token;
            push attention_mask, 1;
            int last_type_id = token_type_ids ? token_type_ids[token_type_ids.size() - 1] : 0;
            push token_type_ids, last_type_id;

            if (causal_lm_stop_token_ids.contains(next_token)) {
                finish_reason = "stop";
                break;
            }
        }

        string generated_text = generated_ids
            ? causal_tokenizer.decode(generated_ids, causal_lm_skip_special_tokens)
            : "";

        return {
            "task": "causal-lm",
            "kind": "text-generation",
            "prompt": prompt,
            "text": generated_text,
            "generated_token_ids": generated_ids,
            "input_token_count": input_token_count,
            "generated_token_count": generated_ids.size(),
            "finish_reason": finish_reason,
            "stop_token_ids": causal_lm_stop_token_ids,
            "kv_cache": causal_lm_kv_cache_metadata,
            "last_outputs": last_outputs,
        };
    }

    #! Builds ONNX tensor inputs for one causal-LM decode step
    private hash<auto> buildCausalLmInputs(list<int> token_ids, list<int> attention_mask,
            list<int> token_type_ids) {
        hash<auto> inputs;
        if (!onnx_input_names.hasKey(causal_lm_input_ids_name)) {
            throw "PROCESSOR-ERROR",
                sprintf("registry-model task 'causal-lm' requires ONNX input tensor %y; available inputs: %y",
                    causal_lm_input_ids_name, sort(keys onnx_input_names));
        }
        inputs{causal_lm_input_ids_name} = QoreRegistryModelProcessor::makeInt64Tensor(
            (token_ids,), token_ids.size(), causal_lm_input_ids_name);

        if (onnx_input_names.hasKey(causal_lm_attention_mask_name)) {
            inputs{causal_lm_attention_mask_name} = QoreRegistryModelProcessor::makeInt64Tensor(
                (attention_mask,), attention_mask.size(), causal_lm_attention_mask_name);
        }
        if (onnx_input_names.hasKey(causal_lm_token_type_ids_name)) {
            inputs{causal_lm_token_type_ids_name} = QoreRegistryModelProcessor::makeInt64Tensor(
                (token_type_ids,), token_type_ids.size(), causal_lm_token_type_ids_name);
        }
        return inputs;
    }

    #! Resolves the configured logits output name against the current ONNX outputs
    private string resolveCausalLmLogitsOutputName(hash<auto> outputs) {
        if (outputs.hasKey(causal_lm_logits_output_name)) {
            return causal_lm_logits_output_name;
        }
        if (outputs.hasKey("logits")) {
            return "logits";
        }
        list<string> output_names = sort(keys outputs);
        if (!output_names) {
            throw "PROCESSOR-ERROR",
                "registry-model task 'causal-lm' produced no ONNX outputs";
        }
        return output_names[0];
    }

    #! Produces compact metadata for the last causal-LM output tensors
    private static hash<auto> summarizeCausalLmOutputs(hash<auto> outputs) {
        hash<auto> rv;
        foreach string name in (keys outputs) {
            auto value = outputs{name};
            if (value instanceof ML::Tensor) {
                ML::Tensor tensor = cast<ML::Tensor>(value);
                rv{name} = {
                    "dtype": tensor.getDtype(),
                    "shape": tensor.getShape(),
                };
            } else {
                rv{name} = value;
            }
        }
        return rv;
    }

    #! Selects the highest-scoring token ID from common causal-LM logits shapes
    private static int selectGreedyToken(auto logits, int input_token_count) {
        if (logits instanceof ML::Tensor) {
            return QoreRegistryModelProcessor::selectGreedyTokenFromTensor(
                cast<ML::Tensor>(logits), input_token_count);
        }
        auto row = QoreRegistryModelProcessor::selectLogitRow(logits, input_token_count);
        return QoreRegistryModelProcessor::selectBestTokenFromLogitRow(row);
    }

    #! Selects the highest-scoring token ID from tensor logits without materializing the full vocab scores
    private static int selectGreedyTokenFromTensor(ML::Tensor logits, int input_token_count) {
        list<int> shape = logits.getShape();
        if (!shape || shape[shape.lsize() - 1] <= 0) {
            throw "PROCESSOR-ERROR",
                sprintf("registry-model task 'causal-lm' expected logits with a non-empty vocab axis; got shape %y",
                    shape);
        }

        auto indices = logits.argmax(axis: -1).toList();
        switch (shape.lsize()) {
            case 1:
                return indices.toInt();
            case 2: {
                if (indices.typeCode() != NT_LIST || !indices) {
                    throw "PROCESSOR-ERROR",
                        sprintf("registry-model task 'causal-lm' expected argmax token IDs for logits shape %y; got %y",
                            shape, indices);
                }
                int idx = shape[0] == 1
                    ? 0
                    : QoreRegistryModelProcessor::clampSequenceIndex(input_token_count, indices.lsize());
                return indices[idx].toInt();
            }
            case 3: {
                if (indices.typeCode() != NT_LIST || !indices || indices[0].typeCode() != NT_LIST || !indices[0]) {
                    throw "PROCESSOR-ERROR",
                        sprintf("registry-model task 'causal-lm' expected batch token IDs for logits shape %y; got %y",
                            shape, indices);
                }
                auto sequence_indices = indices[0];
                int idx = QoreRegistryModelProcessor::clampSequenceIndex(input_token_count,
                    sequence_indices.lsize());
                return sequence_indices[idx].toInt();
            }
        }

        auto row = QoreRegistryModelProcessor::selectLogitRow(logits.toList(), input_token_count);
        return QoreRegistryModelProcessor::selectBestTokenFromLogitRow(row);
    }

    #! Selects the final sequence index for causal-LM outputs
    private static int clampSequenceIndex(int input_token_count, int row_count) {
        int idx = input_token_count - 1;
        if (idx < 0) {
            idx = 0;
        }
        if (idx >= row_count) {
            idx = row_count - 1;
        }
        return idx;
    }

    #! Selects the maximum-scoring token ID from one logits row
    private static int selectBestTokenFromLogitRow(auto row) {
        if (row.typeCode() != NT_LIST || !row) {
            throw "PROCESSOR-ERROR",
                sprintf("registry-model task 'causal-lm' expected a non-empty logits vector; got %y",
                    row);
        }

        int best_token = 0;
        float best_score = row[0].toFloat();
        for (int i = 1; i < row.lsize(); ++i) {
            float score = row[i].toFloat();
            if (score > best_score) {
                best_score = score;
                best_token = i;
            }
        }
        return best_token;
    }

    #! Selects the final available sequence row from [B,S,V], [S,V], [B,V], or [V] logits
    private static auto selectLogitRow(auto logits, int input_token_count) {
        if (logits.typeCode() != NT_LIST || !logits) {
            return logits;
        }
        auto first = logits[0];
        if (first.typeCode() != NT_LIST) {
            return logits;
        }
        if (!first) {
            return first;
        }
        if (first[0].typeCode() == NT_LIST) {
            list<auto> sequence_rows = first;
            int idx = QoreRegistryModelProcessor::clampSequenceIndex(input_token_count, sequence_rows.lsize());
            return sequence_rows[idx];
        }

        if (logits.lsize() == 1) {
            return first;
        }
        int idx = QoreRegistryModelProcessor::clampSequenceIndex(input_token_count, logits.lsize());
        return logits[idx];
    }

    #! Resolves optional causal-LM stop token IDs from options, plan, or tokenizer EOS metadata
    private list<int> resolveCausalStopTokenIds(hash<auto> copts) {
        if (exists copts.stop_token_ids) {
            return QoreRegistryModelProcessor::requireIntList(copts.stop_token_ids,
                "stop_token_ids");
        }
        *auto plan_ids = getExecutionPlanValue(
            ("stop_token_ids", "eos_token_ids", "stop_ids"), ("generation",));
        if (exists plan_ids) {
            return QoreRegistryModelProcessor::requireIntList(plan_ids,
                "execution plan stop_token_ids");
        }

        int eos = causal_tokenizer.getEosTokenId();
        if (eos >= 0) {
            return (eos,);
        }
        return ();
    }

    #! Builds truthful KV-cache metadata for initial causal-LM execution
    private hash<auto> buildCausalKvCacheMetadata() {
        hash<auto> rv = {
            "enabled": False,
            "supported": False,
            "reason": "stateful KV-cache reuse is not exposed by the current Qore ONNX execution path",
        };
        *auto plan_cache = getExecutionPlanValue(("kv_cache", "past_key_values"), ("generation",));
        if (exists plan_cache) {
            rv.requested = True;
            rv.plan = plan_cache;
        }
        return rv;
    }

    #! Reads a value from the execution plan root or named subsections
    private *auto getExecutionPlanValue(list<string> plan_keys, list<string> sections) {
        if (!execution_plan) {
            return;
        }
        foreach string key in (plan_keys) {
            if (execution_plan.hasKey(key)) {
                return execution_plan{key};
            }
        }
        foreach string section in (sections) {
            if (!execution_plan.hasKey(section) || execution_plan{section}.typeCode() != NT_HASH) {
                continue;
            }
            hash<auto> subplan = execution_plan{section};
            foreach string key in (plan_keys) {
                if (subplan.hasKey(key)) {
                    return subplan{key};
                }
            }
        }
        return NOTHING;
    }

    #! Reads an optional string from the execution plan
    private *string getExecutionPlanString(list<string> plan_keys, list<string> sections) {
        *auto value = getExecutionPlanValue(plan_keys, sections);
        if (exists value && value.val()) {
            return string(value);
        }
        return NOTHING;
    }

    #! Reads a string from the execution plan with a default value
    private string getExecutionPlanStringDefault(list<string> plan_keys, list<string> sections,
            string default_value) {
        *string value = getExecutionPlanString(plan_keys, sections);
        return value.val() ? value : default_value;
    }

    #! Reads an integer from the execution plan with a default value
    private int getExecutionPlanInt(list<string> plan_keys, list<string> sections, int default_value) {
        *auto value = getExecutionPlanValue(plan_keys, sections);
        if (exists value && value.val()) {
            return int(value);
        }
        return default_value;
    }

    #! Reads a boolean from the execution plan with a default value
    private bool getExecutionPlanBool(list<string> plan_keys, list<string> sections, bool default_value) {
        *auto value = getExecutionPlanValue(plan_keys, sections);
        if (exists value) {
            return value ? True : False;
        }
        return default_value;
    }

    #! Converts a list-like value to list<int> with clear validation errors
    private static list<int> requireIntList(auto value, string desc) {
        if (value.typeCode() != NT_LIST) {
            throw "PROCESSOR-ERROR",
                sprintf("%s must be a list of token IDs; got %y", desc, value);
        }
        list<int> rv;
        foreach auto v in (value) {
            push rv, int(v);
        }
        return rv;
    }

    #! Converts integer rows to a dense int64 tensor
    private static auto makeInt64Tensor(list<list<int>> rows, int width, string input_name) {
        int row_count = rows.size();
        buffer<int64> data = buffer<int64>::sized(row_count * width);
        int offset = 0;
        for (int i = 0; i < row_count; ++i) {
            if (rows[i].size() != width) {
                throw "PROCESSOR-ERROR",
                    sprintf("token tensor '%s' row %d has length %d; expected %d",
                        input_name, i, rows[i].size(), width);
            }
            for (int j = 0; j < width; ++j) {
                data[offset++] = rows[i][j];
            }
        }
        return ML::Tensor::fromBuffer(data, (row_count, width));
    }

    #! Applies task-aware output metadata for event-mode ONNX registry models
    private hash<auto> applyTaskOutputPlan(hash<auto> model_output) {
        if (!execution_task.val() || execution_task == "raw") {
            return model_output;
        }
        switch (execution_task) {
            case "regression":
                return makeRegressionOutput(model_output);
            case "classification":
            case "sequence-classification":
                return makeClassificationOutput(model_output);
        }
        return model_output;
    }

    private hash<auto> makeRegressionOutput(hash<auto> model_output) {
        string output_name = resolveTaskOutputName(model_output,
            ("value_output", "prediction_output", "score_output", "output_name"));
        return {
            "task": execution_task,
            "kind": "regression",
            "value": scalarOutputValue(model_output{output_name}),
            "output_name": output_name,
            "raw_outputs": model_output,
        };
    }

    private hash<auto> makeClassificationOutput(hash<auto> model_output) {
        string label_name = resolveTaskOutputName(model_output,
            ("label_output", "label_name", "class_output"));
        *string scores_name = resolveOptionalTaskOutputName(model_output,
            ("scores_output", "probabilities_output", "probability_output", "logits_output"),
            ("probabilities", "probability", "scores", "logits"));

        hash<auto> rv = {
            "task": execution_task,
            "kind": "classification",
            "label": scalarOutputValue(model_output{label_name}),
            "label_output_name": label_name,
            "raw_outputs": model_output,
        };
        if (scores_name.val()) {
            rv.scores = model_output{scores_name};
            rv.scores_output_name = scores_name;
        }
        return rv;
    }

    private string resolveTaskOutputName(hash<auto> model_output, list<string> plan_keys) {
        *string rv = resolveOptionalTaskOutputName(model_output, plan_keys, ());
        if (rv.val()) {
            return rv;
        }
        list<string> output_names = sort(keys model_output);
        if (!output_names) {
            throw "PROCESSOR-ERROR",
                sprintf("registry-model task %y produced no ONNX outputs", execution_task);
        }
        return output_names[0];
    }

    private *string resolveOptionalTaskOutputName(hash<auto> model_output,
            list<string> plan_keys, list<string> preferred_names) {
        if (execution_plan) {
            foreach string key in (plan_keys) {
                if (execution_plan.hasKey(key) && execution_plan{key}.val()) {
                    return string(execution_plan{key});
                }
            }
            if (exists execution_plan.output && execution_plan.output.typeCode() == NT_HASH) {
                foreach string key in (plan_keys) {
                    if (execution_plan.output.hasKey(key) && execution_plan.output{key}.val()) {
                        return string(execution_plan.output{key});
                    }
                }
                if (execution_plan.output.name.val()) {
                    return string(execution_plan.output.name);
                }
            }
        }

        foreach string preferred in (preferred_names) {
            if (model_output.hasKey(preferred)) {
                return preferred;
            }
        }
        return NOTHING;
    }

    private auto scalarOutputValue(auto value) {
        if (value.typeCode() == NT_LIST && value.lsize() == 1) {
            return value[0];
        }
        return value;
    }

    #! Appends compact column output for one registry ONNX tensor inference window
    private appendTensorWindowColumnResult(reference<hash<auto>> result, list<list<float>> matrix,
            bool single_window) {
        hash<auto> outputs = model.runFeatureMatrixTensors(matrix, input_tensor);
        int count = matrix.lsize();
        foreach string okey in (keys outputs) {
            appendTensorOutputColumns(\result, okey, cast<ML::Tensor>(outputs{okey}), count, single_window);
        }
    }

    private appendTensorOutputColumns(reference<hash<auto>> result, string name, ML::Tensor tensor,
            int row_count, bool single_window) {
        list<int> shape = tensor.getShape();
        if ((shape.lsize() == 1 && shape[0] == row_count)
                || (shape.lsize() == 2 && shape[0] == row_count && shape[1] == 1)) {
            appendColumnValues(\result, name, tensor.getBuffer(), single_window);
            return;
        }

        if (shape.lsize() == 2 && shape[0] == row_count) {
            list<auto> columns = tensor.toColumnBuffers();
            for (int j = 0; j < shape[1]; ++j) {
                appendColumnValues(\result, sprintf("%s_%d", name, j), columns[j], single_window);
            }
            return;
        }

        auto nested = tensor.toList();
        if (nested.typeCode() == NT_LIST && nested.lsize() == row_count) {
            appendColumnValues(\result, name, nested);
            return;
        }

        appendColumnValues(\result, name,
            DataProvider::AbstractDataProcessor::repeatValue(nested, row_count));
    }

    private appendColumnValues(reference<hash<auto>> result, string name, auto values, bool single_window = False) {
        if (single_window && values.typeCode() == NT_BUFFER && !result.hasKey(name)) {
            result{name} = values;
            return;
        }
        if (!result.hasKey(name)) {
            result{name} = ();
        }
        if (values.typeCode() == NT_BUFFER) {
            result{name} += values.toList();
            return;
        }
        result{name} += values;
    }

    private hash<auto> processTensorShapeWindows(list<list<float>> matrix, int count) {
        if (count <= window_size) {
            return model.runFeatureMatrixTensors(matrix, input_tensor);
        }

        list<hash<auto>> windows;
        for (int start = 0; start < count; start += window_size) {
            int end = start + window_size;
            if (end > count) {
                end = count;
            }
            list<list<float>> window = matrix[start..(end - 1)];
            push windows, model.runFeatureMatrixTensors(window, input_tensor);
        }
        return {
            "row_count": count,
            "window_size": window_size,
            "windows": windows,
        };
    }

    private hash<auto> processMappedTensorShapeWindows(hash<auto> mapped_matrices, int count) {
        if (count <= window_size) {
            return runMappedTensorWindow(mapped_matrices, 0, count, True);
        }

        list<hash<auto>> windows;
        for (int start = 0; start < count; start += window_size) {
            int end = start + window_size;
            if (end > count) {
                end = count;
            }
            push windows, runMappedTensorWindow(mapped_matrices, start, end - start, False);
        }
        return {
            "row_count": count,
            "window_size": window_size,
            "windows": windows,
        };
    }

    private hash<auto> runMappedTensorWindow(hash<auto> mapped_matrices, int start, int count, bool single_window) {
        hash<auto> inputs;
        hash<string, list<string>> imap = input_map;
        foreach string tensor_name in (keys imap) {
            list<list<float>> matrix = cast<list<list<float>>>(mapped_matrices{tensor_name});
            list<list<float>> window = single_window && count == matrix.lsize()
                ? matrix
                : matrix[start..(start + count - 1)];
            string dtype = model.getOnnxInputTensorDtype(tensor_name);
            inputs{tensor_name} = new ML::Tensor(window, NOTHING, dtype);
        }
        return model.runTensors(inputs);
    }

    #! Converts a tensor output to row-addressable values where possible
    private auto tensorOutputToRows(ML::Tensor tensor, int row_count) {
        list<int> shape = tensor.getShape();
        if (shape.lsize() == 1 && shape[0] == row_count) {
            return tensor.getBuffer().toList();
        }
        if (shape.lsize() == 2 && shape[0] == row_count && shape[1] == 1) {
            return tensor.getBuffer().toList();
        }
        auto nested = tensor.toList();
        if (nested.typeCode() == NT_LIST && nested.lsize() == row_count) {
            return nested;
        }
        return nested;
    }
}

} # namespace DataProviderML
