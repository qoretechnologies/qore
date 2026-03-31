# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AbstractSchema class definition

/** AbstractSchema.qc Copyright 2014 - 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the Schema module
public namespace Schema {

    #! the AbstractSchema class is a base class to assist with automatic schema management
    /** This class allows the following actions to be performed:
        - schema creation, upgrades, and downgrades (through schema alignment)
        - schema deletion / dropping

        This class does not provide for any intelligence based on the schema template's version
        and a schema version string stored in the schema itself; this is provided instead by the
        @ref Schema::AbstractVersionedSchema "AbstractVersionedSchema" class.

        The following abstract methods must be re-implemented in subclasses to define basic schema attributes:
        - getNameImpl(): returns the schema name
        - getVersionImpl(): returns the schema version

        The following methods should be re-implemented in subclasses to define schema definitions from which DDL will be generated::
        - getTablesImpl(): returns table definitions
        - getSequencesImpl(): returns sequence definitions
        - getTypesImpl(): returns type definitions
        - getFunctionsImpl(): returns function definitions
        - getProceduresImpl(): returns procedure definitions
        - getPackagesImpl(): returns (Oracle) package definitions
        - getMaterializedViewsImpl(): returns materialized view definitions

        The following methods should be re-implemented in subclasses to define schema creation options to affect the generated DDL:
        - getIndexOptionsImpl(): returns index options
        - getColumnOptionsImpl(): returns column options
        - getGenericOptionsImpl(): returns generic options

        The following methods should be re-implemented in subclasses to define schema reference data:
        - getStrictReferenceDataHashImpl(): returns a hash of "strict reference data", describing the only data that can appear in the target table
        - getReferenceDataHashImpl(): returns a hash of "normal reference data", describing data that must appear in the target table, however additional rows are also acceptable
        - getCreateOnlyReferenceDataImpl(): returns a hash of "create-only reference data", describing data that is written to the table when the table is created
        - getInsertOnlyReferenceDataImpl(): returns a hash of "insert-only reference data", describing data that will only be inserted if missing, otherwise existing rows with changed data are left unchanged
    */
    public class AbstractSchema {
        private {
            #! the name of the schema
            string name;
            #! the version of the schema
            string version;

            #! the schema template, as assembled from method callbacks providing schema element definitions
            hash schema;

            #! index options, as provided by getIndexOptions()
            *hash index_options;
            #! creation options, as provided by getCreationOptions()
            *hash creation_options;
            #! column options, as provided by getColumnOptions()
            *hash column_options;
            #! combined creation and column options
            *hash all_options;

            #! the datasource for the schema
            Qore::SQL::AbstractDatasource ds;
            #! the name of the database driver
            string drv;

            #! the Database object for the schema
            SqlUtil::Database db;

            #! just callback options
            hash pure_callback_opts;
            #! callback options plus all options
            hash callback_opts;

            #! the info callback for schema operations; can be used for explicit logging
            code info_callback;

            #! explicit data tablespace name
            *string data_ts;

            #! explicit index tablespace name
            *string index_ts;
        }

        #! creates the object from the arguments
        /** @param ds the datasource for the schema
            @param dts an optional data tablespace name for the schema
            @param its an optional index tablespace name for the schema
            @param opts options for @ref SqlUtil::Database::constructor()
        */
        constructor(Qore::SQL::AbstractDatasource ds, *string dts, *string its, *hash opts) {
            init(ds, <SchemaOptionInfo>{
                "data_ts": dts,
                "index_ts": its,
                "db_opts": opts,
            });
        }

        #! creates the object from the arguments
        /** @param ds the datasource for the schema
            @param opts options for @ref SqlUtil::Database::constructor()
        */
        constructor(Qore::SQL::AbstractDatasource ds, *hash<SchemaOptionInfo> opts) {
            init(ds, opts);
        }

        #! Protected constructor for subclasses that call init() themselves
        /** @since Schema 1.1
        */
        private constructor() {
        }

        #! returns the name of the schema
        string getName() {
            return getNameImpl();
        }

        #! returns the version of the schema
        string getVersion() {
            return getVersionImpl();
        }

        #! returns the datasource used for the schema
        Qore::SQL::AbstractDatasource getDatasource() {
            return ds;
        }

        #! returns the @ref SqlUtil::AbstractDatabase "AbstractDatabase" object used for the schema
        SqlUtil::AbstractDatabase getDatabase() {
            return db.getDatabase();
        }

        #! outputs a log message prefixed with the schema name
        /** the log string is output with a following newline, and the terminal I/O is flushed
        */
        logpf(string fmt, ...) {
            printf("%s: %s\n", db.getDatasourceDesc(), vsprintf(fmt, argv));
            flush();
        }

        #! outputs a log message
        /** the log string is output with a following newline, and the terminal I/O is flushed
        */
        log(string fmt, ...) {
            vprintf(fmt + "\n", argv);
            flush();
        }

        #! outputs a log message without any newline
        /** the terminal I/O is flushed as well
        */
        logProgress(string str, ...) {
            printf(str, argv);
            flush();
        }

        #! executes a schema alignment action on the database with the current schema template
        /** this causes the database to be aligned with the schema template, which will result in:
            - the schema being created in an empty schema
            - the schema being upgraded if an older version of the schema exists in the database
            - the schema being downgraded if a newer version of the schema exists in the database

            @return the number of changes made to the schema

            @note if other database objects not belonging to the schema already exist in the given database, they are
            not touched unless explicitly dropped by upgrade or downgrade code in the schema class; note that any
            undefined attributes of tables defined in the schema (columns, indexes, constraints, etc) will be dropped,
            however
        */
        int align(bool force = False, int verbose = 0, bool do_msgs = True) {
            SchemaCallbackHelper sch = alignIntern(force, verbose, do_msgs);
            return sch.change_count + (foldl $1 + $2, sch.data_change_map.iterator());
        }

        #! executes a schema alignment action on the database with the current schema template
        /** this causes the database to be aligned with the schema template, which will result in:
            - the schema being created in an empty schema
            - the schema being upgraded if an older version of the schema exists in the database
            - the schema being downgraded if a newer version of the schema exists in the database

            @return the SchemaCallbackHelper with information on the changes made

            @note if other database objects not belonging to the schema already exist in the given database, they are
            not touched unless explicitly dropped by upgrade or downgrade code in the schema class; note that any
            undefined attributes of tables defined in the schema (columns, indexes, constraints, etc) will be dropped,
            however
        */
        hash<SchemaAlignmentInfo> alignWithInfo(bool force = False, int verbose = 0, bool do_msgs = True) {
            return alignIntern(force, verbose, do_msgs).getInfo();
        }

        #! executes a schema alignment action on the database with the current schema template
        /** this causes the database to be aligned with the schema template, which will result in:
            - the schema being created in an empty schema
            - the schema being upgraded if an older version of the schema exists in the database
            - the schema being downgraded if a newer version of the schema exists in the database

            @return the SchemaCallbackHelper with information on the changes made

            @note if other database objects not belonging to the schema already exist in the given database, they are
            not touched unless explicitly dropped by upgrade or downgrade code in the schema class; note that any
            undefined attributes of tables defined in the schema (columns, indexes, constraints, etc) will be dropped,
            however
        */
        private SchemaCallbackHelper alignIntern(bool force = False, int verbose = 0, bool do_msgs = True) {
            SchemaCallbackHelper sch(self, verbose);

            pure_callback_opts = sch.getCallbacks();
            info_callback = pure_callback_opts.info_callback;
            callback_opts = all_options + pure_callback_opts;

            bool first_time_install = checkFirstTimeInstall();

            *hash<auto> initial_schema_info;

            if (first_time_install) {
                if (do_msgs) {
                    logpf("creating %s schema %s", name, version);
                }
            } else {
                if (!checkUpdateSchema(force, \initial_schema_info)) {
                    return sch;
                }
                sch.sql_cache = ();
            }

            on_error {
                ds.rollback();
                if (verbose > 0) {
                    logpf("ALIGNMENT ROLLED BACK");
                }
            }
            on_success {
                ds.commit();
                if (verbose > 0) {
                    logpf("COMMITTED ALIGNMENT");
                }
            }

            Tables table_cache();

%ifndef QoreDebug
            db.getAlignSql(schema, callback_opts + ("force": True), table_cache);
%else
            list<string> al = db.getAlignSql(schema, callback_opts + ("force": True), table_cache);

            if (al.size() != sch.sql_cache.size()) {
                if (verbose >= 0) {
                    printf("callback had %d lines, getAlignSql() had %d lines\n", sch.sql_cache.size(), al.size());
                }

                hash<string, bool> schh;

                # make a hash of the sqlcache
                if (al.size() > sch.sql_cache.size()) {
                    map schh.$1 = True, sch.sql_cache;

                    foreach string sql in (al) {
                        if (!schh{sql} && verbose >= 0) {
                            printf("MISSING IN CALLBACK %d: %s\n", $#, sql);
                        }
                    }
                } else {
                    map schh.$1 = True, al;

                    foreach string sql in (sch.sql_cache) {
                        if (!schh{sql} && verbose >= 0) {
                            printf("MISSING IN ALIGN %d: %s\n", $#, sql);
                        }
                    }
                }

                throw "GET-ALIGN-SCHEMA-DEBUG-ERROR", "unexpected result";
            }
%endif

            if (do_msgs) {
                if (sch.dot_count) {
                    logProgress("\n");
                    sch.dot_count = 0;
                }
                if (sch.change_count) {
                    log("%d change%s made; schema is now at version %s", sch.change_count, sch.change_count == 1 ? "" : "s", version);
                } else {
                    log("no changes made to schema version %s", version);
                }
            }

            doPostAlignment(table_cache, first_time_install, initial_schema_info, verbose);

            *hash<auto> strict_data = getStrictReferenceDataHash();
            *hash<auto> ref_data = getReferenceDataHash();
            *hash<auto> insert_only_data = getInsertOnlyReferenceData();
            *hash<auto> co_data = getCreateOnlyReferenceData();

            if (strict_data || ref_data || co_data || insert_only_data) {
                if (verbose >= 0) {
                    if (verbose != 1) {
                        log("verifying reference data");
                    } else {
                        logProgress("verifying reference data: ");
                    }
                }

                int upsert_strategy = getUpsertStrategy(first_time_install);

                # install "strict reference data", where there can be no other rows in the table
                if (strict_data) {
                    map AbstractSchema::doTable(table_cache{$1.key}, $1.value, upsert_strategy, True, verbose,
                        \sch.data_change_map), strict_data.pairIterator();
                }

                # ensure that "normal" reference data is in place
                if (ref_data) {
                    map AbstractSchema::doTable(table_cache{$1.key}, $1.value, upsert_strategy, False, verbose,
                        \sch.data_change_map), ref_data.pairIterator();
                }

                # do "insert-only" data
                if (insert_only_data) {
                    map AbstractSchema::doTable(table_cache{$1.key}, $1.value, AbstractTable::UpsertInsertOnly, False,
                        verbose, \sch.data_change_map), insert_only_data.pairIterator();
                }

                # do "create only" data
                if (co_data && first_time_install) {
                    map AbstractSchema::doTable(table_cache{$1.key}, $1.value, upsert_strategy, False, verbose,
                        \sch.data_change_map), co_data.pairIterator();
                }

                postDataActions(table_cache, first_time_install, initial_schema_info, verbose);

                if (verbose >= 0) {
                    if (!verbose) {
                        logProgress("\n");
                    }

                    if (!sch.data_change_map) {
                        log("reference data: OK (no changes)");
                    } else {
                        log("reference data: %s", (foldl $1 + ", " + $2, (map sprintf("%s: %d", $1.key, $1.value),
                            sch.data_change_map.pairIterator())));
                    }
                }
            }

            return sch;
        }

        #! executes a schema drop operation in the database
        drop(bool force = False, int verbose = 0, bool do_msgs = True) {
            SchemaCallbackHelper sch(self, verbose);

            callback_opts = sch.getCallbacks(force);

            if (!checkExistence()) {
                if (!force) {
                    if (do_msgs) {
                        logpf("schema appears to be empty: use -f to force the drop");
                    }
                    return;
                }
                if (do_msgs) {
                    logpf("schema appears to be empty: drop forced");
                }
            } else {
                if (!checkDropSchema(force))
                    return;
                sch.sql_cache = ();
            }

            on_success ds.commit();
            on_error ds.rollback();

%ifndef QoreDebug
            db.getDropSchemaSql(schema, callback_opts);
%else
            list<auto> al = db.getDropSchemaSql(schema, callback_opts);

            if (al.size() != sch.sql_cache.size()) {
                printf("ERROR: callback had %d lines, getDropSchemaSql() had %d lines\n", sch.sql_cache.size(), al.size());

                # make a hash of each
                hash<string, bool> schh = map {$1: True}, sch.sql_cache;

                foreach string sql in (al) {
                    if (!schh{sql})
                        printf("MISSING IN CALLBACK %d: %s\n", $#, sql);
                }
                throw "DROP-SCHEMA-DEBUG-ERROR", "unexpected result";
            }
%endif

            if (do_msgs) {
                if (sch.dot_count)
                    logProgress("\n");
                log("%d object%s dropped (%d error%s); schema has been dropped", sch.change_count,
                    sch.change_count == 1 ? "" : "s", sch.error_count, sch.error_count == 1 ? "" : "s");
            }
        }

        #! returns index options
        /** this function wraps getIndexOptionsImpl() and sets the index tablespace
            name as an index option automatically if set in the constructor() if not
            already set
        */
        *hash<auto> getIndexOptions() {
            *hash h += getIndexOptionsImpl();
            if (index_ts && !h.index_tablespace)
                h.index_tablespace = index_ts;
            return h;
        }

        #! returns schema creation options
        /** this function wraps getGenericOptionsImpl() and getIndexOptionsImpl()
            and sets the data and index tablespace names as creation options
            automatically if set in the constructor() if not already set
        */
        *hash<auto> getCreationOptions() {
            *hash<auto> h = combineOptions(getGenericOptionsImpl(), getIndexOptionsImpl());
            if (data_ts && !h.data_tablespace)
                h.data_tablespace = data_ts;
            if (index_ts && !h.index_tablespace)
                h.index_tablespace = index_ts;
            return h;
        }

        #! returns column options
        /** this function wraps getColumnOptionsImpl(); for forwards-compatibility, do not override this
            method in subclasses, instead re-implement getColumnOptionsImpl()
        */
        *hash<auto> getColumnOptions() {
            return getColumnOptionsImpl();
        }

        #! returns table definitions
        /** this function wraps getTablesImpl(); for forwards-compatibility, do not override this
            method in subclasses, instead re-implement getTablesImpl()
        */
        *hash<auto> getTables() {
            return getTablesImpl();
        }

        #! returns sequence definitions
        /** this function wraps getSequencesImpl(); for forwards-compatibility, do not override this
            method in subclasses, instead re-implement getSequencesImpl()
        */
        *hash<auto> getSequences() {
            return getSequencesImpl();
        }

        #! returns type definitions
        /** this function wraps getTypesImpl(); for forwards-compatibility, do not override this
            method in subclasses, instead re-implement getTypesImpl()
        */
        *hash<auto> getTypes() {
            return getTypesImpl();
        }

        #! returns function definitions
        /** this function wraps getFunctionsImpl(); for forwards-compatibility, do not override this
            method in subclasses, instead re-implement getFunctionsImpl()
        */
        *hash<auto> getFunctions() {
            return getFunctionsImpl();
        }

        #! returns stored procedure definitions
        /** this function wraps getProceduresImpl(); for forwards-compatibility, do not override this
            method in subclasses, instead re-implement getProceduresImpl()
        */
        *hash<auto> getProcedures() {
            return getProceduresImpl();
        }

        #! returns (Oracle) package definitions
        /** this function wraps getPackagesImpl(); for forwards-compatibility, do not override this
            method in subclasses, instead re-implement getPackagesImpl()
        */
        *hash<auto> getPackages() {
            return getPackagesImpl();
        }

        #! returns materialized view definitions
        /** this function wraps getMaterializedViewsImpl(); for forwards-compatibility, do not override this
            method in subclasses, instead re-implement getMaterializedViewsImpl()
        */
        *hash<auto> getMaterializedViews() {
            return getMaterializedViewsImpl();
        }

        #! returns a hash of "strict reference data", describing the only data that can appear in the target table
        /** this function wraps getStrictReferenceDataHashImpl(); for forwards-compatibility, do not override this
            method in subclasses, instead re-implement getStrictReferenceDataHashImpl()
        */
        *hash<auto> getStrictReferenceDataHash() {
            return getStrictReferenceDataHashImpl();
        }

        #! returns a hash of "normal reference data", describing data that must appear in the target table, however additional rows are also acceptable
        /** this function wraps getReferenceDataHashImpl(); for forwards-compatibility, do not override this
            method in subclasses, instead re-implement getReferenceDataHashImpl()
        */
        *hash<auto> getReferenceDataHash() {
            return getReferenceDataHashImpl();
        }

        #! returns a hash of "create-only reference data", describing data that is written to the table when the table is created
        /** this function wraps (); for forwards-compatibility, do not override this
            method in subclasses, instead re-implement ()
        */
        *hash<auto> getCreateOnlyReferenceData() {
            return getCreateOnlyReferenceDataImpl();
        }

        #! returns a hash of "create-only reference data", describing data that will only be inserted if missing, otherwise existing rows with changed data are left unchanged
        /** this function wraps (); for forwards-compatibility, do not override this
            method in subclasses, instead re-implement ()
        */
        *hash<auto> getInsertOnlyReferenceData() {
            return getInsertOnlyReferenceDataImpl();
        }

        #! shared constructor initialization
        private init(Qore::SQL::AbstractDatasource ds, *hash<SchemaOptionInfo> opts) {
            self.ds = ds;
            name = getName();
            version = getVersion();
            data_ts = opts.data_ts;
            index_ts = opts.index_ts;

            drv = ds.getDriverName();

            db = new Database(ds, opts.db_opts);

            index_options = getIndexOptions();
            creation_options = getCreationOptions();
            column_options = getColumnOptions();

            all_options = AbstractSchema::combineOptions(creation_options, column_options);

            schema = {
                "tables": getTables(),
                "sequences": getSequences(),
                "types": getTypes(),
                "functions": getFunctions(),
                "procedures": getProcedures(),
                "packages": getPackages(),
                "materialized_views": getMaterializedViews(),
            };
        }

        #! returns the name of the schema
        private abstract string getNameImpl();

        #! returns the version of the schema
        private abstract string getVersionImpl();

        #! returns index options
        /** the default behavior of this method is to return @ref nothing, reimplement in subclasses to return custom options
        */
        private *hash<auto> getIndexOptionsImpl() {
        }

        #! returns column options
        /** the default behavior of this method is to return @ref nothing, reimplement in subclasses to return custom options
        */
        private *hash<auto> getColumnOptionsImpl() {
        }

        #! returns schema creation options
        /** the default behavior of this method is to return @ref nothing, reimplement in subclasses to return custom options
        */
        private *hash<auto> getGenericOptionsImpl() {
        }

        #! returns table definitions
        /** the default behavior of this method is to return @ref nothing, reimplement in subclasses to return custom configuration
        */
        private *hash<auto> getTablesImpl() {
        }

        #! returns sequence definitions
        /** the default behavior of this method is to return @ref nothing, reimplement in subclasses to return custom configuration
        */
        private *hash<auto> getSequencesImpl() {
        }

        #! returns type definitions
        /** the default behavior of this method is to return @ref nothing, reimplement in subclasses to return custom configuration
        */
        private *hash<auto> getTypesImpl() {
        }

        #! returns function definitions
        /** the default behavior of this method is to return @ref nothing, reimplement in subclasses to return custom configuration
        */
        private *hash<auto> getFunctionsImpl() {
        }

        #! returns stored procedure definitions
        /** the default behavior of this method is to return @ref nothing, reimplement in subclasses to return custom configuration
        */
        private *hash<auto> getProceduresImpl() {
        }

        #! returns (Oracle) package definitions
        /** the default behavior of this method is to return @ref nothing, reimplement in subclasses to return custom configuration
        */
        private *hash<auto> getPackagesImpl() {
        }

        #! returns materialized view definitions
        /** the default behavior of this method is to return @ref nothing, reimplement in subclasses to return custom configuration
        */
        private *hash<auto> getMaterializedViewsImpl() {
        }

        #! returns a hash of "strict reference data", describing the only data that can appear in the target table
        /** the default behavior of this method is to return @ref nothing, reimplement in subclasses to return custom configuration
        */
        private *hash<auto> getStrictReferenceDataHashImpl() {
        }

        #! returns a hash of "normal reference data", describing data that must appear in the target table, however additional rows are also acceptable
        /** the default behavior of this method is to return @ref nothing, reimplement in subclasses to return custom configuration
        */
        private *hash<auto> getReferenceDataHashImpl() {
        }

        #! returns a hash of "create-only reference data", describing data that is written to the table when the table is created
        /** the default behavior of this method is to return @ref nothing, reimplement in subclasses to return custom configuration
        */
        private *hash<auto> getCreateOnlyReferenceDataImpl() {
        }

        #! returns a hash of "insert-only reference data", describing data that will only be inserted if missing, otherwise existing rows with changed data are left unchanged
        /** the default behavior of this method is to return @ref nothing, reimplement in subclasses to return custom configuration
        */
        private *hash<auto> getInsertOnlyReferenceDataImpl() {
        }

        #! returns @ref True "True" if the method can detect that DB objects defined in the schema exist in the database
        /** Currently this method checks if any of tables, sequences, functions, or procedures defined in the schema template exists.
            If so, this method returns @ref True "True", otherwise it returns @ref False "False"
        */
        private bool checkExistence() {
            # NOTE: we cannot check firstKey() only as before. It fails
            # if the tables hash is changed to contain non-existing table
            # at the 1st place.

            # object types are sorted by their cost of construction DESC
            if (db.supportsSequences() && schema.sequences) {
                foreach string name in (keys schema.sequences) {
                    if (db.getSequence(name)) {
                        return True;
                    }
                }
            }
            if (schema.functions) {
                foreach string name in (keys schema.functions) {
                    if (db.getFunction(name)) {
                        return True;
                    }
                }
            }
            if (schema.procedures) {
                foreach string name in (keys schema.procedures) {
                    if (db.getProcedure(name)) {
                        return True;
                    }
                }
            }
            if (schema.tables) {
                foreach string name in (keys schema.tables) {
                    if (db.getTable(name)) {
                        return True;
                    }
                }
            }
            return False;
        }

        #! returns @ref True "True" if checkExistence() returns @ref False "False"
        private bool checkFirstTimeInstall() {
            return !checkExistence();
        }

        #! this method returns @ref True "True" if the schema can be dropped
        private bool checkDropSchema(bool force) {
            logpf("dropping schema version %s", version);
            return True;
        }

        #! this method returns @ref True "True" if the schema can be updated
        private bool checkUpdateSchema(bool force, reference<hash<auto>> initial_schema_info) {
            logpf("verifying/aligning schema version %s", version);
            return True;
        }

        #! this method is called after schema structural alignment has been executed but before schema reference data management
        private doPostAlignment(Tables table_cache, bool first_time_install, *hash<auto> initial_schema_info, int verbose) {
            doPostAlignment(table_cache, first_time_install, initial_schema_info);
        }

        #! this method is called after schema structural alignment has been executed but before schema reference data management
        private doPostAlignment(Tables table_cache, bool first_time_install, *hash<auto> initial_schema_info) {
            # this method intentionally left empty
        }

        #! this method is called to return the upsert strategy before schema reference data management
        private int getUpsertStrategy(bool first_time_install) {
            return first_time_install ? AbstractTable::UpsertInsertFirst : AbstractTable::UpsertUpdateFirst;
        }

        #! this method is called after schema reference data has been managed
        private postDataActions(Tables table_cache, bool first_time_install, *hash<auto> initial_schema_info, int verbose) {
            postDataActions(first_time_install);
        }

        #! this method is called after schema reference data has been managed
        private postDataActions(bool first_time_install) {
            # this method intentionally left empty
        }

        #! this method is called to perform schema reference data managment on the given table with the given data
        private doTable(AbstractTable table, list rows, int upsert_strategy, bool delete_others, int verbose,
                reference<hash<string, int>> sh) {
            # if we are reporting the status, then change to "UpsertSelectFirst" so that we can only report updated rows
            if (verbose > 0) {
                if (upsert_strategy != AbstractTable::UpsertInsertOnly)
                    upsert_strategy = AbstractTable::UpsertSelectFirst;
                log("checking table %s", table.getName());
            }

            bool dot = False;

            hash<string, int> tsh;
            code cb = sub (string table_name, hash row, int result) {
                if (result == AbstractTable::UR_Unchanged) {
                    if (verbose > 1) {
                        logProgress(".");
                        dot = True;
                    }
                    return;
                }
                string change = AbstractTable::UpsertResultMap{result};
                # increment table change
                ++tsh{change};
                # increment global change
                ++sh{change};
                if (!verbose)
                    logProgress(AbstractTable::UpsertResultLetterMap{result});
                else if (verbose > 1) {
                    if (dot) {
                        dot = False;
                        logProgress("\n");
                    }
                    log("* reference data %s: %y: %s", table_name, row, change);
                    log("checking table %s", table_name);
                }
            };

            table.upsertFromIterator(AbstractSchema::getRows(rows).iterator(), upsert_strategy, ("delete_others": delete_others, "info_callback": cb));

            # show table summary if verbose = 1
            if (verbose > 0) {
                if (tsh) {
                    if (verbose == 1 || dot)
                        logProgress("\n");
                    log("** reference data %s: %s", table.getName(), (foldl $1 + ", " + $2,
                        (map sprintf("%s: %d", $1.key, $1.value), tsh.pairIterator())));
                } else {
                    if (verbose > 1) {
                        if (dot)
                            logProgress("\n");
                        logProgress("** ");
                    }
                    log("OK (%d row%s verified)", (rows.size() - 1), (rows.size() - 1) == 1 ? "" : "s");
                }
            }
        }

        #! returns a list of hashes from a list of lists where the first list element in the top-level list is a list of column names, and subsequent lists are column values
        /** meant to be used to translate reference data lists into lists of hashes to be used for upserting in reference data tables during schema management
        */
        static list getRows(*softlist<auto> l) {
            list<auto> ql = ();
            *softlist<auto> hdr = shift l;

            foreach softlist<auto> data in (l) {
                ql += (map {$1: data[$#]}, hdr);
            }

            return ql;
        }

        #! combines option hashes into a single target option hash
        static hash combineOptions(*hash h, ...) {
            foreach *hash h1 in (argv) {
                # h1 can be NOTHING which will fail with RUNTIME-TYPE-ERROR: <lvalue> expects type '*hash', but got type 'int' instead
                if (!h1)
                    continue;
                map h{$1} = AbstractSchema::combineOptions(h{$1}, remove h1{$1}), keys h, h1.hasKey($1);
                h += h1;
            }
            # also the h can be NOTHING in case of no inputs
            return h ?? {};
        }
    }
} # namespace Schema
