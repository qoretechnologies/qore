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

        #! the keys of a partition description that give SQL text
        const PartitionSqlKeys = ("bound_sql", "bound_from_sql", "bound_to_sql", "values_sql");

        #! the regular expression of a name given where the SQL has a name: ASCII letters, digits, \c "_" and \c "$"
        const IdentifierRegex = "\\A[A-Za-z_][A-Za-z0-9_$]*\\z";
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
            if (schema_data{section}) {
                SchemaAccessCheck::refuse(section, "the source of database objects");
            }
        }
        foreach string section in (("index_options", "column_options")) {
            foreach hash<auto> i in (schema_data{section}.pairIterator()) {
                SchemaAccessCheck::checkOptionValue(sprintf("%s.%s", section, i.key), i.value);
            }
        }
        foreach hash<auto> t in (schema_data.tables.pairIterator()) {
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
            case "add_column":
            case "modify_column":
                if (step.type.typeCode() == NT_HASH) {
                    SchemaAccessCheck::checkColumn(loc + ".type", step.type);
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
                } else {
                    foreach hash<auto> i in (v.pairIterator()) {
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

    private static checkTable(string loc, hash<auto> t) {
        if (t.triggers) {
            SchemaAccessCheck::refuse(loc + ".triggers", "the source of triggers");
        }
        foreach hash<auto> c in (t.columns.pairIterator()) {
            if (c.value.typeCode() == NT_HASH) {
                SchemaAccessCheck::checkColumn(sprintf("%s.columns.%s", loc, c.key), c.value);
            }
        }
        *hash<auto> column_names = map {$1: True}, keys t.columns;
        foreach hash<auto> ix in (t.indexes.pairIterator()) {
            SchemaAccessCheck::checkIndex(sprintf("%s.indexes.%s", loc, ix.key), ix.value, column_names);
        }
        SchemaAccessCheck::checkPartitions(loc + ".partitions", t.partitions);
        foreach hash<auto> drv in (t.driver.pairIterator()) {
            string dwhere = sprintf("%s.driver.%s", loc, drv.key);
            if (drv.value.typeCode() != NT_HASH) {
                continue;
            }
            if (drv.value.triggers) {
                SchemaAccessCheck::refuse(dwhere + ".triggers", "the source of triggers");
            }
            if (drv.value.functions) {
                SchemaAccessCheck::refuse(dwhere + ".functions", "the source of functions");
            }
            foreach hash<auto> c in (drv.value.columns.pairIterator()) {
                if (c.value.typeCode() == NT_HASH) {
                    SchemaAccessCheck::checkColumn(sprintf("%s.columns.%s", dwhere, c.key), c.value);
                }
            }
            foreach hash<auto> ix in (drv.value.indexes.pairIterator()) {
                SchemaAccessCheck::checkIndex(sprintf("%s.indexes.%s", dwhere, ix.key), ix.value, column_names);
            }
            SchemaAccessCheck::checkPartitions(dwhere + ".partitions", drv.value.partitions);
        }
    }

    private static checkColumn(string loc, hash<auto> c) {
        if (c.default_value_native) {
            SchemaAccessCheck::refuse(loc + ".default_value_native", "a default value as SQL text");
        }
        if (exists c.native_type) {
            SchemaAccessCheck::checkNativeType(loc + ".native_type", c.native_type);
        }
        if (c.populate.typeCode() == NT_HASH && exists c.populate.sql) {
            SchemaAccessCheck::refuse(loc + ".populate.sql", "the SQL expression of a backfill");
        }
        foreach hash<auto> drv in (c.driver.pairIterator()) {
            if (drv.value.typeCode() == NT_HASH) {
                SchemaAccessCheck::checkColumn(sprintf("%s.driver.%s", loc, drv.key), drv.value);
            }
        }
    }

    private static checkNativeType(string loc, auto type) {
        if (type.typeCode() != NT_STRING) {
            return;
        }
        try {
            SqlUtil::sql_check_cast_type(type);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err != "OPERATOR-ERROR") {
                rethrow;
            }
            SchemaAccessCheck::refuse(loc, "a type name that is not only a type name");
        }
    }

    private static checkIndex(string loc, auto ix, *hash<auto> column_names) {
        if (ix.typeCode() != NT_HASH) {
            return;
        }
        foreach hash<auto> i in (ix.pairIterator()) {
            *string kind = UntrustedIndexKeys{i.key};
            if (!kind) {
                SchemaAccessCheck::refuse(sprintf("%s.%s", loc, i.key), sprintf("the index option %y", i.key));
            }
            switch (kind) {
                case "columns":
                    foreach auto col in (i.value) {
                        # a string that is not a column of the table is written into the SQL as an expression
                        if (col.typeCode() != NT_STRING || !column_names{col}) {
                            SchemaAccessCheck::refuse(sprintf("%s.columns", loc),
                                sprintf("the index expression %y (an index column must be a column of the table)",
                                    col));
                        }
                    }
                    break;
                case "identifier":
                    if (i.value.typeCode() != NT_STRING || !regex(i.value, IdentifierRegex)) {
                        SchemaAccessCheck::refuse(sprintf("%s.%s", loc, i.key), "a name that is not only a name");
                    }
                    break;
            }
        }
    }

    private static checkPartitions(string loc, auto v) {
        switch (v.typeCode()) {
            case NT_HASH:
                foreach hash<auto> i in (v.pairIterator()) {
                    if (inlist(i.key, PartitionSqlKeys)) {
                        SchemaAccessCheck::refuse(sprintf("%s.%s", loc, i.key), "the SQL text of a partition bound");
                    }
                    SchemaAccessCheck::checkPartitions(sprintf("%s.%s", loc, i.key), i.value);
                }
                break;
            case NT_LIST:
                foreach auto e in (v) {
                    SchemaAccessCheck::checkPartitions(sprintf("%s[%d]", loc, $#), e);
                }
                break;
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
