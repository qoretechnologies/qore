# -*- mode: qore; indent-tabs-mode: nil -*-

/*  ModelRegistry.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! ML model registry for versioning, tagging, and deployment management
/** The ModelRegistry provides a high-level API for managing trained ML models.
    It delegates storage to a pluggable backend — filesystem, database, or REST.

    The backend is selected automatically based on constructor options:
    - \c backend → use a pre-constructed @ref AbstractModelRegistryBackend
    - \c backend_type = \c "filesystem" (or no \c backend_type with \c path set)
       → @ref FilesystemBackend (default; needs \c path)
    - \c backend_type = \c "database" → @ref DatabaseBackend (needs \c datasource)
    - \c backend_type = \c "rest" → @ref RestBackend (needs \c rest_client + \c rest_base_path)

    @par Example (filesystem):
    @code{.py}
    auto reg = new ModelRegistry({"path": "/var/models"});
    string vid = reg.register(model, {"name": "fraud-detector"});
    auto loaded = reg.load("fraud-detector");
    @endcode

    @par Example (database):
    @code{.py}
    auto reg = new ModelRegistry({
        "backend_type": "database",
        "datasource": ds,
    });
    @endcode

    @par Example (custom backend):
    @code{.py}
    auto backend = new FilesystemBackend("/var/models", "acme-corp");
    auto reg = new ModelRegistry({"backend": backend});
    @endcode

    @since QoreModelRegistry 1.0 (filesystem-only)
    @since QoreModelRegistry 2.0 (pluggable backends)
    @since QoreModelRegistry 2.1 (\c backend_type dispatch on constructor)
*/
public class ModelRegistry {
    private {
        #! Storage backend
        AbstractModelRegistryBackend backend;
    }

    #! Creates a new model registry
    /** @param options a hash with the following keys:
        - \c backend (@ref AbstractModelRegistryBackend, optional): pre-constructed backend;
          when provided, all other options except \c tenant_id are ignored
        - \c backend_type (string, optional): \c "filesystem" (default), \c "database", \c "rest"
        - \c path (string): base directory path; required when \c backend_type is \c "filesystem"
        - \c datasource (AbstractDatasource): required when \c backend_type is \c "database"
        - \c table_prefix (string, optional): table name prefix for \c "database"
        - \c rest_client (object, optional): required when \c backend_type is \c "rest"
        - \c rest_base_path (string, optional): required when \c backend_type is \c "rest"
        - \c rest_options (hash, optional): additional REST backend options
        - \c tenant_id (string, optional): tenant identifier for multi-tenant deployments

        @throw ML-REGISTRY-ERROR if required options are missing for the chosen backend

        @since QoreModelRegistry 2.1 (\c backend_type dispatch)
    */
    constructor(hash<auto> options) {
%ifdef NoJson
        throw "ML-REGISTRY-ERROR", "the QoreModelRegistry module requires the json "
            "module (module-json) to be installed; install it and try again";
%endif
        backend = ModelRegistry::createBackend(options);
    }

    #! Construct the backend implied by an options hash
    /** Centralises the \c backend / \c backend_type dispatch so processors
        and data providers can build backends without duplicating the
        switch. Called by the constructor; subclasses needing a different
        dispatch can override this method.

        @param options the constructor option hash (see @ref constructor)

        @return the selected backend

        @throw ML-REGISTRY-ERROR if required backend-specific options are missing

        @since QoreModelRegistry 2.1
    */
    static AbstractModelRegistryBackend createBackend(hash<auto> options) {
        if (options.backend instanceof AbstractModelRegistryBackend) {
            return options.backend;
        }
        string backend_type = options.backend_type.val()
            ? options.backend_type
            : "filesystem";
        switch (backend_type) {
            case "filesystem": {
                if (!options.path.val()) {
                    throw "ML-REGISTRY-ERROR",
                        "option 'path' is required when backend_type is 'filesystem'";
                }
                return new FilesystemBackend(options.path, options.tenant_id);
            }
%ifndef NoSqlUtil
            case "database": {
                if (!exists options.datasource) {
                    throw "ML-REGISTRY-ERROR",
                        "option 'datasource' is required when backend_type is 'database'";
                }
                hash<auto> db_opts;
                if (options.table_prefix.val()) {
                    db_opts.table_prefix = options.table_prefix;
                }
                if (options.tenant_id.val()) {
                    db_opts.tenant_id = options.tenant_id;
                }
                return new DatabaseBackend(options.datasource, db_opts);
            }
%endif
            case "rest": {
                if (!exists options.rest_client) {
                    throw "ML-REGISTRY-ERROR",
                        "option 'rest_client' is required when backend_type is 'rest'";
                }
                if (!options.rest_base_path.val()) {
                    throw "ML-REGISTRY-ERROR",
                        "option 'rest_base_path' is required when backend_type is 'rest'";
                }
                return new RestBackend(options.rest_client, options.rest_base_path,
                    options.tenant_id, options.rest_options);
            }
            default:
                throw "ML-REGISTRY-ERROR",
                    sprintf("unsupported backend_type %y; valid values: "
                        "'filesystem', 'database', 'rest'", backend_type);
        }
    }

    #! Returns the storage backend
    /** @return the backend instance

        @since QoreModelRegistry 2.0
    */
    AbstractModelRegistryBackend getBackend() {
        return backend;
    }

    #! Registers a fitted model in the registry
    /** @param model the fitted ML model object to register
        @param options a hash with the following keys:
        - \c name (required): the model name
        - \c description: optional description
        - \c metrics: optional hash of performance metrics
        - \c tags: optional list of string tags
        - \c training_data_hash: optional hash identifying training data

        @return the version ID string for the registered model

        @throw ML-REGISTRY-ERROR on failure
    */
    string register(object model, hash<auto> options) {
        *string name = options.name;
        if (!name.val()) {
            throw "ML-REGISTRY-ERROR", "required option 'name' not provided";
        }
        AbstractModelRegistryBackend::validateModelName(name);

        # Generate version ID: timestamp + random hex
        string version_id = makeVersionId();

        # Serialize model
        binary model_data = ML::ml_serialize(model);

        # Build metadata
        hash<ModelVersionInfo> metadata = cast<hash<ModelVersionInfo>>({
            "version_id": version_id,
            "name": name,
            "algorithm": getAlgorithmName(model),
            "created": format_date("YYYY-MM-DDTHH:mm:ssZ", now()),
            "description": options.description,
            "metrics": options.metrics,
            "tags": options.tags ?? (),
            "file_size": model_data.size(),
            "format": "qml",
            "training_data_hash": options.training_data_hash,
            "tenant_id": backend.getTenantId(),
            "framework": "qore",
            "artifact_kind": "executable",
            "runtime": "qore-ml",
            "executable": True,
            "safety": "safe",
            "artifacts": (<ModelArtifactInfo>{
                "name": "model",
                "role": "model",
                "format": "qml",
                "size": model_data.size(),
                "safe_to_parse": True,
                "executable": True,
            },),
            "execution": <ModelExecutionInfo>{
                "runtime": "qore-ml",
                "local_executable": True,
                "entry_artifact": "model",
                "requires": ("ml",),
            },
        });

        backend.registerVersion(name, model_data, metadata);

        return version_id;
    }

