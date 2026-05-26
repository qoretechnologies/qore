# -*- mode: qore; indent-tabs-mode: nil -*-

/*  ModelRegistryDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Main QoreModelRegistry namespace
public namespace QoreModelRegistry {

class ModelRegistryDataProviderHelper {
    static hash<auto> registerPackageRecord(ModelRegistry registry, string model_name, hash<auto> rec) {
        string package_path = requireString(rec, "package_path", "model package registration");
        if (!is_dir(package_path)) {
            throw "ML-REGISTRY-ERROR",
                sprintf("model package registration requires field 'package_path' to be an existing directory; "
                    "got '%s'", package_path);
        }

        string version_id = registry.registerPackage(package_path, makePackageOptions(model_name, rec));
        return makeVersionRecord(registry.getVersionManifest(model_name, version_id));
    }

    static hash<auto> createArtifactRecord(ModelRegistry registry, string model_name, string version_id,
            hash<auto> rec) {
        string artifact_path;
        if (exists rec.artifact_path) {
            artifact_path = requireString(rec, "artifact_path", "named artifact registration");
            if (!is_file(artifact_path)) {
                throw "ML-REGISTRY-ERROR",
                    sprintf("named artifact registration requires field 'artifact_path' to be an existing file; "
                        "got '%s'", artifact_path);
            }
        }

        string artifact_name = rec.artifact_name
            ? requireString(rec, "artifact_name", "named artifact registration")
            : (artifact_path ? basename(artifact_path) : NOTHING);
        if (!artifact_name.val()) {
            throw "ML-REGISTRY-ERROR",
                "named artifact registration requires non-empty string field 'artifact_name' when "
                "'artifact_path' is not provided";
        }

        binary data;
        if (artifact_path) {
            data = ReadOnlyFile::readBinaryFile(artifact_path);
        } else if (exists rec.artifact_data) {
            if (rec.artifact_data.typeCode() != NT_BINARY) {
                throw "ML-REGISTRY-ERROR",
                    sprintf("named artifact registration field 'artifact_data' must be binary; got %y",
                        rec.artifact_data.fullType());
            }
            data = rec.artifact_data;
        } else {
            throw "ML-REGISTRY-ERROR",
                "named artifact registration requires either 'artifact_path' or binary 'artifact_data'";
        }

        hash<ModelArtifactInfo> info = artifact_path
            ? ModelArtifactInspector::inspectFile(artifact_path, artifact_name)
            : makeArtifactInfo(artifact_name, data);
        applyArtifactOverrides(\info, rec);
        registry.registerNamedArtifact(model_name, version_id, artifact_name, data, info);
        return makeArtifactRecord(info);
    }

    static hash<auto> makeVersionRecord(hash<ModelVersionInfo> version) {
        hash<auto> rec = cast<hash<auto>>(version);
        rec.artifact_count = version.artifacts ? version.artifacts.size() : 0;
        if (version.providers) {
            rec.active_provider = version.providers.active_provider;
            rec.provider_diagnostics = version.providers.diagnostics;
        }
        if (version.validation) {
            rec.validation_valid = version.validation.valid;
            rec.validation_issues = version.validation.issues;
        }
        if (version.diagnostics) {
            rec.diagnostics = version.diagnostics;
        }
        if (version.execution) {
            rec.entry_artifact = version.execution.entry_artifact;
            rec.local_executable = version.execution.local_executable;
        }
        if (version.tokenizer) {
            rec.tokenizer_json = version.tokenizer.tokenizer_json;
            rec.tokenizer_config_json = version.tokenizer.tokenizer_config_json;
            rec.model_max_length = version.tokenizer.model_max_length;
            rec.has_chat_template = version.tokenizer.has_chat_template;
        } else {
            rec.has_chat_template = False;
        }
        return rec;
    }

    static hash<auto> makeArtifactRecord(hash<ModelArtifactInfo> artifact) {
        return cast<hash<auto>>(artifact);
    }

    static hash<string, AbstractDataField> getVersionRecordType() {
        return {
            "version_id": new QoreDataField("version_id", "Version identifier",
                StringType),
            "name": new QoreDataField("name", "Model name", StringType),
            "algorithm": new QoreDataField("algorithm", "Algorithm class name",
                StringType),
            "created": new QoreDataField("created", "Registration timestamp",
                StringType),
            "description": new QoreDataField("description", "Version description",
                SoftStringOrNothingType),
            "file_size": new QoreDataField("file_size", "Primary artifact size in bytes",
                IntType),
            "format": new QoreDataField("format", "Primary artifact format",
                StringType),
            "tags": new QoreDataField("tags", "Tags list",
                AbstractDataProviderTypeMap."list"),
            "metrics": new QoreDataField("metrics", "Performance metrics",
                AbstractDataProviderTypeMap."*hash"),
            "training_data_hash": new QoreDataField("training_data_hash",
                "Training data hash", SoftStringOrNothingType),
            "tenant_id": new QoreDataField("tenant_id", "Tenant identifier",
                SoftStringOrNothingType),
            "framework": new QoreDataField("framework", "Source framework",
                SoftStringOrNothingType),
            "artifact_kind": new QoreDataField("artifact_kind", "Artifact kind",
                SoftStringOrNothingType),
            "runtime": new QoreDataField("runtime", "Preferred runtime",
                SoftStringOrNothingType),
            "task": new QoreDataField("task", "Model task identifier",
                SoftStringOrNothingType),
            "opset": new QoreDataField("opset", "ONNX opset version",
                IntOrNothingType),
            "producer": new QoreDataField("producer", "Model producer metadata",
                SoftStringOrNothingType),
            "minimum_onnxruntime_version": new QoreDataField(
                "minimum_onnxruntime_version",
                "Minimum ONNX Runtime version required by deployment policy",
                SoftStringOrNothingType),
            "executable": new QoreDataField("executable", "Executable by Qore",
                BoolType),
            "safety": new QoreDataField("safety", "Safety classification",
                StringType),
            "artifact_count": new QoreDataField("artifact_count",
                "Number of artifacts in the version", IntType),
            "entry_artifact": new QoreDataField("entry_artifact",
                "Runtime entry artifact", SoftStringOrNothingType),
            "local_executable": new QoreDataField("local_executable",
                "Executable in-process by Qore", BoolOrNothingType),
            "active_provider": new QoreDataField("active_provider",
                "Active ONNX Runtime provider", SoftStringOrNothingType),
            "validation_valid": new QoreDataField("validation_valid",
                "ONNX validation result", BoolOrNothingType),
            "tokenizer_json": new QoreDataField("tokenizer_json",
                "Tokenizer artifact name", SoftStringOrNothingType),
            "tokenizer_config_json": new QoreDataField("tokenizer_config_json",
                "Tokenizer config artifact name", SoftStringOrNothingType),
            "model_max_length": new QoreDataField("model_max_length",
                "Tokenizer maximum sequence length", IntOrNothingType),
            "has_chat_template": new QoreDataField("has_chat_template",
                "Tokenizer has a chat template", BoolType),
            "config": new QoreDataField("config", "Version configuration",
                AbstractDataProviderTypeMap."*hash"),
            "inputs": new QoreDataField("inputs", "ONNX input tensor contract",
                AbstractDataProviderTypeMap."list"),
            "outputs": new QoreDataField("outputs", "ONNX output tensor contract",
                AbstractDataProviderTypeMap."list"),
            "dynamic_axes": new QoreDataField("dynamic_axes",
                "Dynamic-axis metadata keyed by tensor name",
                AbstractDataProviderTypeMap."*hash"),
            "external_data": new QoreDataField("external_data",
                "ONNX external-data artifact names",
                AbstractDataProviderTypeMap."list"),
            "preprocess": new QoreDataField("preprocess", "Preprocessing contract",
                AbstractDataProviderTypeMap."*hash"),
            "postprocess": new QoreDataField("postprocess", "Postprocessing contract",
                AbstractDataProviderTypeMap."*hash"),
            "providers": new QoreDataField("providers",
                "ONNX provider deployment diagnostics", AbstractDataProviderTypeMap."*hash"),
            "provider_diagnostics": new QoreDataField("provider_diagnostics",
                "ONNX provider diagnostics list", AbstractDataProviderTypeMap."list"),
            "validation": new QoreDataField("validation",
                "ONNX validation diagnostics", AbstractDataProviderTypeMap."*hash"),
            "validation_issues": new QoreDataField("validation_issues",
                "ONNX validation issues", AbstractDataProviderTypeMap."list"),
            "diagnostics": new QoreDataField("diagnostics",
                "Actionable ONNX/package diagnostics", AbstractDataProviderTypeMap."*hash"),
            "benchmarks": new QoreDataField("benchmarks", "Benchmark metadata",
                AbstractDataProviderTypeMap."*hash"),
            "golden_vectors": new QoreDataField("golden_vectors",
                "Golden validation vectors", AbstractDataProviderTypeMap."list"),
            "optimized_artifacts": new QoreDataField("optimized_artifacts",
                "Optimized ONNX/ORT artifacts", AbstractDataProviderTypeMap."list"),
            "quantized_artifacts": new QoreDataField("quantized_artifacts",
                "Quantized ONNX artifacts", AbstractDataProviderTypeMap."list"),
            "artifacts": new QoreDataField("artifacts", "Artifact metadata list",
                AbstractDataProviderTypeMap."list"),
            "execution": new QoreDataField("execution", "Execution metadata",
                AbstractDataProviderTypeMap."*hash"),
            "tokenizer": new QoreDataField("tokenizer", "Tokenizer metadata",
                AbstractDataProviderTypeMap."*hash"),
        };
    }

    static hash<string, AbstractDataField> getPackageCreateRecordType(bool include_name) {
        hash<string, AbstractDataField> fields = {
            "package_path": new QoreDataField("package_path",
                "Filesystem directory containing the model package", StringType),
            "version_id": new QoreDataField("version_id", "Optional version identifier",
                SoftStringOrNothingType),
            "description": new QoreDataField("description", "Version description",
                SoftStringOrNothingType),
            "metrics": new QoreDataField("metrics", "Performance metrics",
                AbstractDataProviderTypeMap."*hash"),
            "tags": new QoreDataField("tags", "Tags list",
                AbstractDataProviderTypeMap."list"),
            "training_data_hash": new QoreDataField("training_data_hash",
                "Training data hash", SoftStringOrNothingType),
            "algorithm": new QoreDataField("algorithm", "Algorithm name override",
                SoftStringOrNothingType),
            "framework": new QoreDataField("framework", "Framework name override",
                SoftStringOrNothingType),
            "task": new QoreDataField("task", "Model task identifier",
                SoftStringOrNothingType),
            "opset": new QoreDataField("opset", "ONNX opset version",
                IntOrNothingType),
            "producer": new QoreDataField("producer", "Producer metadata",
                SoftStringOrNothingType),
            "minimum_onnxruntime_version": new QoreDataField(
                "minimum_onnxruntime_version",
                "Minimum ONNX Runtime version required by deployment policy",
                SoftStringOrNothingType),
            "dynamic_axes": new QoreDataField("dynamic_axes",
                "Dynamic-axis metadata keyed by tensor name",
                AbstractDataProviderTypeMap."*hash"),
            "preprocess": new QoreDataField("preprocess", "Preprocessing contract",
                AbstractDataProviderTypeMap."*hash"),
            "postprocess": new QoreDataField("postprocess", "Postprocessing contract",
                AbstractDataProviderTypeMap."*hash"),
            "validation": new QoreDataField("validation",
                "ONNX validation diagnostics to record",
                AbstractDataProviderTypeMap."*hash"),
            "benchmarks": new QoreDataField("benchmarks", "Benchmark metadata",
                AbstractDataProviderTypeMap."*hash"),
            "golden_vectors": new QoreDataField("golden_vectors",
                "Golden validation vectors", AbstractDataProviderTypeMap."list"),
            "config": new QoreDataField("config", "Version configuration",
                AbstractDataProviderTypeMap."*hash"),
            "registration_options": new QoreDataField("registration_options",
                "Additional package registration options", AbstractDataProviderTypeMap."*hash"),
        };
        if (include_name) {
            fields.name = new QoreDataField("name", "Model name", StringType);
        }
        return fields;
    }

    static string requireString(hash<auto> rec, string key, string action) {
        if (!exists rec{key} || rec{key}.typeCode() != NT_STRING || !rec{key}.val()) {
            throw "ML-REGISTRY-ERROR",
                sprintf("%s requires non-empty string field '%s'", action, key);
        }
        return rec{key};
    }

    private static hash<auto> makePackageOptions(string model_name, hash<auto> rec) {
        hash<auto> options = {};
        options.name = model_name;
        if (exists rec.registration_options) {
            if (rec.registration_options.typeCode() != NT_HASH) {
                throw "ML-REGISTRY-ERROR",
                    sprintf("model package registration field 'registration_options' must be hash; got %y",
                        rec.registration_options.fullType());
            }
            options += rec.registration_options;
            options.name = model_name;
        }
        foreach string key in (("version_id", "description", "metrics", "tags",
                "training_data_hash", "algorithm", "framework", "task", "opset",
                "producer", "minimum_onnxruntime_version", "dynamic_axes",
                "preprocess", "postprocess", "validation", "benchmarks",
                "golden_vectors", "config")) {
            if (exists rec{key}) {
                options{key} = rec{key};
            }
        }
        return options;
    }

    private static hash<ModelArtifactInfo> makeArtifactInfo(string artifact_name, binary data) {
        string format = inferArtifactFormat(artifact_name);
        return <ModelArtifactInfo>{
            "name": artifact_name,
            "role": inferArtifactRole(artifact_name, format),
            "format": format,
            "size": data.size(),
            "safe_to_parse": format == "qml" || format == "onnx" || format == "json"
                || format == "safetensors",
            "executable": format == "qml" || format == "onnx",
        };
    }

    private static nothing applyArtifactOverrides(reference<hash<ModelArtifactInfo>> info, hash<auto> rec) {
        foreach string key in (("role", "format", "checksum", "safe_to_parse",
                "executable", "path_hint", "metadata")) {
            if (exists rec{key}) {
                info{key} = rec{key};
            }
        }
    }

    private static string inferArtifactFormat(string name) {
        string lname = name.lwr();
        if (lname =~ /[.]qml$/) {
            return "qml";
        }
        if (lname =~ /[.]onnx$/) {
            return "onnx";
        }
        if (lname =~ /[.]json$/) {
            return "json";
        }
        if (lname =~ /[.]safetensors$/) {
            return "safetensors";
        }
        if (lname =~ /[.](pt|pth|bin)$/) {
            return "pytorch-pickle";
        }
        if (lname =~ /[.]pt2$/) {
            return "torch-export";
        }
        return "binary";
    }

    private static string inferArtifactRole(string name, string format) {
        string lname = name.lwr();
        if (lname == "tokenizer.json") {
            return "tokenizer";
        }
        if (lname == "config.json" || lname == "tokenizer_config.json"
                || lname == "generation_config.json" || lname == "special_tokens_map.json") {
            return "config";
        }
        if (format == "safetensors" || format == "pytorch-pickle" || format == "torch-export") {
            return "weights";
        }
        return "model";
    }
}

#! Root data provider for the model registry — lists models as children
/** Navigate: model-registry → {model_name} → {version_id}.

    The root provider supports DataProvider record creation for registering a
    model package. The create record requires \c name and \c package_path and
    accepts the same optional metadata keys used by
    @ref QoreModelRegistry::ModelRegistry::registerPackage() "ModelRegistry::registerPackage()".
    The returned record is the inspected version manifest with convenience
    fields such as \c artifact_count, \c runtime, \c tokenizer_json, and
    \c entry_artifact.

    @since QoreModelRegistry 2.0
*/
public class ModelRegistryDataProvider inherits AbstractDataProvider {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "type": "ModelRegistryDataProvider",
            "supports_create": True,
            "supports_children": True,
            "children_can_support_records": True,
            "children_identical": True,
            "has_record": True,
            "constructor_options": ConstructorOptions,
        };

        #! Supported backend type values for the \c backend_type option
        const BackendTypes = (
            <AllowedValueInfo>{"value": "filesystem", "display_name": "Filesystem"},
            <AllowedValueInfo>{"value": "database", "display_name": "Database (SqlUtil)"},
            <AllowedValueInfo>{"value": "rest", "display_name": "REST API"},
        );

        const ConstructorOptions = {
            "backend_type": <DataProviderOptionInfo>{
                "display_name": "Backend Type",
                "short_desc": "Storage backend type for the model registry",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The type of storage backend to use: 'filesystem' (default), "
                    "'database', or 'rest'; backend-specific options must also be provided",
                "allowed_values": BackendTypes,
                "default_value": "filesystem",
            },
            "path": <DataProviderOptionInfo>{
                "display_name": "Registry Path",
                "short_desc": "Filesystem path for the model registry",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Base directory path for the filesystem-backed model registry; "
                    "required when backend_type is 'filesystem'",
            },
            "datasource": <DataProviderOptionInfo>{
                "display_name": "Datasource",
                "short_desc": "Database datasource for database backend",
                "type": AbstractDataProviderTypeMap."any",
                "desc": "An AbstractDatasource object for database storage; "
                    "required when backend_type is 'database'",
            },
            "table_prefix": <DataProviderOptionInfo>{
                "display_name": "Table Prefix",
                "short_desc": "Table name prefix for database backend",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Prefix for database table names (default 'ml_'); "
                    "only used with backend_type 'database'",
            },
            "rest_client": <DataProviderOptionInfo>{
                "display_name": "REST Client",
                "short_desc": "REST client object for REST backend",
                "type": AbstractDataProviderTypeMap."any",
                "desc": "A REST client object with get/post/put/del methods; "
                    "required when backend_type is 'rest'",
            },
            "rest_base_path": <DataProviderOptionInfo>{
                "display_name": "REST Base Path",
                "short_desc": "Base path for REST API endpoints",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Base URL path for model registry REST endpoints (e.g., '/models'); "
                    "required when backend_type is 'rest'",
            },
            "rest_options": <DataProviderOptionInfo>{
                "display_name": "REST Options",
                "short_desc": "Additional REST backend options",
                "type": AbstractDataProviderTypeMap."hash",
                "desc": "Optional hash with REST backend options: "
                    "'get_headers' (callback returning extra request headers), "
                    "'max_cache_entries' (max cached artifacts, default 100); "
                    "only used with backend_type 'rest'",
            },
            "backend": <DataProviderOptionInfo>{
                "display_name": "Backend",
                "short_desc": "Pre-constructed model registry backend",
                "type": AbstractDataProviderTypeMap."any",
                "desc": "A pre-constructed AbstractModelRegistryBackend instance; "
                    "when provided, all other options except 'tenant_id' are ignored",
            },
            "tenant_id": <DataProviderOptionInfo>{
                "display_name": "Tenant ID",
                "short_desc": "Tenant identifier for multi-tenant registries",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Optional tenant identifier to scope registry access",
            },
        };
    }

    private {
        #! The model registry instance
        ModelRegistry registry;

        #! Cached model list from the backend (avoids redundant round trips)
        *list<string> cached_models;

        #! Timestamp when the model list cache was populated
        *date cache_time;

        #! Cache TTL in seconds; model list is refreshed after this interval
        const CacheTtlSeconds = 5;
    }

    #! Creates from constructor options
    /** Supports the following construction modes:
        - \c backend: a pre-constructed AbstractModelRegistryBackend instance
        - \c backend_type = "filesystem" (default): requires \c path
        - \c backend_type = "database": requires \c datasource
        - \c backend_type = "rest": requires \c rest_client and \c rest_base_path

        @param options constructor option hash; see @ref ConstructorOptions

        @throw CONSTRUCTOR-ERROR if required backend-specific options are missing
    */
    constructor(*hash<auto> options) {
        hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        # qorus#244: backend dispatch now lives on ModelRegistry itself so
        # every registry consumer (this provider, QoreRegistryModelProcessor,
        # direct callers) builds backends through one code path.
        try {
            registry = new ModelRegistry(copts);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "ML-REGISTRY-ERROR") {
                throw "CONSTRUCTOR-ERROR", ex.desc;
            }
            rethrow;
        }
    }

    #! Creates from an existing registry
    constructor(ModelRegistry registry) {
        self.registry = registry;
    }

    string getName() {
        return "model-registry";
    }

    *string getDesc() {
        return "ML model registry — browse models and versions";
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    private *list<string> getChildProviderNamesImpl() {
        return getModelListCached();
    }

    private *AbstractDataProvider getChildProviderImpl(string name) {
        *list<string> models = getModelListCached();
        if (!models || !models.contains(name)) {
            return;
        }
        return new ModelVersionsDataProvider(registry, name);
    }

    private *hash<string, AbstractDataField> getRecordTypeImpl(
            *hash<auto> search_options) {
        return ModelRegistryDataProviderHelper::getVersionRecordType();
    }

    private *hash<string, AbstractDataField> getCreateRecordTypeImpl(
            *hash<auto> create_options) {
        return ModelRegistryDataProviderHelper::getPackageCreateRecordType(True);
    }

    private *hash<auto> createRecordImpl(hash<auto> rec, *hash<auto> create_options) {
        string model_name = ModelRegistryDataProviderHelper::requireString(rec, "name",
            "model package registration");
        hash<auto> record = ModelRegistryDataProviderHelper::registerPackageRecord(registry, model_name, rec);
        remove cached_models;
        remove cache_time;
        return record;
    }

    #! Returns the cached model list, refreshing from the backend when stale
    private *list<string> getModelListCached() {
        if (cached_models && cache_time
                && (now_us() - cache_time).durationSecondsFloat() < CacheTtlSeconds) {
            return cached_models;
        }
        cached_models = registry.listModels();
        cache_time = now_us();
        return cached_models;
    }

}

