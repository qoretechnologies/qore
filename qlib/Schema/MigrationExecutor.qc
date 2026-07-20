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
    "reconcile_table": True,
};

#! Migration actions that can change database metadata
public const DdlActions = {
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
    "reconcile_table": True,
    # Raw SQL and scripts can contain DDL.  Their affected table set is
    # deliberately unknown, so callbacks invalidate the whole datasource.
    "sql": True,
    "script": True,
};

#! Actions requiring trusted or admin access
/** @since Schema 1.5
*/
public const RestrictedActions = {
    "sql": True,
    "script": True,
};

#! Executes one DataSchema migration action
/** This convenience function provides the same access checks, DDL cache
    callback, and bounded action result as @ref MigrationExecutor while
    allowing embedding applications to execute a single reviewed action.

    @param ds datasource used for the action
    @param step one DataSchema migration action
    @param access_level action access level
    @param verbose verbosity level
    @param ddl_callback datasource-aware metadata callback
    @return a bounded action report when the action provides one, otherwise
        @ref NOTHING

    @throw MIGRATION-ACCESS-ERROR if the action is not allowed at the
        requested access level
    @throw MIGRATION-ERROR if the action is invalid
    @throw DATA-SCHEMA-TABLE-MIGRATION-ARG-ERROR if a
        \c reconcile_table contract is invalid
    @throw DATA-SCHEMA-TABLE-MIGRATION-PREFLIGHT-ERROR if current metadata or
        data no longer supports a reviewed reconciliation
    @throw DATA-SCHEMA-TABLE-MIGRATION-VALIDATION-ERROR if a reconciliation
        outcome cannot be verified
    @throw SCHEMA-DDL-CHANGE-CALLBACK-ERROR if DDL was committed but the
        metadata callback failed

    @par Example
    Execute one reviewed additive reconciliation and invalidate the embedding
    application's table metadata cache with the exact datasource object:
    @code{.py}
code<nothing(AbstractDatasource, list<string>)> callback =
    nothing sub (AbstractDatasource changed_ds, list<string> tables) {
        application_cache.invalidate(changed_ds, tables);
    };
hash<auto> report = execute_migration_action(ds, {
    "action": "reconcile_table",
    "contract": reviewed_contract,
}, AL_TRUSTED, 0, callback);
    @endcode

    @since Schema 1.7
*/
public *hash<auto> sub execute_migration_action(
        Qore::SQL::AbstractDatasource ds, hash<auto> step,
        string access_level = AL_TRUSTED, int verbose = 0,
        *code<nothing(Qore::SQL::AbstractDatasource, list<string>)>
            ddl_callback = NOTHING) {
    MigrationExecutor executor(ds, access_level, verbose, NOTHING,
        ddl_callback);
    return executor.executeStep(step, NOTHING);
}

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

        #! Optional datasource-aware table metadata cache callback
        *code<nothing(Qore::SQL::AbstractDatasource, list<string>)>
            ddl_change_callback;

        #! Schema info passed through to migration scripts
        *hash<auto> schema_info;

        #! Local cache for tables referenced by migration steps without an external alignment cache
        SqlUtil::Tables migration_table_cache;

        #! Bounded reports returned by migration actions with deterministic verification
        list<hash<auto>> action_results;
    }

    #! Creates the executor
    /** @param n_ds the datasource for SQL execution
        @param al the access level (AL_UNTRUSTED, AL_TRUSTED, AL_ADMIN)
        @param v verbosity level
        @param log optional logging callback: code<nothing(string)>
        @param ddl_callback optional callback invoked after an action can
            have changed table metadata; an empty table list means the whole
            datasource
    */
    constructor(Qore::SQL::AbstractDatasource n_ds, string al = AL_TRUSTED, int v = 0,
            *code<nothing(string)> log,
            *code<nothing(Qore::SQL::AbstractDatasource, list<string>)>
                ddl_callback) {
        ds = n_ds;
        access_level = al;
        verbose = v;
        log_callback = log;
        ddl_change_callback = ddl_callback;
        migration_table_cache = new SqlUtil::Tables();
    }

    #! Executes migration steps for the given version transition
    /** Filters migrations by version, then executes the specified phase (pre_align or post_align).

        @param migrations the full list of migration definitions from the schema
        @param current_version the current schema version in the database
        @param target_version the target schema version
        @param phase "pre_align" or "post_align"
        @param table_cache optional Tables cache (available in post_align phase)
        @param n_schema_info optional schema info hash
    */
    executeMigrations(*list<auto> migrations, string current_version, string target_version,
            string phase, *SqlUtil::Tables table_cache, *hash<auto> n_schema_info) {
        if (!migrations) {
            return;
        }

        schema_info = n_schema_info;

        foreach hash<auto> migration in (migrations) {
            if (!MigrationExecutor::migrationApplies(migration, current_version, target_version)) {
                continue;
            }

            *list<auto> steps = migration{phase};
            if (!steps) {
                continue;
            }

            # Apply per-step source-version gating before ordering; steps whose source-version
            # bounds exclude the current (source) database version are skipped
            steps = select steps, MigrationExecutor::stepApplies($1, current_version);
            if (!steps) {
                continue;
            }

            logMsg(sprintf("executing %s migration steps for version %s: %s",
                phase, migration.version, migration.description ?? ""));

            # Resolve step ordering (dependency-aware)
            list<auto> ordered = resolveStepOrder(steps);

            foreach hash<auto> step in (ordered) {
                if (*hash<auto> result = executeStep(step, table_cache)) {
                    action_results += result;
                }
            }
        }
    }

    #! Returns bounded reports produced by executed migration actions
    /** Most migration actions return no report.  Actions such as
        \c reconcile_table return deterministic aggregate verification without
        exposing row values.

        @return reports accumulated by the executor

        @since Schema 1.7
    */
    list<hash<auto>> getActionResults() {
        return action_results;
    }

    #! Returns @ref True "True" if a migration applies for the given version transition
    /** A migration applies when its declared @c version advances the schema (i.e.
        <tt>current_version < migration.version <= target_version</tt>) @b and the source (current)
        database version satisfies the migration's optional @c min_source_version and
        @c max_source_version bounds (both inclusive).

        Source-version gating lets a migration that operates on objects introduced in a later
        schema version (e.g. a backfill on a table that did not exist in older databases) be skipped
        when upgrading from a version that predates those objects; such databases create the objects
        fresh during DDL alignment and require no migration.

        @param migration the migration definition hash
        @param current_version the current (source) schema version in the database
        @param target_version the target schema version

        @return @ref True "True" if the migration should be executed

        @since Schema 1.6
    */
    static bool migrationApplies(hash<auto> migration, string current_version, string target_version) {
        # the migration must advance the version: current < migration.version <= target
        if (Util::compare_version(migration.version, current_version) <= 0) {
            return False;
        }
        if (Util::compare_version(migration.version, target_version) > 0) {
            return False;
        }
        return MigrationExecutor::sourceVersionInRange(migration, current_version);
    }

    #! Returns @ref True "True" if a single migration step applies for the given source version
    /** A step applies when the source (current) database version satisfies the step's optional
        @c min_source_version and @c max_source_version bounds (both inclusive). A step with no
        bounds always applies.

        @param step the migration step (action) hash
        @param current_version the current (source) schema version in the database

        @return @ref True "True" if the step should be executed

        @since Schema 1.6
    */
    static bool stepApplies(hash<auto> step, string current_version) {
        return MigrationExecutor::sourceVersionInRange(step, current_version);
    }

    #! Returns @ref True "True" if \a current_version is within the optional source-version bounds of \a def
    /** @param def a migration or step hash that may carry @c min_source_version and/or
        @c max_source_version keys
        @param current_version the current (source) schema version in the database

        @return @ref True "True" if \a current_version is >= @c min_source_version (when set) and
        <= @c max_source_version (when set)

        @since Schema 1.6
    */
    private static bool sourceVersionInRange(hash<auto> def, string current_version) {
        if (def.min_source_version.val()
                && Util::compare_version(current_version, def.min_source_version) < 0) {
            return False;
        }
        if (def.max_source_version.val()
                && Util::compare_version(current_version, def.max_source_version) > 0) {
            return False;
        }
        return True;
    }

    #! Executes a single migration step
    /** @param step the migration action hash
        @param table_cache optional table cache for table operations

        @return a bounded action report when the action provides one,
            otherwise @ref NOTHING

        @throw MIGRATION-ACCESS-ERROR if the action requires higher access than currently allowed
        @throw MIGRATION-ERROR unknown action, missing required field, or step execution failure
    */
    *hash<auto> executeStep(hash<auto> step,
            *SqlUtil::Tables table_cache) {
        string action = step.action;

        # Access level check
        if (RestrictedActions{action} && access_level == AL_UNTRUSTED) {
            throw "MIGRATION-ACCESS-ERROR", sprintf("action %y requires 'trusted' or 'admin' access level; "
                "current level is %y", action, access_level);
        }

        logMsg(sprintf("  step: %s%s", action, step.table ? sprintf(" on %s", step.table) : ""));

        *hash<auto> result;
        try {
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
                case "reconcile_table":
                    validateStepField(step, "contract");
                    result = DataSchemaTableMigration::execute_with_report(
                        ds, step.contract, verbose);
                    break;
                case "sql":
                    executeSql(step);
                    break;
                case "script":
                    executeScript(step, table_cache);
                    break;
                default:
                    throw "MIGRATION-ERROR", sprintf(
                        "unknown migration action: %y", action);
            }
        } catch (hash<ExceptionInfo> ex) {
            if (DdlActions{action}) {
                notifyDdlChangeNoThrow(affectedTables(step));
            }
            rethrow;
        }
        if (DdlActions{action}) {
            notifyDdlChange(affectedTables(step));
        }
        return result;
    }

    #! Returns the bounded affected-table projection for a DDL action
    private list<string> affectedTables(hash<auto> step) {
        hash<string, bool> tables = {};
        if (step.table.val()) {
            tables{step.table} = True;
        }
        if (step.to.val() && step.action == "rename_table") {
            tables{step.to} = True;
        }
        if (step.target_table.val()) {
            tables{step.target_table} = True;
        }
        if (step.action == "reconcile_table") {
            hash<auto> contract = step.contract.typeCode() == NT_HASH
                ? step.contract : {};
            if (contract.table.val()) {
                tables{contract.table} = True;
            }
            if (contract.backfill.quarantine_table.val()) {
                tables{contract.backfill.quarantine_table} = True;
            }
        }
        return sort(keys tables);
    }

    #! Invokes the configured metadata callback after DDL
    private notifyDdlChange(list<string> tables) {
        if (!ddl_change_callback) {
            return;
        }
        try {
            ddl_change_callback(ds, tables);
        } catch (hash<ExceptionInfo> ex) {
            throw "SCHEMA-DDL-CHANGE-CALLBACK-ERROR", sprintf(
                "DDL was applied, but the datasource metadata callback "
                    "failed: %s: %s", ex.err, ex.desc);
        }
    }

    #! Preserves an action exception if conservative invalidation also fails
    private notifyDdlChangeNoThrow(list<string> tables) {
        if (!ddl_change_callback) {
            return;
        }
        try {
            ddl_change_callback(ds, tables);
        } catch (hash<ExceptionInfo> ex) {
            logMsg(sprintf("    DDL metadata callback failed while "
                "preserving the migration error: %s: %s", ex.err, ex.desc));
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
        string sql;
        table.renameColumn(step.from, step.to, \sql);
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
        table.cache();
        string sql;
        if (table_cache) {
            table.rename(step.to, \sql, table_cache);
        } else {
            table.rename(step.to, \sql, migration_table_cache);
        }
    }

    #! Executes an ADD TRIGGER action
    /** @param step requires \c table, \c name, and \c source fields
        @param table_cache optional table cache
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
        on_error ds.rollback();
        on_success ds.commit();
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
        validateStepField(step, "file");
        on_error ds.rollback();
        on_success ds.commit();
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
            if (table_cache.hasKey(table_name)) {
                return table_cache{table_name};
            }
        } else if (migration_table_cache.hasKey(table_name)) {
            return migration_table_cache{table_name};
        }
        # Fall back to creating table from datasource
        AbstractTable table = (new SqlUtil::Table(ds, table_name)).getTable();
        if (!table_cache) {
            migration_table_cache.add(table_name, table);
        }
        return table;
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