    #! Registers a filesystem model package directory
    /** Inspects a HuggingFace/PyTorch/ONNX/Qore package directory, registers the
        primary artifact as the version artifact, and stores companion artifacts
        by name. PyTorch pickle artifacts are recorded as unsafe/source artifacts
        and are not executable by Qore.

        @param package_path filesystem directory containing model package files
        @param options registration options; \c name is required

        @return the version ID string

        @throw ML-REGISTRY-ERROR if the package cannot be inspected or registered

        @since QoreModelRegistry 2.2
    */
    string registerPackage(string package_path, hash<auto> options) {
        *string name = options.name;
        if (!name.val()) {
            throw "ML-REGISTRY-ERROR", "required option 'name' not provided";
        }
        AbstractModelRegistryBackend::validateModelName(name);

        hash<auto> inspect_options = options + {
            "version_id": options.version_id.val() ? options.version_id : makeVersionId(),
        };
        hash<ModelVersionInfo> metadata =
            ModelArtifactInspector::inspectDirectory(package_path, name, inspect_options);
        metadata.tenant_id = backend.getTenantId();

        string primary_name = metadata.execution.entry_artifact
            ?? (metadata.artifacts ? metadata.artifacts[0].name : "model");
        string primary_path = package_path + DirSep + primary_name;
        if (!is_file(primary_path)) {
            throw "ML-REGISTRY-ERROR",
                sprintf("primary artifact '%s' not found in package '%s'",
                    primary_name, package_path);
        }

        binary primary_artifact = ReadOnlyFile::readBinaryFile(primary_path);
        metadata.file_size = primary_artifact.size();
        backend.registerVersion(name, primary_artifact, metadata);

        foreach hash<ModelArtifactInfo> artifact_info in (metadata.artifacts) {
            if (artifact_info.name == primary_name) {
                continue;
            }
            string artifact_path = package_path + DirSep + artifact_info.name;
            if (is_file(artifact_path)) {
                backend.registerNamedArtifact(name, metadata.version_id, artifact_info.name,
                    ReadOnlyFile::readBinaryFile(artifact_path), artifact_info);
            }
        }

        return metadata.version_id;
    }

    #! Lists all versions of a named model
    /** @param name the model name

        @return a list of version metadata hashes, ordered oldest to newest
    */
    list<hash<ModelVersionInfo>> listVersions(string name) {
        return backend.listVersions(name);
    }

    #! Loads a specific version of a model (or the latest version)
    /** @param name the model name
        @param version_id the version ID to load, or NOTHING/"latest" for the most recent

        @return the deserialized ML model object

        @throw ML-REGISTRY-ERROR if the model or version is not found
    */
    object load(string name, *string version_id) {
        if (!version_id.val() || version_id == "latest") {
            # Find latest version
            list<hash<ModelVersionInfo>> versions = backend.listVersions(name);
            if (!versions) {
                throw "ML-REGISTRY-ERROR",
                    sprintf("no versions found for model '%s'", name);
            }
            version_id = versions.last().version_id;
        }

        hash<ModelVersionInfo> meta = backend.getVersionMetadata(name, version_id);
        if (meta.format != "qml" && meta.runtime != "qore-ml") {
            throw "ML-REGISTRY-ERROR",
                sprintf("model '%s' version '%s' is not a native Qore ML artifact "
                    "(format=%y, runtime=%y); use loadExecutable(), loadArtifact(), "
                    "or loadNamedArtifact() according to the version metadata",
                    name, version_id, meta.format, meta.runtime);
        }

        binary artifact = backend.loadArtifact(name, version_id);
        return ML::ml_deserialize(artifact);
    }

    #! Loads a model version using its declared execution plan
    /** Native Qore ML artifacts are deserialized with \c ML::ml_deserialize().
        ONNX artifacts are loaded into @ref ML::OnnxModel when ONNX Runtime is
        available. Source-only, weights-only, tokenizer-only, and metadata-only
        packages raise an informative error.

        @param name the model name
        @param version_id the version ID to load, or NOTHING/"latest"

        @return an executable Qore object

        @throw ML-REGISTRY-ERROR if the version has no local execution plan

        @since QoreModelRegistry 2.2
    */
    object loadExecutable(string name, *string version_id) {
        if (!version_id.val() || version_id == "latest") {
            list<hash<ModelVersionInfo>> versions = backend.listVersions(name);
            if (!versions) {
                throw "ML-REGISTRY-ERROR",
                    sprintf("no versions found for model '%s'", name);
            }
            version_id = versions.last().version_id;
        }

        hash<ModelVersionInfo> meta = backend.getVersionManifest(name, version_id);
        string runtime = meta.runtime ?? (meta.execution ? meta.execution.runtime : NOTHING)
            ?? "metadata-only";
        string entry_artifact = meta.execution ? (meta.execution.entry_artifact ?? "model") : "model";

        if (runtime == "qore-ml" || meta.format == "qml") {
            return ML::ml_deserialize(backend.loadNamedArtifact(name, version_id, entry_artifact));
        }
        if (runtime == "onnxruntime" || meta.format == "onnx") {
            return loadOnnxExecutable(name, version_id, meta, entry_artifact);
        }

        throw "ML-REGISTRY-ERROR",
            sprintf("model '%s' version '%s' is not locally executable "
                "(artifact_kind=%y, runtime=%y, safety=%y); convert it to ONNX, "
                "register a native Qore ML artifact, or configure an external runtime",
                name, version_id, meta.artifact_kind, runtime, meta.safety);
    }

    #! Loads a model version and wraps it in a uniform execution adapter
    /** This method preserves @ref loadExecutable() behavior while exposing a
        stable adapter API for consumers that need to handle native Qore ML and
        ONNX Runtime models without branching on the concrete model class.

        @param name the model name
        @param version_id the version ID to load, or NOTHING/"latest"

        @return a @ref ModelExecutableAdapter for the loaded model

        @throw ML-REGISTRY-ERROR if the version has no local execution plan

        @since QoreModelRegistry 2.3
    */
    ModelExecutableAdapter loadExecutableAdapter(string name, *string version_id) {
        hash<ModelVersionInfo> manifest = getVersionManifest(name, version_id);
        return new ModelExecutableAdapter(loadExecutable(name, manifest.version_id), manifest);
    }

