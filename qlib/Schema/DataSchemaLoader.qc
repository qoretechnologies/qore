# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DataSchemaLoader class definition

/** DataSchemaLoader.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Column type mapping from .ysm/.jsm shorthand to GenericColumnInfo format
/** @since Schema 1.5
*/
public const ColumnTypeMap = {
    "int": {"qore_type": Type::Int},
    "varchar": {"qore_type": SqlUtil::VARCHAR},
    "char": {"qore_type": SqlUtil::CHAR},
    "timestamp": {"qore_type": Type::Date},
    "date": {"qore_type": Type::Date,
        "driver": {"oracle": {"native_type": "date"}, "pgsql": {"native_type": "date"}}},
    "number": {"qore_type": SqlUtil::NUMERIC},
    "blob": {"qore_type": SqlUtil::BLOB},
    "clob": {"qore_type": SqlUtil::CLOB},
    "binary": {"qore_type": Type::Binary},
    # pgvector extension types (PostgreSQL only)
    "vector": {"driver": {"pgsql": {"native_type": "vector"}}},
    "halfvec": {"driver": {"pgsql": {"native_type": "halfvec"}}},
    "sparsevec": {"driver": {"pgsql": {"native_type": "sparsevec"}}},
};

#! Loads and normalizes .ysm (YAML) and .jsm (JSON) schema definition files
/** This class parses data schema files, optionally validates them against the meta-schema,
    and normalizes the content into the hash format expected by AbstractVersionedSchema.

    @since Schema 1.5
*/
public class DataSchemaLoader {
    #! Loads a schema definition from a file
    /** @param file_path the path to the .ysm or .jsm file
        @param validate if True, validate against the meta-schema (requires JsonSchema or YamlSchema module)

        @return the normalized schema definition hash

        @throw DATA-SCHEMA-ERROR if the file cannot be loaded, parsed, or validated
    */
    static hash<auto> loadFile(string file_path, bool validate = True) {
        hash<auto> raw;

        if (file_path =~ /\.ysm$/i) {
%ifndef NoYaml
            raw = DataSchemaLoader::loadYamlFile(file_path);
%else
            throw "DATA-SCHEMA-ERROR", "cannot load .ysm file: the yaml module is not available";
%endif
        } else if (file_path =~ /\.jsm$/i) {
%ifndef NoJson
            raw = DataSchemaLoader::loadJsonFile(file_path);
%else
            throw "DATA-SCHEMA-ERROR", "cannot load .jsm file: the json module is not available";
%endif
        } else {
            throw "DATA-SCHEMA-ERROR", sprintf("unsupported file extension in %y; expected .ysm or .jsm",
                file_path);
        }

        if (validate) {
            DataSchemaLoader::validate(raw);
        }

        return DataSchemaLoader::normalize(raw);
    }

    #! Loads and normalizes a schema from a pre-parsed hash
    /** @param raw the raw schema hash (as parsed from YAML or JSON)
        @param validate if True, validate against the meta-schema

        @return the normalized schema definition hash
    */
    static hash<auto> loadHash(hash<auto> raw, bool validate = True) {
        if (validate) {
            DataSchemaLoader::validate(raw);
        }
        return DataSchemaLoader::normalize(raw);
    }

    #! Validates a raw schema hash against the meta-schema
    /** @param raw the schema hash to validate

        @throw DATA-SCHEMA-VALIDATION-ERROR if validation fails
    */
    static validate(hash<auto> raw) {
        # Basic structural validation (always available)
        if (!raw.hasKey("schema")) {
            throw "DATA-SCHEMA-VALIDATION-ERROR", "missing required 'schema' key";
        }
        if (!raw.schema.hasKey("name")) {
            throw "DATA-SCHEMA-VALIDATION-ERROR", "missing required 'schema.name' key";
        }
        if (!raw.schema.hasKey("version")) {
            throw "DATA-SCHEMA-VALIDATION-ERROR", "missing required 'schema.version' key";
        }

        # Full JSON Schema validation if available
%ifndef NoJson
        try {
            Qore::Json::JsonSchema js(DataSchemaJsonSchema);
            hash<auto> result = js.validateWithErrors(raw);
            if (!result.valid) {
                string errors = (map sprintf("%s: %s", $1.path, $1.message), result.errors).join("; ");
                throw "DATA-SCHEMA-VALIDATION-ERROR", sprintf("schema validation failed: %s", errors);
            }
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "DATA-SCHEMA-VALIDATION-ERROR") {
                rethrow;
            }
            # JsonSchema not available or other error — fall through to basic validation
        }
%endif

