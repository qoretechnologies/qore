# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DataSchema class definition

/** DataSchema.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Access level constants for controlling migration action permissions
/** @since Schema 1.5
*/
public const AL_UNTRUSTED = "untrusted";
public const AL_TRUSTED = "trusted";
public const AL_ADMIN = "admin";

#! A data-driven schema class that loads schema definitions from .ysm/.jsm files
/** This class extends AbstractVersionedSchema to support pure data schema definitions
    loaded from YAML (.ysm) or JSON (.jsm) files.

    It handles:
    - Loading and normalizing schema definitions
    - Auto-trigger expansion for the connected database driver
    - DPQL expression evaluation in reference data
    - Version-based migration execution with access level enforcement

    @par Example usage
    @code{.py}
    Datasource ds("pgsql:user/pass@host/db");
    DataSchema schema(ds, "my_schema.ysm");
    schema.align(True, 1);
    @endcode

    @since Schema 1.5
*/
public class DataSchema inherits AbstractVersionedSchema {
    private {
        #! The normalized schema data (auto! prevents type narrowing on mixed-type values)
        hash<auto!> schema_data;

        #! The access level for migration actions
        string access_level;

        #! Expanded tables (with auto_triggers resolved for the driver)
        *hash<auto> expanded_tables;

        #! Columns with \c populate expressions that need backfill after DDL alignment.
        #! Keyed by table name → column name → \c {populate: <expr>, notnull: bool}.
        #! Populated by \c extractPopulateColumns() in \c getTablesImpl(); consumed by
        #! \c executePopulateBackfill() in \c doPostAlignment().
        *hash<string, hash<string, hash<auto>>> pending_populates;

        #! Stored verbose level for use in checkUpgrade (set by alignIntern)
        int align_verbose = 0;
    }

    #! Creates the schema from a file path
    /** @param ds the datasource connection
        @param file_path the path to the .ysm or .jsm file
        @param opts schema options (data_ts, index_ts, db_opts)
        @param al access level for migration actions
    */
    constructor(Qore::SQL::AbstractDatasource ds, string file_path,
            *hash<SchemaOptionInfo> opts, string al = AL_TRUSTED) {
        access_level = al;
        schema_data = DataSchemaLoader::loadFile(file_path);
        initDataSchema(ds, opts);
    }

    #! Creates the schema from a pre-loaded hash
    /** @param ds the datasource connection
        @param data the pre-loaded and normalized schema data hash
        @param opts schema options
        @param al access level for migration actions
    */
    constructor(Qore::SQL::AbstractDatasource ds, hash<auto> data,
            *hash<SchemaOptionInfo> opts, string al = AL_TRUSTED) {
        access_level = al;
        schema_data = data;
        initDataSchema(ds, opts);
    }

    #! Initializes the schema after schema_data is set
    /** Calls the parent init() and then sets versioned schema fields.
        This avoids the issue where the parent constructor calls virtual
        methods before schema_data is available.

        @since Schema 1.1
    */
    private initDataSchema(Qore::SQL::AbstractDatasource ds, *hash<SchemaOptionInfo> opts) {
        init(ds, opts);
        # Set versioned schema fields (normally done in AbstractVersionedSchema constructor)
        version_table = getVersionTable();
        version_column = getVersionColumn();
        version_where = getVersionWhere();
    }

    #! Returns the schema name
    private string getNameImpl() {
        return schema_data.schema.name;
    }

    #! Returns the schema version
    private string getVersionImpl() {
        return schema_data.schema.version;
    }

    #! Returns the version table name
    private string getVersionTableImpl() {
        return schema_data.schema.version_table ?? "schema_version";
    }

    #! Returns the version column name
    private string getVersionColumnImpl() {
        return schema_data.schema.version_column ?? "version";
    }

    #! Returns the where clause hash for the version row
    private hash getVersionWhereImpl() {
        return schema_data.schema.version_where ?? {"name": schema_data.schema.name};
    }