#! Data provider for a specific model's versions — supports record operations
/** Lists and manages versions of a specific model.

    Records are model version manifests. Creating a record registers another
    package version for the current model and requires \c package_path. Version
    IDs are also exposed as child provider names; each version child is a
    @ref ModelVersionArtifactsDataProvider that lists and accepts named
    artifacts for that version.

    @since QoreModelRegistry 2.0
*/
public class ModelVersionsDataProvider inherits AbstractDataProvider {
    public {
        const SearchOptions = {
            "version_id": <DataProviderOptionInfo>{
                "display_name": "Version ID",
                "short_desc": "Filter by version identifier",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Filter versions by their unique version identifier",
            },
            "algorithm": <DataProviderOptionInfo>{
                "display_name": "Algorithm",
                "short_desc": "Filter by algorithm name",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Filter versions by algorithm class name (e.g., `KMeans`, `LinearRegression`)",
            },
        };

        const ProviderInfo = <DataProviderInfo>{
            "type": "ModelVersionsDataProvider",
            "supports_read": True,
            "supports_create": True,
            "supports_delete": True,
            "supports_native_search": True,
            "supports_children": True,
            "children_can_support_records": True,
            "children_identical": True,
            "has_record": True,
            "search_options": SearchOptions,
        };
    }

    private {
        ModelRegistry registry;
        string model_name;
    }

