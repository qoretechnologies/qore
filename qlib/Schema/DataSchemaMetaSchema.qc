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
            "required": ("type",),
            "properties": {
                "type": {
                    "type": "string",
                    "enum": ("int", "varchar", "char", "timestamp", "date", "number",
                        "blob", "clob", "binary"),
                    "description": "The column data type",
                },
                "size": {
                    "type": "integer",
                    "minimum": 1,
                    "description": "Column size (for varchar, char, number)",
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
                "foreign_constraints": {
                    "type": "object",
                    "additionalProperties": {
                        "$ref": "#/$defs/foreignConstraintDefinition",
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
                "action": {
                    "type": "string",
                    "enum": (
                        "update", "insert", "delete", "insert_from_select",
                        "add_column", "drop_column", "modify_column",
                        "rename_column", "rename_table",
                        "add_index", "drop_index",
                        "add_constraint", "drop_constraint",
                        "add_trigger", "drop_trigger",
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
