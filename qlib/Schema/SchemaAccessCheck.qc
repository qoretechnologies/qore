# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SchemaAccessCheck.qc checks a schema description against the access level of a DataSchema

/*  SchemaAccessCheck.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! the Schema namespace
public namespace Schema {
#! Checks a schema description against the access level of a DataSchema
/** A schema at the @ref Schema::AL_UNTRUSTED "untrusted" access level runs no SQL text from its description: the
    description is refused when the schema is created, with \c SCHEMA-ACCESS-ERROR, if any key of it gives SQL text
    (see @ref schema_upgrade_access), and @ref Schema::MigrationExecutor "MigrationExecutor" refuses each such migration
    step at that level before it runs (qore#6578).

    @since Schema 1.5
*/
public class SchemaAccessCheck {
    public {
        #! the access levels
        const AccessLevels = {
            AL_UNTRUSTED: True,
            AL_TRUSTED: True,
            AL_ADMIN: True,
        };

        #! top-level sections whose values are SQL source text
        const SqlSourceSections = ("types", "functions", "procedures", "packages", "materialized_views");

        #! the index keys that a schema at the untrusted access level can give, with the kind of their value
        const UntrustedIndexKeys = {
            "columns": "columns",
            "unique": "bool",
            "replace": "bool",
            "partition_aligned": "bool",
            "index_tablespace": "identifier",
        };

        #! the keys of a table that a schema at the untrusted access level can give (a table-level \c driver block
        #! can replace the columns and indexes of the table, so it is not one of them)
        const UntrustedTableKeys = {
            "columns": True,
            "primary_key": True,
            "indexes": True,
            "unique_constraints": True,
            "foreign_constraints": True,
            "comment": True,
            "partition_strategy": True,
            "partitions": True,
            "auto_triggers": True,
        };

        #! the keys of a column whose values are written into the SQL as text: checked or quoted
        const UntrustedColumnStringKeys = {
            "qore_type": True,
            "native_type": True,
            "comment": True,
            "default_value": True,
        };

        #! the keys of a partition strategy that a schema at the untrusted access level can give
        const UntrustedPartitionStrategyKeys = {
            "method": "identifier",
            "columns": "identifiers",
            "subpartition": "strategy",
        };

        #! the keys of a partition that a schema at the untrusted access level can give
        const UntrustedPartitionKeys = {
            "name": "identifier",
            "method": "identifier",
            "columns": "identifiers",
            "bound_from": "value",
            "bound_to": "value",
            "values": "value",
            "modulus": "value",
            "remainder": "value",
            "is_default": "value",
            "subpartitions": "partitions",
            "relation_name": "identifier",
            "sql_name": "identifier",
        };

        #! the regular expression of a name given where the SQL has a name: ASCII letters, digits and \c "_"
        const IdentifierRegex = "\\A[A-Za-z_][A-Za-z0-9_]*\\z";

        #! the size of a word of a type name (as for a cast; see SqlUtil::sql_check_cast_type())
        const TypeSizeRegex = "(?:\\((?:[0-9]+|(?i:max)|\\*)(?:, ?[0-9]+)?(?: (?i:char|byte))?\\))?";

        #! a type name of one word, with an optional schema, size and array brackets
        const SingleWordTypeRegex = "\\A[A-Za-z_][A-Za-z0-9_]*(?:\\.[A-Za-z_][A-Za-z0-9_]*)?" + TypeSizeRegex
            + "(?:\\[\\])*\\z";

        #! the type names of more than one word, without their sizes: the words after a type in a column definition
        #! are column clauses (ex: \c "int references t" or \c "int default 1"), so only these are accepted
        const MultiWordTypes = {
            "double precision": True,
            "character varying": True,
            "national character": True,
            "national character varying": True,
            "national char": True,
            "national char varying": True,
            "bit varying": True,
            "timestamp with time zone": True,
            "timestamp without time zone": True,
            "timestamp with local time zone": True,
            "time with time zone": True,
            "time without time zone": True,
            "interval year to month": True,
            "interval day to second": True,
            "long raw": True,
            "long varchar": True,
            "tinyint unsigned": True,
            "smallint unsigned": True,
            "mediumint unsigned": True,
            "int unsigned": True,
            "integer unsigned": True,
            "bigint unsigned": True,
            "decimal unsigned": True,
            "float unsigned": True,
            "double unsigned": True,
        };
    }