    #! Loads a named ONNX deployment artifact as an executable model
    /** This method is intended for optimized and quantized ONNX/ORT artifacts
        registered on a model version.  It uses the artifact's deployment
        metadata as the default ONNX Runtime session configuration and rejects
        explicit provider requests that do not match hardware-specific optimized
        artifact metadata.

        @param name the model name
        @param version_id the version ID, or NOTHING/"latest"
        @param artifact_name named ONNX/ORT artifact to load
        @param session_config optional ONNX Runtime session configuration

        @return executable @ref ML::OnnxModel object

        @throw ML-REGISTRY-ERROR if the artifact is missing, is not executable
            ONNX/ORT, or provider metadata does not match the requested provider

        @since QoreModelRegistry 2.4
    */
    object loadOnnxArtifactExecutable(string name, *string version_id, string artifact_name,
            *hash<ML::OnnxSessionConfig> session_config) {
        hash<ModelVersionInfo> manifest = getVersionManifest(name, version_id);
        hash<ModelArtifactInfo> artifact = findArtifactInfo(manifest, artifact_name);
        if (!artifact.executable || (artifact.format != "onnx" && artifact.format != "ort")) {
            throw "ML-REGISTRY-ERROR",
                sprintf("artifact '%s' for model '%s' version '%s' is not an executable "
                    "ONNX/ORT deployment artifact (role=%y, format=%y)",
                    artifact_name, name, manifest.version_id, artifact.role, artifact.format);
        }

        hash<ML::OnnxSessionConfig> config =
            buildOnnxArtifactSessionConfig(artifact, session_config);
        validateOnnxArtifactProviderRequest(name, manifest.version_id, artifact, config);

        object model = new ML::OnnxModel(
            backend.loadNamedArtifact(name, manifest.version_id, artifact_name), config);
        validateOnnxArtifactEffectiveProvider(name, manifest.version_id, artifact,
            model.getEffectiveProviderReport());
        return model;
    }

    #! Loads a named ONNX deployment artifact and wraps it in an adapter
    /** @param name the model name
        @param version_id the version ID, or NOTHING/"latest"
        @param artifact_name named ONNX/ORT artifact to load
        @param session_config optional ONNX Runtime session configuration

        @return model executable adapter

        @since QoreModelRegistry 2.4
    */
    ModelExecutableAdapter loadOnnxArtifactAdapter(string name, *string version_id,
            string artifact_name, *hash<ML::OnnxSessionConfig> session_config) {
        hash<ModelVersionInfo> manifest = getVersionManifest(name, version_id);
        if (manifest.execution) {
            manifest.execution.entry_artifact = artifact_name;
        }
        return new ModelExecutableAdapter(
            loadOnnxArtifactExecutable(name, manifest.version_id, artifact_name,
                session_config), manifest);
    }

    #! Loads a model artifact as raw binary data (without deserializing)
    /** @param name the model name
        @param version_id the version ID to load, or NOTHING/"latest" for the most recent

        @return the raw serialized model binary data

        @throw ML-REGISTRY-ERROR if the model or version is not found

        @since QoreModelRegistry 2.0
    */
    binary loadArtifact(string name, *string version_id) {
        if (!version_id.val() || version_id == "latest") {
            list<hash<ModelVersionInfo>> versions = backend.listVersions(name);
            if (!versions) {
                throw "ML-REGISTRY-ERROR",
                    sprintf("no versions found for model '%s'", name);
            }
            version_id = versions.last().version_id;
        }

        return backend.loadArtifact(name, version_id);
    }

    #! Registers an additional named artifact for an existing model version
    /** @param name the model name
        @param version_id the version ID
        @param artifact_name stable artifact name, for example \c "tokenizer.json"
        @param artifact artifact binary data
        @param metadata optional artifact metadata

        @since QoreModelRegistry 2.2
    */
    nothing registerNamedArtifact(string name, string version_id, string artifact_name,
            binary artifact, *hash<ModelArtifactInfo> metadata) {
        if (!artifact_name.val()) {
            throw "ML-REGISTRY-ERROR", "artifact_name must not be empty";
        }
        hash<ModelArtifactInfo> info = metadata ?? <ModelArtifactInfo>{
            "name": artifact_name,
            "format": inferArtifactFormat(artifact_name),
            "size": artifact.size(),
        };
        info.name = artifact_name;
        if (!info.size) {
            info.size = artifact.size();
        }
        if (!info.format.val()) {
            info.format = inferArtifactFormat(artifact_name);
        }
        backend.registerNamedArtifact(name, version_id, artifact_name, artifact, info);
    }

    #! Optimizes an ONNX model version and stores the optimized artifact metadata
    /** The source version must have an ONNX Runtime execution plan. The optimized
        artifact is stored as a named artifact on the same version with lineage
        metadata linking it to the source entry artifact.

        @param name the model name
        @param version_id the version ID, or NOTHING/"latest"
        @param artifact_name name to use for the optimized artifact
        @param options optional @ref QoreOnnxTools::OnnxOptimizationOptions hash

        @return artifact metadata for the registered optimized artifact

        @throw ML-REGISTRY-ERROR if the source version is not an ONNX Runtime model
            or if the artifact name is invalid

        @since QoreModelRegistry 2.4
    */
    hash<ModelArtifactInfo> registerOptimizedOnnxArtifact(string name, *string version_id,
            string artifact_name, *hash<QoreOnnxTools::OnnxOptimizationOptions> options) {
        if (!artifact_name.val()) {
            artifact_name = "model.optimized.onnx";
        }
        validatePackageArtifactName(artifact_name);

        hash<ModelVersionInfo> meta = getVersionManifest(name, version_id);
        string entry_artifact = requireOnnxEntryArtifact(name, meta);
        FsUtil::TmpDir dir("qore-model-registry-onnx-opt-", "");
        string input_path = materializeOnnxPackage(name, meta.version_id, meta, entry_artifact,
            dir.path);
        string output_path = dir.path + DirSep + artifact_name;

        hash<QoreOnnxTools::OnnxOptimizationOptions> opts =
            options ?? <QoreOnnxTools::OnnxOptimizationOptions>{};
        opts.force = True;
        hash<QoreOnnxTools::OnnxOptimizationResult> result =
            QoreOnnxTools::OnnxModelTools::optimize(input_path, output_path, opts);
        binary data = ReadOnlyFile::readBinaryFile(output_path);
        hash<ModelArtifactInfo> info = <ModelArtifactInfo>{
            "name": artifact_name,
            "role": "optimized_model",
            "format": result.ort_format ? "ort" : "onnx",
            "size": data.size(),
            "safe_to_parse": True,
            "executable": True,
            "metadata": {
                "lineage": {
                    "type": "onnx-optimization",
                    "source_version_id": meta.version_id,
                    "source_artifact": entry_artifact,
                    "ort_format": result.ort_format,
                },
                "optimization": result.optimization,
                "validation": result.validation,
                "deployment": makeOnnxDeploymentMetadata(result.ort_format ? "ORT" : "ONNX",
                    opts.session_config, result.optimization.provider_report, True),
            },
        };
        backend.registerNamedArtifact(name, meta.version_id, artifact_name, data, info);
        return info;
    }

