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
    private *hash<auto> getTablesImpl() {
        if (!schema_data.tables) {
            return NOTHING;
        }

        if (!expanded_tables) {
            string driver = getDatasource().getDriverName();
            expanded_tables = DataSchemaLoader::expandAutoTriggers(schema_data.tables, driver);
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
        if (schema_data.migrations && !first_time_install && initial_schema_info.current_version) {
            MigrationExecutor executor(getDatasource(), access_level, verbose);
            executor.executeMigrations(schema_data.migrations, initial_schema_info.current_version,
                getVersion(), "post_align", table_cache, initial_schema_info);
        }
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
