# -*- mode: qore; indent-tabs-mode: nil -*-

/*  FilesystemBackend.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Filesystem-backed model registry storage backend
/** Stores model artifacts as files on the local filesystem with a JSON-based
    index for version tracking. Supports multi-tenant deployment with
    per-tenant subdirectories.

    Storage layout (single-tenant):
    @verbatim
    <base_path>/
    ├── <model_name>/
    │   ├── registry.json               # version index
    │   ├── v_<version_id>.qml          # model artifact
    │   └── v_<version_id>.meta.json    # version metadata
    └── ...
    @endverbatim

    Storage layout (multi-tenant):
    @verbatim
    <base_path>/
    ├── <tenant_id>/
    │   ├── <model_name>/
    │   │   ├── registry.json
    │   │   └── ...
    │   └── ...
    └── ...
    @endverbatim

    @since QoreModelRegistry 2.0
*/
public class FilesystemBackend inherits AbstractModelRegistryBackend {
    private {
        #! Base path for the model registry storage
        string base_path;

        #! In-memory index cache: model_dir → {"index": hash, "mtime": date}
        hash<string, hash<auto>> index_cache;

        #! Mutex for thread-safe cache access
        Mutex index_mutex();
    }

    #! Creates a new filesystem backend
    /** @param base_path the base directory path for registry storage
        @param tenant_id optional tenant identifier for multi-tenant deployments

        @throw ML-REGISTRY-ERROR if base_path is not provided
    */
    constructor(string base_path, *string tenant_id)
            : AbstractModelRegistryBackend(tenant_id) {
        self.base_path = base_path;

        # Create base directory (and tenant subdirectory) if needed
        string effective_base = getTenantDir();
        if (!is_dir(effective_base)) {
            mkdir_ex(effective_base, 0755, True);
        }
    }

    string registerVersion(string name, binary artifact,
            hash<ModelVersionInfo> metadata) {
        string version_id = metadata.version_id;
        string ext = metadata.format ?? "qml";
        if (!metadata.artifacts) {
            metadata.artifacts = (<ModelArtifactInfo>{
                "name": "model",
                "role": "model",
                "format": ext,
                "size": artifact.size(),
                "safe_to_parse": ext == "qml" || ext == "onnx",
                "executable": ext == "qml" || ext == "onnx",
            },);
        }

        # Create model directory
        string model_dir = getModelDir(name);
        if (!is_dir(model_dir)) {
            mkdir_ex(model_dir, 0755, True);
        }

        # Save artifact to file (extension matches format)
        string artifact_file = model_dir + DirSep + "v_" + version_id + "." + ext;
        try {
            {
                File f();
                f.open(artifact_file, O_CREAT | O_WRONLY | O_TRUNC, 0644);
                f.write(artifact);
            }

            # Save metadata file
%ifndef NoJson
            writeVersionMetadataFiles(model_dir, version_id, metadata);
%endif
        } catch (hash<ExceptionInfo> ex) {
            # Clean up orphaned files on error
            if (is_file(artifact_file)) {
                unlink(artifact_file);
            }
            rethrow;
        }

        # Update registry index
        updateIndex(model_dir, metadata);

        return version_id;
    }

    nothing registerNamedArtifact(string name, string version_id, string artifact_name,
            binary artifact, hash<ModelArtifactInfo> metadata) {
        validateArtifactName(artifact_name);
        string model_dir = getModelDir(name);
        hash<ModelVersionInfo> version_meta = getVersionMetadata(name, version_id);

        string artifact_dir = getVersionArtifactDir(model_dir, version_id);
        if (!is_dir(artifact_dir)) {
            mkdir_ex(artifact_dir, 0755, True);
        }

        string artifact_file = artifact_dir + DirSep + artifact_name;
        {
            File f();
            f.open(artifact_file, O_CREAT | O_WRONLY | O_TRUNC, 0644);
            f.write(artifact);
        }

        metadata.name = artifact_name;
        metadata.size = artifact.size();
        if (!metadata.path_hint.val()) {
            metadata.path_hint = "v_" + version_id + DirSep + "artifacts" + DirSep
                + artifact_name;
        }

        list<hash<ModelArtifactInfo>> artifacts = version_meta.artifacts ?? ();
        bool replaced = False;
        for (int i = 0; i < artifacts.size(); ++i) {
            if (artifacts[i].name == artifact_name) {
                artifacts[i] = metadata;
                replaced = True;
                break;
            }
        }
        if (!replaced) {
            push artifacts, metadata;
        }
        version_meta.artifacts = artifacts;

        updateVersionMetadata(model_dir, version_id, version_meta);
    }