    #! Quantizes an ONNX model version and stores the quantized artifact metadata
    /** The source version must have an ONNX Runtime execution plan. The
        quantized artifact is stored as a named artifact on the same version
        with lineage metadata linking it to the source entry artifact.

        @param name the model name
        @param version_id the version ID, or NOTHING/"latest"
        @param artifact_name name to use for the quantized artifact
        @param options optional @ref QoreOnnxTools::OnnxQuantizationOptions hash

        @return artifact metadata for the registered quantized artifact

        @throw ML-REGISTRY-ERROR if the source version is not an ONNX Runtime model
            or if the artifact name is invalid
        @throw QORE-ONNX-TOOLS-ERROR if quantization dependencies are unavailable
            or ONNX Runtime quantization fails

        @since QoreModelRegistry 2.4
    */
    hash<ModelArtifactInfo> registerQuantizedOnnxArtifact(string name, *string version_id,
            string artifact_name, *hash<QoreOnnxTools::OnnxQuantizationOptions> options) {
        if (!artifact_name.val()) {
            artifact_name = "model.quantized.onnx";
        }
        validatePackageArtifactName(artifact_name);

        hash<ModelVersionInfo> meta = getVersionManifest(name, version_id);
        string entry_artifact = requireOnnxEntryArtifact(name, meta);
        FsUtil::TmpDir dir("qore-model-registry-onnx-quant-", "");
        string input_path = materializeOnnxPackage(name, meta.version_id, meta, entry_artifact,
            dir.path);
        string output_path = dir.path + DirSep + artifact_name;

        hash<QoreOnnxTools::OnnxQuantizationOptions> opts =
            options ?? <QoreOnnxTools::OnnxQuantizationOptions>{};
        opts.force = True;
        hash<QoreOnnxTools::OnnxQuantizationResult> result =
            QoreOnnxTools::OnnxQuantizer::quantize(input_path, output_path, opts);
        *hash<QoreOnnxTools::OnnxCompareResult> comparison;
        if (result.quantized_model) {
            comparison = QoreOnnxTools::OnnxModelTools::compare(input_path, output_path,
                <QoreOnnxTools::OnnxCompareOptions>{
                    "benchmark": True,
                    "warmup_runs": 1,
                    "runs": 3,
                });
        }
        binary data = ReadOnlyFile::readBinaryFile(output_path);
        hash<auto> lineage = {
            "type": "onnx-quantization",
            "source_version_id": meta.version_id,
            "source_artifact": entry_artifact,
            "method": result.method,
        };
        if (result.calibration_fingerprint) {
            lineage.calibration_fingerprint = result.calibration_fingerprint;
        }
        hash<ModelArtifactInfo> info = <ModelArtifactInfo>{
            "name": artifact_name,
            "role": "quantized_model",
            "format": "onnx",
            "size": data.size(),
            "safe_to_parse": True,
            "executable": True,
            "metadata": {
                "lineage": lineage,
                "quantization": result,
                "comparison": comparison,
                "deployment": makeOnnxDeploymentMetadata("ONNX", NOTHING,
                    result.quantized_model ? result.quantized_model.provider_report : NOTHING,
                    False),
            },
        };
        backend.registerNamedArtifact(name, meta.version_id, artifact_name, data, info);
        return info;
    }

    #! Inspects an ONNX model version or named ONNX/ORT artifact
    /** The selected artifact is materialized with any declared external-data
        companions before @ref QoreOnnxTools::OnnxModelTools::inspect() is
        called.  When \a artifact_name is omitted, the version's execution
        entry artifact is inspected.

        @param name the model name
        @param version_id the version ID, or NOTHING/"latest"
        @param artifact_name optional named artifact; defaults to the entry artifact
        @param session_config optional ONNX Runtime session configuration

        @return ONNX inspection metadata

        @since QoreModelRegistry 2.5
    */
    hash<auto> inspectOnnxVersion(string name, *string version_id,
            *string artifact_name, *hash<ML::OnnxSessionConfig> session_config) {
        hash<ModelVersionInfo> meta = getVersionManifest(name, version_id);
        string selected = resolveOnnxArtifactName(name, meta, artifact_name);
        hash<ModelArtifactInfo> artifact = findArtifactInfo(meta, selected);
        FsUtil::TmpDir dir("qore-model-registry-onnx-inspect-", "");
        string path = materializeOnnxPackage(name, meta.version_id, meta, selected, dir.path);
        return QoreOnnxTools::OnnxModelTools::inspect(path,
            buildOnnxActionSessionConfig(meta, artifact, session_config));
    }

    #! Validates an ONNX model version or named ONNX/ORT artifact
    /** @param name the model name
        @param version_id the version ID, or NOTHING/"latest"
        @param artifact_name optional named artifact; defaults to the entry artifact
        @param session_config optional ONNX Runtime session configuration

        @return structured ONNX validation diagnostics

        @since QoreModelRegistry 2.5
    */
    hash<QoreOnnxTools::OnnxValidationResult> validateOnnxVersion(string name,
            *string version_id, *string artifact_name,
            *hash<ML::OnnxSessionConfig> session_config) {
        hash<ModelVersionInfo> meta = getVersionManifest(name, version_id);
        string selected = resolveOnnxArtifactName(name, meta, artifact_name);
        hash<ModelArtifactInfo> artifact = findArtifactInfo(meta, selected);
        FsUtil::TmpDir dir("qore-model-registry-onnx-validate-", "");
        string path = materializeOnnxPackage(name, meta.version_id, meta, selected, dir.path);
        return QoreOnnxTools::OnnxModelTools::validate(path,
            buildOnnxActionSessionConfig(meta, artifact, session_config));
    }

    #! Benchmarks an ONNX model version or named ONNX/ORT artifact
    /** @param name the model name
        @param version_id the version ID, or NOTHING/"latest"
        @param artifact_name optional named artifact; defaults to the entry artifact
        @param options optional benchmark options

        @return benchmark result

        @since QoreModelRegistry 2.5
    */
    hash<QoreOnnxTools::OnnxBenchmarkResult> benchmarkOnnxVersion(string name,
            *string version_id, *string artifact_name,
            *hash<QoreOnnxTools::OnnxBenchmarkOptions> options) {
        hash<ModelVersionInfo> meta = getVersionManifest(name, version_id);
        string selected = resolveOnnxArtifactName(name, meta, artifact_name);
        hash<ModelArtifactInfo> artifact = findArtifactInfo(meta, selected);
        FsUtil::TmpDir dir("qore-model-registry-onnx-bench-", "");
        string path = materializeOnnxPackage(name, meta.version_id, meta, selected, dir.path);
        hash<QoreOnnxTools::OnnxBenchmarkOptions> opts =
            options ?? <QoreOnnxTools::OnnxBenchmarkOptions>{};
        if (!opts.session_config) {
            opts.session_config = buildOnnxActionSessionConfig(meta, artifact, NOTHING);
        }
        return QoreOnnxTools::OnnxModelTools::benchmark(path, opts);
    }

