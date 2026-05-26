# -*- mode: qore; indent-tabs-mode: nil -*-

/*  AbstractModelRegistryBackend.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Hash describing how a model artifact is stored
/** The artifact reference supports extensibility for future storage backends
    (e.g., object store) by using a type discriminator.

    @since QoreModelRegistry 2.0
*/
public hashdecl ArtifactRef {
    #! Storage type: "inline" for blob storage, "objectstore" for future object store support
    string type = "inline";
    #! Size of the artifact in bytes
    int size;
    #! SHA-256 checksum of the artifact for integrity verification
    string checksum;
    #! Object store bucket (only for type "objectstore")
    *string bucket;
    #! Object store key (only for type "objectstore")
    *string key;
}

#! Hash describing a named artifact in a model package
/** A model version can contain multiple named artifacts: the primary model,
    tokenizer files, framework configuration, weights, and execution metadata.

    @since QoreModelRegistry 2.2
*/
public hashdecl ModelArtifactInfo {
    #! Stable artifact name within the version, for example "model" or "tokenizer.json"
    string name;
    #! Artifact role: "model", "tokenizer", "config", "weights", "manifest", etc.
    string role = "model";
    #! Artifact format: "qml", "onnx", "json", "safetensors", "pytorch-pickle", etc.
    string format;
    #! Artifact size in bytes
    int size;
    #! Optional SHA-256 checksum
    *string checksum;
    #! Whether the artifact can be inspected without executing code
    bool safe_to_parse = False;
    #! Whether this artifact can be executed locally by Qore
    bool executable = False;
    #! Optional storage reference for remote/object-store backends
    *hash<ArtifactRef> artifact_ref;
    #! Optional backend-specific path or object key hint
    *string path_hint;
    #! Optional free-form metadata extracted from or associated with the artifact
    *hash<auto> metadata;
}

#! Hash describing a tokenizer companion package
/** @since QoreModelRegistry 2.2
*/
public hashdecl TokenizerArtifactInfo {
    #! Name of the tokenizer.json artifact, if present
    *string tokenizer_json;
    #! Name of the tokenizer_config.json artifact, if present
    *string tokenizer_config_json;
    #! Name of the special_tokens_map.json artifact, if present
    *string special_tokens_map_json;
    #! Name of the generation_config.json artifact, if present
    *string generation_config_json;
    #! Maximum model/tokenizer sequence length if known
    *int model_max_length;
    #! Whether a chat template is available
    bool has_chat_template = False;
    #! Optional special-token IDs and values
    *hash<auto> special_tokens;
}

#! Hash describing how a model version can be executed
/** @since QoreModelRegistry 2.2
*/
public hashdecl ModelExecutionInfo {
    #! Runtime identifier: "qore-ml", "onnxruntime", "external-http", "metadata-only"
    string runtime = "metadata-only";
    #! Task identifier, for example "raw", "embedding", "reranker", or "sequence-classification"
    *string task;
    #! Whether Qore can execute this version in-process
    bool local_executable = False;
    #! Name of the primary artifact used by this runtime
    *string entry_artifact;
    #! Structured execution plan metadata used by task-aware processors
    *hash<auto> plan;
    #! Optional preprocessing contract metadata
    *hash<auto> preprocess;
    #! Optional postprocessing contract metadata
    *hash<auto> postprocess;
    #! Optional list of required Qore modules or external capabilities
    list<string> requires = ();
    #! Optional input schema/tensor metadata
    *hash<auto> input_schema;
    #! Optional output schema/tensor metadata
    *hash<auto> output_schema;
    #! Optional runtime configuration
    *hash<auto> config;
    #! Compact actionable execution diagnostics summary
    *hash<auto> diagnostics;
}

