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
    "float": {"qore_type": Type::Float},
    "bool": {"qore_type": Type::Boolean},
    # "text" renders as the native TEXT type on drivers that support it (pgsql, mysql);
    # it falls back to CLOB on drivers that do not (e.g. oracle)
    "text": {"qore_type": SqlUtil::CLOB,
        "driver": {"pgsql": {"native_type": "text"}, "mysql": {"native_type": "text"}}},
    # pgvector extension types (PostgreSQL only)
    "vector": {"driver": {"pgsql": {"native_type": "vector"}}},
    "halfvec": {"driver": {"pgsql": {"native_type": "halfvec"}}},
    "sparsevec": {"driver": {"pgsql": {"native_type": "sparsevec"}}},
};

#! The severity of a problem found in a data schema definition that is rejected
/** @see @ref Schema::DataSchemaLoader::loadHashChecked() "DataSchemaLoader::loadHashChecked()"

    @since Schema 1.9
*/
public const DataSchemaIssueError = "error";

#! The severity of a problem found in a data schema definition that is accepted but likely wrong
/** @see @ref Schema::DataSchemaLoader::loadHashChecked() "DataSchemaLoader::loadHashChecked()"

    @since Schema 1.9
*/
public const DataSchemaIssueWarning = "warning";

#! A problem found in a data schema definition
/** @see @ref Schema::DataSchemaLoader::loadHashChecked() "DataSchemaLoader::loadHashChecked()"

    @since Schema 1.9
*/
public hashdecl DataSchemaIssueInfo {
    #! @ref Schema::DataSchemaIssueError "DataSchemaIssueError" or @ref Schema::DataSchemaIssueWarning "DataSchemaIssueWarning"
    string severity;

    #! The kind of the problem, one of the codes in @ref schema_data_schema_issues
    string code;

    #! The message; for an error that @ref Schema::DataSchemaLoader::loadHash() "loadHash()" raises with a code of
    #! the loader (\c DATA-SCHEMA-VALIDATION-ERROR or \c DATA-SCHEMA-ERROR), that of its exception; see
    #! @ref schema_data_schema_issues
    string message;

    #! The dotted path of the node of the definition that the problem concerns (ex: \c "tables.customers.columns.email.size")
    /** The keys of the hashes from the top of the definition, separated by \c "."; an element of a list is given by its
        index.  For a key that is missing, the path of the key; for a problem of the definition as a whole, no value.
    */
    *string path;
}

#! The result of @ref Schema::DataSchemaLoader::loadHashChecked() "DataSchemaLoader::loadHashChecked()"
/** @since Schema 1.9
*/
public hashdecl DataSchemaCheckInfo {
    #! The normalized definition, as @ref Schema::DataSchemaLoader::loadHash() "loadHash()" returns it; no value if an
    #! error was found
    *hash<auto> normalized;

    #! Every problem found, errors and warnings, in the order of the nodes in the definition
    list<hash<DataSchemaIssueInfo>> issues = ();
}

#! Marks a table entry whose shared definition cannot be resolved, in collect mode (not public)
/** No value parsed from YAML or JSON is an object, so a definition cannot hold one by accident.
*/
class DataSchemaUnresolvedEntry {
}

#! Reports the problems found in a data schema definition: raises an exception for the first error, or collects them
/** With \c collect @ref False, @ref error() raises the exception at once and warnings are dropped, so the checks of
    the loader behave as they always did; with @ref True, every problem is kept with its path.
*/
class DataSchemaIssueCollector {
    private {
        #! True to collect the problems, False to raise an exception for the first error
        bool collect;

        #! The problems found by the checks of the loader, in the order they were found
        list<hash<DataSchemaIssueInfo>> issues = ();

        #! The errors of the meta-schema, which are kept where no check of the loader found a problem at or below
        #! their node
        list<hash<DataSchemaIssueInfo>> meta_issues = ();

        #! The problems already kept, by code, path and message: validate() and normalize() check some keys twice
        hash<string, bool> seen = {};

        #! True once an error has been kept
        bool has_errors = False;
    }

    constructor(bool collect) {
        self.collect = collect;
    }

    #! Returns True if the problems are collected
    bool collecting() {
        return collect;
    }

    #! Reports an error: raises \a err with \a message, or keeps it
    error(string err, string code, *string path, string message) {
        if (!collect) {
            throw err, message;
        }
        add(DataSchemaIssueError, code, path, message);
    }

    #! Reports a warning: kept only when the problems are collected
    warning(string code, *string path, string message) {
        if (collect) {
            add(DataSchemaIssueWarning, code, path, message);
        }
    }

    #! Keeps an error of the meta-schema
    metaError(*string path, string message) {
        meta_issues += <DataSchemaIssueInfo>{
            "severity": DataSchemaIssueError,
            "code": "META-SCHEMA",
            "message": message,
            "path": path,
        };
    }

    #! Returns True if an error was found
    bool hasErrors() {
        if (has_errors) {
            return True;
        }
        return getMetaIssues().size() > 0;
    }

    #! Returns every problem found, in the order of their nodes in \a raw
    list<hash<DataSchemaIssueInfo>> getIssues(hash<auto> raw) {
        list<hash<DataSchemaIssueInfo>> rv = issues + getMetaIssues();
        # the position of each node in the definition, in document order
        hash<string, int> order = {};
        int n = 0;
        DataSchemaIssueCollector::numberNodes(\order, \n, NOTHING, raw);
        # each problem is placed at its node, or at its closest ancestor in the definition (a missing key); problems
        # of the same node keep the order in which they were found
        list<hash<auto>> keyed = ();
        int seq = 0;
        foreach hash<DataSchemaIssueInfo> issue in (rv) {
            keyed += {"pos": DataSchemaIssueCollector::getNodePosition(order, issue.path), "seq": seq++,
                "issue": issue};
        }
        keyed = sort(keyed, int sub (hash<auto> l, hash<auto> r) {
            int cmp = l.pos <=> r.pos;
            return cmp ? cmp : l.seq <=> r.seq;
        });
        return map $1.issue, keyed;
    }

    #! Returns the errors of the meta-schema that concern a node where no check of the loader found a problem
    private list<hash<DataSchemaIssueInfo>> getMetaIssues() {
        list<hash<DataSchemaIssueInfo>> rv = ();
        foreach hash<DataSchemaIssueInfo> meta in (meta_issues) {
            bool covered;
            foreach hash<DataSchemaIssueInfo> issue in (issues) {
                # a warning concerns what is accepted, so it leaves the error of the meta-schema
                if (issue.severity == DataSchemaIssueError
                        && DataSchemaIssueCollector::isAtOrBelow(issue.path, meta.path)) {
                    covered = True;
                    break;
                }
            }
            if (!covered) {
                rv += meta;
            }
        }
        return rv;
    }

