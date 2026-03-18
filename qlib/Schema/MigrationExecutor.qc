# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MigrationExecutor class definition

/** MigrationExecutor.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Actions allowed at any access level
/** @since Schema 1.5
*/
public const UnrestrictedActions = {
    "update": True,
    "insert": True,
    "delete": True,
    "insert_from_select": True,
    "add_column": True,
    "drop_column": True,
    "modify_column": True,
    "rename_column": True,
    "rename_table": True,
    "add_index": True,
    "drop_index": True,
    "add_constraint": True,
    "drop_constraint": True,
    "add_trigger": True,
    "drop_trigger": True,
};

#! Actions requiring trusted or admin access
/** @since Schema 1.5
*/
public const RestrictedActions = {
    "sql": True,
    "script": True,
};

#! Executes data-driven migration actions defined in .ysm/.jsm schema files
/** This class translates migration action hashes into SqlUtil API calls, enforces access level
    restrictions, and supports dependency-based step ordering.

    @since Schema 1.5
*/
public class MigrationExecutor {
    private {
        #! The datasource
        Qore::SQL::AbstractDatasource ds;

        #! The access level
        string access_level;

        #! Verbosity level
        int verbose;

        #! Optional log callback
        *code<nothing(string)> log_callback;

        #! Schema info passed through to migration scripts
        *hash<auto> schema_info;
    }

    #! Creates the executor
    /** @param n_ds the datasource for SQL execution
        @param al the access level (AL_UNTRUSTED, AL_TRUSTED, AL_ADMIN)
        @param v verbosity level
        @param log optional logging callback: code<nothing(string)>
    */
    constructor(Qore::SQL::AbstractDatasource n_ds, string al = AL_TRUSTED, int v = 0,
            *code<nothing(string)> log) {
        ds = n_ds;
        access_level = al;
        verbose = v;
        log_callback = log;
    }

    #! Executes migration steps for the given version transition
    /** Filters migrations by version, then executes the specified phase (pre_align or post_align).

        @param migrations the full list of migration definitions from the schema
        @param current_version the current schema version in the database
        @param target_version the target schema version
        @param phase "pre_align" or "post_align"
        @param table_cache optional Tables cache (available in post_align phase)
        @param schema_info optional schema info hash
    */
    executeMigrations(*list<auto> migrations, string current_version, string target_version,
            string phase, *SqlUtil::Tables table_cache, *hash<auto> n_schema_info) {
        if (!migrations) {
            return;
        }

        schema_info = n_schema_info;

        foreach hash<auto> migration in (migrations) {
            # Only execute migrations for versions > current and <= target
            if (Util::compare_version(migration.version, current_version) <= 0) {
                continue;
            }
            if (Util::compare_version(migration.version, target_version) > 0) {
                continue;
            }

            *list<auto> steps = migration{phase};
            if (!steps) {
                continue;
            }

            logMsg(sprintf("executing %s migration steps for version %s: %s",
                phase, migration.version, migration.description ?? ""));

            # Resolve step ordering (dependency-aware)
            list<auto> ordered = resolveStepOrder(steps);

            foreach hash<auto> step in (ordered) {
                executeStep(step, table_cache);
            }
        }
    }

    #! Executes a single migration step
    /** @param step the migration action hash
        @param table_cache optional table cache for table operations

        @throw MIGRATION-ACCESS-ERROR if the action requires higher access than currently allowed
        @throw MIGRATION-ERROR unknown action, missing required field, or step execution failure
    */
    executeStep(hash<auto> step, *SqlUtil::Tables table_cache) {
        string action = step.action;

        # Access level check
        if (RestrictedActions{action} && access_level == AL_UNTRUSTED) {
            throw "MIGRATION-ACCESS-ERROR", sprintf("action %y requires 'trusted' or 'admin' access level; "
                "current level is %y", action, access_level);
        }

        logMsg(sprintf("  step: %s%s", action, step.table ? sprintf(" on %s", step.table) : ""));

        switch (action) {
            case "update":
                executeUpdate(step, table_cache);
                break;
            case "insert":
                executeInsert(step, table_cache);
                break;
            case "delete":
                executeDelete(step, table_cache);
                break;
            case "insert_from_select":
                executeInsertFromSelect(step, table_cache);
                break;
            case "add_column":
                executeAddColumn(step, table_cache);
                break;
            case "drop_column":
                executeDropColumn(step, table_cache);
                break;
            case "modify_column":
                executeModifyColumn(step, table_cache);
                break;
            case "rename_column":
                executeRenameColumn(step, table_cache);
                break;
            case "add_index":
                executeAddIndex(step, table_cache);
                break;
            case "drop_index":
                executeDropIndex(step, table_cache);
                break;
            case "add_constraint":
                executeAddConstraint(step, table_cache);
                break;
            case "drop_constraint":
                executeDropConstraint(step, table_cache);
                break;
            case "rename_table":
                executeRenameTable(step, table_cache);
                break;
            case "add_trigger":
                executeAddTrigger(step, table_cache);
                break;
            case "drop_trigger":
                executeDropTrigger(step, table_cache);
                break;
            case "sql":
                executeSql(step);
                break;
            case "script":
                executeScript(step, table_cache);
                break;
            default:
                throw "MIGRATION-ERROR", sprintf("unknown migration action: %y", action);
        }
    }

