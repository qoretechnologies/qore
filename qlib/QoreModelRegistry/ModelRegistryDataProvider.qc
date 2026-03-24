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

        const ConstructorOptions = {
            "path": <DataProviderOptionInfo>{
                "display_name": "Registry Path",
                "short_desc": "Filesystem path for the model registry",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Base directory path for the filesystem-backed model registry",
                "required": True,
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
    }

    #! Creates from constructor options
    constructor(*hash<auto> options) {
        hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        registry = new ModelRegistry(copts);
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
        return registry.listModels();
    }

    private *AbstractDataProvider getChildProviderImpl(string name) {
        list<string> models = registry.listModels();
        if (!models.contains(name)) {
            return;
        }
        return new ModelVersionsDataProvider(registry, name);
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