    #! Compares the entry ONNX artifacts of two versions of the same model
    /** @param name the model name
        @param source_version_id source version ID
        @param candidate_version_id candidate version ID
        @param options optional comparison options

        @return ONNX output/latency comparison result

        @since QoreModelRegistry 2.5
    */
    hash<QoreOnnxTools::OnnxCompareResult> compareOnnxVersions(string name,
            string source_version_id, string candidate_version_id,
            *hash<QoreOnnxTools::OnnxCompareOptions> options) {
        hash<ModelVersionInfo> source_meta = getVersionManifest(name, source_version_id);
        hash<ModelVersionInfo> candidate_meta = getVersionManifest(name, candidate_version_id);
        string source_artifact = requireOnnxEntryArtifact(name, source_meta);
        string candidate_artifact = requireOnnxEntryArtifact(name, candidate_meta);
        return compareOnnxMaterializedArtifacts(name, source_meta, source_artifact,
            candidate_meta, candidate_artifact, options);
    }

    #! Compares two ONNX/ORT artifacts attached to one model version
    /** @param name the model name
        @param version_id version ID, or NOTHING/"latest"
        @param source_artifact_name source artifact name
        @param candidate_artifact_name candidate artifact name
        @param options optional comparison options

        @return ONNX output/latency comparison result

        @since QoreModelRegistry 2.5
    */
    hash<QoreOnnxTools::OnnxCompareResult> compareOnnxVersionArtifacts(string name,
            *string version_id, string source_artifact_name, string candidate_artifact_name,
            *hash<QoreOnnxTools::OnnxCompareOptions> options) {
        hash<ModelVersionInfo> meta = getVersionManifest(name, version_id);
        resolveOnnxArtifactName(name, meta, source_artifact_name);
        resolveOnnxArtifactName(name, meta, candidate_artifact_name);
        return compareOnnxMaterializedArtifacts(name, meta, source_artifact_name, meta,
            candidate_artifact_name, options);
    }

    #! Loads a named artifact from a model package
    /** @param name the model name
        @param version_id the version ID, or NOTHING/"latest"
        @param artifact_name the artifact name

        @return artifact binary data

        @since QoreModelRegistry 2.2
    */
    binary loadNamedArtifact(string name, *string version_id, string artifact_name) {
        if (!version_id.val() || version_id == "latest") {
            list<hash<ModelVersionInfo>> versions = backend.listVersions(name);
            if (!versions) {
                throw "ML-REGISTRY-ERROR",
                    sprintf("no versions found for model '%s'", name);
            }
            version_id = versions.last().version_id;
        }
        return backend.loadNamedArtifact(name, version_id, artifact_name);
    }

    #! Loads and parses a named JSON artifact from a model package
    /** @param name the model name
        @param version_id the version ID, or NOTHING/"latest"
        @param artifact_name the JSON artifact name

        @return parsed JSON hash

        @throw ML-REGISTRY-ERROR if the artifact is not a JSON artifact or cannot be parsed

        @since QoreModelRegistry 2.2
    */
    hash<auto> loadJsonArtifact(string name, *string version_id, string artifact_name) {
%ifdef NoJson
        throw "ML-REGISTRY-ERROR",
            "the json module is required to load JSON model artifacts";
%else
        binary data = loadNamedArtifact(name, version_id, artifact_name);
        try {
            return parse_json(data.toString());
        } catch (hash<ExceptionInfo> ex) {
            throw "ML-REGISTRY-ERROR",
                sprintf("could not parse JSON artifact '%s' for model '%s': %s",
                    artifact_name, name, ex.desc);
        }
%endif
    }

    #! Loads the tokenizer.json artifact for a model package
    /** If \a artifact_name is omitted, the tokenizer artifact is selected from
        the version manifest. This lets HuggingFace-style packages be registered
        once and then reused by token counting, embedding, re-ranking, and other
        AI pipeline processors without duplicating tokenizer files.

        @param name the model name
        @param version_id the version ID, or NOTHING/"latest"
        @param artifact_name optional tokenizer artifact name; defaults to the
            manifest tokenizer entry

        @return parsed tokenizer.json hash

        @throw ML-REGISTRY-ERROR if no tokenizer artifact is available

        @since QoreModelRegistry 2.2
    */
    hash<auto> loadTokenizerConfig(string name, *string version_id, *string artifact_name) {
        hash<ModelVersionInfo> manifest = getVersionManifest(name, version_id);
        if (!artifact_name.val()) {
            artifact_name = manifest.tokenizer ? manifest.tokenizer.tokenizer_json : NOTHING;
        }
        if (!artifact_name.val()) {
            foreach hash<ModelArtifactInfo> artifact in (manifest.artifacts) {
                if (artifact.role == "tokenizer") {
                    artifact_name = artifact.name;
                    break;
                }
            }
        }
        if (!artifact_name.val()) {
            throw "ML-REGISTRY-ERROR",
                sprintf("model '%s' version '%s' does not contain a tokenizer.json artifact",
                    name, manifest.version_id);
        }
        return loadJsonArtifact(name, manifest.version_id, artifact_name);
    }

    #! Loads tokenizer.json plus optional tokenizer companion configuration artifacts
    /** The returned hash always contains \c tokenizer_json. When present in the
        package manifest, it also contains \c tokenizer_config,
        \c special_tokens_map, and \c generation_config parsed from the matching
        HuggingFace JSON artifacts.

        @param name the model name
        @param version_id the version ID, or NOTHING/"latest"
        @param artifact_name optional tokenizer.json artifact name

        @return parsed tokenizer package configuration

        @throw ML-REGISTRY-ERROR if no tokenizer artifact is available or a
            companion JSON artifact cannot be parsed

        @since QoreModelRegistry 2.3
    */
    hash<auto> loadTokenizerPackageConfig(string name, *string version_id,
            *string artifact_name) {
        hash<ModelVersionInfo> manifest = getVersionManifest(name, version_id);
        hash<auto> rv = {
            "tokenizer_json": loadTokenizerConfig(name, manifest.version_id, artifact_name),
        };

        if (manifest.tokenizer) {
            if (manifest.tokenizer.tokenizer_config_json.val()) {
                rv{"tokenizer_config"} = loadJsonArtifact(name, manifest.version_id,
                    manifest.tokenizer.tokenizer_config_json);
            }
            if (manifest.tokenizer.special_tokens_map_json.val()) {
                rv{"special_tokens_map"} = loadJsonArtifact(name, manifest.version_id,
                    manifest.tokenizer.special_tokens_map_json);
            }
            if (manifest.tokenizer.generation_config_json.val()) {
                rv{"generation_config"} = loadJsonArtifact(name, manifest.version_id,
                    manifest.tokenizer.generation_config_json);
            }
        }

        return rv;
    }