        # Validate tables have columns
        if (raw.tables) {
            foreach hash<auto> pair in (raw.tables.pairIterator()) {
                if (!pair.value.columns) {
                    throw "DATA-SCHEMA-VALIDATION-ERROR",
                        sprintf("table %y is missing required 'columns' key", pair.key);
                }
            }
        }
    }

    #! Normalizes a raw schema hash into the format expected by AbstractVersionedSchema
    /** Converts column type shorthand to GenericColumnInfo hashes and normalizes reference data format.

        @param raw the raw (validated) schema hash
        @return the normalized schema hash
    */
    static hash<auto> normalize(hash<auto> raw) {
        hash<auto!> result = {
            "schema": raw.schema,
        };

        # Normalize tables
        if (raw.tables) {
            hash<auto!> tables();
            foreach hash<auto> pair in (raw.tables.pairIterator()) {
                tables{pair.key} = DataSchemaLoader::normalizeTable(pair.key, pair.value);
            }
            result.tables = tables;
        }

        # Pass through simple sections
        foreach string key in (("sequences", "types", "functions", "procedures", "packages",
                "materialized_views", "index_options", "column_options")) {
            if (raw{key}) {
                result{key} = raw{key};
            }
        }

        # Normalize reference data
        if (raw.reference_data) {
            result.reference_data = DataSchemaLoader::normalizeReferenceData(raw.reference_data);
        }

        # Pass through migrations as-is (processed at execution time)
        if (raw.migrations) {
            result.migrations = raw.migrations;
        }

        return result;
    }

    #! Normalizes a single table definition
    /** Converts column type shorthand to GenericColumnInfo format.

        @param table_name the table name (for error messages)
        @param table_def the raw table definition hash
        @return the normalized table definition hash
    */
    static hash<auto> normalizeTable(string table_name, hash<auto> table_def) {
        hash<auto!> result();

        # Normalize columns
        hash<auto!> columns();
        foreach hash<auto> pair in (table_def.columns.pairIterator()) {
            columns{pair.key} = DataSchemaLoader::normalizeColumn(table_name, pair.key, pair.value);
        }
        result.columns = columns;

        # Pass through structural definitions
        if (table_def.primary_key) {
            result.primary_key = table_def.primary_key;
        }
        if (table_def.indexes) {
            result.indexes = table_def.indexes;
        }
        if (table_def.foreign_constraints) {
            result.foreign_constraints = table_def.foreign_constraints;
        }

        # Store auto_triggers for expansion at alignment time (driver-dependent)
        if (table_def.auto_triggers) {
            result.auto_triggers = table_def.auto_triggers;
        }

        # Pass through raw triggers
        if (table_def.triggers) {
            result.triggers = table_def.triggers;
        }

        # Pass through driver overrides
        if (table_def.driver) {
            result.driver = table_def.driver;
        }

        return result;
    }

    #! Normalizes a column definition from shorthand to GenericColumnInfo format
    /** @param table_name the table name (for error messages)
        @param col_name the column name (for error messages)
        @param col_def the raw column definition hash (with "type", "size", etc.)
        @return a GenericColumnInfo-compatible hash
    */
    static hash<auto> normalizeColumn(string table_name, string col_name, hash<auto> col_def) {
        *hash<auto> base_type = ColumnTypeMap{col_def.type};
        if (!base_type) {
            throw "DATA-SCHEMA-ERROR", sprintf("table %y column %y: unknown type %y; valid types: %y",
                table_name, col_name, col_def.type, ColumnTypeMap.keys());
        }

        # Build result as a generic hash to avoid type narrowing issues
        hash<auto!> result();
        result += base_type;

        # Add size if specified
        if (exists col_def.size) {
            result.size = col_def.size;
        }

        # Add scale if specified
        if (exists col_def.scale) {
            result.scale = col_def.scale;
        }

        # Add NOT NULL constraint
        if (col_def.notnull) {
            result.notnull = True;
        }

        # Add default value
        if (exists col_def.default_value) {
            result.default_value = col_def.default_value;
        }

        # Add comment
        if (col_def.comment.val()) {
            result.comment = col_def.comment;
        }

        return result;
    }

    #! Normalizes reference data from .ysm/.jsm format to Schema.qm format
    /** Converts from {columns: [...], rows: [[...], ...]} format to the list-of-lists format
        expected by AbstractSchema::getRows().

        @param ref_data the raw reference_data hash
        @return a hash with strict/normal/create_only/insert_only keys, each mapping
            table names to list-of-lists data
    */
    static hash<auto> normalizeReferenceData(hash<auto> ref_data) {
        hash<auto!> result();

        foreach string category in (("strict", "normal", "create_only", "insert_only")) {
            if (ref_data{category}) {
                hash<auto!> cat_data();
                foreach hash<auto> pair in (ref_data{category}.pairIterator()) {
                    # Convert {columns: [a, b], rows: [[1, 2], [3, 4]]}
                    # to ((a, b), (1, 2), (3, 4))  -- list-of-lists format
                    list<auto> data();
                    push data, pair.value.columns;
                    if (pair.value.rows) {
                        foreach auto row in (pair.value.rows) {
                            push data, row;
                        }
                    }
                    cat_data{pair.key} = data;
                }
                result{category} = cat_data;
            }
        }

        return result;
    }

    #! Expands auto_triggers in table definitions for a specific driver
    /** Called at alignment time when the driver is known.

        @param tables the normalized tables hash
        @param driver_name the database driver name
        @return the tables hash with auto_triggers expanded to driver-specific trigger definitions
    */
    static hash<auto> expandAutoTriggers(hash<auto> tables, string driver_name) {
        hash<auto> result;
        foreach hash<auto> pair in (tables.pairIterator()) {
            hash<auto> table_def = pair.value;
            if (table_def.auto_triggers) {
                # Pass PK columns for MSSQL trigger generation
                *list<string> pk_cols = table_def.primary_key.columns;
                hash<auto> auto_trigs = SqlUtil::generate_auto_triggers(driver_name, pair.key,
                    table_def.auto_triggers, False, pk_cols);

                # Merge generated triggers into existing triggers
                if (auto_trigs.triggers) {
                    table_def.triggers = (table_def.triggers ?? {}) + auto_trigs.triggers;
                }

                # Merge driver-specific definitions (e.g., PostgreSQL trigger functions)
                if (auto_trigs.driver) {
                    table_def.driver = (table_def.driver ?? {}) + auto_trigs.driver;
                }

                # Remove the auto_triggers key (it's been expanded)
                delete table_def.auto_triggers;
            }
            result{pair.key} = table_def;
        }
        return result;
    }