#! Hash describing a model version's metadata
/** @since QoreModelRegistry 2.0
*/
public hashdecl ModelVersionInfo {
    #! Unique version identifier
    string version_id;
    #! Model name
    string name;
    #! Algorithm class name (e.g., "KMeans", "LinearRegression")
    string algorithm;
    #! ISO-8601 timestamp of registration
    string created;
    #! Optional description
    *string description;
    #! Optional performance metrics
    *hash<auto> metrics;
    #! Tags applied to this version
    list<string> tags = ();
    #! Size of the serialized model in bytes
    int file_size;
    #! Serialization format: "qml", "onnx", "json"
    string format;
    #! Optional hash identifying training data
    *string training_data_hash;
    #! Tenant identifier (present only in multi-tenant registries)
    *string tenant_id;
    #! Artifact storage reference
    *hash<ArtifactRef> artifact_ref;
    #! Optional version-level configuration (e.g., tokenizer config for ONNX models)
    /** @since QoreModelRegistry 2.1
    */
    *hash<auto> config;
    #! Source framework name, for example "qore", "onnx", "pytorch", or "huggingface"
    /** @since QoreModelRegistry 2.2
    */
    *string framework;
    #! Version artifact kind: "executable", "source", "weights-only", "tokenizer-only", "metadata-only", or "external-runtime"
    /** @since QoreModelRegistry 2.2
    */
    *string artifact_kind;
    #! Preferred runtime: "qore-ml", "onnxruntime", "external-http", or "metadata-only"
    /** @since QoreModelRegistry 2.2
    */
    *string runtime;
    #! Model task identifier, for example "raw", "embedding", "reranker", or "sequence-classification"
    /** @since QoreModelRegistry 2.4
    */
    *string task;
    #! ONNX opset version, when known
    /** @since QoreModelRegistry 2.5
    */
    *int opset;
    #! Producer framework/tool metadata, when known
    /** @since QoreModelRegistry 2.5
    */
    *string producer;
    #! Minimum ONNX Runtime version required by deployment policy, when declared
    /** @since QoreModelRegistry 2.5
    */
    *string minimum_onnxruntime_version;
    #! ONNX input tensor contract, when known
    /** @since QoreModelRegistry 2.5
    */
    *list<hash<auto>> inputs;
    #! ONNX output tensor contract, when known
    /** @since QoreModelRegistry 2.5
    */
    *list<hash<auto>> outputs;
    #! Dynamic-axis metadata keyed by tensor name, when known
    /** @since QoreModelRegistry 2.5
    */
    *hash<auto> dynamic_axes;
    #! ONNX external-data artifact names required by the entry artifact
    /** @since QoreModelRegistry 2.5
    */
    *list<string> external_data;
    #! Version-level preprocessing contract metadata
    /** @since QoreModelRegistry 2.5
    */
    *hash<auto> preprocess;
    #! Version-level postprocessing contract metadata
    /** @since QoreModelRegistry 2.5
    */
    *hash<auto> postprocess;
    #! ONNX Runtime provider deployment diagnostics and policy metadata
    /** @since QoreModelRegistry 2.5
    */
    *hash<auto> providers;
    #! ONNX validation diagnostics recorded at package registration time
    /** @since QoreModelRegistry 2.5
    */
    *hash<auto> validation;
    #! Compact actionable ONNX/package diagnostics summary
    /** @since QoreModelRegistry 2.5
    */
    *hash<auto> diagnostics;
    #! Benchmark metadata supplied by deployment tooling or registry callers
    /** @since QoreModelRegistry 2.5
    */
    *hash<auto> benchmarks;
    #! Golden input/output vectors supplied for deployment validation
    /** @since QoreModelRegistry 2.5
    */
    *list<hash<auto>> golden_vectors;
    #! True if this version can be executed locally by Qore
    /** @since QoreModelRegistry 2.2
    */
    bool executable = True;
    #! Safety classification: "safe", "safe-metadata", "trusted-only", "unsafe-pickle", or "unknown"
    /** @since QoreModelRegistry 2.2
    */
    string safety = "safe";
    #! Named artifacts in this model package
    /** @since QoreModelRegistry 2.2
    */
    list<hash<ModelArtifactInfo>> artifacts = ();
    #! Execution plan metadata
    /** @since QoreModelRegistry 2.2
    */
    *hash<ModelExecutionInfo> execution;
    #! Tokenizer companion metadata
    /** @since QoreModelRegistry 2.2
    */
    *hash<TokenizerArtifactInfo> tokenizer;
    #! Optimized ONNX/ORT deployment artifacts derived from named artifacts
    /** @since QoreModelRegistry 2.4
    */
    list<hash<ModelArtifactInfo>> optimized_artifacts = ();
    #! Quantized ONNX deployment artifacts derived from named artifacts
    /** @since QoreModelRegistry 2.4
    */
    list<hash<ModelArtifactInfo>> quantized_artifacts = ();
}

#! Options for pruning model versions
/** @since QoreModelRegistry 2.0
*/
public hashdecl PruneOptions {
    #! If True, preserve versions that have tags even if they are older (default True)
    bool keep_tagged = True;
    #! Number of most recent versions to always keep (default 5)
    int keep_latest = 5;
}

#! Abstract backend interface for model registry storage
/** Concrete implementations provide storage for model artifacts and metadata.
    All operations are scoped by tenant_id when provided.

    Available implementations:
    - @ref FilesystemBackend — stores models on the local filesystem
    - @ref DatabaseBackend — stores models in a database via SqlUtil
    - @ref RestBackend — stores models via a REST API

    @since QoreModelRegistry 2.0
*/
public class AbstractModelRegistryBackend {
    private {
        #! Optional tenant ID for multi-tenant deployments
        *string tenant_id;
    }