    #! Lists named artifacts for a model version
    /** @param name the model name
        @param version_id the version ID, or NOTHING/"latest"

        @return list of artifact metadata hashes

        @since QoreModelRegistry 2.2
    */
    list<hash<ModelArtifactInfo>> listArtifacts(string name, *string version_id) {
        if (!version_id.val() || version_id == "latest") {
            list<hash<ModelVersionInfo>> versions = backend.listVersions(name);
            if (!versions) {
                return ();
            }
            version_id = versions.last().version_id;
        }
        return backend.listArtifacts(name, version_id);
    }

    #! Returns the full model package manifest for a version
    /** @param name the model name
        @param version_id the version ID, or NOTHING/"latest"

        @return version manifest metadata

        @since QoreModelRegistry 2.2
    */
    hash<ModelVersionInfo> getVersionManifest(string name, *string version_id) {
        if (!version_id.val() || version_id == "latest") {
            list<hash<ModelVersionInfo>> versions = backend.listVersions(name);
            if (!versions) {
                throw "ML-REGISTRY-ERROR",
                    sprintf("no versions found for model '%s'", name);
            }
            version_id = versions.last().version_id;
        }
        return addDerivedArtifactLists(backend.getVersionManifest(name, version_id));
    }

    #! Tags a specific version of a model
    /** @param name the model name
        @param version_id the version ID to tag
        @param tag_name the tag to apply

        @throw ML-REGISTRY-ERROR if the version is not found
    */
    nothing tag(string name, string version_id, string tag_name) {
        backend.tag(name, version_id, tag_name);
    }

    #! Removes a tag from a specific version
    /** @param name the model name
        @param version_id the version ID
        @param tag_name the tag to remove

        @throw ML-REGISTRY-ERROR if the version is not found
    */
    nothing untag(string name, string version_id, string tag_name) {
        backend.untag(name, version_id, tag_name);
    }

    #! Loads the latest version of a model that has a specific tag
    /** @param name the model name
        @param tag_name the tag to search for

        @return the deserialized ML model object

        @throw ML-REGISTRY-ERROR if no version with the given tag is found
    */
    object loadByTag(string name, string tag_name) {
        list<hash<ModelVersionInfo>> versions = backend.listVersions(name);
        # Find latest version with the tag (search from end)
        for (int i = versions.size() - 1; i >= 0; --i) {
            if (versions[i].tags.contains(tag_name)) {
                return load(name, versions[i].version_id);
            }
        }
        throw "ML-REGISTRY-ERROR",
            sprintf("no version with tag '%s' found for model '%s'", tag_name, name);
    }

    #! Loads an executable model version by tag
    /** This method has the same tag lookup semantics as @ref loadByTag(), but
        uses @ref loadExecutable() so ONNX packages can be executed and
        metadata-only PyTorch/HuggingFace packages produce an informative error.

        @param name the model name
        @param tag_name the tag to search for

        @return the executable model object

        @throw ML-REGISTRY-ERROR if no version with the tag is found or the version
            has no local execution plan

        @since QoreModelRegistry 2.2
    */
    object loadExecutableByTag(string name, string tag_name) {
        list<hash<ModelVersionInfo>> versions = backend.listVersions(name);
        for (int i = versions.size() - 1; i >= 0; --i) {
            if (versions[i].tags.contains(tag_name)) {
                return loadExecutable(name, versions[i].version_id);
            }
        }
        throw "ML-REGISTRY-ERROR",
            sprintf("no version with tag '%s' found for model '%s'", tag_name, name);
    }

    #! Loads an executable model version by tag and wraps it in an adapter
    /** @param name the model name
        @param tag_name the tag to search for

        @return a @ref ModelExecutableAdapter for the latest matching version

        @throw ML-REGISTRY-ERROR if no version with the tag is found or the version
            has no local execution plan

        @since QoreModelRegistry 2.3
    */
    ModelExecutableAdapter loadExecutableAdapterByTag(string name, string tag_name) {
        list<hash<ModelVersionInfo>> versions = backend.listVersions(name);
        for (int i = versions.size() - 1; i >= 0; --i) {
            if (versions[i].tags.contains(tag_name)) {
                hash<ModelVersionInfo> manifest =
                    getVersionManifest(name, versions[i].version_id);
                return new ModelExecutableAdapter(loadExecutable(name, manifest.version_id),
                    manifest);
            }
        }
        throw "ML-REGISTRY-ERROR",
            sprintf("no version with tag '%s' found for model '%s'", tag_name, name);
    }

    #! Prunes old versions of a model
    /** @param name the model name
        @param options a hash with the following optional keys:
        - \c keep_tagged (default True): preserve tagged versions
        - \c keep_latest (default 5): number of most recent versions to keep

        @return the number of versions deleted
    */
    int prune(string name, *hash<PruneOptions> options) {
        return backend.prune(name, options);
    }

    #! Lists all model names in the registry
    /** @return a sorted list of model name strings
    */
    list<string> listModels() {
        return backend.listModels();
    }

    #! Lists all tenant IDs in the registry
    /** @return a sorted list of tenant ID strings

        @note Only meaningful for admin-level registries (no tenant_id)
    */
    list<string> listTenants() {
        return backend.listTenants();
    }

    #! Returns the tenant ID for this registry instance, or NOTHING if single-tenant
    /** @return the tenant ID string, or NOTHING
    */
    *string getTenantId() {
        return backend.getTenantId();
    }

    #! Deletes a model and all its versions from the registry
    /** @param name the model name

        @throw ML-REGISTRY-ERROR if the model is not found
    */
    nothing deleteModel(string name) {
        backend.deleteModel(name);
    }

    #! Returns version metadata for a specific version
    /** @param name the model name
        @param version_id the version ID

        @return the metadata hash for the version

        @throw ML-REGISTRY-ERROR if the version is not found
    */
    hash<ModelVersionInfo> getVersionMetadata(string name, string version_id) {
        return backend.getVersionMetadata(name, version_id);
    }

    #! Deletes all model storage for a specific tenant
    /** Used for cleanup when a user/tenant is deleted.

        @param tid the tenant ID whose storage should be deleted

        @throw ML-REGISTRY-ERROR if the tenant is not found or deletion fails

        @since QoreModelRegistry 2.0
    */
    nothing deleteTenantStorage(string tid) {
        backend.deleteTenantStorage(tid);
    }

    #! Private: determines the algorithm name from a model object
    private string getAlgorithmName(object model) {
        string class_name = model.className();
        # Strip namespace prefix if present
        if (class_name =~ /::/) {
            class_name = (class_name =~ x/([^:]+)$/)[0];
        }
        return class_name;
    }

    #! Private: creates a sortable unique version ID
    private static string makeVersionId() {
        return format_date("YYYYMMDDHHmmss", now()) + "_"
            + get_random_bytes(4).toHex();
    }