    constructor(ModelRegistry registry, string model_name) {
        self.registry = registry;
        self.model_name = model_name;
    }

    string getName() {
        return model_name;
    }

    *string getDesc() {
        return sprintf("Versions of model '%s'", model_name);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    private *list<string> getChildProviderNamesImpl() {
        list<string> version_ids();
        foreach hash<auto> v in (registry.listVersions(model_name)) {
            push version_ids, v.version_id;
        }
        return version_ids;
    }

    private *AbstractDataProvider getChildProviderImpl(string name) {
        *list<string> version_ids = getChildProviderNamesImpl();
        if (!version_ids || !version_ids.contains(name)) {
            return;
        }
        return new ModelVersionArtifactsDataProvider(registry, model_name, name);
    }

    private *hash<string, AbstractDataField> getRecordTypeImpl(
            *hash<auto> search_options) {
        return ModelRegistryDataProviderHelper::getVersionRecordType();
    }

    private *hash<string, AbstractDataField> getCreateRecordTypeImpl(
            *hash<auto> create_options) {
        return ModelRegistryDataProviderHelper::getPackageCreateRecordType(False);
    }

    private AbstractDataProviderRecordIterator<hash<auto>> searchRecordsImpl(
            *hash<auto> where_cond, *hash<auto> search_options) {
        list<hash<auto>> versions();
        foreach hash<ModelVersionInfo> version in (registry.listVersions(model_name)) {
            push versions, ModelRegistryDataProviderHelper::makeVersionRecord(version);
        }

        # Apply simple where filtering if provided
        if (where_cond) {
            list<hash<auto>> filtered();
            foreach hash<auto> v in (versions) {
                bool match = True;
                foreach string key in (keys where_cond) {
                    if (v{key} != where_cond{key}) {
                        match = False;
                        break;
                    }
                }
                if (match) {
                    push filtered, v;
                }
            }
            versions = filtered;
        }

        return new DefaultRecordIterator(versions);
    }

    private *hash<auto> createRecordImpl(hash<auto> rec, *hash<auto> create_options) {
        return ModelRegistryDataProviderHelper::registerPackageRecord(registry, model_name, rec);
    }

    private int deleteRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        if (where_cond.version_id) {
            # Delete specific version via prune — we don't have a direct
            # version delete in the registry API, so we delete the whole model
            # only if explicitly requested
            throw "ML-REGISTRY-ERROR",
                "individual version deletion not supported via DataProvider; "
                "use prune() or deleteModel() instead";
        }
        # Delete the entire model
        registry.deleteModel(model_name);
        return 1;
    }
}

