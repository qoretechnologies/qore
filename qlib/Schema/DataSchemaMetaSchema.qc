# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DataSchema meta-schema definition

/** DataSchemaMetaSchema.qc Copyright 2026 Qore Technologies, s.r.o.

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

%modern

public namespace Schema {

#! JSON Schema (Draft 2020-12) for validating .ysm and .jsm data schema files
/** This schema defines the structure of pure data schema definitions used by the DataSchema class.

    @since Schema 1.5
*/
public const DataSchemaJsonSchema = {
    "$schema": "https://json-schema.org/draft/2020-12/schema",
    "title": "Qore Data Schema Definition",
    "description": "Schema definition for .ysm (YAML) and .jsm (JSON) data schema files",
    "type": "object",
    "required": ("schema",),
    "properties": {
        "schema": {
            "$ref": "#/$defs/schemaMetadata",
        },
        "tables": {
            "type": "object",
            "additionalProperties": {
                "$ref": "#/$defs/tableDefinition",
            },
        },
        "sequences": {
            "type": "object",
            "additionalProperties": {
                "type": "object",
            },
        },
        "types": {
            "type": "object",
        },
        "functions": {
            "type": "object",
        },
        "procedures": {
            "type": "object",
        },
        "packages": {
            "type": "object",
        },
        "materialized_views": {
            "type": "object",
        },
        "reference_data": {
            "$ref": "#/$defs/referenceData",
        },
        "migrations": {
            "type": "array",
            "items": {
                "$ref": "#/$defs/migration",
            },
        },
        "index_options": {
            "type": "object",
        },
        "column_options": {
            "type": "object",
        },
    },
    "$defs": {
        "schemaMetadata": {
            "type": "object",
            "required": ("name", "version"),
            "properties": {
                "name": {
                    "type": "string",
                    "description": "The schema name",
                },
                "version": {
                    "type": "string",
                    "description": "The schema version string",
                },
                "datasource": {
                    "type": "string",
                    "description": "Datasource alias used by oload to resolve the target database "
                        "connection; required by oload when aligning .ysm/.jsm schemas, ignored by "
                        "qschema (qschema takes the connection on the command line)",
                },
                "version_table": {
                    "type": "string",
                    "description": "Table containing the schema version",
                },
                "version_column": {
                    "type": "string",
                    "description": "Column containing the schema version",
                },
                "version_where": {
                    "type": "object",
                    "description": "Where clause hash to locate the version row",
                },
            },
        },
        "columnDefinition": {
            "type": "object",
            "properties": {
                "type": {
                    "type": "string",
                    "enum": ("int", "varchar", "char", "timestamp", "date", "number",
                        "blob", "clob", "binary", "float", "text", "bool",
                        "vector", "halfvec", "sparsevec"),
                    "description": "The column data type; may be omitted for pure driver-override "
                        "columns that set 'driver' with per-driver native_type/size/etc.",
                },
                "size": {
                    "type": "integer",
                    "minimum": 1,
                    "description": "Column size (for varchar, char, number, and pgvector types)",
                },
                "scale": {
                    "type": "integer",
                    "minimum": 0,
                    "description": "Numeric scale (for number type)",
                },
                "notnull": {
                    "type": "boolean",
                    "description": "NOT NULL constraint",
                },
                "default_value": {
                    "description": "Default value for the column",
                },
                "comment": {
                    "type": "string",
                    "description": "Column comment",
                },
                "driver": {
                    "type": "object",
                    "description": "Per-driver overrides merged into the base type (e.g. "
                        "{pgsql: {native_type: text}}); may be used alone when 'type' is omitted",
                },
                "populate": {
                    "description": "Expression or literal value to backfill existing rows when this "
                        "column is added to a non-empty table. Automates the add-nullable -> UPDATE "
                        "-> SET NOT NULL migration pattern. Can be: a scalar literal, a DPQL expression "
                        "hash ({exp: 'upr', args: [{field: 'name'}]}), a field reference shorthand "
                        "({field: 'other_col'}), or a per-driver SQL escape hatch "
                        "({sql: {pgsql: 'digest(name, sha256)'}}).",
                },
            },
        },
        "indexDefinition": {
            "type": "object",
            "required": ("columns",),
            "properties": {
                "columns": {
                    "type": "array",
                    "items": {"type": "string"},
                    "minItems": 1,
                },
                "unique": {
                    "type": "boolean",
                },
                "method": {
                    "type": "string",
                    "description": "Access method (e.g. btree, hash, gist, gin, brin, ivfflat, hnsw on "
                        "pgsql; btree or hash on mysql; bitmap or a domain indextype on oracle; "
                        "clustered / nonclustered / columnstore on mssql)",
                },
                "with": {
                    "type": "object",
                    "description": "Method-specific storage options rendered as WITH (k = v, ...) on "
                        "pgsql/mssql or PARAMETERS('k=v ...') on Oracle domain indexes",
                },
                "opclass": {
                    "type": "object",
                    "additionalProperties": {"type": "string"},
                    "description": "Per-column operator class (pgsql only): column name → opclass name",
                },
                "driver": {
                    "type": "object",
                    "description": "Per-driver overrides for any of the above keys",
                },
            },
        },
        "primaryKeyDefinition": {
            "type": "object",
            "required": ("name", "columns"),
            "properties": {
                "name": {
                    "type": "string",
                },
                "columns": {
                    "type": "array",
                    "items": {"type": "string"},
                    "minItems": 1,
                },
            },
        },
        "foreignConstraintDefinition": {
            "type": "object",
            "required": ("columns", "table"),
            "properties": {
                "columns": {
                    "type": "array",
                    "items": {"type": "string"},
                    "minItems": 1,
                },
                "table": {
                    "type": "string",
                    "description": "Target table name",
                },
                "target_columns": {
                    "type": "array",
                    "items": {"type": "string"},
                    "description": "Target columns (defaults to same as columns)",
                },
            },
        },
        "autoTriggerConfig": {
            "type": "object",
            "properties": {
                "created": {
                    "type": "boolean",
                    "description": "Auto-populate created timestamp on INSERT",
                },
                "modified": {
                    "type": "boolean",
                    "description": "Auto-populate modified timestamp on INSERT/UPDATE",
                },
                "sequence_columns": {
                    "type": "object",
                    "additionalProperties": {"type": "string"},
                    "description": "Map of column name to sequence name",
                },
                "insert_only": {
                    "type": "boolean",
                    "description": "If true, the generated trigger fires only on INSERT (useful for "
                        "append-only tables, or for tables that carry a modified column maintained "
                        "manually by the application)",
                },
            },
        },
        "partitionStrategy": {
            "type": "object",
            "required": ("columns",),
            "properties": {
                "method": {
                    "type": "string",
                    "enum": ("range", "list", "hash"),
                    "default": "range",
                    "description": "Portable partitioning method, subject to driver capabilities",
                },
                "columns": {
                    "type": "array",
                    "items": {"type": "string"},
                    "minItems": 1,
                    "description": "Partition key columns in declaration order",
                },
                "auto": {
                    "type": "object",
                    "description": "Driver-specific automatic partitioning strategy",
                },
                "driver": {
                    "type": "object",
                    "description": "Driver-specific partition strategy overrides",
                },
                "subpartition": {
                    "$ref": "#/$defs/partitionStrategy",
                    "description": "Recursive child partition strategy",
                },
            },
        },
        "partitionSpec": {
            "type": "object",
            "properties": {
                "name": {
                    "type": "string",
                    "description": "Canonical partition name",
                },
                "method": {
                    "type": "string",
                    "enum": ("range", "list", "hash"),
                    "default": "range",
                },
                "columns": {
                    "type": "array",
                    "items": {"type": "string"},
                    "minItems": 1,
                },
                "bound_from": {
                    "description": "Inclusive lower range bound as a typed value",
                },
                "bound_to": {
                    "description": "Exclusive upper range bound as a typed value",
                },
                "bound_from_sql": {
                    "type": "string",
                    "description": "Inclusive lower range bound as a SQL expression",
                },
                "bound_to_sql": {
                    "type": "string",
                    "description": "Exclusive upper range bound as a SQL expression",
                },
                "bound_sql": {
                    "type": "string",
                    "description": "Complete driver-native partition bound SQL",
                },
                "values": {
                    "type": "array",
                    "minItems": 1,
                    "description": "Typed values assigned to a list partition",
                },
                "values_sql": {
                    "type": "string",
                    "description": "Driver-native list values when typed values cannot be reconstructed",
                },
                "modulus": {
                    "type": "integer",
                    "minimum": 1,
                    "description": "Explicit hash bucket modulus",
                },
                "remainder": {
                    "type": "integer",
                    "minimum": 0,
                    "description": "Explicit hash bucket remainder",
                },
                "is_default": {
                    "type": "boolean",
                    "default": False,
                },
                "schema": {
                    "type": "string",
                },
                "relation_name": {
                    "type": "string",
                },
                "sql_name": {
                    "type": "string",
                },
                "driver": {
                    "type": "object",
                    "description": "Driver-specific partition overrides",
                },
                "subpartitions": {
                    "type": "object",
                    "additionalProperties": {
                        "$ref": "#/$defs/partitionSpec",
                    },
                    "description": "Nested partition definitions keyed by canonical child name",
                },
            },
        },
        "tableDefinition": {
            "type": "object",
            "required": ("columns",),
            "properties": {
                "columns": {
                    "type": "object",
                    "additionalProperties": {
                        "$ref": "#/$defs/columnDefinition",
                    },
                    "minProperties": 1,
                },
                "primary_key": {
                    "$ref": "#/$defs/primaryKeyDefinition",
                },
                "indexes": {
                    "type": "object",
                    "additionalProperties": {
                        "$ref": "#/$defs/indexDefinition",
                    },
                },
                "unique_constraints": {
                    "type": "object",
                    "additionalProperties": {
                        "type": "object",
                        "required": ("columns",),
                        "properties": {
                            "columns": {
                                "type": "array",
                                "items": {"type": "string"},
                                "minItems": 1,
                            },
                        },
                    },
                    "description": "Named unique constraints (separate from indexes; a unique "
                        "constraint may be backed by an index of the same name)",
                },
                "foreign_constraints": {
                    "type": "object",
                    "additionalProperties": {
                        "$ref": "#/$defs/foreignConstraintDefinition",
                    },
                },
                "partition_strategy": {
                    "$ref": "#/$defs/partitionStrategy",
                },
                "partitions": {
                    "type": "object",
                    "additionalProperties": {
                        "$ref": "#/$defs/partitionSpec",
                    },
                },
                "auto_triggers": {
                    "$ref": "#/$defs/autoTriggerConfig",
                },
                "triggers": {
                    "type": "object",
                    "description": "Raw trigger definitions (driver-specific)",
                },
                "driver": {
                    "type": "object",
                    "description": "Driver-specific overrides",
                },
            },
        },
        "referenceDataTable": {
            "type": "object",
            "required": ("columns", "rows"),
            "properties": {
                "columns": {
                    "type": "array",
                    "items": {"type": "string"},
                    "minItems": 1,
                },
                "rows": {
                    "type": "array",
                    "items": {
                        "type": "array",
                    },
                },
            },
        },
        "referenceData": {
            "type": "object",
            "properties": {
                "strict": {
                    "type": "object",
                    "additionalProperties": {
                        "$ref": "#/$defs/referenceDataTable",
                    },
                },
                "normal": {
                    "type": "object",
                    "additionalProperties": {
                        "$ref": "#/$defs/referenceDataTable",
                    },
                },
                "create_only": {
                    "type": "object",
                    "additionalProperties": {
                        "$ref": "#/$defs/referenceDataTable",
                    },
                },
                "insert_only": {
                    "type": "object",
                    "additionalProperties": {
                        "$ref": "#/$defs/referenceDataTable",
                    },
                },
            },
        },
        "tableReconciliationColumn": {
            "type": "object",
            "required": ("name", "type"),
            "additionalProperties": False,
            "properties": {
                "name": {"type": "string"},
                "type": {
                    "type": "string",
                    "enum": ("date", "number", "int", "float", "bool",
                        "string", "binary"),
                },
                "notnull": {"type": "boolean"},
                "primary_key": {"type": "boolean"},
                "missing_live": {"type": "boolean"},
                "size": {
                    "type": "integer",
                    "minimum": 1,
                    "maximum": 1000000,
                },
                "scale": {
                    "type": "integer",
                    "minimum": 0,
                    "maximum": 1000,
                },
                "default_value": {},
            },
        },
        "tableReconciliationMapping": {
            "type": "object",
            "required": ("target", "source_kind"),
            "additionalProperties": False,
            "properties": {
                "target": {"type": "string"},
                "source": {"type": "string"},
                "source_kind": {
                    "type": "string",
                    "enum": ("existing_column", "legacy_column",
                        "constant_null", "schema_default",
                        "empty_table_initialization"),
                },
                "conversion": {
                    "type": "string",
                    "enum": ("identity", "to_timestamp",
                        "numeric_compatible", "to_number",
                        "string_size_validation", "to_string",
                        "validated_cast", "none", "schema_default"),
                },
            },
        },
        "tableReconciliationIndex": {
            "type": "object",
            "required": ("name", "columns"),
            "additionalProperties": False,
            "properties": {
                "name": {"type": "string"},
                "columns": {
                    "type": "array",
                    "items": {"type": "string"},
                    "minItems": 1,
                },
                "unique": {"type": "boolean"},
            },
        },
        "tableReconciliationContract": {
            "type": "object",
            "required": ("contract_version", "migration_id", "table",
                "canonical_columns", "column_mappings", "backfill",
                "constraints_and_indexes"),
            "additionalProperties": False,
            "properties": {
                "contract_version": {
                    "type": "integer",
                    "enum": (1,),
                },
                "migration_id": {"type": "string"},
                "table": {"type": "string"},
                "canonical_columns": {
                    "type": "array",
                    "items": {
                        "$ref": "#/$defs/tableReconciliationColumn",
                    },
                    "minItems": 1,
                    "maxItems": 128,
                },
                "column_mappings": {
                    "type": "array",
                    "items": {
                        "$ref": "#/$defs/tableReconciliationMapping",
                    },
                    "minItems": 1,
                    "maxItems": 128,
                },
                "backfill": {
                    "type": "object",
                    "required": ("batch_size", "quarantine_table"),
                    "additionalProperties": False,
                    "properties": {
                        "batch_size": {
                            "type": "integer",
                            "minimum": 1,
                            "maximum": 10000,
                        },
                        "quarantine_table": {"type": "string"},
                    },
                },
                "constraints_and_indexes": {
                    "type": "object",
                    "required": ("primary_key",),
                    "additionalProperties": False,
                    "properties": {
                        "primary_key": {
                            "type": "object",
                            "required": ("columns",),
                            "additionalProperties": False,
                            "properties": {
                                "name": {"type": "string"},
                                "columns": {
                                    "type": "array",
                                    "items": {"type": "string"},
                                    "minItems": 1,
                                    "maxItems": 1,
                                },
                            },
                        },
                        "indexes": {
                            "type": "array",
                            "items": {
                                "$ref":
                                    "#/$defs/tableReconciliationIndex",
                            },
                            "maxItems": 64,
                        },
                    },
                },
            },
        },
        "migrationAction": {
            "type": "object",
            "required": ("action",),
            "properties": {
                "id": {
                    "type": "string",
                    "description": "Optional step identifier for dependency references",
                },
                "depends_on": {
                    "type": "array",
                    "items": {"type": "string"},
                    "description": "Step IDs that must complete before this step",
                },
                "min_source_version": {
                    "type": "string",
                    "description": "Minimum source (current) database version (inclusive) for this "
                        "step to execute; the step is skipped when the database is older",
                },
                "max_source_version": {
                    "type": "string",
                    "description": "Maximum source (current) database version (inclusive) for this "
                        "step to execute",
                },
                "action": {
                    "type": "string",
                    "enum": (
                        "update", "insert", "delete", "insert_from_select",
                        "add_column", "drop_column", "modify_column",
                        "rename_column", "rename_table",
                        "add_index", "drop_index",
                        "add_constraint", "drop_constraint",
                        "add_trigger", "drop_trigger",
                        "reconcile_table",
                        "sql", "script",
                    ),
                },
                "table": {"type": "string"},
                "column": {"type": "string"},
                "name": {"type": "string"},
                "from": {"type": "string"},
                "to": {"type": "string"},
                "type": {},
                "set": {"type": "object"},
                "where": {},
                "where_all": {
                    "type": "boolean",
                    "description": "Set to true to allow update/delete without a where clause (affects all rows)",
                },
                "row": {"type": "object"},
                "rows": {"type": "array"},
                "columns": {
                    "type": "array",
                    "items": {"type": "string"},
                },
                "source_table": {"type": "string"},
                "select": {"type": "object"},
                "unique": {"type": "boolean"},
                "auto_triggers": {
                    "$ref": "#/$defs/autoTriggerConfig",
                },
                "statements": {
                    "type": "array",
                    "items": {"type": "string"},
                },
                "driver": {
                    "type": "object",
                    "description": "Driver-specific statement overrides",
                },
                "file": {
                    "type": "string",
                    "description": "Path to a .qr migration script",
                },
                "source": {
                    "type": "string",
                    "description": "Trigger source SQL for add_trigger action",
                },
                "constraint_type": {
                    "type": "string",
                    "enum": ("unique", "foreign", "check"),
                    "description": "Constraint type for add_constraint action",
                },
                "target_table": {
                    "type": "string",
                    "description": "Target table for foreign key constraint or insert_from_select",
                },
                "target_columns": {
                    "type": "array",
                    "items": {"type": "string"},
                    "description": "Target columns for foreign key constraint",
                },
                "expression": {
                    "type": "string",
                    "description": "Check constraint expression",
                },
                "contract": {
                    "$ref": "#/$defs/tableReconciliationContract",
                    "description": "Closed data-preserving table reconciliation contract for "
                        "the reconcile_table action",
                },
            },
        },
        "migration": {
            "type": "object",
            "required": ("version",),
            "properties": {
                "version": {
                    "type": "string",
                    "description": "The schema version this migration applies to",
                },
                "description": {
                    "type": "string",
                },
                "min_source_version": {
                    "type": "string",
                    "description": "Minimum source (current) database version (inclusive) for this "
                        "migration to apply; databases older than this skip the migration (the "
                        "relevant objects are created fresh during DDL alignment)",
                },
                "max_source_version": {
                    "type": "string",
                    "description": "Maximum source (current) database version (inclusive) for this "
                        "migration to apply",
                },
                "pre_align": {
                    "type": "array",
                    "items": {
                        "$ref": "#/$defs/migrationAction",
                    },
                },
                "post_align": {
                    "type": "array",
                    "items": {
                        "$ref": "#/$defs/migrationAction",
                    },
                },
            },
        },
    },
};

} # namespace Schema