    #! Private: adds derived optimized/quantized artifact lists to a manifest
    private static hash<ModelVersionInfo> addDerivedArtifactLists(hash<ModelVersionInfo> meta) {
        list<hash<ModelArtifactInfo>> optimized();
        list<hash<ModelArtifactInfo>> quantized();
        foreach hash<ModelArtifactInfo> artifact in (meta.artifacts) {
            if (artifact.role == "optimized_model") {
                push optimized, artifact;
            } else if (artifact.role == "quantized_model") {
                push quantized, artifact;
            }
        }
        meta.optimized_artifacts = optimized;
        meta.quantized_artifacts = quantized;
        return meta;
    }

    #! Private: finds a named artifact in a manifest
    private static hash<ModelArtifactInfo> findArtifactInfo(hash<ModelVersionInfo> meta,
            string artifact_name) {
        validatePackageArtifactName(artifact_name);
        foreach hash<ModelArtifactInfo> artifact in (meta.artifacts) {
            if (artifact.name == artifact_name) {
                return artifact;
            }
        }
        throw "ML-REGISTRY-ERROR",
            sprintf("artifact '%s' not found for model '%s' version '%s'",
                artifact_name, meta.name, meta.version_id);
    }

    #! Private: builds deployment metadata for ONNX/ORT artifacts
    private static hash<auto> makeOnnxDeploymentMetadata(string model_format,
            *hash<ML::OnnxSessionConfig> session_config, *hash<auto> provider_report,
            bool hardware_specific) {
        hash<auto> rv = {
            "model_format": model_format,
            "hardware_specific": hardware_specific,
        };
        if (session_config) {
            rv.session_config = session_config;
        }
        if (provider_report) {
            rv.provider_report = provider_report;
            if (provider_report.active_provider.val()) {
                rv.active_provider = provider_report.active_provider;
            }
            if (exists provider_report.requested_providers) {
                rv.requested_providers = provider_report.requested_providers;
            }
            if (exists provider_report.available_providers) {
                rv.available_providers = provider_report.available_providers;
            }
        }
        return rv;
    }

    #! Private: resolves and validates an ONNX or ORT artifact name
    private static string resolveOnnxArtifactName(string name, hash<ModelVersionInfo> meta,
            *string artifact_name) {
        string selected = artifact_name.val()
            ? artifact_name
            : requireOnnxEntryArtifact(name, meta);
        hash<ModelArtifactInfo> artifact = findArtifactInfo(meta, selected);
        if (artifact.format != "onnx" && artifact.format != "ort") {
            throw "ML-REGISTRY-ERROR",
                sprintf("artifact '%s' for model '%s' version '%s' is not an ONNX/ORT "
                    "artifact (role=%y, format=%y)", selected, name, meta.version_id,
                    artifact.role, artifact.format);
        }
        return selected;
    }

    #! Private: builds an ONNX session config for registry action methods
    private static hash<ML::OnnxSessionConfig> buildOnnxActionSessionConfig(
            hash<ModelVersionInfo> meta, hash<ModelArtifactInfo> artifact,
            *hash<ML::OnnxSessionConfig> session_config) {
        hash<ML::OnnxSessionConfig> config;
        if (session_config) {
            config = session_config;
        } else if (artifact.metadata && artifact.metadata.deployment
                && artifact.metadata.deployment.session_config) {
            config = cast<hash<ML::OnnxSessionConfig>>(
                artifact.metadata.deployment.session_config);
        } else if (meta.execution && meta.execution.config) {
            config = cast<hash<ML::OnnxSessionConfig>>(meta.execution.config);
        } else {
            config = <ML::OnnxSessionConfig>{"providers": ()};
        }
        if (artifact.format == "ort") {
            config.load_model_format = "ORT";
        }
        # propagate the manifest's device-binding policy into the session unless the
        # caller-supplied session config already carries one
        if (!exists config.device_binding && meta.execution && meta.execution.device_binding) {
            config.device_binding =
                cast<hash<ML::OnnxDeviceBindingConfig>>(meta.execution.device_binding);
        }
        return config;
    }

    #! Private: compares two already-resolved registry ONNX artifacts
    private hash<QoreOnnxTools::OnnxCompareResult> compareOnnxMaterializedArtifacts(
            string name, hash<ModelVersionInfo> source_meta, string source_artifact,
            hash<ModelVersionInfo> candidate_meta, string candidate_artifact,
            *hash<QoreOnnxTools::OnnxCompareOptions> options) {
        hash<ModelArtifactInfo> source_info = findArtifactInfo(source_meta, source_artifact);
        hash<ModelArtifactInfo> candidate_info =
            findArtifactInfo(candidate_meta, candidate_artifact);
        FsUtil::TmpDir dir("qore-model-registry-onnx-compare-", "");
        string source_dir = dir.path + DirSep + "source";
        string candidate_dir = dir.path + DirSep + "candidate";
        mkdir_ex(source_dir, 0700, True);
        mkdir_ex(candidate_dir, 0700, True);
        string source_path = materializeOnnxPackage(name, source_meta.version_id,
            source_meta, source_artifact, source_dir);
        string candidate_path = materializeOnnxPackage(name, candidate_meta.version_id,
            candidate_meta, candidate_artifact, candidate_dir);
        hash<QoreOnnxTools::OnnxCompareOptions> opts =
            options ?? <QoreOnnxTools::OnnxCompareOptions>{};
        if (!opts.source_session_config) {
            opts.source_session_config =
                buildOnnxActionSessionConfig(source_meta, source_info, NOTHING);
        }
        if (!opts.candidate_session_config) {
            opts.candidate_session_config =
                buildOnnxActionSessionConfig(candidate_meta, candidate_info, NOTHING);
        }
        return QoreOnnxTools::OnnxModelTools::compare(source_path, candidate_path, opts);
    }

    #! Private: builds the ONNX session config for a named deployment artifact
    private static hash<ML::OnnxSessionConfig> buildOnnxArtifactSessionConfig(
            hash<ModelArtifactInfo> artifact, *hash<ML::OnnxSessionConfig> session_config) {
        hash<ML::OnnxSessionConfig> config = <ML::OnnxSessionConfig>{"providers": ()};
        if (session_config) {
            config = session_config;
        } else if (artifact.metadata && artifact.metadata.deployment
                && artifact.metadata.deployment.session_config) {
            config = cast<hash<ML::OnnxSessionConfig>>(
                artifact.metadata.deployment.session_config);
        }
        if (artifact.format == "ort") {
            config.load_model_format = "ORT";
        }
        return config;
    }

    #! Private: rejects provider requests incompatible with a deployment artifact
    private static nothing validateOnnxArtifactProviderRequest(string name, string version_id,
            hash<ModelArtifactInfo> artifact, hash<ML::OnnxSessionConfig> config) {
        if (!artifact.metadata || !artifact.metadata.deployment
                || !artifact.metadata.deployment.hardware_specific) {
            return;
        }
        *string expected = artifact.metadata.deployment.active_provider;
        if (!expected.val()) {
            return;
        }
        *string requested = getExplicitPrimaryProvider(config);
        if (requested.val() && requested != expected) {
            throw "ML-REGISTRY-ERROR",
                sprintf("artifact '%s' for model '%s' version '%s' was optimized for "
                    "ONNX Runtime provider %s, but the requested session uses %s",
                    artifact.name, name, version_id, expected, requested);
        }
    }

