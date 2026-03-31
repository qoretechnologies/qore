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
    - \c path → @ref FilesystemBackend
    - \c backend → use a pre-constructed @ref AbstractModelRegistryBackend

    @par Example (filesystem):
    @code{.py}
    auto reg = new ModelRegistry({"path": "/var/models"});
    string vid = reg.register(model, {"name": "fraud-detector"});
    auto loaded = reg.load("fraud-detector");
    @endcode

    @par Example (custom backend):
    @code{.py}
    auto backend = new FilesystemBackend("/var/models", "acme-corp");
    auto reg = new ModelRegistry({"backend": backend});
    @endcode

    @since QoreModelRegistry 1.0 (filesystem-only)
    @since QoreModelRegistry 2.0 (pluggable backends)
*/
public class ModelRegistry {
    private {
        #! Storage backend
        AbstractModelRegistryBackend backend;
    }

    #! Creates a new model registry
    /** @param options a hash with the following keys:
        - \c path (string): base directory path for filesystem storage
        - \c tenant_id (string, optional): tenant identifier for multi-tenant deployments
        - \c backend (@ref AbstractModelRegistryBackend, optional): pre-constructed backend;
          when provided, \c path and \c tenant_id are ignored

        @throw ML-REGISTRY-ERROR if required options are missing
    */
    constructor(hash<auto> options) {
%ifdef NoJson
        throw "ML-REGISTRY-ERROR", "the QoreModelRegistry module requires the json "
            "module (module-json) to be installed; install it and try again";
%endif
        if (options.backend instanceof AbstractModelRegistryBackend) {
            backend = options.backend;
        } else if (options.path.val()) {
            backend = new FilesystemBackend(options.path, options.tenant_id);
        } else {
            throw "ML-REGISTRY-ERROR",
                "required option 'path' or 'backend' not provided";
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
        string version_id = format_date("YYYYMMDDHHmmss", now()) + "_"
            + get_random_bytes(4).toHex();

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
        });

        backend.registerVersion(name, model_data, metadata);

        return version_id;
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

        binary artifact = backend.loadArtifact(name, version_id);
        return ML::ml_deserialize(artifact);
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
}

} # namespace QoreModelRegistry