    #! Resolves step ordering based on dependencies
    /** Steps with no id/depends_on execute in declaration order. Steps with depends_on are
        topologically sorted with declaration order as tiebreaker.

        @param steps the list of migration steps
        @return the ordered list of steps

        @throw MIGRATION-ERROR circular dependency detected
    */
    static list<auto> resolveStepOrder(list<auto> steps) {
        # Check if any step has dependencies
        bool has_deps = False;
        foreach hash<auto> step in (steps) {
            if (step.depends_on) {
                has_deps = True;
                break;
            }
        }

        if (!has_deps) {
            return steps;
        }

        # Build dependency graph
        hash<string, int> id_to_index();
        for (int i = 0; i < steps.size(); ++i) {
            if (steps[i].id.val()) {
                id_to_index{steps[i].id} = i;
            }
        }

        # Topological sort (Kahn's algorithm)
        hash<string, list<int>> dependents();  # id -> list of indices that depend on it
        list<int> in_degree();
        for (int i = 0; i < steps.size(); ++i) {
            push in_degree, 0;
        }

        for (int i = 0; i < steps.size(); ++i) {
            if (steps[i].depends_on) {
                foreach string dep_id in (steps[i].depends_on) {
                    if (!id_to_index.hasKey(dep_id)) {
                        throw "MIGRATION-ERROR", sprintf("step %y depends on unknown step id %y",
                            steps[i].id ?? sprintf("#%d", i), dep_id);
                    }
                    int dep_idx = id_to_index{dep_id};
                    if (!dependents.hasKey(dep_id)) {
                        dependents{dep_id} = ();
                    }
                    push dependents{dep_id}, i;
                    in_degree[i] = in_degree[i] + 1;
                }
            }
        }

        # Start with steps that have no dependencies
        list<int> queue();
        for (int i = 0; i < steps.size(); ++i) {
            if (in_degree[i] == 0) {
                push queue, i;
            }
        }

        list<auto> result();
        int processed = 0;
        while (queue.size() > 0) {
            int idx = shift queue;
            push result, steps[idx];
            ++processed;

            # Release dependents
            if (steps[idx].id.val() && dependents.hasKey(steps[idx].id)) {
                foreach int dep_idx in (dependents{steps[idx].id}) {
                    in_degree[dep_idx] = in_degree[dep_idx] - 1;
                    if (in_degree[dep_idx] == 0) {
                        push queue, dep_idx;
                    }
                }
            }
        }

        if (processed < steps.size()) {
            throw "MIGRATION-ERROR", "circular dependency detected in migration steps";
        }

        return result;
    }

    #! Executes an UPDATE action
    /** @throw MIGRATION-ERROR missing required field
    */
    private executeUpdate(hash<auto> step, *SqlUtil::Tables table_cache) {
        validateStepField(step, "table");
        validateStepField(step, "set");
        if (!step.hasKey("where") && !step.where_all) {
            throw "MIGRATION-ERROR", sprintf("migration step %y action 'update' requires a 'where' "
                "clause or 'where_all: true' to update all rows",
                step.id ?? "unknown");
        }
        AbstractTable table = getTable(step.table, table_cache);
        table.update(step.set, step.where);
    }

    #! Executes an INSERT action
    /** @throw MIGRATION-ERROR missing required field
    */
    private executeInsert(hash<auto> step, *SqlUtil::Tables table_cache) {
        validateStepField(step, "table");
        AbstractTable table = getTable(step.table, table_cache);
        if (step.rows) {
            foreach hash<auto> row in (step.rows) {
                table.insert(row);
            }
        } else if (step.row) {
            table.insert(step.row);
        } else {
            throw "MIGRATION-ERROR", sprintf("migration step %y action 'insert' requires 'row' or 'rows' field",
                step.id ?? "unknown");
        }
    }