    #! Returns the table definitions with auto_triggers expanded for the current driver
    /** Also extracts \c populate metadata from column definitions: for columns with
        \c populate + \c notnull that are genuinely new (not already in the DB), the \c notnull
        flag is temporarily stripped so DDL alignment adds them as nullable. The \c populate
        expressions are stored in \c pending_populates for \c executePopulateBackfill().
    */
    private *hash<auto> getTablesImpl() {
        if (!schema_data.tables) {
            return NOTHING;
        }

        if (!expanded_tables) {
            string driver = getDatasource().getDriverName();
            expanded_tables = DataSchemaLoader::expandAutoTriggers(schema_data.tables, driver);
            # extract populate metadata and prepare columns for safe DDL addition
            pending_populates = extractPopulateColumns(\expanded_tables);
        }

        return expanded_tables;
    }

    #! Returns sequence definitions
    private *hash<auto> getSequencesImpl() {
        return schema_data.sequences;
    }

    #! Returns type definitions
    private *hash<auto> getTypesImpl() {
        return schema_data.types;
    }

    #! Returns function definitions
    private *hash<auto> getFunctionsImpl() {
        return schema_data.functions;
    }

    #! Returns procedure definitions
    private *hash<auto> getProceduresImpl() {
        return schema_data.procedures;
    }

    #! Returns package definitions
    private *hash<auto> getPackagesImpl() {
        return schema_data.packages;
    }

    #! Returns materialized view definitions
    private *hash<auto> getMaterializedViewsImpl() {
        return schema_data.materialized_views;
    }

    #! Returns index options
    private *hash<auto> getIndexOptionsImpl() {
        return schema_data.index_options;
    }

    #! Returns column options
    private *hash<auto> getColumnOptionsImpl() {
        return schema_data.column_options;
    }

    #! Returns strict reference data with DPQL expressions evaluated
    private *hash<auto> getStrictReferenceDataHashImpl() {
        return evalReferenceData(schema_data.reference_data.strict);
    }

    #! Returns normal reference data with DPQL expressions evaluated
    private *hash<auto> getReferenceDataHashImpl() {
        return evalReferenceData(schema_data.reference_data.normal);
    }

    #! Returns create-only reference data with DPQL expressions evaluated
    private *hash<auto> getCreateOnlyReferenceDataImpl() {
        return evalReferenceData(schema_data.reference_data.create_only);
    }

    #! Returns insert-only reference data with DPQL expressions evaluated
    private *hash<auto> getInsertOnlyReferenceDataImpl() {
        return evalReferenceData(schema_data.reference_data.insert_only);
    }

    #! Returns the access level
    string getAccessLevel() {
        return access_level;
    }

    #! Returns the loaded schema data hash
    hash<auto> getSchemaData() {
        return schema_data;
    }

    #! Returns the list of migrations defined in the schema
    *list<auto> getMigrations() {
        return schema_data.migrations;
    }

    #! Overrides alignIntern to capture verbose level for use in checkUpgrade
    private SchemaCallbackHelper alignIntern(bool force = False, int verbose = 0, bool do_msgs = True) {
        align_verbose = verbose;
        return AbstractSchema::alignIntern(force, verbose, do_msgs);
    }

    #! Executes pre-alignment migration steps when upgrading
    /** Called by AbstractVersionedSchema when the database schema is older than the target.

        @param current_version the current schema version in the database
        @return True to proceed with the upgrade
    */
    private bool checkUpgrade(string current_version) {
        if (schema_data.migrations) {
            MigrationExecutor executor(getDatasource(), access_level, align_verbose);
            executor.executeMigrations(schema_data.migrations, current_version,
                getVersion(), "pre_align");
        }
        return True;
    }

    #! Executes post-alignment migration steps
    /** Called after DDL alignment is complete, before reference data management.

        @param table_cache the table cache populated during alignment
        @param first_time_install True if this is the initial schema creation
        @param initial_schema_info schema info hash with current_version
        @param verbose the verbosity level
    */
    private doPostAlignment(SqlUtil::Tables table_cache, bool first_time_install,
            *hash<auto> initial_schema_info, int verbose) {
        # backfill new columns with populate expressions (must run before migrations so
        # migration steps can depend on the populated columns)
        if (pending_populates && !first_time_install) {
            executePopulateBackfill(table_cache, verbose);
        }

        if (schema_data.migrations && !first_time_install && initial_schema_info.current_version) {
            MigrationExecutor executor(getDatasource(), access_level, verbose);
            executor.executeMigrations(schema_data.migrations, initial_schema_info.current_version,
                getVersion(), "post_align", table_cache, initial_schema_info);
        }
    }

