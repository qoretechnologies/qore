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
public namespace DataProviderML;

#! Pipeline processor that loads a pre-trained model from the model registry and runs inference
/** Loads a trained ML model from a @ref QoreModelRegistry::ModelRegistry "ModelRegistry"
    by name (and optional version/tag), then applies \c predict() to each window of records.

    The model is loaded once at construction time and reused for all subsequent windows.
    Supports any model type that provides a \c predict() method (KMeans, LinearRegression,
    IsolationForest, etc.).

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
public class QoreRegistryModelProcessor inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreRegistryModelProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_AGGREGATE,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = {
            "registry_path": <DataProviderOptionInfo>{
                "display_name": "Registry Path",
                "short_desc": "Filesystem path to the model registry",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Base directory path for the model registry. For Qorus, "
                    "this is typically `$OMQ_DIR/user/models`.",
                "required": True,
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
                "type": AbstractDataProviderTypeMap."list",
                "desc": "List of field names from each record to pass as input "
                    "features to the model's `predict()` method.",
                "required": True,
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
        #! The loaded model object
        object model;

        #! Model name (for output events)
        string model_name;

        #! Version ID of the loaded model
        string loaded_version;

        #! Input field names
        list<string> fields;

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
        fields = copts.fields;
        if (copts.window_size) {
            window_size = copts.window_size;
        }

        # Create registry and load model
        hash<auto> reg_opts = {"path": copts.registry_path};
        if (copts.tenant_id) {
            reg_opts.tenant_id = copts.tenant_id;
        }
        QoreModelRegistry::ModelRegistry registry(reg_opts);

        if (copts.tag) {
            model = registry.loadByTag(model_name, copts.tag);
            # Get the version ID for reporting
            list<hash<auto>> versions = registry.listVersions(model_name);
            foreach hash<auto> v in (versions) {
                if (v.tags.contains(copts.tag)) {
                    loaded_version = v.version_id;
                }
            }
        } else {
            model = registry.load(model_name, copts.version);
            # Determine loaded version
            if (copts.version && copts.version != "latest") {
                loaded_version = copts.version;
            } else {
                list<hash<auto>> versions = registry.listVersions(model_name);
                if (versions) {
                    loaded_version = versions.last().version_id;
                }
            }
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
        return False;
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
}