    #! Executes a DELETE action
    /** @throw MIGRATION-ERROR missing required field
    */
    private executeDelete(hash<auto> step, *SqlUtil::Tables table_cache) {
        validateStepField(step, "table");
        if (!step.hasKey("where") && !step.where_all) {
            throw "MIGRATION-ERROR", sprintf("migration step %y action 'delete' requires a 'where' "
                "clause or 'where_all: true' to delete all rows",
                step.id ?? "unknown");
        }
        AbstractTable table = getTable(step.table, table_cache);
        table.del(step.where);
    }

    #! Executes an INSERT FROM SELECT action
    /** @throw MIGRATION-ERROR missing required field
    */
    private executeInsertFromSelect(hash<auto> step, *SqlUtil::Tables table_cache) {
        validateStepField(step, "table");
        validateStepField(step, "columns");
        validateStepField(step, "source_table");
        AbstractTable target = getTable(step.table, table_cache);
        AbstractTable source = getTable(step.source_table, table_cache);
        target.insertFromSelect(step.columns, source, step.select ?? {});
    }

    #! Executes an ADD COLUMN action
    private executeAddColumn(hash<auto> step, *SqlUtil::Tables table_cache) {
        validateStepField(step, "table");
        validateStepField(step, "column");
        AbstractTable table = getTable(step.table, table_cache);
        hash<auto> col_def = step.type.typeCode() == NT_HASH
            ? DataSchemaLoader::normalizeColumn(step.table, step.column, step.type)
            : step.type;
        table.addColumn(step.column, col_def, True);
    }

    #! Executes a DROP COLUMN action
    private executeDropColumn(hash<auto> step, *SqlUtil::Tables table_cache) {
        validateStepField(step, "table");
        validateStepField(step, "column");
        AbstractTable table = getTable(step.table, table_cache);
        table.dropColumn(step.column);
    }

    #! Executes a MODIFY COLUMN action
    private executeModifyColumn(hash<auto> step, *SqlUtil::Tables table_cache) {
        validateStepField(step, "table");
        validateStepField(step, "column");
        AbstractTable table = getTable(step.table, table_cache);
        hash<auto> col_def = step.type.typeCode() == NT_HASH
            ? DataSchemaLoader::normalizeColumn(step.table, step.column, step.type)
            : step.type;
        table.modifyColumn(step.column, col_def, True);
    }

    #! Executes a RENAME COLUMN action
    private executeRenameColumn(hash<auto> step, *SqlUtil::Tables table_cache) {
        validateStepField(step, "table");
        validateStepField(step, "from");
        validateStepField(step, "to");
        AbstractTable table = getTable(step.table, table_cache);
        table.renameColumn(step.from, step.to);
    }

    #! Executes an ADD INDEX action
    private executeAddIndex(hash<auto> step, *SqlUtil::Tables table_cache) {
        validateStepField(step, "table");
        validateStepField(step, "name");
        validateStepField(step, "columns");
        AbstractTable table = getTable(step.table, table_cache);
        table.addIndex(step.name, step.unique ?? False, step.columns);
    }

    #! Executes a DROP INDEX action
    private executeDropIndex(hash<auto> step, *SqlUtil::Tables table_cache) {
        validateStepField(step, "table");
        validateStepField(step, "name");
        AbstractTable table = getTable(step.table, table_cache);
        table.dropIndex(step.name);
    }

    #! Executes a DROP CONSTRAINT action
    private executeDropConstraint(hash<auto> step, *SqlUtil::Tables table_cache) {
        validateStepField(step, "table");
        validateStepField(step, "name");
        AbstractTable table = getTable(step.table, table_cache);
        table.dropConstraint(step.name);
    }

    #! Executes a RENAME TABLE action
    private executeRenameTable(hash<auto> step, *SqlUtil::Tables table_cache) {
        validateStepField(step, "table");
        validateStepField(step, "to");
        AbstractTable table = getTable(step.table, table_cache);
        table.rename(step.to);
    }

    #! Executes an ADD TRIGGER action
    /** @param step requires \c table, \c name, and \c source fields
    */
    private executeAddTrigger(hash<auto> step, *SqlUtil::Tables table_cache) {
        validateStepField(step, "table");
        validateStepField(step, "name");
        validateStepField(step, "source");
        AbstractTable table = getTable(step.table, table_cache);
        table.addTrigger(step.name, step.source);
    }

