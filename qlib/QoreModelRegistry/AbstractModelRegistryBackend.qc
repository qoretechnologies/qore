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
    - @ref DatabaseBackend — stores models in a database via SqlUtil (future)
    - @ref RestBackend — stores models via a REST API (future)

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