    #! Creates a backend with optional tenant scoping
    /** @param tenant_id optional tenant identifier; when provided, all operations
        are scoped to this tenant
    */
    constructor(*string tenant_id) {
        if (tenant_id.val()) {
            # Validate tenant_id: only allow safe characters
            if (tenant_id !~ /^[a-zA-Z0-9_-]+$/) {
                throw "ML-REGISTRY-ERROR",
                    sprintf("invalid tenant_id '%s': must contain only alphanumeric "
                        "characters, hyphens, and underscores", tenant_id);
            }
            self.tenant_id = tenant_id;
        }
    }

    #! Registers a new model version with its serialized artifact
    /** @param name the model name
        @param artifact the serialized model binary data
        @param metadata a hash with the following keys:
        - \c version_id (required): unique version identifier
        - \c algorithm: algorithm class name
        - \c description: optional description
        - \c metrics: optional hash of performance metrics
        - \c tags: optional list of string tags
        - \c training_data_hash: optional hash identifying training data
        - \c format: serialization format ("qml", "onnx", "json")
        - \c file_size: size of the artifact in bytes

        @return the version ID string

        @throw ML-REGISTRY-ERROR on failure
    */
    abstract string registerVersion(string name, binary artifact,
        hash<ModelVersionInfo> metadata);

    #! Loads a model artifact by name and version
    /** @param name the model name
        @param version_id the version to load

        @return the serialized model binary data

        @throw ML-REGISTRY-ERROR if the model or version is not found
    */
    abstract binary loadArtifact(string name, string version_id);

    #! Registers a named artifact for an existing model version
    /** Default implementation rejects the operation. Backends with package
        support override this method.

        @param name the model name
        @param version_id the version ID
        @param artifact_name the stable artifact name
        @param artifact the artifact binary payload
        @param metadata artifact metadata

        @throw ML-REGISTRY-ERROR if named artifacts are not supported by this backend

        @since QoreModelRegistry 2.2
    */
    nothing registerNamedArtifact(string name, string version_id, string artifact_name,
            binary artifact, hash<ModelArtifactInfo> metadata) {
        throw "ML-REGISTRY-ERROR",
            sprintf("backend %y does not support named model artifacts",
                self.className());
    }

    #! Loads a named artifact by model/version/artifact name
    /** The default implementation maps artifact name "model" to the legacy
        single-artifact loader and rejects all other names.

        @param name the model name
        @param version_id the version ID
        @param artifact_name the artifact name

        @return the artifact binary payload

        @since QoreModelRegistry 2.2
    */
    binary loadNamedArtifact(string name, string version_id, string artifact_name) {
        if (artifact_name == "model") {
            return loadArtifact(name, version_id);
        }
        throw "ML-REGISTRY-ERROR",
            sprintf("artifact '%s' not found for model '%s' version '%s'",
                artifact_name, name, version_id);
    }

    #! Lists named artifacts for a model version
    /** The default implementation synthesizes the legacy single artifact from
        version metadata.

        @param name the model name
        @param version_id the version ID

        @return list of artifact metadata hashes

        @since QoreModelRegistry 2.2
    */
    list<hash<ModelArtifactInfo>> listArtifacts(string name, string version_id) {
        hash<ModelVersionInfo> meta = getVersionMetadata(name, version_id);
        if (meta.artifacts) {
            return meta.artifacts;
        }
        return (<ModelArtifactInfo>{
            "name": "model",
            "role": "model",
            "format": meta.format,
            "size": meta.file_size,
            "safe_to_parse": meta.format == "qml" || meta.format == "onnx",
            "executable": meta.format == "qml" || meta.format == "onnx",
        },);
    }

    #! Returns package manifest metadata for a model version
    /** @param name the model name
        @param version_id the version ID

        @return version metadata plus named artifact and execution metadata

        @since QoreModelRegistry 2.2
    */
    hash<ModelVersionInfo> getVersionManifest(string name, string version_id) {
        return getVersionMetadata(name, version_id);
    }

    #! Normalizes model version metadata after JSON or REST decoding
    /** JSON and REST transports preserve hash contents but not Qore hashdecl
        types. This helper restores nested typed metadata before backend
        implementations return @ref ModelVersionInfo values.

        @param meta decoded version metadata

        @return typed version metadata

        @since QoreModelRegistry 2.2
    */
    static hash<ModelVersionInfo> normalizeVersionInfo(hash<auto> meta) {
        if (meta.artifact_ref) {
            meta.artifact_ref = cast<hash<ArtifactRef>>(meta.artifact_ref);
        }
        if (meta.artifacts) {
            meta.artifacts = AbstractModelRegistryBackend::normalizeArtifactInfos(meta.artifacts);
        }
        if (meta.optimized_artifacts) {
            meta.optimized_artifacts = AbstractModelRegistryBackend::normalizeArtifactInfos(
                meta.optimized_artifacts);
        }
        if (meta.quantized_artifacts) {
            meta.quantized_artifacts = AbstractModelRegistryBackend::normalizeArtifactInfos(
                meta.quantized_artifacts);
        }
        if (meta.execution) {
            meta.execution = cast<hash<ModelExecutionInfo>>(meta.execution);
        }
        if (meta.tokenizer) {
            meta.tokenizer = cast<hash<TokenizerArtifactInfo>>(meta.tokenizer);
        }
        return cast<hash<ModelVersionInfo>>(meta);
    }