#! Data provider for artifacts belonging to one model version
/** Lists package artifacts for one registered model version and allows new
    named artifacts to be attached to that version.

    Creation accepts either \c artifact_path or binary \c artifact_data.
    \c artifact_name defaults to \c basename(artifact_path) when a path is
    supplied. Optional fields such as \c role, \c format, \c safe_to_parse,
    \c executable, \c checksum, \c path_hint, and \c metadata override the
    inferred artifact metadata before it is stored.

    @since QoreModelRegistry 2.2
*/
public class ModelVersionArtifactsDataProvider inherits AbstractDataProvider {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "type": "ModelVersionArtifactsDataProvider",
            "supports_read": True,
            "supports_create": True,
            "supports_native_search": True,
            "has_record": True,
        };
    }

    private {
        ModelRegistry registry;
        string model_name;
        string version_id;
    }

    constructor(ModelRegistry registry, string model_name, string version_id) {
        self.registry = registry;
        self.model_name = model_name;
        self.version_id = version_id;
    }

    string getName() {
        return version_id;
    }

    *string getDesc() {
        return sprintf("Artifacts of model '%s' version '%s'", model_name, version_id);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    private *hash<string, AbstractDataField> getRecordTypeImpl(
            *hash<auto> search_options) {
        return {
            "name": new QoreDataField("name", "Artifact name", StringType),
            "role": new QoreDataField("role", "Artifact role", StringType),
            "format": new QoreDataField("format", "Artifact format", StringType),
            "size": new QoreDataField("size", "Artifact size in bytes", IntType),
            "checksum": new QoreDataField("checksum", "Artifact checksum",
                SoftStringOrNothingType),
            "safe_to_parse": new QoreDataField("safe_to_parse",
                "Safe to inspect without executing code", BoolType),
            "executable": new QoreDataField("executable",
                "Executable in-process by Qore", BoolType),
            "path_hint": new QoreDataField("path_hint", "Path or object-key hint",
                SoftStringOrNothingType),
            "metadata": new QoreDataField("metadata", "Artifact metadata",
                AbstractDataProviderTypeMap."*hash"),
            "artifact_ref": new QoreDataField("artifact_ref", "Storage reference",
                AbstractDataProviderTypeMap."*hash"),
        };
    }

    private *hash<string, AbstractDataField> getCreateRecordTypeImpl(
            *hash<auto> create_options) {
        hash<string, AbstractDataField> fields = getRecordTypeImpl();
        fields.artifact_name = new QoreDataField("artifact_name",
            "Artifact name to register", StringType);
        fields.artifact_path = new QoreDataField("artifact_path",
            "Filesystem artifact path", SoftStringOrNothingType);
        fields.artifact_data = new QoreDataField("artifact_data",
            "Artifact binary payload", BinaryOrNothingType);
        return fields;
    }

    private AbstractDataProviderRecordIterator<hash<auto>> searchRecordsImpl(
            *hash<auto> where_cond, *hash<auto> search_options) {
        list<hash<auto>> artifacts();
        foreach hash<ModelArtifactInfo> artifact in (registry.listArtifacts(model_name, version_id)) {
            hash<auto> record = ModelRegistryDataProviderHelper::makeArtifactRecord(artifact);
            if (where_cond) {
                bool match = True;
                foreach string key in (keys where_cond) {
                    if (record{key} != where_cond{key}) {
                        match = False;
                        break;
                    }
                }
                if (!match) {
                    continue;
                }
            }
            push artifacts, record;
        }
        return new DefaultRecordIterator(artifacts);
    }

    private *hash<auto> createRecordImpl(hash<auto> rec, *hash<auto> create_options) {
        return ModelRegistryDataProviderHelper::createArtifactRecord(registry, model_name, version_id, rec);
    }
}

#! Simple record iterator wrapping a list iterator
/** @since QoreModelRegistry 2.0
*/
public class DefaultRecordIterator inherits AbstractDataProviderRecordIterator<hash<auto>> {
    private {
        list<hash<auto>> data;
        int pos = -1;
    }

    constructor(list<hash<auto>> data) {
        self.data = data;
    }

    string getName() {
        return "DefaultRecordIterator";
    }

    private bool nextImpl() {
        ++pos;
        return pos < data.size();
    }

    bool valid() {
        return pos >= 0 && pos < data.size();
    }

    hash<auto> getValue() {
        if (!valid()) {
            throw "INVALID-ITERATOR", "iterator is not pointing at a valid element";
        }
        return data[pos];
    }
}

} # namespace QoreModelRegistry