    #! Extracts populate metadata and strips populate/notnull from genuinely new columns
    /** For each table that has columns with a \c populate attribute, this method:
        - snapshots the current DB columns (to detect which columns are genuinely new)
        - for each populate column that is new: stores the populate info, strips \c notnull
          (so DDL adds it nullable) and strips \c populate (not a GenericColumnInfo key)
        - for columns that already exist: just strips \c populate

        @param tables reference to the expanded tables hash (modified in place)
        @return a hash of populate metadata (table → column → info), or NOTHING if none
    */
    private *hash<string, hash<string, hash<auto>>> extractPopulateColumns(
            reference<hash<auto>> tables) {
        Qore::SQL::AbstractDatasource ds = getDatasource();
        hash<string, hash<string, hash<auto>>> result();

        foreach hash<auto> tpair in (tables.pairIterator()) {
            string table_name = tpair.key;
            if (!tpair.value.columns) {
                continue;
            }

            # quick scan: does any column have a populate key?
            bool has_populate = False;
            foreach hash<auto> cpair in (tpair.value.columns.pairIterator()) {
                if (exists cpair.value.populate) {
                    has_populate = True;
                    break;
                }
            }
            if (!has_populate) {
                continue;
            }

            # snapshot the columns that currently exist in the DB for this table
            hash<string, bool> existing_cols();
            try {
                SqlUtil::Table t(ds, table_name);
                AbstractTable at = t.getTable();
                if (at.checkExistence()) {
                    existing_cols = map {$1: True}, at.describe().keys();
                }
            } catch () {
                # table doesn't exist yet — no existing columns
            }

            foreach hash<auto> cpair in (tpair.value.columns.pairIterator()) {
                string col_name = cpair.key;
                if (!exists cpair.value.populate) {
                    continue;
                }

                auto populate_val = cpair.value.populate;
                # strip populate from the column def (not a GenericColumnInfo key)
                delete tables{table_name}.columns{col_name}.populate;

                if (existing_cols{col_name}) {
                    # column already exists in the DB — skip populate, keep notnull as-is
                    continue;
                }

                # column is genuinely new — store populate info and temporarily strip notnull
                hash<auto!> info();
                info.populate = populate_val;
                info.notnull = tables{table_name}.columns{col_name}.notnull ?? False;

                if (!result.hasKey(table_name)) {
                    result{table_name} = {};
                }
                result{table_name}{col_name} = info;

                # strip notnull so DDL adds the column as nullable
                if (info.notnull) {
                    delete tables{table_name}.columns{col_name}.notnull;
                }
            }
        }

        return result ? result : NOTHING;
    }

    #! Executes backfill and NOT NULL re-application for columns with populate attributes
    /** For each table/column in \c pending_populates:
        1. Renders the populate expression to a SQL UPDATE
        2. Executes \c UPDATE ... SET col = <expr> WHERE col IS NULL
        3. If the column was originally NOT NULL: \c ALTER TABLE ... ALTER COLUMN ... SET NOT NULL

        @param table_cache the table cache from alignment
        @param verbose the verbosity level
    */
    private executePopulateBackfill(SqlUtil::Tables table_cache, int verbose) {
        Qore::SQL::AbstractDatasource ds = getDatasource();

        foreach hash<auto> tpair in (pending_populates.pairIterator()) {
            string table_name = tpair.key;

            # get a live table object reflecting the current DB state after DDL alignment
            AbstractTable table = (new SqlUtil::Table(ds, table_name)).getTable();

            foreach hash<auto> cpair in (tpair.value.pairIterator()) {
                string col_name = cpair.key;
                hash<auto> pop_info = cpair.value;

                if (verbose > 0) {
                    info_callback(sprintf("populating %s.%s", table_name, col_name),
                        AbstractDatabase::AC_Update);
                }

                # render and execute the backfill UPDATE
                string update_sql = renderPopulateUpdate(table, col_name, pop_info.populate);
                int rows = ds.exec(update_sql);

                if (verbose > 0) {
                    info_callback(sprintf("  %d row%s populated", rows, rows == 1 ? "" : "s"),
                        AbstractDatabase::AC_Update);
                }

                # re-apply NOT NULL if it was stripped
                if (pop_info.notnull) {
                    string alter_sql = sprintf("alter table %s alter column %s set not null",
                        table.getSqlName(), col_name);
                    ds.exec(alter_sql);
                    if (verbose > 0) {
                        info_callback(sprintf("  NOT NULL constraint applied to %s.%s",
                            table_name, col_name), AbstractDatabase::AC_Update);
                    }
                }
            }
        }
    }

