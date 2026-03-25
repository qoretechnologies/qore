# -*- mode: qore; indent-tabs-mode: nil -*-

/*  DatabaseBackend.qc Copyright 2026 Qore Technologies, s.r.o.

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

%ifndef NoSqlUtil

#! Database-backed model registry storage backend using SqlUtil
/** Stores model artifacts and metadata in database tables via SqlUtil.
    All tables include a \c tenant_id column for multi-tenant isolation.

    Tables used:
    - \c {prefix}models — model metadata (name, algorithm, type, provider)
    - \c {prefix}versions — version records with metadata
    - \c {prefix}blobs — artifact storage (BLOB)

    The caller is responsible for ensuring the tables exist in the database.
    Use @ref DatabaseBackend::getTableDefinitions() to get the table schemas
    for creation.

    @par Example:
    @code{.py}
    AbstractDatasource ds = ...; # your datasource
    auto backend = new DatabaseBackend(ds, {"table_prefix": "ml_", "tenant_id": "acme"});
    auto reg = new ModelRegistry({"backend": backend});
    @endcode

    @since QoreModelRegistry 2.0
*/
public class DatabaseBackend inherits AbstractModelRegistryBackend {
    private {
        #! Datasource for database operations
        AbstractDatasource ds;

        #! Table prefix for all registry tables
        string table_prefix;

        #! Models table
        AbstractTable models_table;

        #! Versions table
        AbstractTable versions_table;

        #! Blobs table
        AbstractTable blobs_table;
    }

    #! Creates a new database backend
    /** @param ds the datasource to use for database operations
        @param options a hash with the following keys:
        - \c table_prefix (default "ml_"): prefix for all table names
        - \c tenant_id (optional): tenant identifier for multi-tenant deployments

        @throw ML-REGISTRY-ERROR if the datasource is not provided or tables don't exist
    */
    constructor(AbstractDatasource ds, *hash<auto> options)
            : AbstractModelRegistryBackend(options.tenant_id) {
        self.ds = ds;
        table_prefix = options.table_prefix ?? "ml_";

        # Get table references
        models_table = AbstractTable::getTable(ds, table_prefix + "models");
        versions_table = AbstractTable::getTable(ds, table_prefix + "versions");
        blobs_table = AbstractTable::getTable(ds, table_prefix + "blobs");
    }

    string registerVersion(string name, binary artifact,
            hash<ModelVersionInfo> metadata) {
        string version_id = metadata.version_id;
        string eff_tenant = tenant_id ?? "";

        # Ensure model record exists
        *hash<auto> model_row = models_table.selectRow({
            "where": {
                "tenant_id": eff_tenant,
                "name": name,
            },
        });

        int modelid;
        if (!model_row) {
            # Insert new model record
            hash<auto> model_insert = {
                "tenant_id": eff_tenant,
                "name": name,
                "algorithm": metadata.algorithm,
                "created": now_us(),
                "modified": now_us(),
            };
            models_table.insert(model_insert);
            # Re-select to get the ID
            model_row = models_table.selectRow({
                "where": {
                    "tenant_id": eff_tenant,
                    "name": name,
                },
            });
            modelid = model_row.modelid;
        } else {
            modelid = model_row.modelid;
            # Update modified timestamp
            models_table.update({"modified": now_us()}, {
                "modelid": modelid,
            });
        }

        # Insert version record
        hash<auto> version_insert = {
            "tenant_id": eff_tenant,
            "modelid": modelid,
            "version_id": version_id,
            "algorithm": metadata.algorithm,
            "format": metadata.format ?? "qml",
            "artifact_size": artifact.size(),
            "metrics": metadata.metrics ? make_json(metadata.metrics) : NOTHING,
            "tags": metadata.tags ? make_json(metadata.tags) : "[]",
            "training_data_hash": metadata.training_data_hash,
            "description": metadata.description,
            "config": metadata.config ? make_json(metadata.config) : NOTHING,
            "created": now_us(),
        };
        versions_table.insert(version_insert);

        # Get the version row ID
        *hash<auto> ver_row = versions_table.selectRow({
            "where": {
                "tenant_id": eff_tenant,
                "modelid": modelid,
                "version_id": version_id,
            },
        });

        # Insert artifact blob
        blobs_table.insert({
            "tenant_id": eff_tenant,
            "versionid": ver_row.versionid,
            "data": artifact,
        });

        ds.commit();
        return version_id;
    }