    private add(string severity, string code, *string path, string message) {
        string key = sprintf("%s\x01%s\x01%s", code, path ?? "", message);
        if (seen{key}) {
            return;
        }
        seen{key} = True;
        issues += <DataSchemaIssueInfo>{
            "severity": severity,
            "code": code,
            "message": message,
            "path": path,
        };
        if (severity == DataSchemaIssueError) {
            has_errors = True;
        }
    }

    #! Returns the dotted path of \a key under \a path (no value: the top of the definition)
    static string joinPath(*string path, softstring key) {
        return path.val() ? path + "." + key : key;
    }

    #! Returns True if \a path is \a ancestor or a node below it; every node is below the definition (no path)
    private static bool isAtOrBelow(*string path, *string ancestor) {
        if (!ancestor.val()) {
            return True;
        }
        if (!path.val()) {
            return False;
        }
        return path == ancestor || path.substr(0, ancestor.size() + 1) == ancestor + ".";
    }

    #! Numbers the nodes of \a val in document order
    private static numberNodes(reference<hash<string, int>> order, reference<int> n, *string path, auto val) {
        if (exists path) {
            order{path} = n++;
        }
        switch (val.typeCode()) {
            case NT_HASH:
                foreach hash<auto> pair in (val.pairIterator()) {
                    DataSchemaIssueCollector::numberNodes(\order, \n, DataSchemaIssueCollector::joinPath(path,
                        pair.key), pair.value);
                }
                break;
            case NT_LIST:
                for (int i = 0; i < val.size(); ++i) {
                    DataSchemaIssueCollector::numberNodes(\order, \n, DataSchemaIssueCollector::joinPath(path, i),
                        val[i]);
                }
                break;
        }
    }