    #! Executes a DROP TRIGGER action
    private executeDropTrigger(hash<auto> step, *SqlUtil::Tables table_cache) {
        validateStepField(step, "table");
        validateStepField(step, "name");
        AbstractTable table = getTable(step.table, table_cache);
        table.dropTrigger(step.name);
    }

    #! Executes an ADD CONSTRAINT action
    /** Dispatches to the appropriate SqlUtil method based on constraint_type:
        - \c "unique": addUniqueConstraint(name, columns)
        - \c "foreign": addForeignConstraint(name, columns, target_table, target_columns)
        - \c "check": addCheckConstraint(name, expression)

        @throw MIGRATION-ERROR unknown constraint_type
    */
    private executeAddConstraint(hash<auto> step, *SqlUtil::Tables table_cache) {
        validateStepField(step, "table");
        validateStepField(step, "name");
        validateStepField(step, "constraint_type");
        AbstractTable table = getTable(step.table, table_cache);

        switch (step.constraint_type) {
            case "unique":
                validateStepField(step, "columns");
                table.addUniqueConstraint(step.name, step.columns);
                break;
            case "foreign":
                validateStepField(step, "columns");
                validateStepField(step, "target_table");
                table.addForeignConstraint(step.name, step.columns, step.target_table,
                    step.target_columns);
                break;
            case "check":
                validateStepField(step, "expression");
                table.addCheckConstraint(step.name, step.expression);
                break;
            default:
                throw "MIGRATION-ERROR", sprintf("unknown constraint_type %y for add_constraint action; "
                    "expected 'unique', 'foreign', or 'check'", step.constraint_type);
        }
    }

    #! Executes raw SQL statements
    /** @throw DBI-ERROR SQL execution failure (propagated from execRaw)
        @throw MIGRATION-ERROR missing required field
    */
    private executeSql(hash<auto> step) {
        *list<string> stmts;
        string drv = ds.getDriverName();

        # Check for driver-specific overrides
        if (step.driver{drv}) {
            stmts = step.driver{drv};
        } else {
            stmts = step.statements;
        }

        if (!stmts) {
            throw "MIGRATION-ERROR", sprintf("migration step %y action 'sql' has no SQL statements "
                "for driver %y and no default 'statements' list",
                step.id ?? "unknown", drv);
        }

        foreach string stmt in (stmts) {
            logMsg(sprintf("    SQL: %s", stmt));
            ds.execRaw(stmt);
        }
    }

    #! Executes a migration script
    /** The script must define a \c migrate(AbstractDatasource, *Tables, *hash, int) entry point.

        @throw FILE-READ-ERROR script file cannot be read
        @throw PARSE-EXCEPTION script has syntax errors
        @throw RUNTIME-ERROR script execution failure
    */
    private executeScript(hash<auto> step, *SqlUtil::Tables table_cache) {
        string file_path = step.file;
        logMsg(sprintf("    script: %s", file_path));

        Program pgm(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS);
        pgm.loadModule("SqlUtil");
        pgm.loadModule("Util");
        pgm.parsePending(ReadOnlyFile::readTextFile(file_path), file_path);
        pgm.parse("", "migration-runner");

        # Call the migration entry point: migrate(ds, table_cache, schema_info, verbose)
        pgm.callFunction("migrate", ds, table_cache, schema_info, verbose);
    }

    #! Gets a table from the cache or creates one from the datasource
    private AbstractTable getTable(string table_name, *SqlUtil::Tables table_cache) {
        if (table_cache) {
            *AbstractTable t = table_cache{table_name};
            if (t) {
                return t;
            }
        }
        # Fall back to creating table from datasource
        return (new SqlUtil::Table(ds, table_name)).getTable();
    }

    #! Validates that a required field exists in a migration step
    /** @param step the migration step hash
        @param field the required field name

        @throw MIGRATION-ERROR the field is missing
    */
    private validateStepField(hash<auto> step, string field) {
        if (!step.hasKey(field)) {
            throw "MIGRATION-ERROR", sprintf("migration step %y action %y is missing required field %y",
                step.id ?? "unknown", step.action, field);
        }
    }

    #! Logs a message if verbose
    private logMsg(string msg) {
        if (verbose > 0) {
            if (log_callback) {
                log_callback(msg);
            } else {
                printf("%s\n", msg);
            }
        }
    }
} # class MigrationExecutor

} # namespace Schema