    binary loadArtifact(string name, string version_id) {
        string eff_tenant = tenant_id ?? "";

        # Look up model
        *hash<auto> model_row = models_table.selectRow({
            "where": {
                "tenant_id": eff_tenant,
                "name": name,
            },
        });
        if (!model_row) {
            throw "ML-REGISTRY-ERROR", sprintf("model '%s' not found%s", name,
                tenant_id ? sprintf(" (tenant '%s')", tenant_id) : "");
        }

        # Look up version
        *hash<auto> ver_row = versions_table.selectRow({
            "where": {
                "tenant_id": eff_tenant,
                "modelid": model_row.modelid,
                "version_id": version_id,
            },
        });
        if (!ver_row) {
            throw "ML-REGISTRY-ERROR",
                sprintf("version '%s' of model '%s' not found", version_id, name);
        }

        # Load blob
        *hash<auto> blob_row = blobs_table.selectRow({
            "where": {
                "tenant_id": eff_tenant,
                "versionid": ver_row.versionid,
            },
        });
        if (!blob_row) {
            throw "ML-REGISTRY-ERROR",
                sprintf("artifact for version '%s' of model '%s' not found",
                    version_id, name);
        }

        return blob_row.data;
    }

    list<hash<ModelVersionInfo>> listVersions(string name) {
        string eff_tenant = tenant_id ?? "";

        *hash<auto> model_row = models_table.selectRow({
            "where": {
                "tenant_id": eff_tenant,
                "name": name,
            },
        });
        if (!model_row) {
            return ();
        }

        *list<auto> rows = versions_table.selectRows({
            "where": {
                "tenant_id": eff_tenant,
                "modelid": model_row.modelid,
            },
            "orderby": "created",
        });

        list<hash<ModelVersionInfo>> result();
        foreach hash<auto> row in (rows) {
            push result, cast<hash<ModelVersionInfo>>({
                "version_id": string(row.version_id),
                "name": name,
                "algorithm": string(row.algorithm),
                "created": format_date("YYYY-MM-DDTHH:mm:ssZ", row.created),
                "description": row.description ? string(row.description) : NOTHING,
                "metrics": row.metrics ? parse_json(row.metrics) : NOTHING,
                "tags": row.tags ? parse_json(row.tags) : (),
                "file_size": int(row.artifact_size),
                "format": string(row.format ?? "qml"),
                "training_data_hash": row.training_data_hash
                    ? string(row.training_data_hash) : NOTHING,
                "tenant_id": tenant_id,
                "config": row.config ? parse_json(row.config) : NOTHING,
            });
        }
        return result;
    }

    list<string> listModels() {
        string eff_tenant = tenant_id ?? "";

        *list<auto> rows = models_table.selectRows({
            "where": {
                "tenant_id": eff_tenant,
            },
            "orderby": "name",
        });

        list<string> result();
        foreach hash<auto> row in (rows) {
            push result, row.name;
        }
        return result;
    }

    nothing tag(string name, string version_id, string tag_name) {
        string eff_tenant = tenant_id ?? "";

        hash<auto> ver_info = getVersionRow(name, version_id, eff_tenant);

        # Parse existing tags, add new one if not present
        list<string> tags = ver_info.tags ? parse_json(ver_info.tags) : ();
        if (!tags.contains(tag_name)) {
            push tags, tag_name;
            versions_table.update({"tags": make_json(tags)}, {
                "versionid": ver_info.versionid,
            });
            ds.commit();
        }
    }