    #! Checks an access level and returns it
    /** @throw SCHEMA-ACCESS-ERROR the access level is not @ref Schema::AL_UNTRUSTED "AL_UNTRUSTED",
        @ref Schema::AL_TRUSTED "AL_TRUSTED" or @ref Schema::AL_ADMIN "AL_ADMIN"; an unknown level was taken as
        trusted
    */
    static string checkAccessLevel(string al) {
        if (!AccessLevels{al}) {
            throw "SCHEMA-ACCESS-ERROR", sprintf("unknown access level %y; expecting one of: %y", al,
                keys AccessLevels);
        }
        return al;
    }

    #! Checks a normalized schema description for the untrusted access level
    /** @param schema_data the normalized schema description (see DataSchemaLoader::normalize())

        @throw SCHEMA-ACCESS-ERROR a key of the description gives SQL text, which a schema at the untrusted access
        level cannot run
    */
    static checkUntrustedDescription(hash<auto> schema_data) {
        foreach string section in (SqlSourceSections) {
            if (exists schema_data{section}) {
                SchemaAccessCheck::refuse(section, "the source of database objects");
            }
        }
        foreach string section in (("index_options", "column_options")) {
            if (exists schema_data{section} && schema_data{section}.typeCode() != NT_HASH) {
                SchemaAccessCheck::refuse(section, "options that are not a hash of options");
            }
            foreach hash<auto> i in (schema_data{section}.pairIterator()) {
                SchemaAccessCheck::checkOptionValue(sprintf("%s.%s", section, i.key), i.value);
            }
        }
        if (exists schema_data.tables && schema_data.tables.typeCode() != NT_HASH) {
            SchemaAccessCheck::refuse("tables", "tables that are not table descriptions");
        }
        foreach hash<auto> t in (schema_data.tables.pairIterator()) {
            # SqlUtil takes a "driver" key of the tables as the tables of a driver
            if (t.key == "driver") {
                SchemaAccessCheck::refuse("tables.driver", "the tables of a driver");
            }
            SchemaAccessCheck::checkTable("tables." + t.key, t.value);
        }
        SchemaAccessCheck::checkMigrations("migrations", schema_data.migrations);
    }

    #! Checks a migration step for the untrusted access level
    /** @param loc the location of the step in the description, for the error message
        @param step the step

        @throw SCHEMA-ACCESS-ERROR the step gives SQL text, which a schema at the untrusted access level cannot run
    */
    static checkUntrustedStep(string loc, hash<auto> step) {
        if (step.action.typeCode() != NT_STRING) {
            SchemaAccessCheck::refuse(loc + ".action", "an action that is not an action name");
        }
        switch (step.action) {
            case "sql":
            case "script":
                SchemaAccessCheck::refuse(loc, sprintf("the %y action", step.action));
            case "add_trigger":
                SchemaAccessCheck::refuse(loc, "the source of a trigger (the \"add_trigger\" action)");
            case "add_constraint":
                if (step.constraint_type == "check") {
                    SchemaAccessCheck::refuse(loc, "the expression of a check constraint");
                }
                break;
            case "add_index":
                SchemaAccessCheck::checkIndexColumns(loc + ".columns", step.columns);
                break;
            case "add_column":
            case "modify_column":
                if (step.type.typeCode() == NT_HASH) {
                    # checked as MigrationExecutor gives it to SqlUtil
                    SchemaAccessCheck::checkColumn(loc + ".type", DataSchemaLoader::normalizeColumn(
                        step.table.typeCode() == NT_STRING ? step.table : "",
                        step.column.typeCode() == NT_STRING ? step.column : "", step.type));
                } else if (exists step.type) {
                    SchemaAccessCheck::checkNativeType(loc + ".type", step.type);
                }
                break;
        }
    }

