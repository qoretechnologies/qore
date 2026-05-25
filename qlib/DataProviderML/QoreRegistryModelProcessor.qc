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
                    "row-major feature matrix under `input_tensor`.",
                "required": True,
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
            "window_size": <DataProviderOptionInfo>{
                "display_name": "Window Size",
                "short_desc": "Number of records per processing batch",
                "type": AbstractDataProviderTypeMap."int",
                "desc": "Number of records to accumulate before processing. "
                    "Default: 100.",
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
        fields = DataProvider::AbstractDataProvider::parseFieldList(copts.fields);
        input_tensor = copts.input_tensor;
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

        configured = True;
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

            push events, <RegistryModelEventInfo>{
                "event_type": "registry-model",
                "model_name": model_name,
                "version_id": loaded_version,
                "record_values": record_values,
                "prediction": model_output,
            };
        }

        return events;
    }
}

} # namespace DataProviderML