    binary loadArtifact(string name, string version_id) {
        string model_dir = getModelDir(name);
        if (!is_dir(model_dir)) {
            throw "ML-REGISTRY-ERROR", sprintf("model '%s' not found%s", name,
                tenant_id ? sprintf(" (tenant '%s')", tenant_id) : "");
        }

        # Try known extensions in order of likelihood
        string base = model_dir + DirSep + "v_" + version_id;
        *string artifact_file;
        foreach string ext in ("qml", "onnx", "json") {
            string candidate = base + "." + ext;
            if (is_file(candidate)) {
                artifact_file = candidate;
                break;
            }
        }
        if (!artifact_file) {
            throw "ML-REGISTRY-ERROR",
                sprintf("version '%s' of model '%s' not found", version_id, name);
        }

        File f();
        f.open(artifact_file, O_RDONLY);
        return f.readBinary(-1);
    }

    binary loadNamedArtifact(string name, string version_id, string artifact_name) {
        validateArtifactName(artifact_name);
        if (artifact_name == "model") {
            return loadArtifact(name, version_id);
        }

        string model_dir = getModelDir(name);
        string artifact_file = getVersionArtifactDir(model_dir, version_id)
            + DirSep + artifact_name;
        if (!is_file(artifact_file)) {
            hash<ModelVersionInfo> meta = getVersionMetadata(name, version_id);
            foreach hash<ModelArtifactInfo> artifact_info in (meta.artifacts) {
                if (artifact_info.name == artifact_name) {
                    string primary_file = model_dir + DirSep + "v_" + version_id
                        + "." + artifact_info.format;
                    if (is_file(primary_file)) {
                        File f();
                        f.open(primary_file, O_RDONLY);
                        return f.readBinary(-1);
                    }
                }
            }
            throw "ML-REGISTRY-ERROR",
                sprintf("artifact '%s' not found for model '%s' version '%s'",
                    artifact_name, name, version_id);
        }
        File f();
        f.open(artifact_file, O_RDONLY);
        return f.readBinary(-1);
    }

