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

#! Root data provider for the model registry — lists models as children
/** Navigate: model-registry → {model_name} → versions

    @since QoreModelRegistry 2.0
*/
public class ModelRegistryDataProvider inherits AbstractDataProvider {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "type": "ModelRegistryDataProvider",
            "supports_children": True,
            "children_can_support_records": True,
            "constructor_options": ConstructorOptions,
        };

        #! Supported backend type values for the \c backend_type option
        const BackendTypes = ("filesystem", "database", "rest");

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
        AbstractModelRegistryBackend backend = createBackend(copts);
        registry = new ModelRegistry({"backend": backend});
    }

    #! Creates from an existing registry
    constructor(ModelRegistry registry) {
        self.registry = registry;
    }

    #! Private: creates a backend from validated constructor options
    private static AbstractModelRegistryBackend createBackend(hash<auto> copts) {
        # Pre-constructed backend takes priority
        if (copts.backend instanceof AbstractModelRegistryBackend) {
            return copts.backend;
        }

        string backend_type = copts.backend_type ?? "filesystem";

        switch (backend_type) {
            case "filesystem": {
                if (!copts.path.val()) {
                    throw "CONSTRUCTOR-ERROR",
                        "option 'path' is required when backend_type is 'filesystem'";
                }
                return new FilesystemBackend(copts.path, copts.tenant_id);
            }

%ifndef NoSqlUtil
            case "database": {
                if (!exists copts.datasource) {
                    throw "CONSTRUCTOR-ERROR",
                        "option 'datasource' is required when backend_type is 'database'";
                }
                hash<auto> db_opts;
                if (copts.table_prefix.val()) {
                    db_opts.table_prefix = copts.table_prefix;
                }
                if (copts.tenant_id.val()) {
                    db_opts.tenant_id = copts.tenant_id;
                }
                return new DatabaseBackend(copts.datasource, db_opts);
            }
%endif

            case "rest": {
                if (!exists copts.rest_client) {
                    throw "CONSTRUCTOR-ERROR",
                        "option 'rest_client' is required when backend_type is 'rest'";
                }
                if (!copts.rest_base_path.val()) {
                    throw "CONSTRUCTOR-ERROR",
                        "option 'rest_base_path' is required when backend_type is 'rest'";
                }
                return new RestBackend(copts.rest_client, copts.rest_base_path,
                    copts.tenant_id, copts.rest_options);
            }

            default:
                throw "CONSTRUCTOR-ERROR",
                    sprintf("unsupported backend_type '%s'; valid values: %y",
                        backend_type, BackendTypes);
        }
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
            "supports_delete": True,
            "supports_native_search": True,
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

    private *hash<string, AbstractDataField> getRecordTypeImpl(
            *hash<auto> search_options) {
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
            "file_size": new QoreDataField("file_size", "Artifact size in bytes",
                IntType),
            "format": new QoreDataField("format", "Serialization format",
                StringType),
            "tags": new QoreDataField("tags", "Tags list",
                AbstractDataProviderTypeMap."list"),
        };
    }

    private AbstractDataProviderRecordIterator searchRecordsImpl(
            *hash<auto> where_cond, *hash<auto> search_options) {
        list<hash<auto>> versions = registry.listVersions(model_name);

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

#! Simple record iterator wrapping a list iterator
/** @since QoreModelRegistry 2.0
*/
public class DefaultRecordIterator inherits AbstractDataProviderRecordIterator {
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
