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

#! Filesystem-backed ML model registry for versioning, tagging, and deployment management
public class ModelRegistry {
    private {
        #! Base path for the model registry storage
        string base_path;

        #! Optional tenant ID for multi-tenant deployments
        *string tenant_id;

        #! In-memory index cache: model_dir → {"versions": list, "mtime": date}
        hash<string, hash<auto>> index_cache;

        #! Mutex for thread-safe cache access
        Mutex index_mutex();
    }

    #! Creates a new model registry backed by the given filesystem path
    /** @param options a hash with the following keys:
        - \c path (required): the base directory path for the registry storage
        - \c tenant_id (optional): tenant identifier for multi-tenant deployments;
          when provided, all models are stored under a tenant-specific subdirectory
          and isolated from other tenants
    */
    constructor(hash<auto> options) {
%ifdef NoJson
        throw "ML-REGISTRY-ERROR", "the QoreModelRegistry module requires the json "
            "module (module-json) to be installed; install it and try again";
%endif
        *string path = options.path;
        if (!path.val()) {
            throw "ML-REGISTRY-ERROR", "required option 'path' not provided";
        }
        base_path = path;

        # Validate and set tenant_id if provided
        if (options.tenant_id.val()) {
            string tid = options.tenant_id;
            # Only allow alphanumeric, hyphen, underscore (safe filesystem characters)
            if (tid !~ /^[a-zA-Z0-9_-]+$/) {
                throw "ML-REGISTRY-ERROR",
                    sprintf("invalid tenant_id '%s': must contain only alphanumeric "
                        "characters, hyphens, and underscores", tid);
            }
            tenant_id = tid;
        }

        # Create base directory (and tenant subdirectory) if needed
        string effective_base = getTenantDir();
        if (!is_dir(effective_base)) {
            mkdir_ex(effective_base, 0755, True);
        }
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
    */
    string register(object model, hash<auto> options) {
        *string name = options.name;
        if (!name.val()) {
            throw "ML-REGISTRY-ERROR", "required option 'name' not provided";
        }

        # Generate version ID: timestamp + random hex
        string version_id = format_date("YYYYMMDDHHmmss", now()) + "_"
            + get_random_bytes(4).toHex();

        # Create model directory
        string model_dir = getModelDir(name);
        if (!is_dir(model_dir)) {
            mkdir_ex(model_dir, 0755, True);
        }

        # Serialize model to get size info
        binary model_data = ML::ml_serialize(model);

        # Save model to file
        string model_file = model_dir + DirSep + "v_" + version_id + ".qml";
        ML::ml_save_model(model, model_file);

        # Build metadata
        hash<auto> metadata = {
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
        };
        if (tenant_id) {
            metadata.tenant_id = tenant_id;
        }

        # Save metadata file
        string meta_file = model_dir + DirSep + "v_" + version_id + ".meta.json";
%ifndef NoJson
        File f();
        f.open(meta_file, O_CREAT | O_WRONLY | O_TRUNC);
        f.write(make_json(metadata, JGF_ADD_FORMATTING));
%endif

        # Update registry index
        updateIndex(model_dir, version_id, metadata);

        return version_id;
    }

    #! Lists all versions of a named model
    /** @param name the model name

        @return a list of version metadata hashes, ordered oldest to newest
    */
    list<hash<auto>> listVersions(string name) {
        string model_dir = getModelDir(name);
        hash<auto> index = loadIndex(model_dir);
        return index.versions ?? ();
    }

    #! Loads a specific version of a model (or the latest version)
    /** @param name the model name
        @param version_id the version ID to load, or NOTHING/"latest" for the most recent version

        @return the deserialized ML model object

        @throw ML-REGISTRY-ERROR if the model or version is not found
    */
    object load(string name, *string version_id) {
        string model_dir = getModelDir(name);
        if (!is_dir(model_dir)) {
            throw "ML-REGISTRY-ERROR", sprintf("model '%s' not found%s", name,
                tenant_id ? sprintf(" (tenant '%s')", tenant_id) : "");
        }

        if (!version_id.val() || version_id == "latest") {
            # Find latest version
            list<hash<auto>> versions = listVersions(name);
            if (!versions) {
                throw "ML-REGISTRY-ERROR", sprintf("no versions found for model '%s'", name);
            }
            version_id = versions.last().version_id;
        }

        string model_file = model_dir + DirSep + "v_" + version_id + ".qml";
        if (!is_file(model_file)) {
            throw "ML-REGISTRY-ERROR",
                sprintf("version '%s' of model '%s' not found", version_id, name);
        }

        return ML::ml_load_model(model_file);
    }

    #! Tags a specific version of a model
    /** @param name the model name
        @param version_id the version ID to tag
        @param tag_name the tag to apply

        @throw ML-REGISTRY-ERROR if the version is not found
    */
    nothing tag(string name, string version_id, string tag_name) {
        string model_dir = getModelDir(name);
        index_mutex.lock();
        on_exit index_mutex.unlock();

        hash<auto> index = loadIndexUnlocked(model_dir);

        # Find version and add tag
        bool found = False;
        for (int i = 0; i < index.versions.size(); ++i) {
            if (index.versions[i].version_id == version_id) {
                if (!index.versions[i].tags.contains(tag_name)) {
                    push index.versions[i].tags, tag_name;
                }
                found = True;
                break;
            }
        }
        if (!found) {
            throw "ML-REGISTRY-ERROR",
                sprintf("version '%s' not found for model '%s'", version_id, name);
        }

        saveIndexUnlocked(model_dir, index);
    }

    #! Loads the latest version of a model that has a specific tag
    /** @param name the model name
        @param tag_name the tag to search for

        @return the deserialized ML model object

        @throw ML-REGISTRY-ERROR if no version with the given tag is found
    */
    object loadByTag(string name, string tag_name) {
        list<hash<auto>> versions = listVersions(name);
        # Find latest version with the tag (search from end)
        for (int i = versions.size() - 1; i >= 0; --i) {
            if (versions[i].tags.contains(tag_name)) {
                return load(name, versions[i].version_id);
            }
        }
        throw "ML-REGISTRY-ERROR",
            sprintf("no version with tag '%s' found for model '%s'", tag_name, name);
    }

    #! Prunes old versions of a model, keeping the latest N and optionally tagged versions
    /** @param name the model name
        @param options a hash with the following optional keys:
        - \c keep_tagged (default True): if True, keep versions that have tags even if they are older
        - \c keep_latest (default 5): the number of most recent versions to always keep

        @return the number of versions deleted
    */
    int prune(string name, *hash<auto> options) {
        bool keep_tagged = options.keep_tagged ?? True;
        int keep_latest = options.keep_latest ?? 5;

        string model_dir = getModelDir(name);
        index_mutex.lock();
        on_exit index_mutex.unlock();

        hash<auto> index = loadIndexUnlocked(model_dir);
        list<hash<auto>> versions = index.versions ?? ();

        if (versions.size() <= keep_latest) {
            return 0;
        }

        int deleted = 0;
        list<hash<auto>> kept();

        # Always keep the latest N
        int keep_from = versions.size() - keep_latest;
        for (int i = 0; i < versions.size(); ++i) {
            bool keep = (i >= keep_from);
            if (!keep && keep_tagged && versions[i].tags.size() > 0) {
                keep = True;
            }
            if (keep) {
                push kept, versions[i];
            } else {
                # Delete model and metadata files
                string model_file = model_dir + DirSep + "v_" + versions[i].version_id + ".qml";
                string meta_file = model_dir + DirSep + "v_" + versions[i].version_id
                    + ".meta.json";
                if (is_file(model_file)) {
                    unlink(model_file);
                }
                if (is_file(meta_file)) {
                    unlink(meta_file);
                }
                ++deleted;
            }
        }

        index.versions = kept;
        saveIndexUnlocked(model_dir, index);
        return deleted;
    }

    #! Lists all model names in the registry
    /** @return a sorted list of model name strings
    */
    list<string> listModels() {
        list<string> result();
        Dir d();
        if (!d.chdir(getTenantDir())) {
            return result;
        }
        foreach string entry in (d.listDirs()) {
            push result, entry;
        }
        return sort(result);
    }

    #! Lists all tenant IDs in the registry
    /** Only meaningful when the registry is used in multi-tenant mode.
        This method lists subdirectories of the base path that contain tenant data.

        @return a sorted list of tenant ID strings

        @note This is an administrative method — it should only be called by
        a registry instance created \b without a \c tenant_id (i.e., an admin-level
        registry), as it lists tenants across the entire registry.
    */
    list<string> listTenants() {
        list<string> result();
        Dir d();
        if (!d.chdir(base_path)) {
            return result;
        }
        foreach string entry in (d.listDirs()) {
            # A tenant directory contains model subdirectories with registry.json
            Dir td();
            if (td.chdir(base_path + DirSep + entry)) {
                foreach string subdir in (td.listDirs()) {
                    if (is_file(base_path + DirSep + entry + DirSep + subdir
                            + DirSep + "registry.json")) {
                        push result, entry;
                        break;
                    }
                }
            }
        }
        return sort(result);
    }

    #! Returns the tenant ID for this registry instance, or NOTHING if single-tenant
    /** @return the tenant ID string, or NOTHING
    */
    *string getTenantId() {
        return tenant_id;
    }

    #! Deletes a model and all its versions from the registry
    /** @param name the model name

        @throw ML-REGISTRY-ERROR if the model is not found
    */
    nothing deleteModel(string name) {
        string model_dir = getModelDir(name);
        if (!is_dir(model_dir)) {
            throw "ML-REGISTRY-ERROR", sprintf("model '%s' not found%s", name,
                tenant_id ? sprintf(" (tenant '%s')", tenant_id) : "");
        }
        # Delete all files in directory
        Dir d();
        d.chdir(model_dir);
        foreach string entry in (d.listFiles()) {
            unlink(model_dir + DirSep + entry);
        }
        rmdir(model_dir);

        # Invalidate cache for this model directory
        index_mutex.lock();
        on_exit index_mutex.unlock();
        remove index_cache{model_dir};
    }

    #! Returns version metadata for a specific version
    /** @param name the model name
        @param version_id the version ID

        @return the metadata hash for the version

        @throw ML-REGISTRY-ERROR if the version is not found
    */
    hash<auto> getVersionMetadata(string name, string version_id) {
        list<hash<auto>> versions = listVersions(name);
        foreach hash<auto> v in (versions) {
            if (v.version_id == version_id) {
                return v;
            }
        }
        throw "ML-REGISTRY-ERROR",
            sprintf("version '%s' not found for model '%s'", version_id, name);
    }

    #! Removes a tag from a specific version
    /** @param name the model name
        @param version_id the version ID
        @param tag_name the tag to remove

        @throw ML-REGISTRY-ERROR if the version is not found
    */
    nothing untag(string name, string version_id, string tag_name) {
        string model_dir = getModelDir(name);
        index_mutex.lock();
        on_exit index_mutex.unlock();

        hash<auto> index = loadIndexUnlocked(model_dir);

        bool found = False;
        for (int i = 0; i < index.versions.size(); ++i) {
            if (index.versions[i].version_id == version_id) {
                list<string> new_tags();
                foreach string t in (index.versions[i].tags) {
                    if (t != tag_name) {
                        push new_tags, t;
                    }
                }
                index.versions[i].tags = new_tags;
                found = True;
                break;
            }
        }
        if (!found) {
            throw "ML-REGISTRY-ERROR",
                sprintf("version '%s' not found for model '%s'", version_id, name);
        }

        saveIndexUnlocked(model_dir, index);
    }

    #! Private: loads the registry index, using cache when available and current
    /** Must be called with index_mutex held
    */
    private hash<auto> loadIndexUnlocked(string model_dir) {
        string index_file = model_dir + DirSep + "registry.json";

        # Check cache first
        *hash<auto> cached = index_cache{model_dir};
        if (cached) {
            # Validate cache: check if file mtime has changed
            if (is_file(index_file)) {
                *hash<StatInfo> st = hstat(index_file);
                if (st && st.mtime == cached.mtime) {
                    return cached.index;
                }
            } else {
                # File gone — cached data is stale
                remove index_cache{model_dir};
            }
        }

        # Cache miss or stale — read from disk
        hash<auto> index;
        if (!is_file(index_file)) {
            index = {"versions": ()};
        } else {
%ifndef NoJson
            string json_str = ReadOnlyFile::readTextFile(index_file);
            index = parse_json(json_str);
%else
            index = {"versions": ()};
%endif
        }

        # Update cache
        *hash<StatInfo> st = is_file(index_file) ? hstat(index_file) : NOTHING;
        index_cache{model_dir} = {
            "index": index,
            "mtime": st ? st.mtime : now(),
        };

        return index;
    }

    #! Private: saves the registry index to disk and updates cache
    /** Must be called with index_mutex held
    */
    private nothing saveIndexUnlocked(string model_dir, hash<auto> index) {
        string index_file = model_dir + DirSep + "registry.json";
%ifndef NoJson
        File f();
        f.open(index_file, O_CREAT | O_WRONLY | O_TRUNC);
        f.write(make_json(index, JGF_ADD_FORMATTING));
%endif

        # Update cache with new data and current mtime
        *hash<StatInfo> st = is_file(index_file) ? hstat(index_file) : NOTHING;
        index_cache{model_dir} = {
            "index": index,
            "mtime": st ? st.mtime : now(),
        };
    }

    #! Private: thread-safe index load
    private hash<auto> loadIndex(string model_dir) {
        index_mutex.lock();
        on_exit index_mutex.unlock();
        return loadIndexUnlocked(model_dir);
    }

    #! Private: thread-safe index save
    private nothing saveIndex(string model_dir, hash<auto> index) {
        index_mutex.lock();
        on_exit index_mutex.unlock();
        saveIndexUnlocked(model_dir, index);
    }

    #! Private: atomically loads index, appends version, and saves
    private nothing updateIndex(string model_dir, string version_id, hash<auto> metadata) {
        index_mutex.lock();
        on_exit index_mutex.unlock();
        hash<auto> index = loadIndexUnlocked(model_dir);
        push index.versions, metadata;
        saveIndexUnlocked(model_dir, index);
    }

    #! Private: returns the effective base directory, scoped to tenant if applicable
    private string getTenantDir() {
        if (tenant_id) {
            return base_path + DirSep + tenant_id;
        }
        return base_path;
    }

    #! Private: returns the directory path for a named model, scoped to tenant if applicable
    private string getModelDir(string name) {
        return getTenantDir() + DirSep + name;
    }

    #! Private: determines the algorithm name from a model object
    private string getAlgorithmName(object model) {
        # Determine algorithm name from class name
        string class_name = model.className();
        # Strip namespace prefix if present
        if (class_name =~ /::/) {
            class_name = (class_name =~ x/([^:]+)$/)[0];
        }
        return class_name;
    }
}

} # namespace QoreModelRegistry