    nothing untag(string name, string version_id, string tag_name) {
        string eff_tenant = tenant_id ?? "";

        hash<auto> ver_info = getVersionRow(name, version_id, eff_tenant);

        # Parse existing tags, remove the specified one
        list<string> tags = ver_info.tags ? parse_json(ver_info.tags) : ();
        list<string> new_tags();
        foreach string t in (tags) {
            if (t != tag_name) {
                push new_tags, t;
            }
        }
        versions_table.update({"tags": make_json(new_tags)}, {
            "versionid": ver_info.versionid,
        });
        ds.commit();
    }

    int prune(string name, *hash<PruneOptions> options) {
        bool keep_tagged = options.keep_tagged ?? True;
        int keep_latest = options.keep_latest ?? 5;

        list<hash<ModelVersionInfo>> versions = listVersions(name);
        if (versions.size() <= keep_latest) {
            return 0;
        }

        string eff_tenant = tenant_id ?? "";
        int keep_from = versions.size() - keep_latest;

        # Collect version IDs to delete
        list<string> version_ids_to_delete();
        for (int i = 0; i < versions.size(); ++i) {
            bool keep = (i >= keep_from);
            if (!keep && keep_tagged && versions[i].tags.size() > 0) {
                keep = True;
            }
            if (!keep) {
                push version_ids_to_delete, versions[i].version_id;
            }
        }

        if (!version_ids_to_delete) {
            return 0;
        }

        # Batch lookup version row IDs
        *hash<auto> model_row = models_table.selectRow({
            "where": {
                "tenant_id": eff_tenant,
                "name": name,
            },
        });
        if (model_row) {
            *list<auto> ver_rows = versions_table.selectRows({
                "columns": ("versionid",),
                "where": {
                    "tenant_id": eff_tenant,
                    "modelid": model_row.modelid,
                    "version_id": op_in(version_ids_to_delete),
                },
            });
            if (ver_rows) {
                list<int> vids = map int($1.versionid), ver_rows;
                blobs_table.del({"versionid": op_in(vids)});
                versions_table.del({"versionid": op_in(vids)});
            }
        }

        ds.commit();
        return version_ids_to_delete.size();
    }

    nothing deleteModel(string name) {
        string eff_tenant = tenant_id ?? "";

        *hash<auto> model_row = models_table.selectRow({
            "where": {
                "tenant_id": eff_tenant,
                "name": name,
            },
        });
        if (!model_row) {
            throw "ML-REGISTRY-ERROR", sprintf("model '%s' not found%s", name,
                tenant_id ? sprintf(" (tenant '%s')", tenant_id) : "");
        }

        # Get version IDs for batch blob deletion
        *list<auto> ver_rows = versions_table.selectRows({
            "columns": ("versionid",),
            "where": {
                "tenant_id": eff_tenant,
                "modelid": model_row.modelid,
            },
        });
        if (ver_rows) {
            list<int> vids = map int($1.versionid), ver_rows;
            blobs_table.del({"versionid": op_in(vids)});
        }

        # Delete all versions in one operation
        versions_table.del({
            "tenant_id": eff_tenant,
            "modelid": model_row.modelid,
        });

        # Delete model record
        models_table.del({"modelid": model_row.modelid});

        ds.commit();
    }

    hash<ModelVersionInfo> getVersionMetadata(string name, string version_id) {
        string eff_tenant = tenant_id ?? "";

        *hash<auto> model_row = models_table.selectRow({
            "where": {
                "tenant_id": eff_tenant,
                "name": name,
            },
        });
        if (!model_row) {
            throw "ML-REGISTRY-ERROR", sprintf("model '%s' not found%s", name,
                tenant_id ? sprintf(" (tenant '%s')", tenant_id) : "");
        }

        *hash<auto> row = versions_table.selectRow({
            "where": {
                "tenant_id": eff_tenant,
                "modelid": model_row.modelid,
                "version_id": version_id,
            },
        });
        if (!row) {
            throw "ML-REGISTRY-ERROR",
                sprintf("version '%s' not found for model '%s'", version_id, name);
        }

        return cast<hash<ModelVersionInfo>>({
            "version_id": string(row.version_id),
            "name": name,
            "algorithm": string(row.algorithm),
            "created": format_date("YYYY-MM-DDTHH:mm:ssZ", row.created),
            "description": row.description ? string(row.description) : NOTHING,
            "metrics": row.metrics ? parse_json(row.metrics) : NOTHING,
            "tags": row.tags ? parse_json(row.tags) : (),
            "file_size": int(row.artifact_size),
            "format": string(row.format ?? "qml"),
            "training_data_hash": row.training_data_hash
                ? string(row.training_data_hash) : NOTHING,
            "tenant_id": tenant_id,
            "config": row.config ? parse_json(row.config) : NOTHING,
        });
    }

