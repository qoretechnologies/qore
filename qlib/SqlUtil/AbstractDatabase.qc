# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AbstractDatabase class definition

/** AbstractDatabase.qc Copyright 2019 - 2024 Qore Technologies, s.r.o.

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

#! contains all public definitions in the SqlUtil module
public namespace SqlUtil {
#! the base abstract class for the database implementation
public class AbstractDatabase inherits AbstractSqlUtilBase {
    public {
        #! database options
        /** The following keys can be set for this option:
            - \c native_case: (\c bool_type) if @ref True "True" then objects will be returned in the case the database server returns them in even if the database server uses case-insensitive names; normally in this case %SqlUtil will convert the names to lower-case
        */
        const DatabaseOptions = (
            "native_case": Type::Boolean,
        );

        #! generic cache options
        /** The following keys can be set for this option:
            - \c table_cache: (@ref SqlUtil::Tables "Tables") an optional table cache for maintaining cached tables and foreign key relationships between tables
        */
        const CacheOptions = (
            "table_cache": "Tables",
        );

        #! generic callback options
        /** The following keys can be set for this option:
            - \c info_callback: see @ref info_callback
            - \c sql_callback: see @ref sql_callback
            - \c sql_callback_executed: see @ref sql_callback_executed
        */
        const CallbackOptions = (
            "info_callback": "code",
            "sql_callback": "code",
            "sql_callback_executed": Type::Boolean,
        );

        /** @defgroup info_actions Info Callback Action Codes
            These are the action codes used with the @ref info_callback in schema and table alignment
            @see
            - @ref ActionMap
            - @ref ActionDescMap
            - @ref ActionLetterMap
            */
        #/@{
        #! used when an existing object matches the template and no changes are made
        const AC_Unchanged = 0;

        #! used when a new object is created
        const AC_Create = 1;

        #! used when an object is dropped
        const AC_Drop = 2;

        #! used when an object is renamed
        const AC_Rename = 3;

        #! used when an object is modified in place
        const AC_Modify = 4;

        #! used when a table is truncated
        const AC_Truncate = 5;

        #! used when an element is added to an existing object
        const AC_Add = 6;

        #! used when an object is recreated (usually dropped and recreated in place)
        const AC_Recreate = 7;

        #! used when data is inserted in a table
        const AC_Insert = 8;

        #! used when data is updated in a table
        const AC_Update = 9;

        #! used when data is deleted in a table
        const AC_Delete = 10;

        #! used when dropping object but the object is not present
        const AC_NotFound = 11;
        #/@}

        #! maps from action codes to action descriptions
        const ActionMap = (
            AC_Unchanged: "unchanged",
            AC_Create: "create",
            AC_Drop: "drop",
            AC_Rename: "rename",
            AC_Modify: "modify",
            AC_Truncate: "truncate",
            AC_Add: "add",
            AC_Recreate: "recreate",
            AC_Insert: "insert",
            AC_Update: "update",
            AC_Delete: "delete",
            AC_NotFound: "not found",
        );

        #! maps from action descriptions to action codes
        const ActionDescMap = (
            "unchanged": AC_Unchanged,
            "create": AC_Create,
            "drop": AC_Drop,
            "rename": AC_Rename,
            "modify": AC_Modify,
            "truncate": AC_Truncate,
            "add": AC_Add,
            "recreate": AC_Recreate,
            "insert": AC_Insert,
            "update": AC_Update,
            "delete": AC_Delete,
            "not found": AC_NotFound,
        );

        #! maps from action codes to action letter codes
        const ActionLetterMap = (
            AC_Unchanged: ".",
            AC_Create: "C",
            AC_Drop: "D",
            AC_Rename: "N",
            AC_Modify: "M",
            AC_Truncate: "T",
            AC_Add: "A",
            AC_Recreate: "R",
            AC_Insert: "I",
            AC_Update: "U",
            AC_Delete: "X",
            AC_NotFound: ".",
        );

        #! default generic creation options
        /** This option is comprised of \c AbstractDatabase::CallbackOptions plus the following keys:
            - \c replace: (\c bool_type) if @ref True "True" and supported by the underlying db driver, \c "create or replace" text is used when creating objects
            - \c table_cache: (@ref SqlUtil::Tables "Tables") an optional table cache for maintaining cached tables and foreign key relationships between tables
            - \c data_tablespace: (@ref string_type) a string giving the data tablespace to use for tables
            - \c index_tablespace: (@ref string_type) a string giving the index tablespace to use for indexes
        */
        const CreationOptions = CallbackOptions + (
            "replace": Type::Boolean,
            "table_cache": "Tables",
            "data_tablespace": Type::String,
            "index_tablespace": Type::String,
        );

        #! default generic schema description / alignment options
        /** Currently this option is identical to \c AbstractDatabase::CreationOptions plus the following keys:
            - \c force: (\c bool_type) is @ref True "True" and supported by the underlying db driver, \c FORCE or \c CASCADE options are used when dropping or aligning objects
        */
        const AlignSchemaOptions = CreationOptions + (
            "force": Type::Boolean,
        );

        #! default generic drop schema options
        /** Currently this option is identical to \c AbstractDatabase::CallbackOptions plus the following keys:
            - \c force: (\c bool_type) is @ref True "True" and supported by the underlying db driver, \c FORCE or \c CASCADE options are used when dropping objects
        */
        const DropSchemaOptions = CallbackOptions + (
            "force": Type::Boolean,
        );

        #! default generic schema description keys
        /** The following keys can be set in @ref schema_desc_hash "schema description hashes":
            - \c tables: a hash keyed by table name set to @ref table_desc_hash "table description hash" values
            - \c table_map: a hash for automatically renaming tables; if the source name (key) exists and the target name (value) does not exist, then the source table is automatically renamed
            - \c sequences: a hash keyed by sequence name set to \c AbstractDatabase::SequenceDescriptionOptions "sequence hash<auto> description" values
            - \c sequence_map: a hash for automatically renaming sequences; if the source name (key) exists and the target name (value) does not exist, then the source sequence is automatically renamed
            - \c functions: a hash keyed by function name set to function source string values
            - \c function_map: a hash for automatically renaming functions; if the source name (key) exists and the target name (value) does not exist, then the source function is automatically renamed
            - \c procedures: a hash keyed by procedure name set to procedure source string values
            - \c procedure_map: a hash for automatically renaming procedures; if the source name (key) exists and the target name (value) does not exist, then the source procedure is automatically renamed

            @see @ref schema_desc_hash for more information
        */
        const SchemaDescriptionOptions = (
            "tables": Type::Hash,
            "table_map": Type::Hash,

            "sequences": Type::Hash,
            "sequence_map": Type::Hash,

            "functions": Type::Hash,
            "function_map": Type::Hash,

            "procedures": Type::Hash,
            "procedure_map": Type::Hash,

            #"views": Type::Hash,
            #"view_map": Type::Hash,
        );

        #! default generic sequence description keys
        /** The following keys can be set in sequence description hashes:
            - \c start: (@ref int_type) the starting value for the sequence
            - \c increment: (@ref int_type) the increment value for the sequence
            - \c end: (@ref int_type) the ending value for the sequence
        */
        const SequenceDescriptionOptions = (
            "start": Type::Int,
            "increment": Type::Int,
            "end": Type::Int,
        );

        #! Options for computeStatistics()
        const ComputeStatisticsOptions = (
            "tables"            : "softstringlist",
        );

        #! Options for reclaimSpace()
        const ReclaimSpaceOptions = (
            "tables"            : "softstringlist",
        );
    }

    private {
        #! native case option
        bool native_case = False;

        # AbstractDatabase::getPhysicalSize() return in a case of no data
        const GET_PHYSICAL_DB_SIZE_NOVAL = -1;
    }

    #! creates the object; private constructor
    /** @param nds the AbstractDatasource for the connection to the database
        @param nopts a hash of options for the function creation string; see \c AbstractDatabase::DatabaseOptions for common options; each driver can support additional driver-specific options

        @throw OPTION-ERROR invalid or unsupported option passed
        */
    private constructor(AbstractDatasource nds, *hash nopts) : AbstractSqlUtilBase(nds, nopts) {
        validateOptionsIntern("OPTION-ERROR", getDatabaseOptions(), \nopts);
        if (nopts.native_case)
            native_case = True;
    }

    #! See \c DBFeaturesConstants
    list features() {
        return featuresImpl();
    }

    static doOkCallback(*hash<auto> opt, int ac, string type, string name, *string table, *string info) {
        if (opt.info_callback) {
            bool c = (type == "column");
            if (c)
                name = sprintf("%s.%s", table, name);

            string str = sprintf("%s %s", type, name);
            if (table && !c)
                str += sprintf(" on %s", table);

            if (ac == AC_Unchanged)
                str += " does not need updating";
            else if (ac == AC_NotFound)
                str += " does not exist or has already been dropped";

            if (info)
                str += sprintf(" (%s)", info);

            opt.info_callback(str, ac, type, name, table, NOTHING, info);
        }
    }

    static private runInfoCallback(code info_callback, int ac, string type, string name, *string table,
            *string new_name, *string info) {
        bool c = (type == "column");
        if (c)
            name = sprintf("%s.%s", table, name);
        string str = sprintf("%s %s %s", ActionMap{ac}, type, name);
        if (table && !c)
            str += sprintf(" %s %s", ac == AC_Drop ? "from" : "on", table);
        if (new_name)
            str += sprintf(" to %s", new_name);
        if (info)
            str += sprintf(" (%s)", info);
        info_callback(str, ac, type, name, table, new_name, info);
    }

    static *string doCallback(*hash<auto> opt, *string sql, int ac, string type, string name, *string table,
            *string new_name, *string info) {
        if (!sql)
            return;
        if (opt.info_callback) {
            AbstractDatabase::runInfoCallback(opt.info_callback, ac, type, name, table, new_name, info);
        }
        if (opt.sql_callback) {
            try {
                opt.sql_callback(sql);
            } catch (hash<ExceptionInfo> ex) {
                if (opt.error_callback) {
                    opt.error_callback(ac, type, name, table, new_name, info, sql, ex);
                }
                rethrow;
            }
        }
        return sql;
    }

    static list doCallback(*hash<auto> opt, list sql, int ac, string type, string name, *string table,
            *string new_name, *string info) {
        if (!sql)
            return sql;
        if (opt.info_callback) {
            AbstractDatabase::runInfoCallback(opt.info_callback, ac, type, name, table, new_name, info);
        }
        if (opt.sql_callback) {
            string my_sql;
            try {
                map opt.sql_callback(my_sql = $1), sql;
            } catch (hash<ExceptionInfo> ex) {
                if (opt.error_callback) {
                    opt.error_callback(ac, type, name, table, new_name, info, my_sql, ex);
                }
                printf("ERR: %y\n", my_sql);
                rethrow;
            }
        }
        return sql;
    }

    #! executes some SQL with optional arguments so that if an error occurs the current transaction state is not lost
    /** @par Example:
        @code{.py}
t.tryExec("drop table tmp_table");
        @endcode

        Include any arguments in the parameter list after the \a sql argument

        @param sql the SQL to execute

        @return any return value from the SQL command executed
        */
    auto tryExec(string sql) {
        return tryExecArgsImpl(sql, argv);
    }

    #! executes some SQL with optional arguments so that if an error occurs the current transaction state is not lost
    /** @par Example:
        @code{.py}
t.tryExec("delete from tmp_table where id = %v and name = %v", arglist);
        @endcode

        @param sql the SQL to execute
        @param args the bind / placeholder or other arguments corresponding to the SQL string

        @return any return value from the SQL command executed
        */
    auto tryExecArgs(string sql, *softlist<auto> args) {
        return tryExecArgsImpl(sql, args);
    }

    #! executes some SQL so that if an error occurs the current transaction state is not lost
    /** @par Example:
        @code{.py}
t.tryExecRaw("drop table tmp_table");
        @endcode

        Include any arguments in the parameter list after the \a sql argument

        @param sql the SQL to execute

        @return any return value from the SQL command executed
        */
    auto tryExecRaw(string sql) {
        return tryExecRawImpl(sql);
    }

    #! accepts a hash argument describing a database schema and returns a list of SQL strings that can be used to align the structure and configuration of the current schema with that of the argument; if the objects described are identical then an empty list is returned
    /** @par Example:
        @code{.py}
list l = db.getAlignSql(schema_hash);
        @endcode

        @param schema_hash the @ref schema_desc_hash "schema description hash" that decribes the target state of the schema
        @param opt a hash of options for the SQL creation strings; see \c AbstractDatabase::AlignSchemaOptions for common options; each driver can support additional driver-specific options
        @param table_cache an optional object of class Tables to return the cache of AbstractTable objects keyed by table name for the schema template (ie target table definitions)

        @return a list of SQL strings that can be used to align the structure and configuration of the current schema with that of the argument; if the objects described are identical then an empty list is returned

        @throw SCHEMA-DESCRIPTION-ERROR the @ref schema_desc_hash "schema description hash" has an error or a required object does not exist
        @throw OPTION-ERROR invalid or unsupported option passed
    */
    list<auto> getAlignSql(hash schema_hash, *hash<auto> opt, *Tables table_cache) {
        validateOptionsIntern("SCHEMA-DESCRIPTION-ERROR", getSchemaDescriptionOptions(), \schema_hash);
        validateOptionsIntern("OPTION-ERROR", getAlignSchemaOptions(), \opt);
        validateHashKeysForWhitespaces(schema_hash);

        # get creation options
        *hash copt = opt{getAlignSchemaOptions().keys()};

        if (!table_cache)
            table_cache = new Tables();

        # setup db table cache
        opt.db_table_cache = new Tables();

        list l = ();

        string drv = getDriverName();
        AbstractDatabase::checkDriverOptions(\schema_hash, drv);
        AbstractDatabase::checkDriverOptions(\schema_hash.sequence_map, drv);
        AbstractDatabase::checkDriverOptions(\schema_hash.sequences, drv);

        # call driver-specific align schema sql function (start)
        list tl = getAlignSqlImpl(schema_hash, opt + ("start": True));
        if (tl) l += tl;

        # rename any sequences
        HashIterator i(schema_hash.sequence_map);
        while (i.next()) {
            string key = i.getKey();
            auto value = i.getValue();
            if (value.typeCode() != NT_STRING)
                throw "SCHEMA-DESCRIPTION-ERROR", sprintf("sequence_map %y value is not a string in the schema description hash; got type %y instead (%y)", key, value.type(), value);

            *AbstractSequence seq = getSequence(key);
            if (seq) {
                l += AbstractDatabase::doCallback(opt, seq.getRenameSql(value), AC_Rename, "sequence", key, NOTHING, value);
                # remove sequence from list since we know it exists
                remove schema_hash.sequences{value};
            }
        }

        # create any missing sequences
        i = new HashIterator(schema_hash.sequences);
        while (i.next()) {
            string name = i.getKey();

            if (i.getValue().typeCode() != NT_HASH) {
                throw "SEQUENCE-DESCRIPTION-ERROR",
                        sprintf("sequence '%s' schema definition has to be hash, but it's %s",
                                name, i.getValue().type());
            }

            hash<auto> sh = i.getValue();
            validateOptionsIntern("SEQUENCE-DESCRIPTION-ERROR", getSequenceDescriptionOptions(), \sh);

            # see if sequence exists
            if (getSequence(name))
                continue;

            AbstractSequence seq = makeSequence(name, sh.start, sh.increment, sh.end);
            l += AbstractDatabase::doCallback(opt, seq.getCreateSql(), AC_Create, "sequence", name);
        }

        # create template tables
        table_cache.populate(ds, schema_hash.tables, copt);

        # rename any tables
        AbstractDatabase::checkDriverOptions(\schema_hash.table_map, drv);
        foreach hash<auto> th in (schema_hash.table_map) {
            if (th.value.typeCode() != NT_STRING)
                throw "SCHEMA-DESCRIPTION-ERROR", sprintf("table_map %y value is not a string in the schema description hash; got type %y instead (%y)", th.key, th.value.type(), th.value);

            Table t(ds, th.key);
            if (t.checkExistence()) {
                l += AbstractDatabase::doCallback(opt, t.getRenameSql(th.value), AC_Rename, "table", th.key, NOTHING, th.value);
                opt.db_table_cache.add(th.value, t);
            }
        }

        # now process DB tables
        AbstractDatabase::checkDriverOptions(\schema_hash.tables, drv);
        foreach string name in (schema_hash.tables.keyIterator()) {
            #printf("processing table %y\n", name);
            AbstractTable t;
            if (opt.db_table_cache.hasKey(name))
                t = opt.db_table_cache{name};
            else {
                t = (new Table(ds, name)).getTable();
                # store in db_table_cache
                opt.db_table_cache.add(name, t);
            }

            tl = t.getAlignSql(table_cache{name}, opt + schema_hash.tables{name}.rename + ("table_cache": table_cache));
            if (tl)
                l += tl;
        }

        tl = alignCodeUnlocked("function", schema_hash, \getFunction(), \makeFunction(), opt, Type::String);
        if (tl) l += tl;

        tl = alignCodeUnlocked("procedure", schema_hash, \getProcedure(), \makeProcedure(), opt, Type::String);
        if (tl) l += tl;

        # call driver-specific align schema sql function (end)
        tl = getAlignSqlImpl(schema_hash, opt);
        if (tl) l += tl;

        return l;
    }

    #! accepts a hash argument describing a database schema and returns a list of SQL strings that can be used to drop any existing objects described in the schema; if none of the described objects exist, then an empty list is returned
    /** @par Example:
        @code{.py}
list l = db.getDropSchemaSql(schema_hash);
        @endcode

        @param schema_hash the @ref schema_desc_hash "schema description hash" that decribes the state of the schema
        @param opt a hash of options for the SQL creation strings; see \c AbstractDatabase::DropSchemaOptions for common options; each driver can support additional driver-specific options

        @return a list of SQL strings that can be used to drop any existing objects in the database; if none of the described objects exist, then an empty list is returned

        @throw SCHEMA-DESCRIPTION-ERROR the @ref schema_desc_hash "schema description hash" has an error
        @throw OPTION-ERROR invalid or unsupported option passed
    */
    list<auto> getDropSchemaSql(hash schema_hash, *hash<auto> opt) {
        validateOptionsIntern("SCHEMA-DESCRIPTION-ERROR", getSchemaDescriptionOptions(), \schema_hash);
        validateOptionsIntern("OPTION-ERROR", getDropSchemaOptions(), \opt);

        list l = ();

        string drv = getDriverName();
        AbstractDatabase::checkDriverOptions(\schema_hash, drv);

        # call driver-specific align schema sql function (start)
        list tl = getDropSchemaSqlImpl(schema_hash, opt + ("start": True));
        if (tl) l += tl;

        # now process DB procedures
        AbstractDatabase::checkDriverOptions(\schema_hash.procedures, drv);

        # drop procedures
        tl = dropSqlUnlocked("procedure", schema_hash, \getProcedure(), \makeProcedure(), opt, Type::String);
        if (tl) l += tl;

        # now process DB functions
        AbstractDatabase::checkDriverOptions(\schema_hash.functions, drv);

        # drop functions
        tl = dropSqlUnlocked("function", schema_hash, \getFunction(), \makeFunction(), opt, Type::String);
        if (tl) l += tl;

        # now process DB tables
        AbstractDatabase::checkDriverOptions(\schema_hash.tables, drv);

        # setup tables with names from description but contents from DB
        Tables tables();
        map tables.getIfExists(ds, $1), schema_hash.tables.keyIterator();
        foreach AbstractTable t in (tables.dropIterator()) {
            l += AbstractDatabase::doCallback(opt, t.getDropSql(), AC_Drop, "table", t.getName());
        }

        # now process DB sequences
        AbstractDatabase::checkDriverOptions(\schema_hash.sequences, drv);

        # drop sequences
        tl = dropSqlUnlocked("sequence", schema_hash, \getSequence(), \makeSequenceFromDescription(), opt, Type::Hash);
        if (tl) l += tl;

        # call driver-specific align schema sql function (end)
        # Option "force" is here to drop all objects (where required)
        # without dependencies checking (Oracle NTY for now)
        tl = getDropSchemaSqlImpl(schema_hash, opt + ("force": True) );
        if (tl) l += tl;

        return l;
    }

    private list dropSqlUnlocked(string type, hash schema_hash, code get, code make, *hash<auto> opt,
            string make_arg_type) {
        list l = ();
        hash copt = getCreationOptions();
        string drv = getDriverName();

        # drop objects
        reference<hash> fmap = \schema_hash{type + "s"};
        AbstractDatabase::checkDriverOptions(\fmap, drv);
        foreach string name in (fmap.keyIterator()) {
            AbstractSqlUtilBase::validateOptionIntern("SCHEMA-DESCRIPTION-ERROR", make_arg_type, \fmap{name}, name,
                type + "s");

            # make template object
            object tmpl = make(name, fmap{name}, opt.(copt.keys()));

            # see if object exists
            *object f = get(name);
            if (f)
                l += AbstractDatabase::doCallback(opt, tmpl.getDropSql(opt), AC_Drop, type, name);
            else
                AbstractDatabase::doOkCallback(opt, AC_NotFound, type, name);
        }

        return l;
    }

    private list alignCodeUnlocked(string type, hash schema_hash, code get, code make, *hash<auto> opt,
            string make_arg_type) {
        list l = ();
        hash copt = getCreationOptions();
        string drv = getDriverName();

        hash code_cache;

        # rename objects
        reference<*hash> fmap = \schema_hash{type + "_map"};
        AbstractDatabase::checkDriverOptions(\fmap, drv);
        HashIterator i(fmap);
        while (i.next()) {
            string key = i.getKey();
            auto value = i.getValue();
            if (value.typeCode() != NT_STRING)
                throw "SCHEMA-DESCRIPTION-ERROR", sprintf("%s %y value is not a string in the schema description "
                    "hash; got type %y instead (%y)", type + "_map", key, value.type(), value);

            *AbstractFunction f = get(key);
            if (f) {
                list tl = AbstractDatabase::doCallback(opt, f.getRenameSql(value), AC_Rename, type, key, NOTHING,
                    value);
                if (tl)
                    l += tl;

                # add to function cache
                f.setName(value);
                code_cache{value} = f;
            }
        }

        # create any missing functions; replace updated functions
        reference<hash> fh = \schema_hash{type + "s"};
        AbstractDatabase::checkDriverOptions(\fh, drv);
        foreach string name in (fh.keyIterator()) {
            AbstractSqlUtilBase::validateOptionIntern("SCHEMA-DESCRIPTION-ERROR", make_arg_type, \fh{name}, name, type + "s");

            # make template function
            AbstractFunction templateFunc = make(name, fh{name}, opt.(copt.keys()));

            # see if function exists
            *AbstractFunction f;
            if (code_cache{name})
                f = code_cache{name};
            else {
                f = get(name);
                if (!f) {
                    l += AbstractDatabase::doCallback(opt, templateFunc.getCreateSql(opt), AC_Create, type, name);
                    continue;
                }
            }

            if (!templateFunc.equal(f)) {
                l += AbstractDatabase::doCallback(opt, f.getDropSql(opt), AC_Drop, type, name);
                l += AbstractDatabase::doCallback(opt, templateFunc.getCreateSql(opt), AC_Create, type, name);
            } else
                AbstractDatabase::doOkCallback(opt, AC_Unchanged, type, name);
        }

        return l;
    }

    #! creates a database-specific AbstractSequence object corresponding to the arguments
    /** @par Example:
        @code{.py}
AbstractSequence seq = db.makeSequence("seq_queues");
        @endcode

        @param name the name of the sequence
        @param start the starting value of the sequence
        @param increment the increment value of the sequence
        @param end the endinf value of the sequence, omit for no ending value
        @param opts an optional hash of options for the creation of the sequence; see
        \c AbstractDatabase::CreationOptions for common options; each driver can support additional driver-specific
        options

        @throw OPTION-ERROR invalid or unsupported option passed
        @throw SEQUENCE-ERROR end not compatible with start and increment, increment is zero
    */
    AbstractSequence makeSequence(string name, number start = 1, number increment = 1, *softnumber end,
            *hash<auto> opts) {
        validateOptionsIntern("OPTION-ERROR", getCreationOptions(), \opts);

        # issue #4396: convert letters to lower case if necessary
        if (!opts.native_case) {
            name = name.lwr();
        }
        return makeSequenceImpl(name, start, increment, end, opts);
    }

    AbstractSequence makeSequenceFromDescription(string name, *hash<auto> sh, *hash<auto> opts) {
        validateOptionsIntern("SEQUENCE-DESCRIPTION-ERROR", getSequenceDescriptionOptions(), \sh);

        return makeSequence(name, sh.start, sh.increment, sh.end);
    }

    #! creates a database-specific AbstractTable object corresponding to the arguments
    /** @par Example:
        @code{.py}
AbstractTable table = db.makeTable("table", th);
        @endcode

        @param name the name of the table
        @param desc a @ref table_desc_hash "table description hash" describing the table
        @param opts an optional hash of options for the table creation string; see
        \c SqlUtil::AbstractTable::TableOptions for common options; each driver can support additional
        driver-specific options

        @throw OPTION-ERROR invalid or unsupported option passed

        @see SqlUtil::AbstractTable::constructor()
    */
    AbstractTable makeTable(string name, hash<auto> desc, *hash<auto> opts) {
        # issue #4396: convert letters to lower case if necessary
        if (!opts.native_case) {
            name = name.lwr();
        }
        AbstractTable t = AbstractTable::getTable(ds, name, opts);
        t.setupTable(desc, opts);
        return t;
    }

    #! creates a database-specific AbstractFunction object corresponding to the arguments
    /** @par Example:
        @code{.py}
AbstractFunction f = db.makeFunction("update_queue", src);
        @endcode

        @param name the name of the function
        @param src the database-specific source code of the function (see the database-specific documentation for
        information about the format of this parameter; generally it's simply the normal source code for the function
        required by the database)
        @param opts an optional hash of options for the creation of the function; see
        \c AbstractDatabase::CreationOptions for common options; each driver can support additional driver-specific
        options

        @throw OPTION-ERROR invalid or unsupported option passed
    */
    AbstractFunction makeFunction(string name, string src, *hash<auto> opts) {
        validateOptionsIntern("OPTION-ERROR", getCreationOptions(), \opts);

        # issue #4396: convert letters to lower case if necessary
        if (!opts.native_case) {
            name = name.lwr();
        }
        return makeFunctionImpl(name, src, opts);
    }

    #! creates a database-specific AbstractFunction object for a stored procedure corresponding to the arguments
    /** @par Example:
        @code{.py}
AbstractFunction f = db.makeProcedure("get_queue_info", src);
        @endcode

        @param name the name of the stored procedure
        @param src the database-specific source code of the stored procedure (see the database-specific documentation
        for information about the format of this parameter; generally it's simply the normal source code for the
        stored procedure required by the database)
        @param opt an optional hash of options for the creation of the stored procedure; see
        \c AbstractDatabase::CreationOptions for common options; each driver can support additional driver-specific
        options

        @throw OPTION-ERROR invalid or unsupported option passed
    */
    AbstractFunction makeProcedure(string name, string src, *hash<auto> opt) {
        validateOptionsIntern("OPTION-ERROR", getCreationOptions(), \opt);

        # issue #4396: convert letters to lower case if necessary
        if (!opts.native_case) {
            name = name.lwr();
        }
        return makeProcedureImpl(name, src, opt);
    }

    #! drops the given function if it exists; returns @ref True "True" if the function was dropped, @ref False "False" if not
    /** @par Example:
        @code{.py}
db.dropFunctionIfExists(name);
        @endcode

        @param name the name of the function
        @param opt optional callback options; see \c AbstractDatabase::CallbackOptions for more info

        @return @ref True "True" if the function was dropped, @ref False "False" if not

        @throw OPTION-ERROR invalid or unknown callback option
    */
    bool dropFunctionIfExists(string name, *hash<auto> opt) {
        validateOptionsIntern("OPTION-ERROR", getDropSchemaOptions(), \opt);
        *AbstractFunction f = getFunction(name);
        if (!f)
            return False;
        return AbstractDatabase::doDrop(f.getDropSql(opt), "function", name, opt);
    }

    #! drops the given procedure if it exists; returns @ref True "True" if the procedure was dropped, @ref False "False" if not
    /** @par Example:
        @code{.py}
db.dropProcedureIfExists(name);
        @endcode

        @param name the name of the procedure
        @param opt optional callback options; see \c AbstractDatabase::CallbackOptions for more info

        @return @ref True "True" if the procedure was dropped, @ref False "False" if not

        @throw OPTION-ERROR invalid or unknown callback option
    */
    bool dropProcedureIfExists(string name, *hash<auto> opt) {
        validateOptionsIntern("OPTION-ERROR", getDropSchemaOptions(), \opt);
        *AbstractFunction f = getProcedure(name);
        if (!f)
            return False;
        return AbstractDatabase::doDrop(f.getDropSql(opt), "procedure", name, opt);
    }

    #! drops the given sequence if it exists; returns @ref True "True" if the sequence was dropped, @ref False "False" if not
    /** @par Example:
        @code{.py}
db.dropSequenceIfExists(name);
        @endcode

        @param name the name of the sequence
        @param opt optional callback options; see \c AbstractDatabase::CallbackOptions for more info

        @return @ref True "True" if the sequence was dropped, @ref False "False" if not

        @throw OPTION-ERROR invalid or unknown callback option
    */
    bool dropSequenceIfExists(string name, *hash<auto> opt) {
        validateOptionsIntern("OPTION-ERROR", getDropSchemaOptions(), \opt);
        *AbstractSequence seq = getSequence(name);
        if (!seq)
            return False;
        return AbstractDatabase::doDrop(seq.getDropSql(opt), "sequence", name, opt);
    }

    #! drops the given view if it exists; returns @ref True "True" if the view was dropped, @ref False "False" if not
    /** @par Example:
        @code{.py}
db.dropViewIfExists(name);
        @endcode

        @param name the name of the view
        @param opt optional callback options; see \c AbstractDatabase::CallbackOptions for more info

        @return @ref True "True" if the view was dropped, @ref False "False" if not

        @throw OPTION-ERROR invalid or unknown callback option
    */
    bool dropViewIfExists(string name, *hash<auto> opt) {
        validateOptionsIntern("OPTION-ERROR", getDropSchemaOptions(), \opt);
        *AbstractView obj = getView(name);
        if (!obj)
            return False;
        return AbstractDatabase::doDrop(obj.getDropSql(opt), "view", name, opt);
    }

    #! drops the given table if it exists; returns @ref True "True" if the table was dropped, @ref False "False" if not
    /** @par Example:
        @code{.py}
db.dropTableIfExists(name);
        @endcode

        @param name the name of the table
        @param opt optional callback options; see \c AbstractDatabase::CallbackOptions for more info

        @return @ref True "True" if the table was dropped, @ref False "False" if not

        @throw OPTION-ERROR invalid or unknown callback option
    */
    bool dropTableIfExists(string name, *hash<auto> opt) {
        validateOptionsIntern("OPTION-ERROR", getDropSchemaOptions(), \opt);
        *AbstractTable obj = getTable(name);
        if (!obj)
            return False;
        obj.drop(opt);
        return True;
    }

    #! returns the SQL require to drop the given function if it exists or @ref nothing if the named function does not exist
    /** @par Example:
        @code{.py}
string sql = db.getDropFunctionSqlIfExists(name);
        @endcode

        @param name the name of the function
        @param opt optional callback options; see \c AbstractDatabase::CallbackOptions for more info

        @return the SQL require to drop the given function if it exists or @ref nothing if the named function does not exist

        @throw OPTION-ERROR invalid or unknown callback option
    */
    *string getDropFunctionSqlIfExists(string name, *hash<auto> opt) {
        validateOptionsIntern("OPTION-ERROR", getDropSchemaOptions(), \opt);
        *AbstractFunction f = getFunction(name);
        if (!f)
            return;
        return AbstractDatabase::doDropSql(f.getDropSql(opt), "function", name, opt);
    }

    #! returns the SQL require to drop the given procedure if it exists or @ref nothing if the named procedure does not exist
    /** @par Example:
        @code{.py}
string sql = db.getDropProcedureSqlIfExists(name);
        @endcode

        @param name the name of the procedure
        @param opt optional callback options; see \c AbstractDatabase::CallbackOptions for more info

        @return the SQL require to drop the given procedure if it exists or @ref nothing if the named procedure does not exist

        @throw OPTION-ERROR invalid or unknown callback option
    */
    *string getDropProcedureSqlIfExists(string name, *hash<auto> opt) {
        validateOptionsIntern("OPTION-ERROR", getDropSchemaOptions(), \opt);
        *AbstractFunction f = getProcedure(name);
        if (!f)
            return;
        return AbstractDatabase::doDropSql(f.getDropSql(opt), "procedure", name, opt);
    }

    #! returns the SQL require to drop the given sequence if it exists or @ref nothing if the named sequence does not exist
    /** @par Example:
        @code{.py}
string sql = db.getDropSequenceSqlIfExists(name);
        @endcode

        @param name the name of the sequence
        @param opt optional callback options; see \c AbstractDatabase::CallbackOptions for more info

        @return the SQL require to drop the given sequence if it exists or @ref nothing if the named sequence does not exist

        @throw OPTION-ERROR invalid or unknown callback option
    */
    *string getDropSequenceSqlIfExists(string name, *hash<auto> opt) {
        validateOptionsIntern("OPTION-ERROR", getDropSchemaOptions(), \opt);
        *AbstractSequence obj = getSequence(name);
        if (!obj)
            return;
        return AbstractDatabase::doDropSql(obj.getDropSql(opt), "sequence", name, opt);
    }

    #! returns the SQL require to drop the given table if it exists or @ref nothing if the named table does not exist
    /** @par Example:
        @code{.py}
string sql = db.getDropTableSqlIfExists(name);
        @endcode

        @param name the name of the table
        @param opt optional callback options; see \c AbstractDatabase::CallbackOptions for more info

        @return the SQL require to drop the given table if it exists or @ref nothing if the named table does not exist

        @throw OPTION-ERROR invalid or unknown callback option
    */
    *list<auto> getDropTableSqlIfExists(string name, *hash<auto> opt) {
        validateOptionsIntern("OPTION-ERROR", getDropSchemaOptions(), \opt);
        *AbstractTable t = getTable(name);
        if (!t)
            return;
        return t.getDropSql(opt);
    }

    doDropSql(*softlist l, string type, string name, *hash<auto> opt) {
        if (l)
            AbstractDatabase::doCallback(opt, l, AC_Drop, type, name);
    }

    bool doDrop(*softlist l, string type, string name, *hash<auto> opt) {
        if (l) {
            AbstractDatabase::doCallback(opt, l, AC_Drop, type, name);
            on_success ds.commit();
            on_error ds.rollback();
            map ds.execRaw($1), l;
            return True;
        }
        return False;
    }

    #! returns a list of SQL strings that can be used to update a function in the database to the function definition passed as an argument
    /** @par Example:
        @code{.py}
list l = db.getAlignFunctionSql(f);
        @endcode

        @param f the template or target version of the function to compare to the database version
        @param opt an optional hash of options for the generation of the SQL strings; see \c AbstractDatabase::CreationOptions for common options; each driver can support additional driver-specific options

        @return a list of SQL strings that can be used to update a function in the database to the function definition passed as an argument

        @throw OPTION-ERROR invalid or unsupported option passed
        */
    list<auto> getAlignFunctionSql(AbstractFunction f, *hash<auto> opt) {
        validateOptionsIntern("OPTION-ERROR", getAlignSchemaOptions(), \opt);

        *AbstractFunction of = getFunctionImpl(f.name);

        list l = ();

        if (!of) {
            l += AbstractDatabase::doCallback(opt, f.getCreateSql(opt), AC_Create, "function", f.name);
        } else if (!of.equal(f)) {
            l += AbstractDatabase::doCallback(opt, of.getDropSql(opt), AC_Drop, "function", f.name);
            l += AbstractDatabase::doCallback(opt, of.getCreateSql(opt), AC_Create, "function", f.name);
        } else
            AbstractDatabase::doOkCallback(opt, AC_Unchanged, "function", f.name);

        return l;
    }

    #! returns a list of SQL strings that can be used to update a stored procedure in the database to the stored procedure definition passed as an argument
    /** @par Example:
        @code{.py}
list l = db.getAlignProcedureSql(f);
        @endcode

        @param f the template or target version of the stored procedure to compare to the database version
        @param opt an optional hash of options for the generation of the SQL strings; see \c AbstractDatabase::CreationOptions for common options; each driver can support additional driver-specific options

        @return a list of SQL strings that can be used to update a stored procedure in the database to the stored procedure definition passed as an argument

        @throw OPTION-ERROR invalid or unsupported option passed
        */
    list<auto> getAlignProcedureSql(AbstractFunction f, *hash<auto> opt) {
        *AbstractFunction of = getProcedureImpl(f.name);

        list l = ();

        if (!of)
            l += f.getCreateSql(opt);
        else if (!of.equal(f)) {
            l += of.getDropSql(opt);
            l += f.getCreateSql(opt);
        }

        return l;
    }

    #! returns an AbstractTable argument for the given table name or @ref nothing if the table cannot be found
    /** @par Example:
        @code{.py}
*AbstractTable table = db.getTable("queues");
        @endcode

        @param name the uniquely-identifying name of the table to retrieve

        @return an AbstractTable argument for the given table name or @ref nothing if the table cannot be found
        */
    *AbstractTable getTable(string name) {
        AbstractTable t = AbstractTable::getTable(ds, name);
        return t.checkExistence() ? t : NOTHING;
    }

    #! returns an AbstractSequence argument for the given sequence name or @ref nothing if the sequence cannot be found
    /** @par Example:
        @code{.py}
*AbstractSequence seq = db.getSequence("seq_queues");
        @endcode

        @param name the uniquely-identifying name of the sequence to retrieve

        @return an AbstractSequence argument for the given sequence name or @ref nothing if the sequence cannot be found
        */
    *AbstractSequence getSequence(string name) {
        return getSequenceImpl(name);
    }

    #! returns an AbstractFunction argument for the given function name or @ref nothing if the function cannot be found
    /** @par Example:
        @code{.py}
*AbstractFunction f = db.getFunction("update_queue");
        @endcode

        @param name the uniquely-identifying name of the function to retrieve

        @return an AbstractFunction argument for the given function name or @ref nothing if the function cannot be found

        @note for databases that support overloaded functions, the arguments must be passed in parentheses after the name to match overloaded functions properly
        */
    *AbstractFunction getFunction(string name) {
        return getFunctionImpl(name);
    }

    #! returns an AbstractFunction argument for the given stored procedure name or @ref nothing if the stored procedure cannot be found
    /** @par Example:
        @code{.py}
*AbstractFunction f = db.getProcedure("update_queue");
        @endcode

        @param name the uniquely-identifying name of the stored procedure to retrieve

        @return an AbstractFunction argument for the given stored procedure name or @ref nothing if the stored procedure cannot be found

        @note for databases that support overloaded stored procedures, the arguments must be passed in parentheses after the name to match overloaded stored procedures properly
        */
    *AbstractFunction getProcedure(string name) {
        return getProcedureImpl(name);
    }

    #! returns an AbstractView argument for the given view name or @ref nothing if the view cannot be found
    /** @par Example:
        @code{.py}
*AbstractFunction f = db.getView("my_view");
        @endcode

        @param name the uniquely-identifying name of the view to retrieve

        @return an AbstractView argument for the given view name or @ref nothing if the sview cannot be found
        */
    *AbstractView getView(string name) {
        return getViewImpl(name);
    }

    #! returns the next value in the given sequence
    /** @par Example:
        @code{.py}
number n = db.getNextSequenceValue("seq_customers");
        @endcode

        @param name the name of the sequence

        @return the next value in the given sequence
    */
    int getNextSequenceValue(string name) {
        return getNextSequenceValueImpl(name);
    }

    #! returns the last value issued for the given sequence in the current session
    /** @par Example:
        @code{.py}
number n = db.getCurrentSequenceValue("seq_customers");
        @endcode

        @param name the name of the sequence

        @return the last value issued for the given sequence in the current session
    */
    int getCurrentSequenceValue(string name) {
        return getCurrentSequenceValueImpl(name);
    }

    #! returns an SQL string corresponding to the list of commands in the argument
    /** @par Example:
        @code{.py}
string sql = t.getSqlFromList(list);
        @endcode

        @param l a list of SQL commands

        @return an SQL string corresponding to the list of commands in the argument
    */
    string getSqlFromList(list l) {
        return getCreateSqlImpl(l);
    }

    #! returns @ref True "True" if the database supports sequences
    bool supportsSequences() {
        return supportsSequencesImpl();
    }

    #! returns @ref True "True" if the database supports named types
    bool supportsTypes() {
        return supportsTypesImpl();
    }

    #! returns @ref True "True" if the database supports packages
    bool supportsPackages() {
        return supportsPackagesImpl();
    }

    #! returns a list of string table names in the database
    list<string> listTables() {
        return sort(listTablesImpl());
    }

    #! returns an iterator listing the string table names in the database
    Qore::ListIterator tableIterator() {
        return sort(listTablesImpl()).iterator();
    }

    #! returns a list of string function names in the database
    list<string> listFunctions() {
        return sort(listFunctionsImpl());
    }

    #! returns an iterator listing the string function names in the database
    Qore::ListIterator functionIterator() {
        return sort(listFunctionsImpl()).iterator();
    }

    #! returns a list of string procedure names in the database
    list<string> listProcedures() {
        return sort(listProceduresImpl());
    }

    #! returns an iterator listing the string procedure names in the database
    Qore::ListIterator procedureIterator() {
        return sort(listProceduresImpl()).iterator();
    }

    #! returns a list of string sequence names in the database
    list<string> listSequences() {
        return sort(listSequencesImpl());
    }

    #! returns an iterator listing the string sequence names in the database
    Qore::ListIterator sequenceIterator() {
        return sort(listSequencesImpl()).iterator();
    }

    #! returns a list of string view names in the database
    list<string> listViews() {
        return sort(listViewsImpl());
    }

    #! returns an iterator listing the string view names in the database
    Qore::ListIterator viewIterator() {
        return sort(listViewsImpl()).iterator();
    }

    #! Rebuild an index in the DB
    /** See @ref dba_indexes
        @param name an index name
        @param options an optional database-specific option hash

        @return bool True if the index was rebuild, False it it was left untouched

        @throw REBUILD-INDEX-ERROR in case of MySQL or in case of wrong options given

        @since %SqlUtil 1.2
        */
    bool rebuildIndex(string name, *hash<auto> options) {
        validateOptionsIntern("REBUILD-INDEX-ERROR", getRebuildIndexOptions(), \options);
        return rebuildIndexImpl(name, options);
    }

    #! Rebuild an index in the DB
    /** See @ref dba_indexes
        @param index an AbstractIndex object
        @param options an optional database-specific option hash

        @throw REBUILD-INDEX-ERROR in case of MySQL or in case of wrong options given

        @since %SqlUtil 1.2
        */
    bool rebuildIndex(AbstractIndex index, *hash<auto> options) {
        validateOptionsIntern("REBUILD-INDEX-ERROR", getRebuildIndexOptions(), \options);
        return rebuildIndexImpl(index.name, options);
    }

    #! Compute database statistics
    /** See @ref dba_statistics
        @param options an optional database-specific option hash

        @throw COMPUTE-STATISTICS-ERROR in case of wrong options given

        @since %SqlUtil 1.2
        */
    computeStatistics(*hash<auto> options) {
        validateOptionsIntern("COMPUTE-STATISTICS-ERROR", getComputeStatisticsOptions(), \options);
        computeStatisticsImpl(options);
    }

    #! Reclaim taken but unused space in the DB
    /** See @ref dba_space_management
        @param options an optional database-specific option hash

        @throw RECLAIM-SPACE-ERROR in case of wrong options given

        @since %SqlUtil 1.2
        */
    reclaimSpace(*hash<auto> options) {
        validateOptionsIntern("RECLAIM-SPACE-ERROR", getReclaimSpaceOptions(), \options);
        reclaimSpaceImpl(options);
    }

    #! Get the current database physical size in bytes.
    /** @return int with size or -1 in case there is no implementation
                available.

        @note value returned from this method is subject of particular
                DBMS implementation. Value can be calculated eg. by a block
                size, extents usage etc. Please refer your DBMS documentation.

        @since %SqlUtil 1.5.2
    */
    int getPhysicalSize() {
        return getPhysicalSizeImpl();
    }

    #! Returns @ref True if the driver requires a scale to support decimal values in numeric or decimal columns
    /** @return @ref True if the driver requires a scale to support decimal values in numeric or decimal columns

        @since %SqlUtil 1.7.5
    */
    bool requiresScale() {
        return False;
    }

    #! Returns the maximum precision for numeric or decimal columns
    /** @return -1 means, no maximum

        @since %SqlUtil 1.7.5
    */
    int getMaximumPrecision() {
        return -1;
    }

    private validateOptionsIntern(string err, hash<auto> ropt, reference<hash> opt) {
        AbstractSqlUtilBase::validateOptionsIntern(err, ropt, \opt, ds.getDBName() ?? "");
    }

    private validateOptionsIntern(string err, hash<auto> ropt, reference<hash> opt, string tag) {
        AbstractSqlUtilBase::validateOptionsIntern(err, ropt, \opt, tag);
    }

    static AbstractDatabase getDatabase(AbstractDatasource nds, *hash<auto> opts) {
        string drv;
        string mn = AbstractDatabase::loadModule(nds, \drv);

        if (opts) {
            AbstractDatabase::checkDriverOptions(\opts, drv);
        }

        # try to get AbstractDatabase object
        try {
            return call_function(mn + "::" + "get_database", nds, opts);
        } catch (hash<ExceptionInfo> ex) {
            throw "DATABASE-DRIVER-ERROR", sprintf("%s: error retrieving AbstractDatabase object for driver %y: %s: "
                "%s", get_ex_pos(ex), drv, ex.err, ex.desc);
        }
    }

    static string loadModule(AbstractDatasource nds, *reference<string> drv) {
        # get Qore driver name
        string qname = nds.getDriverName();
        try {
            drv = nds.getDriverRealName();
        } catch (hash<ExceptionInfo> ex) {
            rethrow "DATABASE-DRIVER-ERROR", sprintf("cannot determine real driver name through the %y driver: %s: "
                "%s", qname, ex.err, ex.desc);
        }

        # generate module and module namespace name
        string mn = qname[0].upr() + qname.substr(1);
        if (drv != qname) {
            # convert to lower case and remove everything but the first word
            drv =~ tr/A-Z/a-z/;
            drv =~ s/ .*//;
            mn += drv[0].upr() + drv.substr(1);
        }
        mn += "SqlUtil";

        # try to load module
        try {
            load_module(mn);
        } catch (hash<ExceptionInfo> ex) {
            throw "DATABASE-DRIVER-ERROR", sprintf("%s: no support for driver %y yet: %s: %s", get_ex_pos(ex), drv,
                ex.err, ex.desc);
        }
        return mn;
    }

    static AbstractDatabase getDatabase(string dsstr, *hash<auto> opts) {
        Datasource nds(dsstr);
        return AbstractDatabase::getDatabase(nds, opts);
    }

    static AbstractDatabase getDatabase(hash<auto> dsh, *hash<auto> opts) {
        Datasource nds(dsh);
        return AbstractDatabase::getDatabase(nds, opts);
    }

    static checkDriverOptions(reference<hash<auto>> h, string drv) {
        if (h.driver) {
            if (h.driver{drv}) {
                hash<auto> nh = h + h.driver{drv};
                h = nh;
            }
            delete h.driver;
        }
    }

    #! override in subclasses to return driver-specific options
    private hash<auto> getDatabaseOptions() {
        return DatabaseOptions;
    }

    #! override in subclasses to return driver-specific options
    private hash<auto> getCallbackOptions() {
        return CallbackOptions;
    }

    #! override in subclasses to return driver-specific options
    private hash<auto> getCreationOptions() {
        return CreationOptions;
    }

    #! override in subclasses to return driver-specific options
    private hash<auto> getCacheOptions() {
        return CacheOptions;
    }

    #! override in subclasses to return driver-specific options
    private hash<auto> getAlignSchemaOptions() {
        return AlignSchemaOptions;
    }

    #! override in subclasses to return driver-specific options
    private hash<auto> getDropSchemaOptions() {
        return DropSchemaOptions;
    }

    #! override in subclasses to return driver-specific options
    private hash<auto> getSchemaDescriptionOptions() {
        return SchemaDescriptionOptions;
    }

    #! override in subclasses to return driver-specific options
    private hash<auto> getSequenceDescriptionOptions() {
        return SequenceDescriptionOptions;
    }

    #! override in subclasses to return driver-specific options
    private hash<auto> getRebuildIndexOptions() {
        return hash();
    }

    #! override in subclasses to return driver-specific options
    private hash<auto> getComputeStatisticsOptions() {
        return ComputeStatisticsOptions;
    }

    #! override in subclasses to return driver-specific options
    private hash<auto> getReclaimSpaceOptions() {
        return ReclaimSpaceOptions;
    }

    #! tries to execute a command so that if an error occurs the current transaction status is not lost
    private auto tryExecArgsImpl(string sql, *softlist<auto> args) {
        return ds.vexec(sql, args);
    }

    #! tries to execute a command so that if an error occurs the current transaction status is not lost
    private auto tryExecRawImpl(string sql) {
        return ds.execRaw(sql);
    }

    private abstract string getCreateSqlImpl(list l);
    private abstract list<auto> getAlignSqlImpl(hash schema_hash, *hash<auto> opt);
    private abstract list<auto> getDropSchemaSqlImpl(hash schema_hash, *hash<auto> opt);

    private abstract *AbstractSequence getSequenceImpl(string name);
    private abstract *AbstractFunction getFunctionImpl(string name);
    private abstract *AbstractFunction getProcedureImpl(string name);
    private abstract *AbstractView getViewImpl(string name);

    private abstract AbstractSequence makeSequenceImpl(string name, number start = 1, number increment = 1,
            *softnumber end, *hash<auto> opts);
    private abstract AbstractFunction makeFunctionImpl(string name, string src, *hash<auto> opts);
    private abstract AbstractFunction makeProcedureImpl(string name, string src, *hash<auto> opts);

    private abstract list<string> featuresImpl();
    private abstract list<string> listTablesImpl();
    private abstract list<string> listFunctionsImpl();
    private abstract list<string> listProceduresImpl();
    private abstract list<string> listSequencesImpl();
    private abstract list<string> listViewsImpl();

    #! returns the next value in the given sequence
    private abstract int getNextSequenceValueImpl(string name);
    #! returns the last value issued for the given sequence in the current session
    private abstract int getCurrentSequenceValueImpl(string name);

    #! returns @ref True "True" if the database supports sequences
    private abstract bool supportsSequencesImpl();
    private abstract bool supportsPackagesImpl();
    private abstract bool supportsTypesImpl();

    private abstract bool rebuildIndexImpl(string name, *hash<auto> options);
    private abstract computeStatisticsImpl(*hash<auto> options);
    private abstract reclaimSpaceImpl(*hash<auto> options);
    private abstract int getPhysicalSizeImpl();
}
}