%ifndef NoYaml
    #! Loads a YAML file and returns the parsed hash
    static hash<auto> loadYamlFile(string file_path) {
        string content = ReadOnlyFile::readTextFile(file_path);
        auto parsed = parse_yaml(content);
        if (parsed.typeCode() != NT_HASH) {
            throw "DATA-SCHEMA-ERROR", sprintf("expected a hash at the top level of %y, got %y",
                file_path, parsed.type());
        }
        return parsed;
    }
%endif

%ifndef NoJson
    #! Loads a JSON file and returns the parsed hash
    static hash<auto> loadJsonFile(string file_path) {
        string content = ReadOnlyFile::readTextFile(file_path);
        auto parsed = parse_json(content);
        if (parsed.typeCode() != NT_HASH) {
            throw "DATA-SCHEMA-ERROR", sprintf("expected a hash at the top level of %y, got %y",
                file_path, parsed.type());
        }
        return parsed;
    }
%endif

    #! Evaluates DPQL expressions in reference data values
    /** Walks the reference data rows and evaluates any DPQL expression hashes
        (values with an "exp" key).

        @param data the reference data in list-of-lists format
        @return the data with expressions evaluated
    */
    static list<auto> evalReferenceDataExpressions(list<auto> data) {
        if (data.size() < 2) {
            return data;
        }

        # First element is column headers — skip
        list<auto> result();
        push result, data[0];

        for (int i = 1; i < data.size(); ++i) {
            list<auto> row = data[i];
            list<auto> eval_row();
            foreach auto val in (row) {
                if (val.typeCode() == NT_HASH && val.hasKey("exp")) {
                    # This is a DPQL expression — evaluate it
                    push eval_row, AbstractDataProvider::evalGenericExpression({},
                        <DataProvider::DataProviderExpression>{
                            "exp": val.exp,
                            "args": val.args ?? (),
                        });
                } else {
                    push eval_row, val;
                }
            }
            push result, eval_row;
        }

        return result;
    }
} # class DataSchemaLoader

} # namespace Schema