    list<string> listTenants() {
        *list<auto> rows = models_table.selectRows({
            "columns": ("tenant_id",),
            "groupby": "tenant_id",
            "orderby": "tenant_id",
        });

        list<string> result();
        foreach hash<auto> row in (rows) {
            if (row.tenant_id.val()) {
                push result, row.tenant_id;
            }
        }
        return result;
    }

    nothing deleteTenantStorage(string tid) {
        if (tid !~ /^[a-zA-Z0-9_-]+$/) {
            throw "ML-REGISTRY-ERROR",
                sprintf("invalid tenant_id '%s' for deletion", tid);
        }

        # Get all models for this tenant
        *list<auto> model_rows = models_table.selectRows({
            "where": {"tenant_id": tid},
        });
        if (!model_rows) {
            throw "ML-REGISTRY-ERROR",
                sprintf("tenant '%s' not found in registry", tid);
        }

        # Batch delete: get all version IDs for this tenant, delete blobs, versions, models
        *list<auto> ver_rows = versions_table.selectRows({
            "columns": ("versionid",),
            "where": {"tenant_id": tid},
        });
        if (ver_rows) {
            list<int> vids = map int($1.versionid), ver_rows;
            blobs_table.del({"versionid": op_in(vids)});
        }
        versions_table.del({"tenant_id": tid});
        models_table.del({"tenant_id": tid});

        ds.commit();
    }