    #! Normalizes artifact metadata after JSON or REST decoding
    /** @param artifact decoded artifact metadata

        @return typed artifact metadata

        @since QoreModelRegistry 2.2
    */
    static hash<ModelArtifactInfo> normalizeArtifactInfo(hash<auto> artifact) {
        if (artifact.artifact_ref) {
            artifact.artifact_ref = cast<hash<ArtifactRef>>(artifact.artifact_ref);
        }
        return cast<hash<ModelArtifactInfo>>(artifact);
    }

    #! Normalizes a list of artifact metadata hashes
    /** @param artifacts decoded artifact metadata list

        @return typed artifact metadata list

        @since QoreModelRegistry 2.2
    */
    static list<hash<ModelArtifactInfo>> normalizeArtifactInfos(auto artifacts) {
        list<hash<ModelArtifactInfo>> result();
        foreach auto artifact in (artifacts) {
            push result, AbstractModelRegistryBackend::normalizeArtifactInfo(
                cast<hash<auto>>(artifact));
        }
        return result;
    }

    #! Lists all versions of a named model
    /** @param name the model name

        @return a list of version metadata hashes, ordered oldest to newest
    */
    abstract list<hash<ModelVersionInfo>> listVersions(string name);

    #! Lists all model names in the registry
    /** @return a sorted list of model name strings
    */
    abstract list<string> listModels();

    #! Tags a specific version of a model
    /** @param name the model name
        @param version_id the version ID to tag
        @param tag_name the tag to apply

        @throw ML-REGISTRY-ERROR if the version is not found
    */
    abstract nothing tag(string name, string version_id, string tag_name);

    #! Removes a tag from a specific version
    /** @param name the model name
        @param version_id the version ID
        @param tag_name the tag to remove

        @throw ML-REGISTRY-ERROR if the version is not found
    */
    abstract nothing untag(string name, string version_id, string tag_name);

    #! Prunes old versions of a model
    /** @param name the model name
        @param options prune options controlling retention policy

        @return the number of versions deleted
    */
    abstract int prune(string name, *hash<PruneOptions> options);

    #! Deletes a model and all its versions from the registry
    /** @param name the model name

        @throw ML-REGISTRY-ERROR if the model is not found
    */
    abstract nothing deleteModel(string name);

    #! Returns version metadata for a specific version
    /** @param name the model name
        @param version_id the version ID

        @return the metadata hash for the version

        @throw ML-REGISTRY-ERROR if the version is not found
    */
    abstract hash<ModelVersionInfo> getVersionMetadata(string name, string version_id);

    #! Returns the tenant ID for this backend, or NOTHING if single-tenant
    /** @return the tenant ID string, or NOTHING
    */
    *string getTenantId() {
        return tenant_id;
    }

    #! Lists all tenant IDs in the registry
    /** @return a sorted list of tenant ID strings

        @note This is an administrative method — it should only be called by
        a registry instance created without a tenant_id.
    */
    abstract list<string> listTenants();

    #! Deletes all model storage for a specific tenant
    /** Used for cleanup when a user/tenant is deleted.

        @param tid the tenant ID whose storage should be deleted

        @throw ML-REGISTRY-ERROR if the tenant is not found or deletion fails
    */
    abstract nothing deleteTenantStorage(string tid);

    #! Validates a model name for safety
    /** Model names must contain only alphanumeric characters, hyphens,
        underscores, and dots. Path separators and traversal patterns are
        rejected to prevent path traversal attacks.

        @param name the model name to validate

        @throw ML-REGISTRY-ERROR if the name is invalid
    */
    static nothing validateModelName(string name) {
        if (!name.val()) {
            throw "ML-REGISTRY-ERROR", "model name must not be empty";
        }
        if (name !~ /^[a-zA-Z0-9_.\-]+$/) {
            throw "ML-REGISTRY-ERROR",
                sprintf("invalid model name '%s': must contain only alphanumeric "
                    "characters, hyphens, underscores, and dots", name);
        }
        if (name == "." || name == "..") {
            throw "ML-REGISTRY-ERROR",
                sprintf("invalid model name '%s': '.' and '..' are not allowed", name);
        }
    }
}

} # namespace QoreModelRegistry