    #! Private: verifies the effective provider after loading an optimized artifact
    private static nothing validateOnnxArtifactEffectiveProvider(string name, string version_id,
            hash<ModelArtifactInfo> artifact, hash<auto> provider_report) {
        if (!artifact.metadata || !artifact.metadata.deployment
                || !artifact.metadata.deployment.hardware_specific) {
            return;
        }
        *string expected = artifact.metadata.deployment.active_provider;
        *string actual = provider_report.active_provider;
        if (expected.val() && actual.val() && actual != expected) {
            throw "ML-REGISTRY-ERROR",
                sprintf("artifact '%s' for model '%s' version '%s' was optimized for "
                    "ONNX Runtime provider %s, but the loaded session uses %s",
                    artifact.name, name, version_id, expected, actual);
        }
    }

    #! Private: returns the explicitly requested primary ONNX provider, if any
    private static *string getExplicitPrimaryProvider(hash<ML::OnnxSessionConfig> config) {
        if (!exists config.providers) {
            return NOTHING;
        }
        if (!config.providers) {
            return "CPUExecutionProvider";
        }
        return normalizeOnnxProviderName(config.providers[0].name);
    }

    #! Private: normalizes common ONNX Runtime provider aliases
    private static string normalizeOnnxProviderName(string name) {
        string trimmed = trim(name);
        switch (trimmed) {
            case "CPU":
            case "CPUExecutionProvider":
                return "CPUExecutionProvider";
            case "CUDA":
            case "CUDAExecutionProvider":
                return "CUDAExecutionProvider";
            case "TensorRT":
            case "Tensorrt":
            case "TensorrtExecutionProvider":
                return "TensorrtExecutionProvider";
            case "OpenVINO":
            case "OpenVINOExecutionProvider":
                return "OpenVINOExecutionProvider";
            case "DirectML":
            case "DmlExecutionProvider":
                return "DmlExecutionProvider";
            case "CoreML":
            case "CoreMLExecutionProvider":
                return "CoreMLExecutionProvider";
            default:
                return trimmed;
        }
    }

    #! Private: infers an artifact format from a filename
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

    #! Loads an ONNX executable, materializing external data files when needed
    private object loadOnnxExecutable(string name, string version_id,
            hash<ModelVersionInfo> meta, string entry_artifact) {
        validatePackageArtifactName(entry_artifact);
        *hash<ML::OnnxSessionConfig> config = meta.execution && meta.execution.config
            ? cast<hash<ML::OnnxSessionConfig>>(meta.execution.config)
            : NOTHING;
        list<string> external_files = getOnnxExternalDataArtifactNames(meta, entry_artifact);

        if (!external_files) {
            binary artifact = backend.loadNamedArtifact(name, version_id, entry_artifact);
            if (config) {
                return new ML::OnnxModel(artifact, config);
            }
            return new ML::OnnxModel(artifact);
        }

        string tmp_dir = tmp_location() + DirSep + "qore_onnx_registry_"
            + get_random_bytes(8).toHex();
        mkdir_ex(tmp_dir, 0700, True);
        on_exit { cleanupTempDir(tmp_dir); }

        string model_path = tmp_dir + DirSep + entry_artifact;
        writeBinaryFile(model_path, backend.loadNamedArtifact(name, version_id, entry_artifact));
        foreach string external_file in (external_files) {
            writeBinaryFile(tmp_dir + DirSep + external_file,
                backend.loadNamedArtifact(name, version_id, external_file));
        }

        if (config) {
            return new ML::OnnxModel(model_path, config);
        }
        return new ML::OnnxModel(model_path);
    }

    #! Requires an ONNX Runtime version and returns the entry artifact name
    private static string requireOnnxEntryArtifact(string name, hash<ModelVersionInfo> meta) {
        string runtime = meta.runtime ?? (meta.execution ? meta.execution.runtime : NOTHING)
            ?? "metadata-only";
        if (runtime != "onnxruntime" && meta.format != "onnx") {
            throw "ML-REGISTRY-ERROR",
                sprintf("model '%s' version '%s' is not an ONNX Runtime model "
                    "(format=%y, runtime=%y)", name, meta.version_id, meta.format, runtime);
        }
        string entry_artifact = meta.execution ? (meta.execution.entry_artifact ?? "model")
            : "model";
        validatePackageArtifactName(entry_artifact);
        return entry_artifact;
    }

    #! Materializes an ONNX package entry artifact and external data files
    private string materializeOnnxPackage(string name, string version_id,
            hash<ModelVersionInfo> meta, string entry_artifact, string target_dir) {
        string model_path = target_dir + DirSep + entry_artifact;
        writeBinaryFile(model_path, backend.loadNamedArtifact(name, version_id, entry_artifact));
        foreach string external_file in (getOnnxExternalDataArtifactNames(meta, entry_artifact)) {
            writeBinaryFile(target_dir + DirSep + external_file,
                backend.loadNamedArtifact(name, version_id, external_file));
        }
        return model_path;
    }

    #! Returns external-data artifact names referenced by ONNX package metadata
    private static list<string> getOnnxExternalDataArtifactNames(
            hash<ModelVersionInfo> meta, string entry_artifact) {
        list<string> rv();
        foreach hash<ModelArtifactInfo> artifact in (meta.artifacts) {
            if (artifact.role == "external_data" && !rv.contains(artifact.name)) {
                validatePackageArtifactName(artifact.name);
                push rv, artifact.name;
            }
            if (artifact.name == entry_artifact && artifact.metadata
                    && artifact.metadata.external_data_files) {
                foreach string external_name in (artifact.metadata.external_data_files) {
                    if (!rv.contains(external_name)) {
                        validatePackageArtifactName(external_name);
                        push rv, external_name;
                    }
                }
            }
        }
        return rv;
    }

    #! Validates an artifact name before materializing it in a temporary package directory
    private static nothing validatePackageArtifactName(string artifact_name) {
        if (!artifact_name.val() || artifact_name == "." || artifact_name == ".."
                || artifact_name =~ /[\\\/]/) {
            throw "ML-REGISTRY-ERROR",
                sprintf("invalid package artifact name '%s'", artifact_name);
        }
    }

    #! Writes a binary file
    private static nothing writeBinaryFile(string path, binary data) {
        File f();
        f.open(path, O_CREAT | O_WRONLY | O_TRUNC, 0600);
        f.write(data);
    }

    #! Removes a temporary directory created for ONNX external-data loading
    private static nothing cleanupTempDir(string path) {
        if (!is_dir(path)) {
            return;
        }
        Dir d();
        if (d.chdir(path)) {
            foreach string file in (d.listFiles()) {
                unlink(path + DirSep + file);
            }
        }
        rmdir(path);
    }
}

} # namespace QoreModelRegistry