    #! Returns the position of the node of \a path in document order, or of its closest ancestor, or -1
    private static int getNodePosition(hash<string, int> order, *string path) {
        while (path.val()) {
            if (exists order{path}) {
                return order{path};
            }
            int i = path.rfind(".");
            if (i < 0) {
                break;
            }
            path = path.substr(0, i);
        }
        return -1;
    }
}

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
            raw = DataSchemaLoader::loadYamlFile(file_path);
        } else if (file_path =~ /\.jsm$/i) {
            raw = DataSchemaLoader::loadJsonFile(file_path);
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

    #! Checks and normalizes a schema from a pre-parsed hash, reporting every problem found with its location
    /** Runs the checks of @ref loadHash() with validation (the structure and the keys of the definition, the
        meta-schema, the shared definitions that the tables draw from with @ref SharedDefinitionKey, the column types
        and the primary key columns), but does not stop at the first error: every problem is returned with the
        dotted path of the node it concerns, so that an editor of a definition can show them all at once; see
        @ref schema_data_schema_issues.

        @par Example
        @code{.py}
hash<DataSchemaCheckInfo> info = DataSchemaLoader::loadHashChecked(parse_yaml(text));
foreach hash<DataSchemaIssueInfo> issue in (info.issues) {
    printf("%s %s at %s: %s\n", issue.severity, issue.code, issue.path ?? "<schema>", issue.message);
}
if (info.normalized) {
    DataSchema schema(info.normalized);
}
        @endcode

        @param raw the raw schema hash (as parsed from YAML or JSON)

        @return the normalized definition, if no error was found, and every problem found, in the order of their
        nodes in \a raw; the message of an error that @ref loadHash() raises with a code of the loader is that of its
        exception (see @ref schema_data_schema_issues for the others)

        @note warnings are reported only here: @ref loadHash() accepts what they concern

        @since Schema 1.9
    */
    static hash<DataSchemaCheckInfo> loadHashChecked(hash<auto> raw) {
        DataSchemaIssueCollector issues(True);
        # a value of a type that no check expects is reported as an error of the definition rather than raised
        try {
            DataSchemaLoader::validateIntern(raw, issues);
        } catch (hash<ExceptionInfo> ex) {
            issues.error(ex.err, "INVALID-DEFINITION", NOTHING, sprintf("%s: %s", ex.err, ex.desc));
        }
        *hash<auto> normalized;
        try {
            normalized = DataSchemaLoader::normalizeIntern(raw, issues);
        } catch (hash<ExceptionInfo> ex) {
            issues.error(ex.err, "INVALID-DEFINITION", NOTHING, sprintf("%s: %s", ex.err, ex.desc));
        }
        hash<DataSchemaCheckInfo> rv({"issues": issues.getIssues(raw)});
        if (!issues.hasErrors()) {
            rv.normalized = normalized;
        }
        return rv;
    }

    #! Validates a raw schema hash against the meta-schema
    /** @param raw the schema hash to validate

        @throw DATA-SCHEMA-VALIDATION-ERROR if validation fails; the top level, a table, its \c auto_triggers
        configuration, or the reference data carrying an unknown key fails validation with a message naming the
        key and, for a key that belongs elsewhere (such as \c sequence_columns, which belongs under
        \c auto_triggers), where it belongs
    */
    static validate(hash<auto> raw) {
        DataSchemaLoader::validateIntern(raw, new DataSchemaIssueCollector(False));
    }

    #! Validates a raw schema hash, reporting the problems found to \a issues
    private static validateIntern(hash<auto> raw, DataSchemaIssueCollector issues) {
        string err = "DATA-SCHEMA-VALIDATION-ERROR";
        # Basic structural validation (always available)
        if (!raw.hasKey("schema")) {
            issues.error(err, "MISSING-KEY", "schema", "missing required 'schema' key");
        } else if (issues.collecting() && raw.schema.typeCode() != NT_HASH) {
            issues.error(err, "INVALID-VALUE", "schema", sprintf("'schema' must be a hash with keys 'name' and "
                "'version'; got type %y", raw.schema.type()));
        } else {
            if (!raw.schema.hasKey("name")) {
                issues.error(err, "MISSING-KEY", "schema.name", "missing required 'schema.name' key");
            }
            if (!raw.schema.hasKey("version")) {
                issues.error(err, "MISSING-KEY", "schema.version", "missing required 'schema.version' key");
            }
        }

        # Unknown keys at the top level and in tables, auto_triggers and reference data are reported here, before
        # the meta-schema is applied, so the error names the key and where it belongs; the meta-schema rejects them
        # as well, but its error names only the enclosing object (e.g. "/tables: Additional property 't' found but
        # was invalid")
        DataSchemaLoader::checkTopLevelKeys(err, raw, issues);
        if (issues.collecting()) {
            DataSchemaLoader::checkSectionTypes(err, raw, issues);
        }
        if (raw.tables.typeCode() == NT_HASH) {
            foreach hash<auto> pair in (raw.tables.pairIterator()) {
                if (pair.value.typeCode() == NT_HASH) {
                    DataSchemaLoader::checkTableKeys(err, pair.key, pair.value, issues);
                }
            }
        }
        if (raw.reference_data.typeCode() == NT_HASH) {
            DataSchemaLoader::checkReferenceDataKeys(err, raw.reference_data, issues);
        }

        # Full JSON Schema validation if available
        *hash<auto> meta_result;
        try {
            Qore::Json::JsonSchema js(DataSchemaJsonSchema);
            hash<auto> result = js.validateWithErrors(raw);
            if (!result.valid) {
                if (issues.collecting()) {
                    # the errors are kept below, outside of this handler
                    meta_result = result;
                } else {
                    string errors = (map sprintf("%s: %s", $1.path, $1.message), result.errors).join("; ");
                    throw err, sprintf("schema validation failed: %s", errors);
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == err) {
                rethrow;
            }
            # JsonSchema not available or other error — fall through to basic validation
        }
        foreach hash<auto> e in (meta_result.errors) {
            issues.metaError(DataSchemaLoader::getMetaSchemaErrorPath(e), sprintf("schema validation failed: %s: %s",
                e.path, e.message));
        }

        # Validate tables have columns
        if (raw.tables && (!issues.collecting() || raw.tables.typeCode() == NT_HASH)) {
            foreach hash<auto> pair in (raw.tables.pairIterator()) {
                if (issues.collecting() && pair.value.typeCode() != NT_HASH) {
                    # reported above
                    continue;
                }
                # in collect mode, columns that are not a hash have been reported above
                if (!pair.value.columns && (!issues.collecting() || !exists pair.value.columns
                        || pair.value.columns.typeCode() == NT_HASH)) {
                    issues.error(err, "MISSING-KEY", sprintf("tables.%s.columns", pair.key),
                        sprintf("table %y is missing required 'columns' key", pair.key));
                }
            }
        }
    }

    #! Reports the sections of a definition that are not of the type that the loader reads (collect mode)
    /** In collect mode, such a section is reported here and then skipped, so that the other problems of the
        definition are still reported; @ref loadHash() raises an exception for it when it is read.
    */
    private static checkSectionTypes(string err, hash<auto> raw, DataSchemaIssueCollector issues) {
        foreach string key in (("tables", "columns", "indexes", "reference_data")) {
            if (exists raw{key} && raw{key}.typeCode() != NT_HASH) {
                issues.error(err, "INVALID-VALUE", key, sprintf("%y must be a hash; got type %y", key,
                    raw{key}.type()));
            }
        }
        if (raw.tables.typeCode() == NT_HASH) {
            foreach hash<auto> pair in (raw.tables.pairIterator()) {
                string path = "tables." + pair.key;
                if (pair.value.typeCode() != NT_HASH) {
                    issues.error(err, "INVALID-VALUE", path, sprintf("table %y must be a hash; got type %y",
                        pair.key, pair.value.type()));
                    continue;
                }
                if (exists pair.value.columns && pair.value.columns.typeCode() != NT_HASH) {
                    issues.error(err, "INVALID-VALUE", path + ".columns", sprintf("table %y: \"columns\" must be a "
                        "hash; got type %y", pair.key, pair.value.columns.type()));
                } else if (pair.value.columns) {
                    foreach hash<auto> col in (pair.value.columns.pairIterator()) {
                        if (col.value.typeCode() != NT_HASH) {
                            issues.error(err, "INVALID-VALUE", sprintf("%s.columns.%s", path, col.key),
                                sprintf("table %y column %y must be a hash; got type %y", pair.key, col.key,
                                    col.value.type()));
                        }
                    }
                }
                auto pk_cols = pair.value.primary_key.typeCode() == NT_HASH ? pair.value.primary_key.columns : NOTHING;
                if (exists pk_cols && pk_cols.typeCode() != NT_STRING) {
                    if (pk_cols.typeCode() != NT_LIST) {
                        issues.error(err, "INVALID-VALUE", path + ".primary_key.columns", sprintf("table %y: the "
                            "primary key columns must be a string or a list of strings; got type %y", pair.key,
                            pk_cols.type()));
                    } else {
                        for (int i = 0; i < pk_cols.size(); ++i) {
                            if (pk_cols[i].typeCode() != NT_STRING) {
                                issues.error(err, "INVALID-VALUE", sprintf("%s.primary_key.columns.%d", path, i),
                                    sprintf("table %y: a primary key column must be a string; got type %y",
                                        pair.key, pk_cols[i].type()));
                            }
                        }
                    }
                }
            }
        }
        if (raw.reference_data.typeCode() == NT_HASH) {
            foreach hash<auto> pair in (raw.reference_data.pairIterator()) {
                if (inlist(pair.key, ReferenceDataCategories) && pair.value.typeCode() != NT_HASH) {
                    issues.error(err, "INVALID-VALUE", "reference_data." + pair.key, sprintf("reference_data.%s "
                        "must be a hash; got type %y", pair.key, pair.value.type()));
                }
            }
        }
        foreach string section in (("columns", "indexes")) {
            if (raw{section}.typeCode() != NT_HASH) {
                continue;
            }
            foreach hash<auto> pair in (raw{section}.pairIterator()) {
                if (pair.value.typeCode() != NT_HASH) {
                    issues.error(err, "INVALID-VALUE", section + "." + pair.key, sprintf("shared %s %y must be a "
                        "hash; got type %y", section == "columns" ? "column" : "index", pair.key, pair.value.type()));
                }
            }
        }
    }

    #! Returns the dotted path of the node that an error of the meta-schema concerns
    /** The meta-schema gives a JSON pointer; an additional property that is not valid is reported at its parent,
        and is given here at the property itself.
    */
    private static *string getMetaSchemaErrorPath(hash<auto> e) {
        list<string> parts = ();
        foreach string part in (e.path.split("/")) {
            if (part.val()) {
                # JSON pointer escapes (RFC 6901)
                parts += replace(replace(part, "~1", "/"), "~0", "~");
            }
        }
        # "Additional property 'x' found but was invalid." is reported at the object that holds x; "... not allowed by
        # schema." at x itself
        if (e.keyword == "additionalProperties"
                && (*list<*string> m = (e.message =~ x/^Additional property '(.*)' found but was invalid/)) && m[0]) {
            parts += m[0];
        }
        return parts ? parts.join(".") : NOTHING;
    }

    #! Normalizes a raw schema hash into the format expected by AbstractVersionedSchema
    /** Converts column type shorthand to GenericColumnInfo hashes and normalizes reference data format.

        @param raw the raw (validated) schema hash
        @return the normalized schema hash

        @throw DATA-SCHEMA-ERROR \a raw carries a key that is not in
        @ref Schema::DataSchemaLoader::TopLevelKeys "TopLevelKeys", or a table, its \c auto_triggers, a shorthand
        column, or the reference data carries an unknown key; the message names the key and, for a key that
        belongs elsewhere (such as a top-level \c auto_triggers, which is declared per table), where it belongs
    */
    static hash<auto> normalize(hash<auto> raw) {
        return DataSchemaLoader::normalizeIntern(raw, new DataSchemaIssueCollector(False));
    }

    #! Normalizes a raw schema hash, reporting the problems found to \a issues
    private static hash<auto> normalizeIntern(hash<auto> raw, DataSchemaIssueCollector issues) {
        # every top-level key is either copied or consumed below; any other would be dropped, so it is rejected
        DataSchemaLoader::checkTopLevelKeys("DATA-SCHEMA-ERROR", raw, issues);

        hash<auto!> result = {
            "schema": raw.schema,
        };

        # Normalize tables, resolving shared column and index definitions first
        bool collecting = issues.collecting();
        if (raw.tables && (!collecting || raw.tables.typeCode() == NT_HASH)) {
            # in collect mode, a section of a type that the loader does not read has been reported in validation
            auto shared_columns = raw.columns;
            auto shared_indexes = raw.indexes;
            # a shared section that is not a hash has been reported: its references are not reported again
            bool columns_invalid = False;
            bool indexes_invalid = False;
            if (collecting) {
                if (exists shared_columns && shared_columns.typeCode() != NT_HASH) {
                    remove shared_columns;
                    columns_invalid = True;
                }
                if (exists shared_indexes && shared_indexes.typeCode() != NT_HASH) {
                    remove shared_indexes;
                    indexes_invalid = True;
                }
            }
            hash<auto!> tables();
            foreach hash<auto> pair in (raw.tables.pairIterator()) {
                if (collecting && pair.value.typeCode() != NT_HASH) {
                    # not a table definition: reported in validation
                    continue;
                }
                # without collecting, the public methods are called, as before, so that a value of a type that
                # they do not take raises the same exception
                if (!collecting) {
                    hash<auto> table_def = DataSchemaLoader::resolveSharedDefinitions(pair.key, pair.value,
                        shared_columns, shared_indexes);
                    tables{pair.key} = DataSchemaLoader::normalizeTable(pair.key, table_def);
                    continue;
                }
                hash<auto> table_def = DataSchemaLoader::resolveSharedDefinitionsIntern(pair.key, pair.value,
                    shared_columns, shared_indexes, issues, columns_invalid, indexes_invalid);
                tables{pair.key} = DataSchemaLoader::normalizeTableIntern(pair.key, table_def, issues);
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
        if (raw.reference_data && (!collecting || raw.reference_data.typeCode() == NT_HASH)) {
            result.reference_data = collecting
                ? DataSchemaLoader::normalizeReferenceDataIntern(raw.reference_data, issues)
                : DataSchemaLoader::normalizeReferenceData(raw.reference_data);
        }

        # Pass through migrations as-is (processed at execution time)
        if (raw.migrations) {
            result.migrations = raw.migrations;
        }

        return result;
    }

    #! The key by which a table entry draws from a shared definition
    public const SharedDefinitionKey = "$use";

    #! Resolves a table's shared column and index references
    /** A schema may declare <tt>columns</tt> and <tt>indexes</tt> at the top level: definitions named once and
        drawn from by any table, so that a column used in nine tables is described in one place. A
        table's entry draws from one with @ref SharedDefinitionKey, and any key it sets alongside
        overrides the shared value:

        @code{.yaml}
    columns:
      audit_ts:
        type: timestamp
        notnull: true

    tables:
      orders:
        columns:
          created:
            $use: audit_ts
          updated:
            $use: audit_ts
            notnull: false      # this one may be null
        @endcode

        Resolution happens here, before normalization, so everything downstream — the normalized
        hash, @ref Schema::DataSchema "DataSchema", the diff and the alignment — sees fully-expanded
        tables and needs to know nothing about sharing. The shared sections are an authoring
        convenience and do not appear in the normalized schema.

        @param table_name the table being resolved (for error messages)
        @param table_def the raw table definition
        @param shared_columns the schema's shared column definitions, if any
        @param shared_indexes the schema's shared index definitions, if any

        @return the table definition with every reference expanded

        @throw DATA-SCHEMA-ERROR if a reference names a definition that does not exist
    */
    static hash<auto> resolveSharedDefinitions(string table_name, hash<auto> table_def,
            *hash<auto> shared_columns, *hash<auto> shared_indexes) {
        return DataSchemaLoader::resolveSharedDefinitionsIntern(table_name, table_def, shared_columns, shared_indexes,
            new DataSchemaIssueCollector(False), False, False);
    }

    #! Resolves a table's shared column and index references, reporting the problems found to \a issues
    private static hash<auto> resolveSharedDefinitionsIntern(string table_name, hash<auto> table_def,
            *hash<auto> shared_columns, *hash<auto> shared_indexes, DataSchemaIssueCollector issues,
            bool columns_invalid, bool indexes_invalid) {
        # the resolved sections are written into a fresh wide hash rather than back into the table
        # passed in: a schema built as a Qore hash literal rather than parsed from YAML or JSON
        # infers a narrow value type for the table (a table whose only column is written as the
        # bare {"$use": name} form gives hash<string, hash<string, hash<string, string>>>), and a
        # resolved column carrying a non-string value from the shared definition -- notnull: True,
        # a numeric size -- cannot be stored in it
        hash<auto!> result();
        foreach hash<auto> pair in (table_def.pairIterator()) {
            result{pair.key} = pair.value;
        }
        if (exists table_def.columns) {
            result.columns = DataSchemaLoader::resolveSharedSection(table_name, "column",
                table_def.columns, shared_columns, issues, columns_invalid);
        }
        if (exists table_def.indexes) {
            result.indexes = DataSchemaLoader::resolveSharedSection(table_name, "index",
                table_def.indexes, shared_indexes, issues, indexes_invalid);
        }
        return result;
    }

    #! Resolves every reference in one named-entry section of a table
    private static auto resolveSharedSection(string table_name, string what, auto entries,
            *hash<auto> shared, DataSchemaIssueCollector issues, bool shared_invalid) {
        if (entries.typeCode() != NT_HASH) {
            return entries;
        }
        hash<auto!> result();
        foreach hash<auto> pair in (entries.pairIterator()) {
            bool unresolved = False;
            auto entry = DataSchemaLoader::resolveSharedEntry(table_name, what, pair.key, pair.value, shared, issues,
                shared_invalid, \unresolved);
            # an entry whose shared definition cannot be resolved (only in collect mode: the error is raised
            # otherwise) has been reported, and is marked so that normalization neither normalizes it nor reports
            # what its missing definition holds
            result{pair.key} = unresolved ? new DataSchemaUnresolvedEntry() : entry;
        }
        return result;
    }

    #! Resolves one entry against the shared definitions, if it draws from them
    private static auto resolveSharedEntry(string table_name, string what, string name, auto def,
            *hash<auto> shared, DataSchemaIssueCollector issues, bool shared_invalid, reference<bool> unresolved) {
        if (def.typeCode() != NT_HASH || !exists def{DataSchemaLoader::SharedDefinitionKey}) {
            return def;
        }
        string path = sprintf("tables.%s.%s.%s.%s", table_name, what == "column" ? "columns" : "indexes", name,
            DataSchemaLoader::SharedDefinitionKey);
        if (issues.collecting()) {
            # a reference or a shared definition of a type that is not read is reported rather than raised
            auto use = def{DataSchemaLoader::SharedDefinitionKey};
            if (use.typeCode() != NT_STRING) {
                issues.error("DATA-SCHEMA-ERROR", "INVALID-VALUE", path, sprintf("table %y %s %y: %y must name a "
                    "shared %s; got type %y", table_name, what, name, DataSchemaLoader::SharedDefinitionKey, what,
                    use.type()));
                unresolved = True;
                return NOTHING;
            }
            if (shared_invalid || (shared && shared.hasKey(use) && shared{use}.typeCode() != NT_HASH)) {
                # the shared section or definition is not a hash: reported in validation
                unresolved = True;
                return NOTHING;
            }
        }
        string ref = def{DataSchemaLoader::SharedDefinitionKey};
        # indexing NOTHING yields NOTHING, so a schema with no shared section at all reaches the
        # same error as one that simply does not declare this name
        *hash<auto> base = shared{ref};
        if (!exists base) {
            issues.error("DATA-SCHEMA-ERROR", "UNKNOWN-SHARED-DEFINITION", path, sprintf("table %y %s %y draws from "
                "unknown shared %s %y; the schema declares %y", table_name, what, name, what, ref,
                shared ? shared.keys() : ()));
            unresolved = True;
            return NOTHING;
        }
        # Resolution is deliberately one level deep, and that is enforced rather than documented:
        # a shared definition that itself draws from another would otherwise merge the unresolved
        # key straight through, and an index carries unknown keys verbatim into SqlUtil.
        if (exists base{DataSchemaLoader::SharedDefinitionKey}) {
            issues.error("DATA-SCHEMA-ERROR", "NESTED-SHARED-DEFINITION", path, sprintf("table %y %s %y draws from "
                "shared %s %y, which itself draws from %y; shared definitions are not resolved recursively",
                table_name, what, name, what, ref, base{DataSchemaLoader::SharedDefinitionKey}));
            unresolved = True;
            return NOTHING;
        }
        # the shared definition first, then the entry's own keys, so a local key wins
        return base + (def - DataSchemaLoader::SharedDefinitionKey);
    }

    #! The keys the top level of a data schema may carry
    /** Any other key is rejected by @ref Schema::DataSchemaLoader::validate() "validate()" and
        @ref Schema::DataSchemaLoader::normalize() "normalize()" rather than dropped; a key that belongs in each
        table, such as \c constraints or \c auto_triggers, is rejected with a message saying where it belongs.

        @since Schema 1.9
    */
    public const TopLevelKeys = ("schema", "tables", "sequences", "types", "functions", "procedures", "packages",
        "materialized_views", "reference_data", "migrations", "index_options", "column_options", "columns",
        "indexes");

    #! The keys a table definition may carry
    /** Any other key is rejected by @ref Schema::DataSchemaLoader::normalizeTable() "normalizeTable()" rather
        than dropped, as a dropped key silently loses what it describes.

        @since Schema 1.9
    */
    public const TableKeys = ("columns", "primary_key", "indexes", "unique_constraints", "foreign_constraints",
        "partition_strategy", "partitions", "auto_triggers", "triggers", "driver", "comment");

    #! The keys a table's \c auto_triggers configuration may carry
    /** @since Schema 1.9
    */
    public const AutoTriggerKeys = ("created", "modified", "sequence_columns", "insert_only");

    #! The keys a column may carry when it is written with the \c type shorthand or as a pure driver override
    /** A column written in the GenericColumnInfo form (with \c qore_type or \c native_type and no \c type) is kept
        verbatim and is validated by SqlUtil when the table is created.

        @since Schema 1.9
    */
    public const ColumnShorthandKeys = ("type", "size", "scale", "notnull", "default_value", "populate", "comment",
        "driver");

    #! The keys of a reference data section
    /** @since Schema 1.9
    */
    public const ReferenceDataCategories = ("strict", "normal", "create_only", "insert_only");

    #! The keys of one table's reference data
    /** @since Schema 1.9
    */
    public const ReferenceDataTableKeys = ("columns", "rows");

    #! Throws \a err for the first key of \a def that is not in \a valid
    /** @param err the exception code to throw
        @param what the object being checked, as the error message names it
        @param def the definition whose keys are checked
        @param valid the keys \a def may carry
        @param path the dotted path of \a def in the definition
        @param issues where the problems are reported
        @param hints optional hints keyed by unknown key, each saying where the key belongs

        @throw err \a def carries a key that is not in \a valid
    */
    private static checkKeys(string err, string what, hash<auto> def, list<string> valid, *string path,
            DataSchemaIssueCollector issues, *hash<string, string> hints) {
        foreach string key in (keys def) {
            if (inlist(key, valid)) {
                continue;
            }
            *string hint = hints{key};
            issues.error(err, "UNKNOWN-KEY", DataSchemaIssueCollector::joinPath(path, key), sprintf("%s: unknown key %y%s; "
                "valid keys: %y", what, key, hint ? "; " + hint : "", valid));
        }
    }

    #! Checks that a table and its \c auto_triggers configuration carry only known keys
    /** @param err the exception code to throw
        @param table_name the table name (for error messages)
        @param table_def the table definition

        @throw err the table carries a key that is not in @ref Schema::DataSchemaLoader::TableKeys "TableKeys", or
        its \c auto_triggers configuration is not a hash or carries a key that is not in
        @ref Schema::DataSchemaLoader::AutoTriggerKeys "AutoTriggerKeys"
    */
    private static checkTableKeys(string err, string table_name, hash<auto> table_def,
            DataSchemaIssueCollector issues) {
        string path = "tables." + table_name;
        DataSchemaLoader::checkKeys(err, sprintf("table %y", table_name), table_def, TableKeys, path, issues,
            DataSchemaLoader::getTableKeyHints(table_name));
        if (exists table_def.auto_triggers) {
            if (table_def.auto_triggers.typeCode() != NT_HASH) {
                issues.error(err, "INVALID-VALUE", path + ".auto_triggers", sprintf("table %y: \"auto_triggers\" "
                    "must be a hash with keys %y; got type %y", table_name, AutoTriggerKeys,
                    table_def.auto_triggers.type()));
            } else {
                DataSchemaLoader::checkKeys(err, sprintf("table %y auto_triggers", table_name),
                    table_def.auto_triggers, AutoTriggerKeys, path + ".auto_triggers", issues);
            }
        }
    }

    #! Checks that reference data carries only known categories, and each table in them only known keys
    /** @param err the exception code to throw
        @param ref_data the raw reference data

        @throw err \a ref_data carries a key that is not in
        @ref Schema::DataSchemaLoader::ReferenceDataCategories "ReferenceDataCategories", or a table's reference
        data carries a key that is not in
        @ref Schema::DataSchemaLoader::ReferenceDataTableKeys "ReferenceDataTableKeys"
    */
    private static checkReferenceDataKeys(string err, hash<auto> ref_data, DataSchemaIssueCollector issues) {
        DataSchemaLoader::checkKeys(err, "reference_data", ref_data, ReferenceDataCategories, "reference_data",
            issues);
        foreach string category in (ReferenceDataCategories) {
            if (ref_data{category}.typeCode() != NT_HASH) {
                continue;
            }
            foreach hash<auto> pair in (ref_data{category}.pairIterator()) {
                if (pair.value.typeCode() == NT_HASH) {
                    DataSchemaLoader::checkKeys(err, sprintf("reference_data.%s table %y", category, pair.key),
                        pair.value, ReferenceDataTableKeys, sprintf("reference_data.%s.%s", category, pair.key),
                        issues);
                }
            }
        }
    }

    #! Checks that the top level of a data schema carries only known keys
    /** @param err the exception code to throw
        @param raw the raw schema

        @throw err \a raw carries a key that is not in @ref Schema::DataSchemaLoader::TopLevelKeys "TopLevelKeys"
    */
    private static checkTopLevelKeys(string err, hash<auto> raw, DataSchemaIssueCollector issues) {
        DataSchemaLoader::checkKeys(err, "data schema", raw, TopLevelKeys, NOTHING, issues,
            DataSchemaLoader::getTopLevelKeyHints());
    }

    #! Returns hints for the unknown top-level keys of a schema that belong in each table
    private static hash<string, string> getTopLevelKeyHints() {
        hash<string, string> hints();
        foreach string key in (TableKeys) {
            if (!inlist(key, TopLevelKeys)) {
                hints{key} = sprintf("%y is declared per table (tables.<table>.%s)", key, key);
            }
        }
        foreach string key in (AutoTriggerKeys) {
            hints{key} = sprintf("%y is declared per table under \"auto_triggers\" "
                "(tables.<table>.auto_triggers.%s)", key, key);
        }
        hints.constraints = "constraints are declared per table with \"primary_key\", \"unique_constraints\" and "
            "\"foreign_constraints\" (tables.<table>.primary_key, tables.<table>.unique_constraints, "
            "tables.<table>.foreign_constraints)";
        return hints;
    }

    #! Returns hints for the unknown keys of a table, saying where each belongs
    private static hash<string, string> getTableKeyHints(string table_name) {
        hash<string, string> hints();
        foreach string key in (AutoTriggerKeys) {
            hints{key} = sprintf("%y belongs under \"auto_triggers\" (tables.%s.auto_triggers.%s)", key,
                table_name, key);
        }
        hints.constraints = "constraints are declared with \"primary_key\", \"unique_constraints\" and "
            "\"foreign_constraints\"";
        return hints;
    }

    #! Normalizes a single table definition
    /** Converts column type shorthand to GenericColumnInfo format.

        @param table_name the table name (for error messages)
        @param table_def the raw table definition hash
        @return the normalized table definition hash

        @throw DATA-SCHEMA-ERROR the table carries a key that is not in
        @ref Schema::DataSchemaLoader::TableKeys "TableKeys", or its \c auto_triggers configuration is not a hash
        or carries a key that is not in @ref Schema::DataSchemaLoader::AutoTriggerKeys "AutoTriggerKeys"; the
        message names the key and, for a key that belongs elsewhere (such as \c sequence_columns, which belongs
        under \c auto_triggers), where it belongs
    */
    static hash<auto> normalizeTable(string table_name, hash<auto> table_def) {
        return DataSchemaLoader::normalizeTableIntern(table_name, table_def, new DataSchemaIssueCollector(False));
    }

    #! Normalizes a single table definition, reporting the problems found to \a issues
    private static hash<auto> normalizeTableIntern(string table_name, hash<auto> table_def,
            DataSchemaIssueCollector issues) {
        # an unknown key is rejected rather than dropped: normalization copies only the keys it knows, so a
        # misplaced key -- e.g. sequence_columns on the table instead of under auto_triggers -- would otherwise be
        # lost without a trace, and with it what it describes
        DataSchemaLoader::checkTableKeys("DATA-SCHEMA-ERROR", table_name, table_def, issues);

        hash<auto!> result();

        # Normalize columns
        bool collecting = issues.collecting();
        hash<auto!> columns();
        # in collect mode, the columns that are not normalized as they have been reported already: the primary key
        # does not report them again as unknown columns
        hash<string, bool> skipped();
        # a table without columns has been reported in validation, and its primary key is not checked then
        bool skip_pk = collecting && (table_def.columns.typeCode() != NT_HASH || !table_def.columns);
        if (!skip_pk) {
            foreach hash<auto> pair in (table_def.columns.pairIterator()) {
                if (collecting && (pair.value.typeCode() != NT_HASH || pair.value instanceof DataSchemaUnresolvedEntry)) {
                    # not a column definition (reported by the meta-schema, if it is available), or one whose
                    # shared definition cannot be resolved (reported)
                    skipped{pair.key} = True;
                    continue;
                }
                columns{pair.key} = collecting
                    ? DataSchemaLoader::normalizeColumnIntern(table_name, pair.key, pair.value, issues)
                    : DataSchemaLoader::normalizeColumn(table_name, pair.key, pair.value);
            }
        }

        # Pass through structural definitions
        if (table_def.primary_key) {
            result.primary_key = table_def.primary_key;
            auto pk_cols = table_def.primary_key.columns;
            if (skip_pk) {
                # no columns to check against
            } else if (pk_cols.typeCode() == NT_STRING) {
                DataSchemaLoader::normalizePrimaryKeyColumn(table_name, \columns, pk_cols,
                    sprintf("tables.%s.primary_key.columns", table_name), issues, skipped);
            } else if (!collecting) {
                int i = 0;
                foreach string pk_col in (pk_cols) {
                    DataSchemaLoader::normalizePrimaryKeyColumn(table_name, \columns, pk_col,
                        sprintf("tables.%s.primary_key.columns.%d", table_name, i++), issues, skipped);
                }
            } else if (pk_cols.typeCode() == NT_LIST) {
                for (int i = 0; i < pk_cols.size(); ++i) {
                    # an element that is not a string has been reported in validation
                    if (pk_cols[i].typeCode() == NT_STRING) {
                        DataSchemaLoader::normalizePrimaryKeyColumn(table_name, \columns, pk_cols[i],
                            sprintf("tables.%s.primary_key.columns.%d", table_name, i), issues, skipped);
                    }
                }
            }
        }
        result.columns = columns;
        if (table_def.indexes) {
            result.indexes = table_def.indexes;
        }
        if (table_def.unique_constraints) {
            result.unique_constraints = table_def.unique_constraints;
        }
        if (table_def.foreign_constraints) {
            result.foreign_constraints = table_def.foreign_constraints;
        }
        # issue #1483: the comment of the table
        if (exists table_def.comment) {
            result.comment = table_def.comment;
        }
        if (table_def.partition_strategy) {
            result.partition_strategy = table_def.partition_strategy;
        }
        if (table_def.partitions) {
            # Partition-bound coercion is datasource-specific and is therefore performed by AbstractTable after
            # driver overrides have been applied and the effective partition columns are known.
            result.partitions = table_def.partitions;
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

    private static normalizePrimaryKeyColumn(string table_name, reference<hash<auto!>> columns, string col_name,
            string path, DataSchemaIssueCollector issues, hash<string, bool> skipped) {
        if (skipped{col_name}) {
            return;
        }
        if (!columns.hasKey(col_name)) {
            issues.error("DATA-SCHEMA-ERROR", "UNKNOWN-PRIMARY-KEY-COLUMN", path, sprintf("table %y primary key "
                "references unknown column %y", table_name, col_name));
            return;
        }
        hash<auto!> column;
        foreach hash<auto> pair in (columns{col_name}.pairIterator()) {
            column{pair.key} = pair.value;
        }
        column.notnull = True;
        columns{col_name} = column;
    }

    #! Normalizes a column definition from shorthand to GenericColumnInfo format
    /** @param table_name the table name (for error messages)
        @param col_name the column name (for error messages)
        @param col_def the raw column definition hash (with "type", "size", etc.)
        @return a GenericColumnInfo-compatible hash

        The column definition may use either of two forms:
        - the \c type shorthand (e.g. \c {type: varchar, size: 240}) — looked up in
          @ref Schema::ColumnTypeMap "ColumnTypeMap"
        - an already-normalized GenericColumnInfo hash using \c qore_type or \c native_type, as emitted by
          SchemaReverse and AbstractTable::getDescriptionHash()
        - a pure driver-override form (e.g. \c {driver: {pgsql: {native_type: vector, size: 1024}}})
          for columns that only exist on one driver

        Any user-provided \c driver hash is recursively merged on top of the base type's driver
        block so the two forms can be combined (e.g. \c {type: int, driver: {pgsql: {identity: true}}}).

        @throw DATA-SCHEMA-ERROR the column has an unknown type, sets neither a type nor a driver override, or is
        written with the \c type shorthand or as a pure driver override and carries a key that is not in
        @ref Schema::DataSchemaLoader::ColumnShorthandKeys "ColumnShorthandKeys"
    */
    static hash<auto> normalizeColumn(string table_name, string col_name, hash<auto> col_def) {
        return DataSchemaLoader::normalizeColumnIntern(table_name, col_name, col_def,
            new DataSchemaIssueCollector(False));
    }

    #! Normalizes a column definition, reporting the problems found to \a issues
    private static hash<auto> normalizeColumnIntern(string table_name, string col_name, hash<auto> col_def,
            DataSchemaIssueCollector issues) {
        string path = sprintf("tables.%s.columns.%s", table_name, col_name);
        # Build result as a generic hash to avoid type narrowing issues
        hash<auto!> result();

        # the shorthand and pure driver-override forms are rebuilt from the keys they know, so any other key
        # would be dropped: it is rejected instead
        if (col_def.type.val() || !(exists col_def.qore_type || col_def.native_type)) {
            hash<string, string> hints();
            foreach string key in (("qore_type", "native_type")) {
                hints{key} = sprintf("a column is written either with the \"type\" shorthand or in the "
                    "GenericColumnInfo form with %y, not both", key);
            }
            DataSchemaLoader::checkKeys("DATA-SCHEMA-ERROR", sprintf("table %y column %y", table_name, col_name),
                col_def, ColumnShorthandKeys, path, issues, hints);
        }

        if (col_def.type.val()) {
            *hash<auto> base_type = ColumnTypeMap{col_def.type};
            if (!base_type) {
                issues.error("DATA-SCHEMA-ERROR", "UNKNOWN-TYPE", path + ".type", sprintf("table %y column %y: "
                    "unknown type %y; valid types: %y", table_name, col_name, col_def.type, ColumnTypeMap.keys()));
            } else {
                result += base_type;
                DataSchemaLoader::checkColumnShorthand(table_name, col_name, col_def, path, issues);
            }
        } else if (exists col_def.qore_type || col_def.native_type) {
            # SchemaReverse and AbstractTable::getDescriptionHash() already emit GenericColumnInfo.  Preserve the
            # complete hash verbatim so native types and driver-neutral options such as default_value_native survive
            # a reverse -> data-schema loader -> setupTable round trip.
            foreach hash<auto> pair in (col_def.pairIterator()) {
                result{pair.key} = pair.value;
            }
            return result;
        } else if (!col_def.driver) {
            # a column must either have a 'type' or a pure-driver override — otherwise the
            # loader has no idea what kind of column to create
            issues.error("DATA-SCHEMA-ERROR", "MISSING-TYPE", path, sprintf("table %y column %y: column definition "
                "must set either 'type' or 'driver' (got %y)", table_name, col_name, col_def.keys()));
        }

        # Add size if specified — for pgvector extension types, size belongs under
        # driver.pgsql.size rather than at the top level, matching how PgsqlSqlUtilBase and
        # the c_vector/c_halfvec/c_sparsevec helpers emit these columns
        if (exists col_def.size) {
            if (col_def.type == "vector" || col_def.type == "halfvec" || col_def.type == "sparsevec") {
                # rebuild result.driver.pgsql as an explicit hash<auto> to avoid type-narrowing
                # from the const ColumnTypeMap entry (which only contains a string native_type)
                hash<auto!> existing_pgsql;
                if (result.driver.typeCode() == NT_HASH && result.driver.pgsql.typeCode() == NT_HASH) {
                    foreach hash<auto> p in (result.driver.pgsql.pairIterator()) {
                        existing_pgsql{p.key} = p.value;
                    }
                }
                existing_pgsql.size = col_def.size;
                hash<auto!> new_driver;
                if (result.driver.typeCode() == NT_HASH) {
                    foreach hash<auto> p in (result.driver.pairIterator()) {
                        new_driver{p.key} = p.value;
                    }
                }
                new_driver.pgsql = existing_pgsql;
                result.driver = new_driver;
            } else {
                result.size = col_def.size;
            }
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

        # Add populate expression for automatic backfill when adding a new column to a non-empty
        # table; the DataSchema alignment layer uses this to automate the
        # add-nullable → UPDATE → SET NOT NULL migration pattern
        if (exists col_def.populate) {
            result.populate = col_def.populate;
        }

        # Add comment
        if (col_def.comment.val()) {
            result.comment = col_def.comment;
        }

        # Merge user-provided driver overrides on top of the base type's driver block.
        # This lets a schema combine `type:` with driver-specific tweaks, or define a
        # column entirely through driver overrides with no shorthand type at all.
        if (col_def.driver.typeCode() == NT_HASH) {
            # copy the existing driver block into a fresh untyped hash to dodge type narrowing
            # from const ColumnTypeMap entries
            hash<auto!> merged_driver;
            if (result.driver.typeCode() == NT_HASH) {
                foreach hash<auto> p in (result.driver.pairIterator()) {
                    merged_driver{p.key} = p.value;
                }
            }
            foreach hash<auto> pair in (col_def.driver.pairIterator()) {
                if (pair.value.typeCode() == NT_HASH) {
                    # merge the inner hash key-by-key
                    hash<auto!> inner;
                    if (merged_driver{pair.key}.typeCode() == NT_HASH) {
                        foreach hash<auto> ip in (merged_driver{pair.key}.pairIterator()) {
                            inner{ip.key} = ip.value;
                        }
                    }
                    foreach hash<auto> ip in (pair.value.pairIterator()) {
                        inner{ip.key} = ip.value;
                    }
                    merged_driver{pair.key} = inner;
                } else {
                    merged_driver{pair.key} = pair.value;
                }
            }
            result.driver = merged_driver;
        }

        return result;
    }

    #! Reports the warnings of a column written with the \c type shorthand
    /** - \c SCALE-IGNORED: a \c scale with a type other than \c number, which only \c number uses
        - \c MISSING-SIZE: a \c varchar column without a \c size, which some databases reject (ex: Oracle)
    */
    private static checkColumnShorthand(string table_name, string col_name, hash<auto> col_def, string path,
            DataSchemaIssueCollector issues) {
        if (!issues.collecting()) {
            return;
        }
        if (exists col_def.scale && col_def.type != "number") {
            issues.warning("SCALE-IGNORED", path + ".scale", sprintf("table %y column %y: a scale is used only by "
                "the \"number\" type; it is ignored for type %y", table_name, col_name, col_def.type));
        }
        if (col_def.type == "varchar" && !exists col_def.size && !exists col_def.driver) {
            issues.warning("MISSING-SIZE", path + ".size", sprintf("table %y column %y: a \"varchar\" column has "
                "no size, which some databases reject (ex: Oracle)", table_name, col_name));
        }
    }

    #! Normalizes reference data from .ysm/.jsm format to Schema.qm format
    /** Converts from {columns: [...], rows: [[...], ...]} format to the list-of-lists format
        expected by AbstractSchema::getRows().

        @param ref_data the raw reference_data hash
        @return a hash with strict/normal/create_only/insert_only keys, each mapping
            table names to list-of-lists data

        @throw DATA-SCHEMA-ERROR \a ref_data carries a key that is not in
        @ref Schema::DataSchemaLoader::ReferenceDataCategories "ReferenceDataCategories", or a table's reference
        data carries a key that is not in
        @ref Schema::DataSchemaLoader::ReferenceDataTableKeys "ReferenceDataTableKeys"
    */
    static hash<auto> normalizeReferenceData(hash<auto> ref_data) {
        return DataSchemaLoader::normalizeReferenceDataIntern(ref_data, new DataSchemaIssueCollector(False));
    }

    #! Normalizes reference data, reporting the problems found to \a issues
    private static hash<auto> normalizeReferenceDataIntern(hash<auto> ref_data, DataSchemaIssueCollector issues) {
        # only the known categories and table keys are converted; anything else would be dropped, and with it
        # the rows it holds, so it is rejected
        DataSchemaLoader::checkReferenceDataKeys("DATA-SCHEMA-ERROR", ref_data, issues);

        hash<auto!> result();

        foreach string category in (ReferenceDataCategories) {
            if (ref_data{category} && (!issues.collecting() || ref_data{category}.typeCode() == NT_HASH)) {
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