    # migrations are a list of steps or a hash of version -> list of steps
    private static checkMigrations(string loc, auto v) {
        switch (v.typeCode()) {
            case NT_HASH:
                if (exists v.action) {
                    SchemaAccessCheck::checkUntrustedStep(loc, v);
                }
                # also the steps of a migration hash that has an "action" key
                foreach hash<auto> i in (v.pairIterator()) {
                    if (i.value.typeCode() == NT_HASH || i.value.typeCode() == NT_LIST) {
                        SchemaAccessCheck::checkMigrations(sprintf("%s.%s", loc, i.key), i.value);
                    }
                }
                break;
            case NT_LIST:
                foreach auto e in (v) {
                    SchemaAccessCheck::checkMigrations(sprintf("%s[%d]", loc, $#), e);
                }
                break;
        }
    }

    private static checkTable(string loc, auto t) {
        if (t.typeCode() != NT_HASH) {
            SchemaAccessCheck::refuse(loc, "a table that is not a table description");
        }
        foreach string key in (keys t) {
            if (!UntrustedTableKeys{key}) {
                SchemaAccessCheck::refuse(sprintf("%s.%s", loc, key), sprintf("the table key %y", key));
            }
        }
        if (exists t.columns && t.columns.typeCode() != NT_HASH) {
            SchemaAccessCheck::refuse(loc + ".columns", "columns that are not column descriptions");
        }
        foreach hash<auto> c in (t.columns.pairIterator()) {
            # SqlUtil takes a "driver" key of the columns as the columns of a driver
            if (c.key == "driver") {
                SchemaAccessCheck::refuse(loc + ".columns.driver", "the columns of a driver");
            }
            SchemaAccessCheck::checkColumn(sprintf("%s.columns.%s", loc, c.key), c.value);
        }
        if (exists t.indexes && t.indexes.typeCode() != NT_HASH) {
            SchemaAccessCheck::refuse(loc + ".indexes", "indexes that are not index descriptions");
        }
        foreach hash<auto> ix in (t.indexes.pairIterator()) {
            if (ix.key == "driver") {
                SchemaAccessCheck::refuse(loc + ".indexes.driver", "the indexes of a driver");
            }
            SchemaAccessCheck::checkIndex(sprintf("%s.indexes.%s", loc, ix.key), ix.value);
        }
        if (exists t.auto_triggers && t.auto_triggers.typeCode() != NT_HASH) {
            SchemaAccessCheck::refuse(loc + ".auto_triggers", "auto triggers that are not a hash");
        }
        if (exists t.partition_strategy) {
            SchemaAccessCheck::checkPartitionStrategy(loc + ".partition_strategy", t.partition_strategy);
        }
        if (exists t.partitions) {
            SchemaAccessCheck::checkPartitions(loc + ".partitions", t.partitions);
        }
    }

    private static checkColumn(string loc, auto c, bool driver_ok = True) {
        if (c.typeCode() != NT_HASH) {
            SchemaAccessCheck::refuse(loc, "a column that is not a column description");
        }
        foreach hash<auto> i in (c.pairIterator()) {
            string kloc = sprintf("%s.%s", loc, i.key);
            switch (i.key) {
                case "default_value_native":
                    if (i.value) {
                        SchemaAccessCheck::refuse(kloc, "a default value as SQL text");
                    }
                    continue;
                case "native_type":
                    SchemaAccessCheck::checkNativeType(kloc, i.value);
                    continue;
                case "populate":
                    if (i.value.typeCode() == NT_HASH && exists i.value.sql) {
                        SchemaAccessCheck::refuse(kloc + ".sql", "the SQL expression of a backfill");
                    }
                    if (i.value.typeCode() == NT_LIST) {
                        SchemaAccessCheck::refuse(kloc, "a backfill that is not a value or a backfill description");
                    }
                    continue;
                case "driver":
                    if (!driver_ok || i.value.typeCode() != NT_HASH) {
                        SchemaAccessCheck::refuse(kloc, "a driver override that is not a column description");
                    }
                    foreach hash<auto> drv in (i.value.pairIterator()) {
                        SchemaAccessCheck::checkColumn(sprintf("%s.%s", kloc, drv.key), drv.value, False);
                    }
                    continue;
            }
            # another key: a boolean or a number (ex: size, notnull, unsigned), or a string that is checked or quoted
            switch (i.value.typeCode()) {
                case NT_BOOLEAN:
                case NT_INT:
                case NT_FLOAT:
                case NT_NUMBER:
                case NT_NOTHING:
                    break;
                case NT_STRING:
                    if (!UntrustedColumnStringKeys{i.key}) {
                        SchemaAccessCheck::refuse(kloc, sprintf("the column option %y", i.key));
                    }
                    break;
                default:
                    if (i.key != "default_value") {
                        SchemaAccessCheck::refuse(kloc, sprintf("the column option %y", i.key));
                    }
                    break;
            }
        }
    }

    private static checkNativeType(string loc, auto type) {
        if (type.typeCode() != NT_STRING || !type.isDataAscii()) {
            SchemaAccessCheck::refuse(loc, "a type name that is not only a type name");
        }
        if (regex(type, SingleWordTypeRegex)) {
            return;
        }
        # a type of more than one word: each word with an optional size, and the words a known type name
        string words = type;
        words =~ s/\((?:[0-9]+|max|\*)(?:, ?[0-9]+)?(?: (?:char|byte))?\)//gi;
        words =~ s/(\[\])+$//;
        bool type_name = True;
        try {
            SqlUtil::sql_check_cast_type(type);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err != "OPERATOR-ERROR") {
                rethrow;
            }
            type_name = False;
        }
        if (!type_name || !MultiWordTypes{words.lwr()}) {
            SchemaAccessCheck::refuse(loc, "a type name that is not only a type name (the words after a type are "
                "column clauses)");
        }
    }

    #! index columns: names only (a string that is not a column is written into the SQL as an expression)
    private static checkIndexColumns(string loc, auto cols) {
        foreach auto col in (cols) {
            if (col.typeCode() != NT_STRING || !regex(col, IdentifierRegex)) {
                SchemaAccessCheck::refuse(loc, sprintf("the index expression %y (an index column must be a name)",
                    col));
            }
        }
    }

    private static checkIndex(string loc, auto ix) {
        if (ix.typeCode() != NT_HASH) {
            SchemaAccessCheck::refuse(loc, "an index that is not an index description");
        }
        foreach hash<auto> i in (ix.pairIterator()) {
            *string kind = UntrustedIndexKeys{i.key};
            if (!kind) {
                SchemaAccessCheck::refuse(sprintf("%s.%s", loc, i.key), sprintf("the index option %y", i.key));
            }
            switch (kind) {
                case "columns":
                    SchemaAccessCheck::checkIndexColumns(sprintf("%s.columns", loc), i.value);
                    break;
                case "identifier":
                    SchemaAccessCheck::checkIdentifier(sprintf("%s.%s", loc, i.key), i.value);
                    break;
                case "bool":
                    if (i.value.typeCode() != NT_BOOLEAN) {
                        SchemaAccessCheck::refuse(sprintf("%s.%s", loc, i.key), "an index option that is not a "
                            "boolean");
                    }
                    break;
            }
        }
    }

    private static checkIdentifier(string loc, auto v) {
        if (v.typeCode() != NT_STRING || !regex(v, IdentifierRegex)) {
            SchemaAccessCheck::refuse(loc, "a name that is not only a name");
        }
    }

    private static checkPartitionStrategy(string loc, auto ps) {
        if (ps.typeCode() != NT_HASH) {
            SchemaAccessCheck::refuse(loc, "a partition strategy that is not a description");
        }
        foreach hash<auto> i in (ps.pairIterator()) {
            string kloc = sprintf("%s.%s", loc, i.key);
            switch (UntrustedPartitionStrategyKeys{i.key}) {
                case "identifier":
                    SchemaAccessCheck::checkIdentifier(kloc, i.value);
                    break;
                case "identifiers":
                    foreach auto v in (i.value) {
                        SchemaAccessCheck::checkIdentifier(kloc, v);
                    }
                    break;
                case "strategy":
                    SchemaAccessCheck::checkPartitionStrategy(kloc, i.value);
                    break;
                default:
                    # "auto" (an interval or expression) and "driver" are SQL text or replace the strategy
                    SchemaAccessCheck::refuse(kloc, sprintf("the partition strategy key %y", i.key));
            }
        }
    }

    private static checkPartitions(string loc, auto v) {
        switch (v.typeCode()) {
            case NT_HASH:
                foreach hash<auto> i in (v.pairIterator()) {
                    SchemaAccessCheck::checkPartition(sprintf("%s.%s", loc, i.key), i.value);
                }
                break;
            case NT_LIST:
                foreach auto e in (v) {
                    SchemaAccessCheck::checkPartition(sprintf("%s[%d]", loc, $#), e);
                }
                break;
            default:
                SchemaAccessCheck::refuse(loc, "partitions that are not partition descriptions");
        }
    }

    private static checkPartition(string loc, auto p) {
        if (p.typeCode() != NT_HASH) {
            SchemaAccessCheck::refuse(loc, "a partition that is not a partition description");
        }
        foreach hash<auto> i in (p.pairIterator()) {
            string kloc = sprintf("%s.%s", loc, i.key);
            switch (UntrustedPartitionKeys{i.key}) {
                case "identifier":
                    SchemaAccessCheck::checkIdentifier(kloc, i.value);
                    break;
                case "identifiers":
                    foreach auto v in (i.value) {
                        SchemaAccessCheck::checkIdentifier(kloc, v);
                    }
                    break;
                case "partitions":
                    SchemaAccessCheck::checkPartitions(kloc, i.value);
                    break;
                case "value":
                    # bound values are converted to literals of the column type; PostgreSQL gives the DDL of
                    # subpartitions in a "do" block, whose quote tag a value must not have
                    foreach auto v in (i.value) {
                        if (v.typeCode() == NT_STRING && v.find("$") != -1) {
                            SchemaAccessCheck::refuse(kloc, "a partition value with \"$\", which can end the quoted "
                                "block of the DDL of a partition");
                        }
                    }
                    break;
                default:
                    # the "_sql" bounds and "driver" are SQL text or replace the partition
                    SchemaAccessCheck::refuse(kloc, sprintf("the partition key %y", i.key));
            }
        }
    }

    private static checkOptionValue(string loc, auto v) {
        switch (v.typeCode()) {
            case NT_STRING:
                if (!regex(v, IdentifierRegex)) {
                    SchemaAccessCheck::refuse(loc, "an option value that is not only a name");
                }
                break;
            case NT_HASH:
                foreach hash<auto> i in (v.pairIterator()) {
                    SchemaAccessCheck::checkOptionValue(sprintf("%s.%s", loc, i.key), i.value);
                }
                break;
            case NT_LIST:
                foreach auto e in (v) {
                    SchemaAccessCheck::checkOptionValue(sprintf("%s[%d]", loc, $#), e);
                }
                break;
        }
    }

    private static refuse(string loc, string what) {
        throw "SCHEMA-ACCESS-ERROR", sprintf("%s: %s is SQL text, which a schema at the %y access level cannot run",
            loc, what, AL_UNTRUSTED);
    }
}
}