    #! Renders a populate expression to a complete UPDATE SQL statement
    /** Supports 4 forms:
        - \c {field: col} — field reference: \c UPDATE t SET new_col = col WHERE new_col IS NULL
        - literal scalar — \c UPDATE t SET col = <literal> WHERE col IS NULL
        - \c {exp: name, args: [...]} — DPQL expression rendered via the SqlUtil expression map
          when possible, otherwise evaluated client-side as a bind value
        - \c {sql: {driver: "..."}} — raw per-driver SQL expression embedded in the UPDATE

        @param table the live AbstractTable object for expression rendering
        @param col_name the column being populated
        @param populate_val the populate value from the schema definition
        @return a complete SQL UPDATE statement ready for execution
    */
    private string renderPopulateUpdate(AbstractTable table, string col_name, auto populate_val) {
        string tname = table.getSqlName();

        # literal scalar: inline the value (SqlUtil will handle quoting via %v)
        if (populate_val.typeCode() != NT_HASH) {
            return sprintf("update %s set %s = %s where %s is null",
                tname, col_name, table.getSqlValue(populate_val), col_name);
        }

        hash<auto> ph = populate_val;

        # {field: col} shorthand — copy from another column
        if (ph.hasKey("field")) {
            return sprintf("update %s set %s = %s where %s is null",
                tname, col_name, ph.field, col_name);
        }

        # {sql: {pgsql: "...", ...}} — raw per-driver SQL
        if (ph.hasKey("sql")) {
            string driver = getDatasource().getDriverName();
            *string raw_sql;
            if (ph.sql.typeCode() == NT_HASH) {
                raw_sql = ph.sql{driver};
            } else if (ph.sql.typeCode() == NT_STRING) {
                raw_sql = ph.sql;
            }
            if (!raw_sql.val()) {
                throw "POPULATE-ERROR", sprintf("table %y column %y: populate.sql has no "
                    "entry for driver %y", table.getName(), col_name, driver);
            }
            return sprintf("update %s set %s = %s where %s is null",
                tname, col_name, raw_sql, col_name);
        }

        # {exp: name, args: [...]} — DPQL expression
        if (ph.hasKey("exp")) {
            # try the SqlUtil expression map first (server-side SQL rendering)
            hash<auto> exp_map = table.getExpressionMap();
            string exp_name = ph.exp;
            *hash<auto> sql_exp = exp_map{exp_name};
            if (!sql_exp) {
                # check OperatorAliasMap for alternate names
                *string alias = DataProvider::OperatorAliasMap{exp_name};
                if (alias.val()) {
                    sql_exp = exp_map{alias};
                }
            }

            if (sql_exp) {
                # expression exists in the SQL expression map — use SqlUtil to render it
                hash<auto> set_hash = {};
                set_hash{col_name} = <DataProviderExpression>{
                    "exp": exp_name,
                    "args": ph.args ?? (),
                };
                hash<auto> where_hash = {col_name: NULL};
                # getUpdateSql returns {sql: "...", args: (...)}
                hash<auto> cmd = table.getUpdateSql(set_hash, where_hash);
                return cmd.sql;
            }

            # fallback: evaluate expression client-side and use as a literal
            auto result_val = AbstractDataProvider::evalGenericExpression({},
                <DataProviderExpression>{
                    "exp": exp_name,
                    "args": ph.args ?? (),
                });
            return sprintf("update %s set %s = %s where %s is null",
                tname, col_name, table.getSqlValue(result_val), col_name);
        }

        throw "POPULATE-ERROR", sprintf("table %y column %y: unrecognized populate format; "
            "expected a literal, {field: col}, {exp: name, args: [...]}, or {sql: {driver: '...'}}; "
            "got: %y", table.getName(), col_name, ph);
    }

    #! Evaluates DPQL expressions in reference data
    private *hash<auto> evalReferenceData(*hash<auto> ref_hash) {
        if (!ref_hash) {
            return NOTHING;
        }

        hash<auto!> result();
        foreach hash<auto> pair in (ref_hash.pairIterator()) {
            result{pair.key} = DataSchemaLoader::evalReferenceDataExpressions(pair.value);
        }
        return result;
    }
} # class DataSchema

} # namespace Schema