    list<hash<ModelArtifactInfo>> listArtifacts(string name, string version_id) {
        hash<ModelVersionInfo> meta = getVersionManifest(name, version_id);
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

    list<hash<ModelVersionInfo>> listVersions(string name) {
        string model_dir = getModelDir(name);
        hash<auto> index = loadIndex(model_dir);
        list<hash<ModelVersionInfo>> result();
        foreach auto version in (index.versions ?? ()) {
            push result, AbstractModelRegistryBackend::normalizeVersionInfo(
                cast<hash<auto>>(version));
        }
        return result;
    }

    list<string> listModels() {
        list<string> result();
        Dir d();
        if (!d.chdir(getTenantDir())) {
            return result;
        }
        foreach string entry in (d.listDirNames()) {
            push result, entry;
        }
        return sort(result);
    }

    nothing tag(string name, string version_id, string tag_name) {
        string model_dir = getModelDir(name);
        index_mutex.lock();
        on_exit index_mutex.unlock();

        hash<auto> index = loadIndexUnlocked(model_dir);

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

    int prune(string name, *hash<PruneOptions> options) {
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
                # Delete artifact files (try all known extensions)
                string base = model_dir + DirSep + "v_"
                    + versions[i].version_id;
                foreach string ext in ("qml", "onnx", "json") {
                    string artifact_file = base + "." + ext;
                    if (is_file(artifact_file)) {
                        unlink(artifact_file);
                    }
                }
                string package_dir = model_dir + DirSep + "v_" + versions[i].version_id;
                if (is_dir(package_dir)) {
                    deleteDirRecursive(package_dir);
                }
                string meta_file = base + ".meta.json";
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

    nothing deleteModel(string name) {
        string model_dir = getModelDir(name);
        if (!is_dir(model_dir)) {
            throw "ML-REGISTRY-ERROR", sprintf("model '%s' not found%s", name,
                tenant_id ? sprintf(" (tenant '%s')", tenant_id) : "");
        }
        deleteDirRecursive(model_dir);

        # Invalidate cache
        index_mutex.lock();
        on_exit index_mutex.unlock();
        remove index_cache{model_dir};
    }

    hash<ModelVersionInfo> getVersionMetadata(string name, string version_id) {
        list<hash<ModelVersionInfo>> versions = listVersions(name);
        foreach hash<ModelVersionInfo> v in (versions) {
            if (v.version_id == version_id) {
                return v;
            }
        }
        throw "ML-REGISTRY-ERROR",
            sprintf("version '%s' not found for model '%s'", version_id, name);
    }

    hash<ModelVersionInfo> getVersionManifest(string name, string version_id) {
        hash<ModelVersionInfo> meta = getVersionMetadata(name, version_id);
%ifndef NoJson
        string manifest_file = getVersionDir(getModelDir(name), version_id)
            + DirSep + "manifest.json";
        if (is_file(manifest_file)) {
            hash<auto> manifest = parse_json(ReadOnlyFile::readTextFile(manifest_file));
            if (manifest.artifacts) {
                meta.artifacts = AbstractModelRegistryBackend::normalizeArtifactInfos(
                    manifest.artifacts);
            }
            if (manifest.execution) {
                meta.execution = cast<hash<ModelExecutionInfo>>(manifest.execution);
            }
            if (manifest.tokenizer) {
                meta.tokenizer = cast<hash<TokenizerArtifactInfo>>(manifest.tokenizer);
            }
        }
%endif
        return meta;
    }

    list<string> listTenants() {
        list<string> result();
        Dir d();
        if (!d.chdir(base_path)) {
            return result;
        }
        foreach string entry in (d.listDirNames()) {
            # A tenant directory contains model subdirectories with registry.json
            Dir td();
            if (td.chdir(base_path + DirSep + entry)) {
                foreach string subdir in (td.listDirNames()) {
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

    nothing deleteTenantStorage(string tid) {
        # Validate the tenant ID
        if (tid !~ /^[a-zA-Z0-9_-]+$/) {
            throw "ML-REGISTRY-ERROR",
                sprintf("invalid tenant_id '%s' for deletion", tid);
        }
        string tenant_dir = base_path + DirSep + tid;
        if (!is_dir(tenant_dir)) {
            throw "ML-REGISTRY-ERROR",
                sprintf("tenant '%s' not found in registry", tid);
        }
        # Recursively delete tenant directory
        deleteDirRecursive(tenant_dir);

        # Invalidate all cache entries for this tenant
        index_mutex.lock();
        on_exit index_mutex.unlock();
        string prefix = tenant_dir + DirSep;
        list<string> keys_to_remove();
        foreach string key in (keys index_cache) {
            if (key.find(prefix) == 0 || key == tenant_dir) {
                push keys_to_remove, key;
            }
        }
        foreach string key in (keys_to_remove) {
            remove index_cache{key};
        }
    }

    #! Private: loads the registry index, using cache when available and current
    /** Must be called with index_mutex held
    */
    private hash<auto> loadIndexUnlocked(string model_dir) {
        string index_file = model_dir + DirSep + "registry.json";

        # Check cache: validate by mtime + size to detect same-second changes
        *hash<auto> cached = index_cache{model_dir};
        if (cached) {
            if (is_file(index_file)) {
                *hash<StatInfo> st = hstat(index_file);
                if (st && st.mtime == cached.mtime && st.size == cached.fsize) {
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

        # Update cache — stat AFTER read to minimize TOCTOU window
        *hash<StatInfo> st = is_file(index_file) ? hstat(index_file) : NOTHING;
        index_cache{model_dir} = {
            "index": index,
            "mtime": st ? st.mtime : now(),
            "fsize": st ? st.size : 0,
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

        # Update cache with new data, mtime, and file size
        *hash<StatInfo> st = is_file(index_file) ? hstat(index_file) : NOTHING;
        index_cache{model_dir} = {
            "index": index,
            "mtime": st ? st.mtime : now(),
            "fsize": st ? st.size : 0,
        };
    }

    #! Private: thread-safe index load
    private hash<auto> loadIndex(string model_dir) {
        index_mutex.lock();
        on_exit index_mutex.unlock();
        return loadIndexUnlocked(model_dir);
    }

    #! Private: atomically loads index, appends version, and saves
    private nothing updateIndex(string model_dir, hash<ModelVersionInfo> metadata) {
        index_mutex.lock();
        on_exit index_mutex.unlock();
        hash<auto> index = loadIndexUnlocked(model_dir);
        push index.versions, metadata;
        saveIndexUnlocked(model_dir, index);
    }

    #! Private: updates a version metadata entry in the index and metadata files
    private nothing updateVersionMetadata(string model_dir, string version_id,
            hash<ModelVersionInfo> metadata) {
        index_mutex.lock();
        on_exit index_mutex.unlock();
        hash<auto> index = loadIndexUnlocked(model_dir);
        bool found = False;
        for (int i = 0; i < index.versions.size(); ++i) {
            if (index.versions[i].version_id == version_id) {
                index.versions[i] = metadata;
                found = True;
                break;
            }
        }
        if (!found) {
            throw "ML-REGISTRY-ERROR",
                sprintf("version '%s' not found", version_id);
        }
%ifndef NoJson
        writeVersionMetadataFiles(model_dir, version_id, metadata);
%endif
        saveIndexUnlocked(model_dir, index);
    }

    #! Private: writes metadata and package manifest files
    private nothing writeVersionMetadataFiles(string model_dir, string version_id,
            hash<ModelVersionInfo> metadata) {
%ifndef NoJson
        string meta_file = model_dir + DirSep + "v_" + version_id + ".meta.json";
        {
            File f();
            f.open(meta_file, O_CREAT | O_WRONLY | O_TRUNC, 0644);
            f.write(make_json(metadata, JGF_ADD_FORMATTING));
        }

        string version_dir = getVersionDir(model_dir, version_id);
        if (!is_dir(version_dir)) {
            mkdir_ex(version_dir, 0755, True);
        }
        string manifest_file = version_dir + DirSep + "manifest.json";
        {
            File f();
            f.open(manifest_file, O_CREAT | O_WRONLY | O_TRUNC, 0644);
            f.write(make_json(metadata, JGF_ADD_FORMATTING));
        }
%endif
    }

    #! Private: returns the package directory for a version
    private string getVersionDir(string model_dir, string version_id) {
        return model_dir + DirSep + "v_" + version_id;
    }

    #! Private: returns the named-artifact directory for a version
    private string getVersionArtifactDir(string model_dir, string version_id) {
        return getVersionDir(model_dir, version_id) + DirSep + "artifacts";
    }

    #! Private: validates an artifact name as a package-local filename
    private static nothing validateArtifactName(string artifact_name) {
        if (!artifact_name.val()) {
            throw "ML-REGISTRY-ERROR", "artifact name must not be empty";
        }
        if (artifact_name == "." || artifact_name == ".."
                || artifact_name =~ /[\\\/]/) {
            throw "ML-REGISTRY-ERROR",
                sprintf("invalid artifact name '%s': path separators and traversal "
                    "segments are not allowed", artifact_name);
        }
    }

    #! Private: returns the effective base directory, scoped to tenant if applicable
    private string getTenantDir() {
        if (tenant_id) {
            return base_path + DirSep + tenant_id;
        }
        return base_path;
    }

    #! Private: returns the directory path for a named model
    private string getModelDir(string name) {
        return getTenantDir() + DirSep + name;
    }

    #! Private: recursively deletes a directory and all contents
    private static nothing deleteDirRecursive(string dir) {
        if (!is_dir(dir)) {
            return;
        }
        Dir d();
        d.chdir(dir);
        # Delete subdirectories first
        foreach string subdir in (d.listDirNames()) {
            FilesystemBackend::deleteDirRecursive(dir + DirSep + subdir);
        }
        # Delete files
        foreach string f in (d.listFileNames()) {
            unlink(dir + DirSep + f);
        }
        rmdir(dir);
    }
}

} # namespace QoreModelRegistry