    #! Returns table definition hashes for creating the registry schema
    /** Use these definitions to create the required tables in your database.

        @param prefix the table name prefix (default "ml_")

        @return a hash of table_name → table_definition suitable for Schema module
    */
    static hash<auto> getTableDefinitions(*string prefix) {
        if (!prefix) {
            prefix = "ml_";
        }
        return {
            (prefix + "models"): {
                "columns": {
                    "modelid": {"qore_type": Type::Int, "notnull": True,
                        "comment": "Model primary key"},
                    "tenant_id": {"qore_type": Type::String, "size": 255,
                        "notnull": True, "default_value": "",
                        "comment": "Tenant identifier"},
                    "name": {"qore_type": Type::String, "size": 255, "notnull": True,
                        "comment": "Model name"},
                    "algorithm": {"qore_type": Type::String, "size": 255,
                        "comment": "Algorithm class name"},
                    "created": {"qore_type": Type::Date, "notnull": True,
                        "comment": "Creation timestamp"},
                    "modified": {"qore_type": Type::Date, "notnull": True,
                        "comment": "Last modification timestamp"},
                },
                "primary_key": {"name": "pk_" + prefix + "models",
                    "columns": ("modelid",)},
                "indexes": {
                    ("uk_" + prefix + "models_tn"): {
                        "columns": ("tenant_id", "name"),
                        "unique": True,
                    },
                },
            },
            (prefix + "versions"): {
                "columns": {
                    "versionid": {"qore_type": Type::Int, "notnull": True,
                        "comment": "Version primary key"},
                    "tenant_id": {"qore_type": Type::String, "size": 255,
                        "notnull": True, "default_value": "",
                        "comment": "Tenant identifier"},
                    "modelid": {"qore_type": Type::Int, "notnull": True,
                        "comment": "FK to models table"},
                    "version_id": {"qore_type": Type::String, "size": 255,
                        "notnull": True, "comment": "User-facing version identifier"},
                    "algorithm": {"qore_type": Type::String, "size": 255,
                        "comment": "Algorithm class name"},
                    "format": {"qore_type": Type::String, "size": 50,
                        "default_value": "qml", "comment": "Serialization format"},
                    "artifact_size": {"qore_type": Type::Int,
                        "comment": "Artifact size in bytes"},
                    "metrics": {"qore_type": "clob",
                        "comment": "Performance metrics JSON"},
                    "tags": {"qore_type": "clob", "default_value": "[]",
                        "comment": "Tags JSON array"},
                    "training_data_hash": {"qore_type": Type::String, "size": 255,
                        "comment": "Training data identifier"},
                    "description": {"qore_type": "clob",
                        "comment": "Version description"},
                    "config": {"qore_type": "clob",
                        "comment": "Version configuration JSON (e.g., tokenizer config)"},
                    "created": {"qore_type": Type::Date, "notnull": True,
                        "comment": "Creation timestamp"},
                },
                "primary_key": {"name": "pk_" + prefix + "versions",
                    "columns": ("versionid",)},
                "indexes": {
                    ("uk_" + prefix + "versions_tmv"): {
                        "columns": ("tenant_id", "modelid", "version_id"),
                        "unique": True,
                    },
                },
                "foreign_keys": {
                    ("fk_" + prefix + "versions_model"): {
                        "columns": ("modelid",),
                        "table": prefix + "models",
                        "columns_ref": ("modelid",),
                    },
                },
            },
            (prefix + "blobs"): {
                "columns": {
                    "blobid": {"qore_type": Type::Int, "notnull": True,
                        "comment": "Blob primary key"},
                    "tenant_id": {"qore_type": Type::String, "size": 255,
                        "notnull": True, "default_value": "",
                        "comment": "Tenant identifier"},
                    "versionid": {"qore_type": Type::Int, "notnull": True,
                        "comment": "FK to versions table"},
                    "data": {"qore_type": "blob", "notnull": True,
                        "comment": "Serialized model artifact"},
                },
                "primary_key": {"name": "pk_" + prefix + "blobs",
                    "columns": ("blobid",)},
                "indexes": {
                    ("uk_" + prefix + "blobs_tv"): {
                        "columns": ("tenant_id", "versionid"),
                        "unique": True,
                    },
                },
                "foreign_keys": {
                    ("fk_" + prefix + "blobs_version"): {
                        "columns": ("versionid",),
                        "table": prefix + "versions",
                        "columns_ref": ("versionid",),
                    },
                },
            },
        };
    }

    #! Private: gets a version row, throwing if not found
    private hash<auto> getVersionRow(string name, string version_id, string eff_tenant) {
        *hash<auto> model_row = models_table.selectRow({
            "where": {
                "tenant_id": eff_tenant,
                "name": name,
            },
        });
        if (!model_row) {
            throw "ML-REGISTRY-ERROR", sprintf("model '%s' not found%s", name,
                tenant_id ? sprintf(" (tenant '%s')", tenant_id) : "");
        }

        *hash<auto> ver_row = versions_table.selectRow({
            "where": {
                "tenant_id": eff_tenant,
                "modelid": model_row.modelid,
                "version_id": version_id,
            },
        });
        if (!ver_row) {
            throw "ML-REGISTRY-ERROR",
                sprintf("version '%s' not found for model '%s'", version_id, name);
        }

        return ver_row;
    }

    #! Private: deletes a single version and its blob
    private nothing deleteVersionByIdInternal(string name, string version_id,
            string eff_tenant) {
        *hash<auto> model_row = models_table.selectRow({
            "where": {
                "tenant_id": eff_tenant,
                "name": name,
            },
        });
        if (!model_row) {
            return;
        }

        *hash<auto> ver_row = versions_table.selectRow({
            "where": {
                "tenant_id": eff_tenant,
                "modelid": model_row.modelid,
                "version_id": version_id,
            },
        });
        if (!ver_row) {
            return;
        }

        blobs_table.del({"versionid": ver_row.versionid});
        versions_table.del({"versionid": ver_row.versionid});
    }
}

%endif # NoSqlUtil

} # namespace QoreModelRegistry
