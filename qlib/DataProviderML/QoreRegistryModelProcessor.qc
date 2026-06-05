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
            "output_names": <DataProviderOptionInfo>{
                "display_name": "Output Tensor Names",
                "short_desc": "Only materialize selected ONNX outputs",
                "type": new SoftListDataType(AbstractDataProviderTypeMap."string", True),
                "desc": "Optional ONNX output tensor names to request from registry-backed "
                    "ONNX Runtime models. Use this for multi-output packages when a "
                    "pipeline only needs specific tensors, for example "
                    "`(\"probabilities\",)` or `\"label,probabilities\"`. Omit this "
                    "option to return all model outputs.",
                "example_value": ("probabilities",),
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

        #! Optional ONNX output tensor names to request
        *list<string> output_names;

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

        #! ONNX output tensor names exposed by the loaded model
        hash<string, bool> onnx_output_names;

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
        bool causal_lm_kv_cache_requested = False;
        bool causal_lm_kv_cache_enabled = False;
        hash<string, string> causal_lm_kv_input_output_map;
        list<string> causal_lm_kv_output_names;
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
        if (exists copts.output_names) {
            output_names = DataProvider::AbstractDataProvider::parseFieldList(copts.output_names);
            if (!output_names) {
                throw "CONSTRUCTOR-ERROR",
                    "output_names must contain at least one ONNX output tensor name; "
                    "omit output_names to request all model outputs";
            }
        }
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
            onnx_output_names = QoreRegistryModelProcessor::buildOnnxOutputNameMap(provider_diagnostics);
            if (exists input_map) {
                hash<string, list<string>> imap = input_map;
                foreach string tensor_name in (keys imap) {
                    model.getOnnxInputTensorDtype(tensor_name);
                }
            }
        } else {
            if (exists input_map) {
                throw "CONSTRUCTOR-ERROR",
                    "input_map is only supported for ONNX Runtime models; native registry models use 'fields'";
            }
            if (exists output_names) {
                throw "CONSTRUCTOR-ERROR",
                    "output_names is only supported for ONNX Runtime registry models; "
                    "native registry models do not expose ONNX output tensor selection";
            }
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
        if (provider_diagnostics && provider_diagnostics.device_binding) {
            # provider_diagnostics is captured at construction; refresh the transfer
            # counters from the live model so getDiagnostics() reports current totals
            hash<auto> dbd = provider_diagnostics.device_binding;
            dbd.transfers = model.getInferenceStats().device_binding;
            rv.device_binding = dbd;
        }
        if (provider_diagnostics && provider_diagnostics.diagnostics) {
            rv.diagnostics = provider_diagnostics.diagnostics;
        } else if (model.getManifest() && model.getManifest().diagnostics) {
            rv.diagnostics = model.getManifest().diagnostics;
        }
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

    #! Runs a registry-backed ONNX feature matrix with the configured output selection
    private hash<auto> runModelFeatureMatrix(list<list<float>> matrix) {
        return model.runFeatureMatrix(matrix, input_tensor, output_names);
    }

    #! Runs a registry-backed ONNX feature matrix and preserves dense tensor outputs
    private hash<auto> runModelFeatureMatrixTensors(list<list<float>> matrix) {
        return model.runFeatureMatrixTensors(matrix, input_tensor, output_names);
    }

    #! Runs a registry-backed ONNX input tensor with the configured output selection
    private hash<auto> runModelFeatureTensor(ML::Tensor input) {
        return model.runFeatureTensor(input, input_tensor, output_names);
    }

    #! Runs mapped registry-backed ONNX tensors with the configured output selection
    private hash<auto> runModelTensors(hash<auto> inputs) {
        return model.runTensors(inputs, output_names);
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

        hash<auto> outputs = runModelFeatureMatrix(matrix);

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
                return cast<hash>(convertColumnOutputShape(cast<hash>(result),
                    DataProvider::DataProviderShape::HashOfLists));
            }
            return cast<hash>(result);
        }
        *auto fallback = processShapeRowsFallback(records, DataProvider::DataProviderShape::HashOfLists);
        if (!exists fallback) {
            return;
        }
        return cast<hash>(fallback);
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
                return convertColumnOutputShape(cast<hash>(result), shape);
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
        int count = DataProvider::AbstractDataProcessor::shapeRecordCount(records, shape);
        if (!count) {
            return;
        }

        if (execution_task == "causal-lm") {
            return processCausalLmShapeWindows(records, shape, count);
        }

        if (exists input_map) {
            return processMappedShapeWindows(records, shape, count);
        }

        if (isTensorOutputMode() || isColumnOutputMode()) {
            *ML::Tensor direct_input = inputTensorFromDenseColumns(records, shape, count, fields,
                model.getOnnxInputTensorDtype(input_tensor));
            if (exists direct_input) {
                if (isTensorOutputMode()) {
                    return processDenseTensorShapeWindows(direct_input, count);
                }

                return processDenseColumnShapeWindows(direct_input, records, shape, count);
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
        if (isTensorOutputMode() || isColumnOutputMode()) {
            *hash<auto> direct_inputs = inputTensorsFromMappedDenseColumns(records, shape, count);
            if (exists direct_inputs) {
                if (isTensorOutputMode()) {
                    return processMappedDenseTensorShapeWindows(direct_inputs, count);
                }

                return processMappedDenseColumnShapeWindows(direct_inputs, records, shape, count);
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
            return ColumnarResult::fromColumnHash(columns);
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

    #! Builds a hash map of declared ONNX output tensor names from diagnostics
    private static hash<string, bool> buildOnnxOutputNameMap(*hash<auto> diagnostics) {
        hash<string, bool> rv = {};
        if (!diagnostics || !exists diagnostics.outputs) {
            return rv;
        }
        foreach hash<auto> output in (diagnostics.outputs) {
            if (output.name.val()) {
                rv{output.name} = True;
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
        if (isTensorOutputMode()) {
            throw "CONSTRUCTOR-ERROR",
                "registry-model task 'causal-lm' supports output_mode 'events', 'columns', "
                "and 'dataframe'; raw tensor output is not supported because generation "
                "runs an iterative tokenizer/decoder loop";
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
        if (!output_names && onnx_output_names.hasKey(causal_lm_logits_output_name)) {
            output_names = (causal_lm_logits_output_name,);
        }

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
        configureCausalKvCache();
        causal_lm_kv_cache_metadata = buildCausalKvCacheMetadata();
    }

    #! Configures causal-LM KV-cache reuse from execution-plan metadata and ONNX tensor names
    private configureCausalKvCache() {
        causal_lm_kv_cache_requested = isCausalKvCacheRequested();
        if (!causal_lm_kv_cache_requested) {
            return;
        }

        causal_lm_kv_input_output_map = resolveCausalKvInputOutputMap();
        causal_lm_kv_output_names = uniqueStringValues(causal_lm_kv_input_output_map);
        causal_lm_kv_cache_enabled = causal_lm_kv_input_output_map.size() > 0;
        if (!causal_lm_kv_cache_enabled) {
            return;
        }

        if (exists output_names) {
            foreach string output_name in (causal_lm_kv_output_names) {
                ensureCausalLmOutputName(output_name);
            }
        } else if (onnx_output_names.hasKey(causal_lm_logits_output_name)) {
            output_names = (causal_lm_logits_output_name,);
            foreach string output_name in (causal_lm_kv_output_names) {
                ensureCausalLmOutputName(output_name);
            }
        }
    }

    #! Returns True when the execution plan requests causal-LM KV-cache reuse
    private bool isCausalKvCacheRequested() {
        *auto plan_cache = getExecutionPlanValue(("kv_cache", "past_key_values"), ("generation",));
        if (!exists plan_cache) {
            return False;
        }
        if (plan_cache.typeCode() == NT_HASH) {
            hash<auto> cache = plan_cache;
            return !exists cache.enabled || cache.enabled.toBool();
        }
        return plan_cache.toBool();
    }

    #! Resolves a mapping from past-input tensor names to present-output tensor names
    private hash<string, string> resolveCausalKvInputOutputMap() {
        *auto plan_cache = getExecutionPlanValue(("kv_cache", "past_key_values"), ("generation",));
        if (exists plan_cache && plan_cache.typeCode() == NT_HASH) {
            hash<auto> cache = plan_cache;
            *auto explicit_map = exists cache.input_output_map ? cache.input_output_map
                : (exists cache.map ? cache.map : NOTHING);
            if (exists explicit_map) {
                if (explicit_map.typeCode() != NT_HASH) {
                    throw "CONSTRUCTOR-ERROR",
                        "causal-lm kv_cache.input_output_map must be a hash of past input names "
                        "to present output names";
                }
                return normalizeCausalKvInputOutputMap(explicit_map);
            }
        }

        hash<string, string> rv;
        foreach string input_name in (sort(keys onnx_input_names)) {
            if (!isLikelyCausalPastInput(input_name)) {
                continue;
            }
            *string output_name = inferCausalPresentOutputName(input_name);
            if (output_name.val()) {
                rv{input_name} = output_name;
            }
        }
        return rv;
    }

    #! Validates an explicitly configured KV-cache input/output map
    private hash<string, string> normalizeCausalKvInputOutputMap(hash<auto> raw_map) {
        hash<string, string> rv;
        foreach auto i in (raw_map.pairIterator()) {
            string input_name = string(i.key);
            string output_name = string(i.value);
            if (!onnx_input_names.hasKey(input_name)) {
                throw "CONSTRUCTOR-ERROR",
                    sprintf("causal-lm kv_cache input %y is not an ONNX input; available inputs: %y",
                        input_name, sort(keys onnx_input_names));
            }
            if (!onnx_output_names.hasKey(output_name)) {
                throw "CONSTRUCTOR-ERROR",
                    sprintf("causal-lm kv_cache output %y is not an ONNX output; available outputs: %y",
                        output_name, sort(keys onnx_output_names));
            }
            rv{input_name} = output_name;
        }
        return rv;
    }

    #! Returns unique hash values while preserving deterministic order
    private static list<string> uniqueStringValues(hash<string, string> values) {
        list<string> rv = ();
        foreach string key in (sort(keys values)) {
            string value = values{key};
            if (!rv.contains(value)) {
                push rv, value;
            }
        }
        return rv;
    }

    #! Adds an output name to the internal ONNX output selection
    private ensureCausalLmOutputName(string output_name) {
        if (!onnx_output_names.hasKey(output_name)) {
            throw "CONSTRUCTOR-ERROR",
                sprintf("registry-model task 'causal-lm' requires ONNX output %y for KV-cache reuse; "
                    "available outputs: %y", output_name, sort(keys onnx_output_names));
        }
        if (!exists output_names) {
            output_names = (output_name,);
            return;
        }
        list<string> names = output_names;
        if (!names.contains(output_name)) {
            push names, output_name;
            output_names = names;
        }
    }

    #! Returns True for common ONNX causal-LM past-key/value input names
    private static bool isLikelyCausalPastInput(string name) {
        return name.find("past") >= 0 || name.find("cache") >= 0;
    }

    #! Infers the present-output tensor name corresponding to a past-input tensor
    private *string inferCausalPresentOutputName(string input_name) {
        list<string> candidates;
        if (startsWith(input_name, "past_key_values.")) {
            push candidates, "present." + input_name[16..];
        }
        if (startsWith(input_name, "past.")) {
            push candidates, "present." + input_name[5..];
        }
        if (startsWith(input_name, "past_")) {
            push candidates, "present_" + input_name[5..];
        }
        if (startsWith(input_name, "past")) {
            push candidates, "present" + input_name[4..];
        }

        foreach string candidate in (candidates) {
            if (onnx_output_names.hasKey(candidate)) {
                return candidate;
            }
        }
        return NOTHING;
    }

    #! Returns True when \a value starts with \a prefix
    private static bool startsWith(string value, string prefix) {
        return value.size() >= prefix.size() && value[0..(prefix.size() - 1)] == prefix;
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
        hash<auto> outputs = runModelFeatureMatrix(matrix);
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

    #! Processes shaped causal-LM input without materializing row event hashes for compact output
    private hash<auto> processCausalLmShapeWindows(auto records,
            DataProvider::DataProviderShape shape, int count) {
        list<string> prompts = causalLmPromptsFromShape(records, shape, count);
        hash<auto> result;

        if (isColumnOutputMode()) {
            addCausalLmInputColumns(\result, records, shape, prompts);
            appendCausalLmColumnPredictions(\result, prompts);
            return result;
        }

        appendCausalLmEventColumns(\result, prompts);
        return result;
    }

    #! Reads causal-LM prompt strings directly from a shaped column block when possible
    private list<string> causalLmPromptsFromShape(auto records,
            DataProvider::DataProviderShape shape, int count) {
        if (prompt_field.find(".") < 0) {
            *auto column = causalLmPromptColumnFromShape(records, shape);
            if (exists column) {
                return QoreRegistryModelProcessor::columnToStrings(column, count, prompt_field);
            }
        }

        list<auto> rows = DataProvider::AbstractDataProcessor::convertShape(
            records, shape, DataProvider::DataProviderShape::RowRecords);
        list<string> prompts;
        foreach auto row in (rows) {
            push prompts, string(DataProvider::AbstractDataProvider::getFieldValue(row, prompt_field));
        }
        return prompts;
    }

    #! Reads the top-level causal-LM prompt column from a shaped block
    private *auto causalLmPromptColumnFromShape(auto records,
            DataProvider::DataProviderShape shape) {
        switch (shape) {
            case DataProvider::DataProviderShape::DataFrameBlock:
                if (records.columnNames().contains(prompt_field)) {
                    return records.getColumn(prompt_field);
                }
                break;
            case DataProvider::DataProviderShape::BufferColumns:
                if (DataProvider::AbstractDataProcessor::isColumnarResult(records)) {
                    if (records.columnNames().contains(prompt_field)) {
                        return records.getColumn(prompt_field);
                    }
                } else if (records.hasKey(prompt_field)) {
                    return records{prompt_field};
                }
                break;
            case DataProvider::DataProviderShape::HashOfLists:
                if (records.hasKey(prompt_field)) {
                    return records{prompt_field};
                }
                break;
        }
        return NOTHING;
    }

    #! Converts a shaped column to string values using row-mode value semantics
    private static list<string> columnToStrings(auto column, int count, string field) {
        list<auto> values = column.typeCode() == NT_BUFFER ? column.toList() : column;
        if (values.lsize() != count) {
            throw "PROCESSOR-SHAPE-ERROR",
                sprintf("text field %y has %d values but the shaped block has %d rows",
                    field, values.lsize(), count);
        }

        list<string> rv;
        foreach auto value in (values) {
            push rv, value.toString();
        }
        return rv;
    }

    #! Adds original causal-LM input fields to compact column output when requested
    private addCausalLmInputColumns(reference<hash<auto>> result, auto records,
            DataProvider::DataProviderShape shape, list<string> prompts) {
        if (!include_input_fields) {
            return;
        }

        foreach string field in (fields) {
            if (field.find(".") >= 0 || result.hasKey(field)) {
                continue;
            }
            if (field == prompt_field) {
                result{field} = prompts;
                continue;
            }
            *auto column = topLevelColumnFromShape(records, shape, field);
            if (exists column) {
                result{field} = column;
            }
        }
    }

    #! Reads a top-level field column from a shaped block
    private *auto topLevelColumnFromShape(auto records,
            DataProvider::DataProviderShape shape, string field) {
        switch (shape) {
            case DataProvider::DataProviderShape::DataFrameBlock:
                if (records.columnNames().contains(field)) {
                    return records.getColumn(field);
                }
                break;
            case DataProvider::DataProviderShape::BufferColumns:
                if (DataProvider::AbstractDataProcessor::isColumnarResult(records)) {
                    if (records.columnNames().contains(field)) {
                        return records.getColumn(field);
                    }
                } else if (records.hasKey(field)) {
                    return records{field};
                }
                break;
            case DataProvider::DataProviderShape::HashOfLists:
                if (records.hasKey(field)) {
                    return records{field};
                }
                break;
        }
        return NOTHING;
    }

    #! Appends compact causal-LM generation columns
    private appendCausalLmColumnPredictions(reference<hash<auto>> result, list<string> prompts) {
        int count = prompts.lsize();
        list<string> texts;
        list<list<int>> generated_token_ids;
        buffer<int64> input_token_counts = buffer<int64>::sized(count);
        buffer<int64> generated_token_counts = buffer<int64>::sized(count);
        list<string> finish_reasons;

        for (int i = 0; i < count; ++i) {
            hash<auto> prediction = generateCausalLmText(prompts[i]);
            push texts, prediction.text;
            push generated_token_ids, prediction.generated_token_ids;
            input_token_counts[i] = prediction.input_token_count;
            generated_token_counts[i] = prediction.generated_token_count;
            push finish_reasons, prediction.finish_reason;

            if (include_diagnostics) {
                push result{"kv_cache"}, prediction.kv_cache;
                push result{"last_outputs"}, prediction.last_outputs;
                push result{"inference_stats"}, model.getInferenceStats();
            }
        }

        result.generated_text = texts;
        result.generated_token_ids = generated_token_ids;
        result.input_token_count = input_token_counts;
        result.generated_token_count = generated_token_counts;
        result.finish_reason = finish_reasons;
    }

    #! Appends event-mode hash-of-lists output for shaped causal-LM input
    private appendCausalLmEventColumns(reference<hash<auto>> result, list<string> prompts) {
        foreach string prompt in (prompts) {
            hash<auto> prediction = generateCausalLmText(prompt);
            hash<auto> record_values = {
                prompt_field: prompt,
            };

            push result{"event_type"}, "registry-model";
            push result{"model_name"}, model_name;
            push result{"version_id"}, loaded_version;
            push result{"record_values"}, record_values;
            push result{"execution_provider"}, execution_provider;
            push result{"provider_diagnostics"}, provider_diagnostics;
            if (include_diagnostics) {
                push result{"inference_stats"}, model.getInferenceStats();
            }
            push result{"prediction"}, prediction;
        }
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
        *hash<auto> kv_cache_values;

        for (int step = 0; step < causal_lm_max_new_tokens; ++step) {
            hash<auto> outputs = runModelTensors(buildCausalLmInputs(token_ids,
                attention_mask, token_type_ids, kv_cache_values));
            last_outputs = summarizeCausalLmOutputs(outputs);
            string logits_name = resolveCausalLmLogitsOutputName(outputs);
            int next_token = QoreRegistryModelProcessor::selectGreedyToken(
                outputs{logits_name}, token_ids.size());
            if (causal_lm_kv_cache_enabled) {
                kv_cache_values = extractCausalKvCacheOutputs(outputs);
            }

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
            list<int> token_type_ids, *hash<auto> kv_cache_values) {
        hash<auto> inputs;
        if (!onnx_input_names.hasKey(causal_lm_input_ids_name)) {
            throw "PROCESSOR-ERROR",
                sprintf("registry-model task 'causal-lm' requires ONNX input tensor %y; available inputs: %y",
                    causal_lm_input_ids_name, sort(keys onnx_input_names));
        }
        bool reuse_cache = causal_lm_kv_cache_enabled && exists kv_cache_values;
        list<int> input_ids = reuse_cache ? (token_ids[token_ids.size() - 1],) : token_ids;
        inputs{causal_lm_input_ids_name} = QoreRegistryModelProcessor::makeInt64Tensor(
            (input_ids,), input_ids.size(), causal_lm_input_ids_name);

        if (onnx_input_names.hasKey(causal_lm_attention_mask_name)) {
            inputs{causal_lm_attention_mask_name} = QoreRegistryModelProcessor::makeInt64Tensor(
                (attention_mask,), attention_mask.size(), causal_lm_attention_mask_name);
        }
        if (onnx_input_names.hasKey(causal_lm_token_type_ids_name)) {
            list<int> step_token_type_ids = reuse_cache && token_type_ids
                ? (token_type_ids[token_type_ids.size() - 1],)
                : token_type_ids;
            inputs{causal_lm_token_type_ids_name} = QoreRegistryModelProcessor::makeInt64Tensor(
                (step_token_type_ids,), step_token_type_ids.size(), causal_lm_token_type_ids_name);
        }
        if (causal_lm_kv_cache_enabled) {
            foreach auto i in (causal_lm_kv_input_output_map.pairIterator()) {
                if (exists kv_cache_values) {
                    if (!kv_cache_values.hasKey(i.value)) {
                        throw "PROCESSOR-ERROR",
                            sprintf("registry-model task 'causal-lm' expected KV-cache output %y "
                                "from the previous decode step; available outputs: %y",
                                i.value, sort(keys kv_cache_values));
                    }
                    inputs{i.key} = kv_cache_values{i.value};
                } else {
                    inputs{i.key} = makeInitialCausalKvTensor(i.key);
                }
            }
        }
        return inputs;
    }

    #! Extracts present-key/value tensors from one causal-LM ONNX output hash
    private hash<auto> extractCausalKvCacheOutputs(hash<auto> outputs) {
        hash<auto> rv;
        foreach string output_name in (causal_lm_kv_output_names) {
            if (!outputs.hasKey(output_name)) {
                throw "PROCESSOR-ERROR",
                    sprintf("registry-model task 'causal-lm' expected KV-cache output %y; "
                        "available outputs: %y", output_name, sort(keys outputs));
            }
            rv{output_name} = outputs{output_name};
        }
        return rv;
    }

    #! Creates an empty initial cache tensor for the first causal-LM decode step
    private auto makeInitialCausalKvTensor(string input_name) {
        hash<auto> info = requireOnnxInputInfo(input_name);
        list<int> shape = resolveInitialCausalKvShape(input_name, info);
        string dtype = onnxTypeToTensorDtype(info.type);
        return QoreRegistryModelProcessor::makeZeroTensor(dtype, shape, input_name);
    }

    #! Returns the ONNX input metadata hash for \a input_name
    private hash<auto> requireOnnxInputInfo(string input_name) {
        if (!provider_diagnostics || !exists provider_diagnostics.inputs) {
            throw "PROCESSOR-ERROR",
                "registry-model task 'causal-lm' cannot initialize KV-cache tensors because "
                "ONNX input metadata is not available";
        }
        foreach hash<auto> input in (provider_diagnostics.inputs) {
            if (input.name == input_name) {
                return input;
            }
        }
        throw "PROCESSOR-ERROR",
            sprintf("registry-model task 'causal-lm' cannot initialize KV-cache input %y; "
                "available inputs: %y", input_name, sort(keys onnx_input_names));
    }

    #! Resolves the initial zero-length cache shape for a past-key/value input
    private list<int> resolveInitialCausalKvShape(string input_name, hash<auto> info) {
        *list<int> configured = getConfiguredInitialCausalKvShape(input_name);
        if (exists configured) {
            return configured;
        }

        list<int> shape = info.shape;
        list<string> symbolic = exists info.symbolic_shape ? info.symbolic_shape : ();
        list<int> rv;
        bool has_zero_cache_dim = False;
        for (int i = 0; i < shape.size(); ++i) {
            int dim = shape[i];
            string symbol = i < symbolic.size() ? symbolic[i] : "";
            if (dim >= 0) {
                push rv, dim;
                if (dim == 0) {
                    has_zero_cache_dim = True;
                }
                continue;
            }
            if (isLikelyPastSequenceSymbol(symbol)) {
                push rv, 0;
                has_zero_cache_dim = True;
            } else if (isLikelyBatchSymbol(symbol) || i == 0) {
                push rv, 1;
            } else {
                throw "PROCESSOR-ERROR",
                    sprintf("registry-model task 'causal-lm' cannot infer an initial shape for "
                        "KV-cache input %y from shape %y and symbolic shape %y; set "
                        "execution.plan.kv_cache.initial_shapes.%s explicitly",
                        input_name, shape, symbolic, input_name);
            }
        }
        if (!has_zero_cache_dim) {
            throw "PROCESSOR-ERROR",
                sprintf("registry-model task 'causal-lm' cannot identify the zero-length cache "
                    "axis for KV-cache input %y from shape %y and symbolic shape %y; set "
                    "execution.plan.kv_cache.initial_shapes.%s explicitly",
                    input_name, shape, symbolic, input_name);
        }
        return rv;
    }

    #! Reads an explicit initial KV-cache shape from the execution plan
    private *list<int> getConfiguredInitialCausalKvShape(string input_name) {
        *auto plan_cache = getExecutionPlanValue(("kv_cache", "past_key_values"), ("generation",));
        if (!exists plan_cache || plan_cache.typeCode() != NT_HASH) {
            return NOTHING;
        }
        hash<auto> cache = plan_cache;
        *auto shape_map = exists cache.initial_shapes ? cache.initial_shapes
            : (exists cache.shapes ? cache.shapes : NOTHING);
        if (!exists shape_map) {
            return NOTHING;
        }
        if (shape_map.typeCode() != NT_HASH) {
            throw "CONSTRUCTOR-ERROR",
                "causal-lm kv_cache.initial_shapes must be a hash of input tensor names to shape lists";
        }
        hash<auto> shapes = shape_map;
        if (!shapes.hasKey(input_name)) {
            return NOTHING;
        }
        return QoreRegistryModelProcessor::requireIntList(shapes{input_name},
            sprintf("kv_cache.initial_shapes.%s", input_name));
    }

    #! Maps ONNX tensor type names to ML::Tensor storage dtypes
    private static string onnxTypeToTensorDtype(string type) {
        switch (type) {
            case "float": return "float32";
            case "double": return "float64";
            case "float16": return "float32";
            case "bfloat16": return "float32";
            case "int8": return "int8";
            case "int16": return "int16";
            case "int32": return "int32";
            case "int64": return "int64";
            case "uint8": return "int64";
            case "uint16": return "int64";
            case "uint32": return "int64";
            case "uint64": return "int64";
            case "bool": return "bool";
        }
        throw "PROCESSOR-ERROR",
            sprintf("registry-model task 'causal-lm' cannot create an initial KV-cache "
                "tensor for ONNX type %y", type);
    }

    #! Creates a zero-filled dense tensor with the requested dtype and shape
    private static auto makeZeroTensor(string dtype, list<int> shape, string input_name) {
        int count = QoreRegistryModelProcessor::shapeElementCount(shape, input_name);
        switch (dtype) {
            case "float32": return ML::Tensor::fromBuffer(buffer<float32>::sized(count), shape, True);
            case "float64": return ML::Tensor::fromBuffer(buffer<float64>::sized(count), shape, True);
            case "int64": return ML::Tensor::fromBuffer(buffer<int64>::sized(count), shape, True);
            case "int32": return ML::Tensor::fromBuffer(buffer<int32>::sized(count), shape, True);
            case "int16": return ML::Tensor::fromBuffer(buffer<int16>::sized(count), shape, True);
            case "int8": return ML::Tensor::fromBuffer(buffer<int8>::sized(count), shape, True);
            case "bool": return ML::Tensor::fromBuffer(buffer<bool>::sized(count), shape, True);
        }
        throw "PROCESSOR-ERROR",
            sprintf("registry-model task 'causal-lm' cannot create an initial KV-cache "
                "tensor with dtype %y for input %y", dtype, input_name);
    }

    #! Computes shape element count for initial cache tensors
    private static int shapeElementCount(list<int> shape, string input_name) {
        int count = 1;
        foreach int dim in (shape) {
            if (dim < 0) {
                throw "PROCESSOR-ERROR",
                    sprintf("registry-model task 'causal-lm' initial KV-cache shape for %y "
                        "contains negative dimension %d", input_name, dim);
            }
            count *= dim;
        }
        return count;
    }

    #! Returns True for symbolic dimensions that identify the past sequence axis
    private static bool isLikelyPastSequenceSymbol(string symbol) {
        return symbol.find("past") >= 0 || symbol.find("cache") >= 0
            || symbol.find("sequence") >= 0 || symbol.find("seq") >= 0;
    }

    #! Returns True for symbolic dimensions that identify a batch axis
    private static bool isLikelyBatchSymbol(string symbol) {
        return symbol.find("batch") >= 0;
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
        hash<auto> rv;
        rv.requested = causal_lm_kv_cache_requested;
        rv.enabled = causal_lm_kv_cache_enabled;
        rv.supported = causal_lm_kv_cache_enabled;
        if (output_names) {
            rv.selected_outputs = output_names;
        }
        if (causal_lm_kv_cache_enabled) {
            rv.input_output_map = causal_lm_kv_input_output_map;
            rv.present_outputs = causal_lm_kv_output_names;
            rv.initial_cache = "empty";
        } else if (causal_lm_kv_cache_requested) {
            rv.reason = "no matching ONNX past-input/present-output tensor pairs were found";
        }
        *auto plan_cache = getExecutionPlanValue(("kv_cache", "past_key_values"), ("generation",));
        if (exists plan_cache) {
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
        return ML::Tensor::fromBuffer(data, (row_count, width), True);
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
        *string planned = getPlannedTaskOutputName(plan_keys);
        if (planned.val()) {
            if (model_output.hasKey(planned)) {
                return planned;
            }
            throw "PROCESSOR-ERROR",
                sprintf("registry-model task %y requires ONNX output %y, but the "
                    "current inference result contains only %y%s",
                    execution_task, planned, sort(keys model_output),
                    exists output_names
                        ? sprintf("; include %y in output_names or use raw columns/tensor output mode",
                            planned)
                        : "");
        }

        list<string> returned_outputs = sort(keys model_output);
        if (!returned_outputs) {
            throw "PROCESSOR-ERROR",
                sprintf("registry-model task %y produced no ONNX outputs", execution_task);
        }
        return returned_outputs[0];
    }

    private *string resolveOptionalTaskOutputName(hash<auto> model_output,
            list<string> plan_keys, list<string> preferred_names) {
        *string planned = getPlannedTaskOutputName(plan_keys);
        if (planned.val() && model_output.hasKey(planned)) {
            return planned;
        }

        foreach string preferred in (preferred_names) {
            if (model_output.hasKey(preferred)) {
                return preferred;
            }
        }
        return NOTHING;
    }

    private *string getPlannedTaskOutputName(list<string> plan_keys) {
        if (execution_plan) {
            *string planned = getExecutionPlanString(plan_keys, ("output", "postprocess"));
            if (planned.val()) {
                return planned;
            }
            foreach string section in (("output", "postprocess")) {
                if (exists execution_plan{section}
                        && execution_plan{section}.typeCode() == NT_HASH
                        && execution_plan{section}.name.val()) {
                    return string(execution_plan{section}.name);
                }
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
        hash<auto> outputs = runModelFeatureMatrixTensors(matrix);
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

    private hash<auto> runDenseTensorWindow(ML::Tensor input, int start, int count, bool single_window) {
        ML::Tensor window = single_window && count == input.getShape()[0]
            ? input
            : input.sliceRows(start, count);
        return runModelFeatureTensor(window);
    }

    private hash<auto> processDenseColumnShapeWindows(ML::Tensor input, auto records,
            DataProvider::DataProviderShape shape, int count) {
        hash<auto> result;
        hash<auto> output_windows;
        hash<auto> output_counts;
        addInputFieldColumns(\result, records, shape, ());
        for (int start = 0; start < count; start += window_size) {
            int end = start + window_size;
            if (end > count) {
                end = count;
            }
            int window_count = end - start;
            bool single_window = start == 0 && end == count;
            hash<auto> outputs = runDenseTensorWindow(input, start, window_count, single_window);
            collectTensorOutputWindow(\output_windows, \output_counts, outputs, window_count);
        }
        appendTensorOutputWindows(\result, output_windows, output_counts, count);
        return result;
    }

    private hash<auto> processDenseTensorShapeWindows(ML::Tensor input, int count) {
        if (count <= window_size) {
            return runDenseTensorWindow(input, 0, count, True);
        }

        list<hash<auto>> windows;
        for (int start = 0; start < count; start += window_size) {
            int end = start + window_size;
            if (end > count) {
                end = count;
            }
            push windows, runDenseTensorWindow(input, start, end - start, False);
        }
        return {
            "row_count": count,
            "window_size": window_size,
            "windows": windows,
        };
    }

    private hash<auto> runMappedDenseTensorWindow(hash<auto> dense_inputs, int start, int count,
            bool single_window) {
        hash<auto> inputs;
        foreach string tensor_name in (keys dense_inputs) {
            ML::Tensor input = cast<ML::Tensor>(dense_inputs{tensor_name});
            inputs{tensor_name} = single_window && count == input.getShape()[0]
                ? input
                : input.sliceRows(start, count);
        }
        return runModelTensors(inputs);
    }

    private hash<auto> processMappedDenseColumnShapeWindows(hash<auto> dense_inputs, auto records,
            DataProvider::DataProviderShape shape, int count) {
        hash<auto> result;
        hash<auto> output_windows;
        hash<auto> output_counts;
        addInputFieldColumns(\result, records, shape, ());
        for (int start = 0; start < count; start += window_size) {
            int end = start + window_size;
            if (end > count) {
                end = count;
            }
            int window_count = end - start;
            bool single_window = start == 0 && end == count;
            hash<auto> outputs = runMappedDenseTensorWindow(dense_inputs, start, window_count, single_window);
            collectTensorOutputWindow(\output_windows, \output_counts, outputs, window_count);
        }
        appendTensorOutputWindows(\result, output_windows, output_counts, count);
        return result;
    }

    private hash<auto> processMappedDenseTensorShapeWindows(hash<auto> dense_inputs, int count) {
        if (count <= window_size) {
            return runMappedDenseTensorWindow(dense_inputs, 0, count, True);
        }

        list<hash<auto>> windows;
        for (int start = 0; start < count; start += window_size) {
            int end = start + window_size;
            if (end > count) {
                end = count;
            }
            push windows, runMappedDenseTensorWindow(dense_inputs, start, end - start, False);
        }
        return {
            "row_count": count,
            "window_size": window_size,
            "windows": windows,
        };
    }

    private collectTensorOutputWindow(reference<hash<auto>> output_windows, reference<hash<auto>> output_counts,
            hash<auto> outputs, int row_count) {
        foreach string okey in (keys outputs) {
            if (!output_windows.hasKey(okey)) {
                output_windows{okey} = ();
                output_counts{okey} = ();
            }
            push output_windows{okey}, cast<ML::Tensor>(outputs{okey});
            push output_counts{okey}, row_count;
        }
    }

    private appendTensorOutputWindows(reference<hash<auto>> result, hash<auto> output_windows,
            hash<auto> output_counts, int total_count) {
        foreach string okey in (keys output_windows) {
            list<ML::Tensor> tensors = cast<list<ML::Tensor>>(output_windows{okey});
            list<int> counts = cast<list<int>>(output_counts{okey});
            if (canConcatTensorOutputWindows(tensors, counts, total_count)) {
                ML::Tensor tensor = tensors.lsize() == 1 ? tensors[0] : ML::Tensor::concatRows(tensors);
                appendTensorOutputColumns(\result, okey, tensor, total_count, True);
                continue;
            }

            for (int i = 0; i < tensors.lsize(); ++i) {
                appendTensorOutputColumns(\result, okey, tensors[i], counts[i], tensors.lsize() == 1);
            }
        }
    }

    private bool canConcatTensorOutputWindows(list<ML::Tensor> tensors, list<int> counts, int total_count) {
        if (!tensors || tensors.lsize() != counts.lsize()) {
            return False;
        }

        list<int> first_shape = tensors[0].getShape();
        if (!first_shape || first_shape[0] != counts[0]) {
            return False;
        }
        string dtype = tensors[0].getDtype();
        int count_sum = counts[0];

        for (int i = 1; i < tensors.lsize(); ++i) {
            list<int> shape = tensors[i].getShape();
            if (!shape || shape[0] != counts[i] || shape.lsize() != first_shape.lsize()
                    || tensors[i].getDtype() != dtype) {
                return False;
            }
            for (int j = 1; j < shape.lsize(); ++j) {
                if (shape[j] != first_shape[j]) {
                    return False;
                }
            }
            count_sum += counts[i];
        }

        return count_sum == total_count;
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
            return runModelFeatureMatrixTensors(matrix);
        }

        list<hash<auto>> windows;
        for (int start = 0; start < count; start += window_size) {
            int end = start + window_size;
            if (end > count) {
                end = count;
            }
            list<list<float>> window = matrix[start..(end - 1)];
            push windows, runModelFeatureMatrixTensors(window);
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
        return runModelTensors(inputs);
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
