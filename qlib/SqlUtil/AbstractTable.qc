# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AbstractTable class definition

/** AbstractTable.qc Copyright 2019 - 2026 Qore Technologies, s.r.o.

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
#! the base abstract class for the table implementation
/** Use @ref SqlUtil::Table if you need to construct table object directly
*/
public class AbstractTable inherits AbstractSqlUtilBase {
    public {
        #! table options
        /** The following keys can be set for this option:
            - \c native_case: (\c bool_type) if @ref True "True" then objects will be returned in the case the
              database server returns them in even if the database server uses case-insensitive names; normally in
              this case %SqlUtil will convert the names to lower-case
            - \c table_cache: (@ref SqlUtil::Tables "Tables") an optional table cache for maintaining cached tables
              and foreign key relationships between tables
        */
        const TableOptions = {
            "native_case": Type::Boolean,
            "table_cache": "Tables",
        };

        #! default index options
        /**
            - \c index_tablespace: (@ref string_type) a string giving the index tablespace to use for indexes
            - \c replace: (\c bool_type) if @ref True "True" and supported by the underlying db driver
              "create or replace" text is used when creating objects
        */
        const IndexOptions = {
            "index_tablespace": Type::String,
            "replace": Type::Boolean,
        };

        #! default constraint options
        /** currently this option is identical to @ref SqlUtil::AbstractTable::IndexOptions
        */
        const ConstraintOptions = IndexOptions;

        #! default cache options
        const CacheOptions = {
            "table_cache": "Tables",
        };

        #! default foreign constraint options
        /** The following keys can be set for this option:
            - \c table_cache: (@ref SqlUtil::Tables "Tables") an optional table cache for maintaining cached tables
              and foreign key relationships between tables
        */
        const ForeignConstraintOptions = ConstraintOptions + {
            "table_cache": "Tables",
        };

        #! default trigger options
        /** currently this option is identical to @ref AbstractDatabase::CreationOptions
        */
        const TriggerOptions = AbstractDatabase::CreationOptions;

        #! default possible select options; can be extended by driver-specific modules
        /** @see @ref select_option_hash for more information on the default possible select options; see
            driver-specific information for any additional select options supported by the driver-specific module
        */
        const SelectOptions = {
            "alias": Type::String,
            "comment": Type::String,
            "hint": Type::String,
            "columns": Type::NothingType,
            "where": "hash/list",
            "orderby": "softstringinthashlist",
            "desc": Type::Boolean,
            "limit": Type::Int,
            "offset": Type::Int,
            "join": Type::Hash,
            "groupby": "softstringinthashlist",
            "having": Type::Hash,
            "superquery": Type::Hash,
            "forupdate": Type::Boolean,
        };

        #! alignment omission options
        const TableOmissionOptions = {
            "indexes": True,
            "foreign_constraints": True,
            "triggers": True,
        };

        #! table creation options
        /** currently this option is a combination of @ref SqlUtil::AbstractTable::IndexOptions and @ref AbstractDatabase::CreationOptions plus the following:
            - \c omit: a list pf attributes to omit; possible values are: \c indexes, \c foreign_constraints, \c triggers (see @ref TableOmissionOptions)
        */
        const TableCreationOptions = IndexOptions + AbstractDatabase::CreationOptions + {
            "omit": "softstringlist",
        };

        #! table alignment options
        /** currently this option is a combination of @ref SqlUtil::AbstractTable::TableCreationOptions and the following options:
            - \c column_map: (@ref hash_type) a hash for automatically renaming columns; if the source name (key) exists and the target name (value) does not exist, then the source column is automatically renamed
            - \c index_map: (@ref hash_type) a hash for automatically renaming indexes; if the source name (key) exists and the target name (value) does not exist, then the source index is automatically renamed
            - \c constraint_map: (@ref hash_type) a hash for automatically renaming constraints; if the source name (key) exists and the target name (value) does not exist, then the source constraint is automatically renamed
            - \c trigger_map: (@ref hash_type) a hash for automatically renaming triggers; if the source name (key) exists and the target name (value) does not exist, then the source trigger is automatically renamed
            - \c db_table_cache: (@ref SqlUtil::Tables "Tables") an optional table cache for maintaining tables in the database and foreign key relationships between tables
            - \c force: (\c bool_type) if @ref True "True" and supported by the driver and object, any objects dropped will be dropped with \c FORCE or \c CASCADE options
        */
        const AlignTableOptions = TableCreationOptions + {
            "column_map": Type::Hash,
            "index_map": Type::Hash,
            "constraint_map": Type::Hash,
            "trigger_map": Type::Hash,
            "db_table_cache": "Tables",
            "force": Type::Boolean,
        };

        #! Table description options
        /** this option is made up of the following keys:
            - \c columns: (@ref column_desc_hash "column description hash") a hash<auto> describing the column
            - \c primary_key : (@ref pk_desc_hash "primary key description hash") a hash<auto> describing the primary key for the table
            - \c indexes: (@ref index_desc_hash "index description hashes") a hash<auto> describing the indexes on the table
            - \c triggers: a hash of trigger information keyed by trigger name; the values are the trigger source code; since triggers are driver-dependent, a driver-independent table description would include trigger hashes under the \c drivers key and the driver key name under that
            - \c foreign_constraints: (@ref fk_desc_hash "foreign constraint hashes") a hash<auto> describing the foreign constraints on the table
            - \c unique_constraints: (@ref uk_desc_hash "unique constraint hashes") a hash<auto> describing the unique constraints on the table
            - \c table_cache: (@ref SqlUtil::Tables "Tables") an optional table cache for maintaining cached tables and foreign key relationships between tables

            @see @ref table_desc_hash
        */
        const TableDescriptionHashOptions = {
            "columns": Type::Hash,
            "primary_key": Type::Hash,
            "indexes": Type::Hash,
            "triggers": Type::Hash,
            "foreign_constraints": Type::Hash,
            "unique_constraints": Type::Hash,
            #"check_constraints": Type::Hash,
            "table_cache": "Tables",
        };

        #! Column description options
        /** this option is made up of the following keys:
            - \c qore_type: (@ref string_type) a qore type string that will be converted to a native DB type with some default conversion
            - \c native_type: (@ref string_type) the native database column type; if both \c native_type and \c qore_type are given then \c native_type is used
            - \c size: (@ref int_type) for data types requiring a size component, the size; for numeric columns this represents the precision for example
            - \c scale: (@ref int_type) for numeric data types, this value gives the scale
            - \c default_value: the default value for the column
            - \c default_value_native: a boolean flag to say if a \c default_value should be validated against table column type (False) or used as it is (True) to allow to use DBMS native functions or features. Defaults to False. It is strongly recommended to use \c default_value_native for \c default_value in \c driver specific sub-hash to avoid non-portable schema hashes
            - \c comment: (@ref string_type) an optional comment for the column
            - \c notnull: if the column should have a "not null" constraint on it; if missing the default value is @ref False "False"
            - \c driver: this key can optionally contain a hash keyed by driver name which contains a hash of values that will be added to the column description hash before processing; this way a column description hash can contain all the information required for the column including driver-specific options; any driver-specific options will overwrite values in the top level of the hash if there are duplicate hash keys, see below for an example
            @see @ref column_desc_hash
        */
        const ColumnDescOptions = {
            "qore_type": Type::String,
            "native_type": Type::String,
            "size": Type::Int,
            "scale": Type::Int,
            "default_value": Type::NothingType,
            "default_value_native": Type::Boolean,
            "comment": Type::String,
            "notnull": Type::Boolean,
            "driver": Type::Hash,
        };

        #! additional column description keys valid when describing columns in a @ref table_desc_hash "table description hash"
        /** the following keys are valid in a @ref table_desc_hash "table description hash" in addition to any \c SqlUtil::AbstractTable::ColumnDescOptions "AbstractTable::ColumnDescOptions":
            - \c notnull: (\c bool_type) if @ref True "True" then the column will have a not null constraint
        */
        const AdditionalColumnDescOptions = {
            "notnull": Type::Boolean,
        };

        #! Column options; this is currently empty and can be extended in database-specific modules
        const ColumnOptions = {};

        #! generic SQL data operation callbacks
        /** The following keys can be set for this option:
            - \c sqlarg_callback: see @ref sqlarg_callback
            - \c tablecode: a closure taking a string table name and returning an AbstractTable object for late resolution of table names in joins
        */
        const SqlDataCallbackOptions = {
            "sqlarg_callback": "code",
            "tablecode": "code",
        };

        #! generic SQL insert options
        /** In addition to any @ref SqlDataCallbackOptions, the following keys can be set for this option:
            - \c returning: a list having elements of one of the two following types:
                - @ref string_type "string": column names to return the value inserted
                - @ref hash_type "hash": a hash having the following keys:
                  - \c "key": (required) the column name to return
                  - \c "type": (optional) the data type for the output placeholder buffer (ex: @ref Qore::Type::Number "Type::Number")

            @note using \c "returning" with a database that does not support this clause will cause an exception to be thrown; see @ref SqlUtil::AbstractTable::hasReturning()
        */
        const InsertOptions = SqlDataCallbackOptions + {
            "returning": "stringhashlist",
        };

        #! default upsert option keys
        /** The following keys can be set for this option:
            - \c commit_block: the number of changes made before an automatic commit is made for upsert methods that
              perform commits
            - \c delete_others: if this option is @ref True "True", then a hash of primary key values in the input
              data is built as the input data is iterated.  After iterating, if the row count of the table and the
              input data matches, then nothing more is done, otherwise, every row of the table is iterated and
              compared to the primary key hash; if a row does not match a primary key value, then it is deleted.
              This operation allows tables to be completely synchronized by removing rows in the target table not
              present in the source table. This operation is expensive for large data sets.
            - \c info_callback: see @ref upsert_info_callback
            - \c omit_update: allows for an asymmetrical upsert where a set of column values is inserted, but a
              smaller set is updated in case the unique key values are present in the target table; the value of this
              key should be set to the columns to omit in the update clause
            - \c upsert_columns: a list of column names that identifies the unique index or constraint to use for the
              upsert operation
        */
        const UpsertOptions = {
            "commit_block": Type::Int,
            "delete_others": Type::Boolean,
            "info_callback": "code",
            "omit_update": "softstringlist",
            "upsert_columns": "softstringlist",
        };

        #! default insert option keys
        /** In addition to any @ref SqlDataCallbackOptions, the following keys can be set for this option:
            - \c info_callback: see @ref insert_info_callback
            - \c commit_block: the number of changes made before an automatic commit is made for insert methods that perform commits
        */
        const InsertFromIteratorOptions = SqlDataCallbackOptions + {
            "info_callback": "code",
            "commit_block": Type::Int,
        };

        /** @defgroup upsert_options Upsert Strategy Codes
            These options are used with:
            - @ref SqlUtil::AbstractTable::getBulkUpsertClosure()
            - @ref SqlUtil::AbstractTable::getUpsertClosure()
            - @ref SqlUtil::AbstractTable::getUpsertClosureWithValidation()
            - @ref SqlUtil::AbstractTable::upsert()
            - @ref SqlUtil::AbstractTable::upsertCommit()
            - @ref SqlUtil::AbstractTable::upsertFromIterator()
            - @ref SqlUtil::AbstractTable::upsertFromIteratorCommit()
            - @ref SqlUtil::AbstractTable::upsertFromSelect()
            - @ref SqlUtil::AbstractTable::upsertFromSelectCommit()

            to specify the upsert strategy when synchronizing table data
        */
        #/@{
        #! Upsert option: insert first, if the insert fails, then update
        /** with this option an insert is attempted, and if it fails due to a duplicate row, then an update is made unconditionally; with
            this upsert strategy, the following row result codes are possible:
            - @ref UR_Inserted
            - @ref UR_Verified (the only value returned by certain drivers with optimized upsert implementations)
        */
        const UpsertInsertFirst = 1;

        #! Upsert option: update first, if the update fails, then insert
        /** with this option an update is attempted, and if it fails due to a missing row, then an insert is performed; with
            this upsert strategy, the following row result codes are possible:
            - @ref UR_Inserted
            - @ref UR_Verified (the only value returned by certain drivers with optimized upsert implementations)
        */
        const UpsertUpdateFirst = 2;

        #! Upsert option: select first, if the row is unchanged, do nothing, if it doesn't exist, insert, otherwise update
        /** with this option the row is selected, if it doesn't exist, an insert is made, and an update is made only if the
            values are different; with this upsert strategy, the following row result codes are possible:
            - @ref UR_Inserted
            - @ref UR_Updated
            - @ref UR_Unchanged
        */
        const UpsertSelectFirst = 3;

        #! Upsert option: if the target table is empty, use @ref UpsertInsertFirst, otherwise use @ref UpsertUpdateFirst
        /** With this upsert strategy, the following row result codes are possible:
            - @ref UR_Inserted
            - @ref UR_Verified (the only value returned by certain drivers with optimized upsert implementations)
        */
        const UpsertAuto = 4;

        #! Upsert option: insert if the row does not exist, otherwise ignore
        /** With this upsert strategy, the following row result codes are possible:
            - @ref UR_Inserted
            - @ref UR_Unchanged
        */
        const UpsertInsertOnly = 5;

        #! Upsert option: update if the row exists, otherwise ignore
        /** With this upsert strategy, the following row result codes are possible:
            - @ref UR_Verified
            - @ref UR_Unchanged
        */
        const UpsertUpdateOnly = 6;

        #! hash mapping upsert strategy codes to a text description
        /** @see @ref UpsertStrategyDescriptionMap for a reverse mapping
        */
        const UpsertStrategyMap = {
            UpsertInsertFirst: "UpsertInsertFirst",
            UpsertUpdateFirst: "UpsertUpdateFirst",
            UpsertSelectFirst: "UpsertSelectFirst",
            UpsertAuto: "UpsertAuto",
            UpsertInsertOnly: "UpsertInsertOnly",
            UpsertUpdateOnly: "UpsertUpdateOnly",
        };

        #! hash mapping upsert strategy descriptions to upsert strategy codes
        /** @see @ref UpsertStrategyMap for a reverse mapping
        */
        const UpsertStrategyDescriptionMap = {
            "UpsertInsertFirst": UpsertInsertFirst,
            "UpsertUpdateFirst": UpsertUpdateFirst,
            "UpsertSelectFirst": UpsertSelectFirst,
            "UpsertAuto": UpsertAuto,
            "UpsertInsertOnly": UpsertInsertOnly,
            "UpsertUpdateOnly": UpsertUpdateOnly,
        };
        #/@}

        /** @defgroup upsert_results Upsert Result Codes
            @see @ref UpsertResultMap and @ref UpsertResultDescriptionMap
        */
        #/@{
        #! row was inserted
        const UR_Inserted = 1;

        #! row was updated unconditionally (not returned with @ref UpsertSelectFirst)
        const UR_Verified = 2;

        #! row was updated because it was different (only possible with @ref UpsertSelectFirst)
        const UR_Updated = 3;

        #! row was unchanged (only possible with @ref UpsertSelectFirst, @ref UpsertInsertOnly, and @ref UpsertUpdateOnly)
        const UR_Unchanged = 4;

        #! row was deleted (only possible with batch upsert methods such as @ref AbstractTable::upsertFromIterator() where \c SqlUtil::AbstractTable::UpsertOptions "upsert option" \c delete_others is @ref True "True")
        const UR_Deleted = 5;
        #/@}

        #! hash mapping upsert results to a description
        /** @see @ref UpsertResultDescriptionMap for a reverse mapping
        */
        const UpsertResultMap = {
            UR_Inserted: "inserted",
            UR_Verified: "verified",
            UR_Updated: "updated",
            UR_Unchanged: "unchanged",
            UR_Deleted: "deleted",
        };

        #! hash mapping upsert descriptions to codes
        /** @see @ref UpsertResultMap for a reverse mapping
        */
        const UpsertResultDescriptionMap = {
            "inserted": UR_Inserted,
            "verified": UR_Verified,
            "updated": UR_Updated,
            "unchanged": UR_Unchanged,
            "deleted": UR_Deleted,
        };

        #! maps upsert result codes to single letter symbols
        const UpsertResultLetterMap = {
            UR_Inserted: "I",
            UR_Verified: "V",
            UR_Updated: "U",
            UR_Unchanged: ".",
            UR_Deleted: "X",
        };
    }

    private {
        #! the table's name
        string name;
        #! column description object
        Columns columns;
        #! primary key description
        AbstractPrimaryKey primaryKey;
        #! index descriptions
        Indexes indexes;
        #! foreign constraints description
        ForeignConstraints foreignConstraints;
        #! constraint descriptions
        Constraints constraints;
        #! trigger descriptions
        Triggers triggers;
        #! native case option
        bool native_case = False;
        #! in database
        bool inDb = False;
        #! manual edits
        bool manual = False;

        hash m_customCopMap = {};
    }

    #! creates the object; private constructor
    /** @param nds the AbstractDatasource for the connection to the database
        @param nname the name of the table
        @param nopts an optional hash of options for the table creation string; see
        @ref SqlUtil::AbstractTable::TableOptions for common options; each driver can support additional
        driver-specific options

        @throw OPTION-ERROR invalid or unsupported option passed
    */
    private constructor(AbstractDatasource nds, string nname, *hash nopts) : AbstractSqlUtilBase(nds, nopts) {
        name = nname;
        doTableOptions(nopts);
    }

    #! copies the object
    copy(AbstractTable old) {
        # copy container objects
        if (columns)
            columns = columns.copy();
        if (primaryKey)
            primaryKey = primaryKey.copy();
        if (indexes)
            indexes = indexes.copy();
        if (foreignConstraints)
            foreignConstraints = foreignConstraints.copy();
        if (constraints)
            constraints = constraints.copy();
        if (triggers)
            triggers = triggers.copy();

        copyImpl(old);
    }

    #! changes the datasource for the table; if the \c inDb flag is @ref True "True", then it is set to @ref False "False" by calling this method
    /** @par Example:
        @code{.py}
Table table(ds1, "table_name");
table.setDatasource(ds2);
table.create();
        @endcode

        @param nds the new datasource for the table

        @note it's generally not safe to call this method while SQL operations are in progress in other threads on the same table
    */
    setDatasource(AbstractDatasource nds) {
        l.lock();
        on_exit l.unlock();

        ds = nds;
        inDb = False;
    }

    private doTableOptions(*hash<auto> nopts) {
        validateOptionsIntern("OPTION-ERROR", getTableOptions(), \nopts);
        if (nopts.native_case)
            native_case = True;
    }

    #! begins a transaction on the underlying @ref Qore::SQL::AbstractDatasource
    beginTransaction() {
        ds.beginTransaction();
    }

    #! commits the current transaction on the underlying @ref Qore::SQL::AbstractDatasource
    commit() {
        ds.commit();
    }

    #! rolls back the current transaction on the underlying @ref Qore::SQL::AbstractDatasource
    rollback() {
        ds.rollback();
    }

    #! returns @ref True "True" if the table has been read from or created in the database, @ref False "False" if not
    /** @par Example:
        @code{.py}
bool b = table.inDb();
        @endcode

        @return @ref True "True" if the table has been read from or created in the database, @ref False "False" if not

        @note this method only returns a flag if the object contains configuration already retrieved from the database, @see AbstractTable::checkExistence() for a method that will check the database if the table exists in case the table is not already known to exist in the database
    */
    bool inDb() {
        return inDb;
    }

    #! returns an iterator for all unique constraints on the table (including the primary key if any)
    /** @par Example:
        @code{.py}
map printf("unique constraint %s\n", $1.getName()), table.getUniqueConstraintIterator();
        @endcode

        @return an iterator for all unique constraints on the table (including the primary key if any)
    */
    Qore::AbstractIterator getUniqueConstraintIterator() {
        bool lck = !l.lockOwner();
        if (lck)
            l.lock();
        on_exit if (lck)
            l.unlock();

        if (!manual) {
            getPrimaryKeyUnlocked();
            getConstraintsUnlocked();
        }

        list lst = ();
        if (!primaryKey.empty())
            lst += primaryKey;

        foreach AbstractConstraint c in (constraints.iterator()) {
            if (c instanceof AbstractUniqueConstraint)
                lst += c;
        }

        return lst.iterator();
    }

    #! drops the table from the database; releases the transaction lock after dropping the table
    /** @par Example:
        @code{.py}
table.dropCommit();
        @endcode

        @param opt optional callback options; see @ref AbstractDatabase::CallbackOptions for more info

        @throw OPTION-ERROR invalid or unknown callback option

        @note The transaction is committed if successful or rolled back if an exception occurs; use @ref drop() to
        execute without any transaction management
    */
    dropCommit(*hash<auto> opt) {
        on_exit ds.commit();
        on_error ds.rollback();
        drop(opt);
    }

    #! drops the table from the database without any transaction management
    /** @par Example:
        @code{.py}
table.drop();
        @endcode

        @param opt optional callback options; see @ref AbstractDatabase::CallbackOptions for more info

        @throw OPTION-ERROR invalid or unknown callback option

        @note Transaction management is normally not performed when dropping tables, however this method uses the
        Qore::SQL::AbstractDatasource::exec() method, which normally participates in acquiring a transaction lock for
        the underlying datasource object; therefore after this method executes normally the transaction lock will be
        dedicated to the calling thread.
    */
    drop(*hash<auto> opt) {
        map ds.execRaw($1), getDropSql(opt);
        inDb = False;
    }

    #! A legacy wrapper for drop()
    deprecated dropNoCommit(*hash<auto> opt) {
        drop(opt);
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

    #! returns the sql required to drop the table; reimplement in subclasses if necessary
    /** @par Example:
        @code{.py}
list l = table.getDropSql();
        @endcode

        @param opt optional callback options; see @ref AbstractDatabase::CallbackOptions for more info

        @return a list of strings that can be used to drop the table and any other objects assocatied with the table
        (for example: PostgreSQL table trigger function(s))

        @throw OPTION-ERROR invalid or unknown callback option
    */
    softlist<auto> getDropSql(*hash<auto> opt) {
        cache();
        return AbstractDatabase::doCallback(opt, getDropSqlImpl(), AbstractDatabase::AC_Drop, "table", getSqlName());
    }

    #! truncates all the table data; releases the transaction lock after executing
    /** @par Example:
        @code{.py}
table.truncateCommit();
        @endcode

        @note The transaction is committed if successful or rolled back if an exception occurs; use @ref truncate() to
        execute without any transaction management
    */
    truncateCommit() {
        on_exit ds.commit();
        on_error ds.rollback();
        truncate();
    }

    #! truncates all the table data without any transaction management
    /** @par Example:
        @code{.py}
table.truncate();
        @endcode

        @note Transaction management may not be applied when truncating tables depending on the database driver (for
        example truncating tables in Oracle does not participate in transaction management), however this method uses
        the Qore::SQL::AbstractDatasource::exec() method, which normally participates in acquiring a transaction lock
        for the underlying datasource object; therefore after this method executes normally the transaction lock will
        be dedicated to the calling thread.
    */
    truncate() {
        ds.exec(getTruncateSqlImpl());
        inDb = False;
    }

    #! A legacy warpper for truncate()
    deprecated truncateNoCommit() {
        truncate();
    }

    #! gets the SQL that can be used to truncate the table
    /** @par Example:
        @code{.py}
string sql = table.getTruncateSql();
        @endcode

        @param opt a hash of options for the SQL string; see @ref SqlUtil::AbstractTable::AlignTableOptions for common
        options; each driver can support additional driver-specific options

        @return the SQL that can be used to truncate the table

        @throw OPTION-ERROR invalid or unsupported option passed

        @see inDb() for a method that tells if the table is already in the database or not

        @note if the @ref sql_callback_executed "sql_callback_executed option" is @ref True "True" in \a opt, then the
        changes are also effected in the current object, if not, then they are not (see @ref sql_callback_executed for
        more information)
    */
    string getTruncateSql(*hash<auto> opt) {
        validateOptionsIntern("OPTION-ERROR", getAlignTableOptions(), \opt);
        return AbstractDatabase::doCallback(opt, getTruncateSqlImpl(), AbstractDatabase::AC_Truncate, "table", name);
    }

    #! creates the table in the database; releases the transaction lock after creating the table
    /** @par Example:
        @code{.py}
table.createCommit();
        @endcode

        @param opt a hash of options for the SQL creation strings

        @note The transaction is committed if successful or rolled back if an exception occurs; use @ref create() to execute without any transaction management
    */
    createCommit(*hash<auto> opt) {
        on_exit ds.commit();
        on_error ds.rollback();
        create(opt);
    }

    #! creates the table with all associated properties (indexes, constraints, etc) without any transaction management
    /** @par Example:
        @code{.py}
table.create();
        @endcode

        @param opt a hash of options for the SQL creation strings

        @note Transaction management is normally not performed when creating tables, however this method uses the
        Qore::SQL::AbstractDatasource::exec() method, which normally participates in acquiring a transaction lock for
        the underlying datasource object; therefore after this method executes normally the transaction lock will be
        dedicated to the calling thread.

        @throw CREATE-TABLE-ERROR table has already been read from or created in the database
    */
    create(*hash<auto> opt) {
        if (inDb)
            throw "CREATE-TABLE-ERROR", sprintf("%s has already been read from or created in the database", getDesc());

        # get and execute all the creation strings
        map ds.execRaw($1), getCreateSql(opt);
        inDb = True;
        manual = False;
    }

    #! A legacy wrapper for create()
    deprecated createNoCommit(*hash<auto> opt) {
        create(opt);
    }

    #! renames the table
    /** if the table is already known to be in the database in the database, then the changes are effected in the
        database also immediately; otherwise it is only updated internally

        @par Example:
        @code{.py}
table.rename("new_name");
        @endcode

        @param new_name the new name for the table
        @param sql an optional reference to a string to retrieve the SQL used to modify the table (only executed if
        the table is already in the database)
        @param table_cache an optional @ref SqlUtil::Tables "Tables" argument to update the table name in any table
        cache

        @see getRenameSql()
    */
    rename(string new_name, *reference<string> sql, *Tables table_cache) {
        # issue #3352: do not execute runtime SQL while holding the lock
        {
            l.lock();
            on_exit l.unlock();

            sql = getRenameSqlImpl(new_name);
        }

        execSql(sql);
        doRenameIntern(new_name, table_cache);
    }

    private doRenameIntern(string new_name, *Tables table_cache) {
        if (table_cache)
            table_cache.tableRenamed(name, new_name, getSqlName());
        name = new_name;
    }

    #! returns @ref True "True" if the table has no data rows, @ref False "False" if not
    /** @par Example:
        @code{.py}
bool b = table.emptyData();
        @endcode

        @return @ref True "True" if the table has no data rows, @ref False "False" if not

        @see
        - empty()
        - rowCount()
    */
    bool emptyData() {
        return findSingle() ? False : True;
    }

    #! returns @ref True "True" if the table has no definitions, @ref False "False" if not
    /** @par Example:
        @code{.py}
bool b = table.empty();
        @endcode

        @return @ref True "True" if the table has no definitions, @ref False "False" if not

        @see emptyData()
    */
    bool empty() {
        l.lock();
        on_exit l.unlock();
        return emptyUnlocked();
    }

    private bool emptyUnlocked() {
        if (!columns.empty()
            || !primaryKey.empty()
            || !indexes.empty()
            || !foreignConstraints.empty()
            || !constraints.empty()
            || !triggers.empty())
            return False;

        return emptyImpl();
    }

    #! creates the object from a table description hash
    /** @param desc a @ref table_desc_hash "table description hash" describing the table
        @param opt an optional hash of options for the table creation string; see
        @ref SqlUtil::AbstractTable::TableOptions for common options; each driver can support additional
        driver-specific options

        @throw OPTION-ERROR invalid or unsupported option passed
        @throw DESCRIPTION-ERROR invalid or unsupported description hash value passed
    */
    setupTable(hash<auto> desc, *hash<auto> opt) {
        # check description
        validateOptionsIntern("DESCRIPTION-ERROR", getTableDescriptionHashOptions(), \desc);

        # check options
        hash copt = getColumnOptions();
        hash tcopt = getTableCreationOptions() + copt;
        opt = opt{tcopt.keys()};
        validateOptionsIntern("OPTION-ERROR", tcopt, \opt);

        # setup table cache
        if (!opt.table_cache)
            opt.table_cache = new Tables();

        l.lock();
        on_exit l.unlock();

        if (!emptyUnlocked()) {
            throw "DESCRIPTION-ERROR", sprintf("%s: cannot set up a non-empty table from a hash description",
                getDesc());
        }

        # add current table to cache
        opt.table_cache.add(name, self);

        string drv = ds.getDriverName();

        # extract column options
        *hash<auto> oc = opt.(copt.keys());

        # process descriptions
        /* issue #1684: sqlutil: schema alignment skips column with name "driver"
            handle driver options with special logic: try to remove any driver
            configurations so that a column with the name "driver" can be
            supported
        */
        if (desc.columns.driver) {
            *hash<auto> h = remove desc.columns.driver{SqlUtilDrivers};
            if (h) {
                # if we have driver-specific info, add it to the columns hash
                if (h{drv})
                    desc.columns += h{drv};
                # if there is no column information, then delete the "driver" key
                if (!desc.columns.driver)
                    delete desc.columns.driver;
            }
        }

        AbstractDatabase::checkDriverOptions(\desc.indexes, drv);
        AbstractDatabase::checkDriverOptions(\desc.primary_key, drv);
        AbstractDatabase::checkDriverOptions(\desc.unique_constraints, drv);
        AbstractDatabase::checkDriverOptions(\desc.foreign_constraints, drv);
        AbstractDatabase::checkDriverOptions(\desc.triggers, drv);

        # process column definitions
        foreach string cn in (desc.columns.keyIterator()) {
            if (desc.columns{cn}.typeCode() != NT_HASH)
                throw "DESCRIPTION-ERROR", sprintf("%s: value assigned to column key %y is not a hash, got type %y "
                    "instead (%y)", getDesc(), cn, desc.columns{cn}.type(), desc.columns{cn});
            AbstractDatabase::checkDriverOptions(\desc.columns{cn}, drv);
        }

        preSetupTableImpl(\desc, opt);

        # add columns to table
        foreach string cn in (desc.columns.keyIterator()) {
            hash<auto> ch = desc.columns{cn};
            softbool nullable = !exists ch.notnull ? True : !(remove ch.notnull);
            # bug #1688: sqlutil: oracle: character_semantics can be
            # different for each column. So the global options (oc)
            # are overwriten with column options (ch).
            addColumnUnlocked(cn, oc + ch, nullable ? True : False);
        }

        # add indexes to table
        foreach string ixn in (desc.indexes.keyIterator()) {
            if (desc.indexes{ixn}.typeCode() != NT_HASH)
                throw "DESCRIPTION-ERROR", sprintf("%s: value assigned to index key %y is not a hash, got type %y "
                    "instead (%y)", getDesc(), ixn, desc.indexes{ixn}.type(), desc.indexes{ixn});

            hash<auto> ih = desc.indexes{ixn};

            if (!ih.columns || !inlist(ih.columns.typeCode(), (NT_LIST, NT_STRING)))
                throw "DESCRIPTION-ERROR", sprintf("%s: value assigned to \"columns\" key for index %y is not a list "
                    "or a string, got type %y instead (%y)", getDesc(), ixn, ih.columns.type(), ih.columns);

            addIndexUnlocked(ixn, ih.unique ? True : False, ih.columns, opt.(getIndexOptions().keys()));
        }

        # add primary key to table
        if (desc.primary_key) {
            if (desc.primary_key.typeCode() != NT_HASH)
                throw "DESCRIPTION-ERROR", sprintf("%s: value assigned to primary_key is not a hash, got type %y "
                    "instead (%y)", getDesc(), desc.primary_key.type(), desc.primary_key);
            if (!desc.primary_key.name || desc.primary_key.name.typeCode() != NT_STRING)
                throw "DESCRIPTION-ERROR", sprintf("%s: missing string value for 'name' key in primary_key hash: %y",
                    getDesc(), desc.primary_key);

            addPrimaryKeyUnlocked(desc.primary_key.name, desc.primary_key.columns,
                opt.(getConstraintOptions().keys()));
        }

        # add unique constraints to table
        foreach string un in (desc.unique_constraints.keyIterator()) {
            if (desc.unique_constraints{un}.typeCode() != NT_HASH)
                throw "DESCRIPTION-ERROR", sprintf("%s: value assigned to index key %y is not a hash, got type %y "
                    "instead (%y)", getDesc(), un, desc.unique_constraints{un}.type(), desc.unique_constraints{un});

            hash<auto> ch = desc.unique_constraints{un};

            if (!ch.columns || !inlist(ch.columns.typeCode(), (NT_LIST, NT_STRING)))
                throw "DESCRIPTION-ERROR", sprintf("%s: value assigned to \"columns\" key for unique constraint %y "
                    "is not a list or a string, got type %y instead (%y)", getDesc(), un, ch.columns.type(),
                    ch.columns);

            if (constraints.hasKey(un))
                continue;
            addUniqueConstraintUnlocked(un, ch.columns, opt.(getConstraintOptions().keys()));
        }

        # add foreign constraints to table
        foreach string cn in (desc.foreign_constraints.keyIterator()) {
            if (desc.foreign_constraints{cn}.typeCode() != NT_HASH)
                throw "DESCRIPTION-ERROR", sprintf("%s: value assigned to foreign constraint key %y is not a hash, "
                    "got type %y instead (%y)", getDesc(), cn, desc.foreign_constraints{cn}.type(),
                    desc.foreign_constraints{cn});

            hash<auto> ch = desc.foreign_constraints{cn};

            if (!ch.columns || !inlist(ch.columns.typeCode(), (NT_LIST, NT_STRING)))
                throw "DESCRIPTION-ERROR", sprintf("%s: value assigned to \"columns\" key for foreign constraint %y "
                    "is not a list or a string, got type %y instead (%y)", getDesc(), cn, ch.columns.type(),
                    ch.columns);

            if (!ch.table || ch.table.typeCode() != NT_STRING)
                throw "DESCRIPTION-ERROR", sprintf("%s: missing string value for \"table\" key in foreign constraint "
                    "%y description: %y", getDesc(), cn, ch);

            if (exists ch.target_columns && !inlist(ch.target_columns.typeCode(), (NT_LIST, NT_STRING)))
                throw "DESCRIPTION-ERROR", sprintf("%s: value assigned to \"target_columns\" key for foreign "
                    "constraint %y is not a list or a string, got type %y instead (%y)", getDesc(), cn,
                    ch.target_columns.type(), ch.target_columns);

            if (!ch.target_columns)
                ch.target_columns = ch.columns;

            try {
                addForeignConstraintUnlocked(cn, ch.columns, ch.table, ch.target_columns,
                    opt.(getForeignConstraintOptions().keys()));
            } catch (hash<ExceptionInfo> ex) {
                rethrow ex.err, sprintf("error setting up %s: %s", getDesc(), ex.desc), ex.arg;
            }
        }

        # add triggers to table
        foreach string tn in (desc.triggers.keyIterator()) {
            if (desc.triggers{tn}.typeCode() != NT_STRING)
                throw "DESCRIPTION-ERROR", sprintf("%s: value assigned to trigger key %y is not a string giving the "
                    "trigger source code, got type %y instead (%y)", getDesc(), tn, desc.triggers{tn}.type(),
                    desc.triggers{tn});
            string src = desc.triggers{tn};

            addTriggerUnlocked(tn, src, opt.(getTriggerOptions().keys()));
        }

        setupTableImpl(desc, opt);
    }

    #! adds a column to the table; if the table is already known to be in the database, then it is added in the database also immediately; otherwise it is only added internally and can be created when create() is called for example
    /** @par Example:
        @code{.py}
table.addColumn("name", ("qore_type": Type::String, "size": 50), False);
        @endcode

        In case the table is already in the database, this method commits the transaction on success and rolls back the transaction if there's an error.

        @param cname the name of the column
        @param opt a hash<auto> describing the column; the following keys are permitted (other column options may be supported depending on the underlying AbstractTable implementation):
        - \c qore_type: a qore type string that will be converted to a native DB type with some default conversion;
        - \c native_type: the native database column type; if both \c native_type and \c qore_type are given then \c native_type is used
        - \c size: for data types requiring a size component, the size; for numeric columns this represents the precision for example
        - \c scale: for numeric data types, this value gives the scale
        - \c default_value: the default value for the column
        - \c default_value_native: a boolean flag to say if a \c default_value should be validated against table column type (False) or used as it is (True) to allow to use DBMS native functions or features. Defaults to False. It is strongly recommended to use \c default_value_native for \c default_value in \c driver specific sub-hash to avoid non-portable schema hashes
        @param nullable if @ref True "True" then the column can hold NULL values; note that primary key columns cannot be nullable
        @param lsql an optional reference to a list of strings to retrieve the SQL used to modify the table (only executed if the table is already in the database)

        @throw COLUMN-ERROR no \c native_type or \c qore_type keys in column option hash, column already exists, invalid column data

        @note make sure and add a \c default_value value when adding a column with a \c "not null" constraint with existing data

        @see inDb() for a method that tells if the table is already in the database or not
    */
    AbstractColumn addColumn(string cname, hash<auto> opt, bool nullable = True, *reference lsql) {
        l.lock();
        on_exit l.unlock();

        return addColumnUnlocked(cname, opt, nullable, \lsql);
    }

    #! returns a list of SQL strings that can be use to add a column to the table
    /** @par Example:
        @code{.py}
list l = table.getAddColumnSql("name", ("qore_type": Type::String, "size": 50), False);
        @endcode

        In case the table is already in the database, this method commits the transaction on success and rolls back the transaction if there's an error.

        @param cname the name of the column
        @param copt a hash<auto> describing the column; the following keys are permitted (other column options may be supported depending on the underlying AbstractTable implementation):
        - \c qore_type: a qore type string that will be converted to a native DB type with some default conversion;
        - \c native_type: the native database column type; if both \c native_type and \c qore_type are given then \c native_type is used
        - \c size: for data types requiring a size component, the size; for numeric columns this represents the precision for example
        - \c scale: for numeric data types, this value gives the scale
        - \c default_value: the default value for the column
        - \c default_value_native: a boolean flag to say if a \c default_value should be validated against table column type (False) or used as it is (True) to allow to use DBMS native functions or features. Defaults to False. It is strongly recommended to use \c default_value_native for \c default_value in \c driver specific sub-hash to avoid non-portable schema hashes
        @param nullable if @ref True "True" then the column can hold NULL values; note that primary key columns cannot be nullable
        @param opt a hash of options for the SQL string; see @ref SqlUtil::AbstractTable::AlignTableOptions for common options; each driver can support additional driver-specific options

        @return a list of SQL strings that can be use to add a column to the table

        @throw OPTION-ERROR invalid or unsupported option passed
        @throw COLUMN-ERROR no \c native_type or \c qore_type keys in column option hash, column already exists, invalid column data

        @note
        - make sure and add a \c default_value value when adding a column with a \c "not null" constraint with existing data
        - if the @ref sql_callback_executed "sql_callback_executed option" is @ref True "True" in \a opt, then the changes are also effected in the current object, if not, then they are not (see @ref sql_callback_executed for more information)

        @see inDb() for a method that tells if the table is already in the database or not
    */
    list<auto> getAddColumnSql(string cname, hash<auto> copt, bool nullable = True, *hash<auto> opt) {
        validateOptionsIntern("OPTION-ERROR", getAlignTableOptions(), \opt);

        list lsql;

        {
            l.lock();
            on_exit l.unlock();
            addColumnUnlocked(cname, copt, nullable, \lsql, False, opt.sql_callback_executed);
        }

        return AbstractDatabase::doCallback(opt, lsql, AbstractDatabase::AC_Add, "column", name, cname);
    }

    private AbstractColumn addColumnUnlocked(string cname, hash<auto> opt, bool nullable = True, *reference lsql,
            bool do_exec = True, bool modify_table = True) {
        if (!columns)
            columns = new Columns();
        else if (columns.hasKey(cname))
            throw "COLUMN-ERROR", sprintf("%s column %y: this column already exists", getDesc(), cname);

        if (!opt.native_type && !opt.qore_type)
            throw "COLUMN-ERROR", sprintf("%s column %y: no native_type or qore_type keys in column option hash: %y",
                getDesc(), cname, opt);

        validateColumnOptions(cname, \opt, nullable);

        # get the new column object
        AbstractColumn c = addColumnImpl(cname, opt, nullable);

        # add column in table if the table is already known to be in the database
        lsql = c.getAddColumnSql(self);
        if (do_exec)
            execSql(lsql);

        # add the column to the internal column representation
        if (modify_table)
            addColumnToTableUnlocked(c);

        return c;
    }

    private addColumnToTableUnlocked(AbstractColumn c) {
        columns.add(c.name, c);
    }

    #! modifies an existing column in the table; if the table is already known to be in the database, then the changes are effected in the database also immediately; otherwise it is only updated internally and the new column definition will be created when create() is called for example
    /** @par Example:
        @code{.py}
table.modifyColumn("name", ("qore_type": Type::String, "size": 240), False);
        @endcode

        In case the table is already in the database, this method commits the transaction on success and rolls back the transaction if there's an error.

        @param cname the name of the column
        @param opt a hash<auto> describing the column; the following keys are permitted (other column options may be supported depending on the underlying AbstractTable implementation):
        - \c qore_type: a qore type string that will be converted to a native DB type with some default conversion;
        - \c native_type: the native database column type; if both \c native_type and \c qore_type are given then \c native_type is used
        - \c size: for data types requiring a size component, the size; for numeric columns this represents the precision for example
        - \c scale: for numeric data types, this value gives the scale
        - \c default_value: the default value for the column
        - \c default_value_native: a boolean flag to say if a \c default_value should be validated against table column type (False) or used as it is (True) to allow to use DBMS native functions or features. Defaults to False. It is strongly recommended to use \c default_value_native for \c default_value in \c driver specific sub-hash to avoid non-portable schema hashes
        @param lsql an optional reference to a list of strings to retrieve the SQL used to modify the table (only executed if the table is already in the database)

        @param nullable if @ref True "True" then the column can hold NULL values; note that primary key columns cannot be nullable

        @throw COLUMN-ERROR no \c native_type or \c qore_type keys in column option hash, column does not exist, invalid column data

        @note make sure and add a \c default_value value when modifying a column to have a \c "not null" constraint with existing data

        @see inDb() for a method that tells if the table is already in the database or not
    */
    AbstractColumn modifyColumn(string cname, hash<auto> opt, bool nullable = True, *reference lsql) {
        l.lock();
        on_exit l.unlock();
        getColumnsUnlocked();
        if (!columns.hasKey(cname))
            throw "COLUMN-ERROR", sprintf("%s column %y: the given column does not exist", getDesc(), cname);

        if (!opt.native_type && !opt.qore_type)
            throw "COLUMN-ERROR", sprintf("%s column %y: no native_type or qore_type keys in column option hash: %y", getDesc(), cname, opt);

        validateColumnOptions(cname, \opt, nullable);

        # get the new column object
        AbstractColumn c = addColumnImpl(cname, opt, nullable);

        # add column in table if the table already exists in the DB
        execSql(lsql = columns{cname}.getModifySql(self, c));

        # replaces the column in the internal column representation with the new definition
        addColumnToTableUnlocked(c);
        return c;
    }

    #! gets a list of SQL strings that can be used to modify an existing column in the table
    /** @par Example:
        @code{.py}
list l = table.getModifyColumnSql("name", ("qore_type": Type::String, "size": 240), False);
        @endcode

        @param cname the name of the column
        @param copt a hash<auto> describing the column; the following keys are permitted (other column options may be supported depending on the underlying AbstractTable implementation):
        - \c qore_type: a qore type string that will be converted to a native DB type with some default conversion;
        - \c native_type: the native database column type; if both \c native_type and \c qore_type are given then \c native_type is used
        - \c size: for data types requiring a size component, the size; for numeric columns this represents the precision for example
        - \c scale: for numeric data types, this value gives the scale
        - \c default_value: the default value for the column
        - \c default_value_native: a boolean flag to say if a \c default_value should be validated against table column type (False) or used as it is (True) to allow to use DBMS native functions or features. Defaults to False. It is strongly recommended to use \c default_value_native for \c default_value in \c driver specific sub-hash to avoid non-portable schema hashes
        @param nullable if @ref True "True" then the column can hold NULL values; note that primary key columns cannot be nullable
        @param opt a hash of options for the SQL string; see @ref SqlUtil::AbstractTable::AlignTableOptions for common options; each driver can support additional driver-specific options

        @return a list of SQL strings that can be used to modify an existing column in the table

        @throw OPTION-ERROR invalid or unsupported option passed
        @throw COLUMN-ERROR no \c native_type or \c qore_type keys in column option hash, column does not exist, invalid column data

        @note
        - make sure and add a \c default_value value when modifying a column to have a \c "not null" constraint with existing data
        - if the @ref sql_callback_executed "sql_callback_executed option" is @ref True "True" in \a opt, then the changes are also effected in the current object, if not, then they are not (see @ref sql_callback_executed for more information)

        @see inDb() for a method that tells if the table is already in the database or not
    */
    list<auto> getModifyColumnSql(string cname, hash<auto> copt, bool nullable = True, *hash<auto> opt) {
        validateOptionsIntern("OPTION-ERROR", getAlignTableOptions(), \opt);

        l.lock();
        on_exit l.unlock();
        getColumnsUnlocked();
        if (!columns.hasKey(cname))
            throw "COLUMN-ERROR", sprintf("%s column %y: the given column does not exist", getDesc(), cname);

        if (!copt.native_type && !copt.qore_type)
            throw "COLUMN-ERROR", sprintf("%s column %y: no native_type or qore_type keys in column option hash: %y", getDesc(), cname, copt);

        validateColumnOptions(cname, \copt, nullable);

        # get the new column object
        AbstractColumn c = addColumnImpl(cname, copt, nullable);
        # replaces the column in the internal column representation with the new definition
        on_success if (opt.sql_callback_executed)
            addColumnToTableUnlocked(c);

        return AbstractDatabase::doCallback(opt, columns{cname}.getModifySql(self, c), AbstractDatabase::AC_Modify, "column", cname, name, NOTHING, c.getNativeTypeString());
    }

    #! renames an existing column; if the table is already known to be in the database, then the changes are effected in the database also immediately; otherwise it is only updated internally
    /** @par Example:
        @code{.py}
AbstractColumn col = table.renameColumn("name", "family_name");
        @endcode

        @param old_name the current name of the column
        @param new_name the new name of the column
        @param sql an optional reference to a string to retrieve the SQL used to modify the table (only executed if the table is already in the database)

        @return the column object that was updated

        @throw COLUMN-ERROR if the given column does not exist in the table

        @see modifyColumn() for a method that allows the column definition to be updated

        @note if any primary key or foreign table constraints refer to the column, the internal representations are also renamed, however check constraints are not updated and will be invalid if they refer to the renamed column
    */
    AbstractColumn renameColumn(string old_name, string new_name, reference<string> sql) {
        l.lock();
        on_exit l.unlock();
        getColumnsUnlocked();
        if (!columns.hasKey(old_name))
            throw "COLUMN-ERROR", sprintf("%s column %y: the given column does not exist", getDesc(), old_name);

        AbstractColumn c = columns{old_name};

        # add column in table if the table already exists in the DB
        execSql(sql = c.getRenameSql(self, new_name));

        renameColumnIntern(c, new_name);

        return c;
    }

    #! gets an SQL string that can be used to rename an existing column in the table
    /** @par Example:
        @code{.py}
string sql = table.getRenameColumnSql("name", "family_name");
        @endcode

        @param old_name the current name of the column
        @param new_name the new name of the column
        @param opt a hash of options for the SQL string; see @ref SqlUtil::AbstractTable::AlignTableOptions for common options; each driver can support additional driver-specific options

        @return an SQL string that can be used to rename an existing column in the table

        @throw OPTION-ERROR invalid or unsupported option passed
        @throw COLUMN-ERROR if the old column does not exist in the table or the new column already does

        @see getModifyColumnSql() for a method that allows the column definition to be updated

        @note if the @ref sql_callback_executed "sql_callback_executed option" is @ref True "True" in \a opt, then the changes are also effected in the current object, if not, then they are not (see @ref sql_callback_executed for more information)
    */
    string getRenameColumnSql(string old_name, string new_name, *hash<auto> opt) {
        validateOptionsIntern("OPTION-ERROR", getAlignTableOptions(), \opt);

        l.lock();
        on_exit l.unlock();
        getColumnsUnlocked();

        if (!columns.hasKey(old_name))
            throw "COLUMN-ERROR", sprintf("rename %s column %y to %y: old column %y column does not exist", getDesc(), old_name, new_name, old_name);

        if (columns.hasKey(new_name))
            throw "COLUMN-ERROR", sprintf("rename %s column %y to %y: new column %y already exists in the table", getDesc(), old_name, new_name, new_name);

        on_success if (opt.sql_callback_executed)
            renameColumnIntern(columns{old_name}, new_name);

        return AbstractDatabase::doCallback(opt, columns{old_name}.getRenameSql(self, new_name), AbstractDatabase::AC_Rename, "column", old_name, name, new_name);
    }

    private AbstractColumn renameColumnIntern(AbstractColumn c, string new_name) {
        string old_name = c.name;

        # rename column after database is updated
        c.name = new_name;

        # replaces the column in the internal column representation with the new definition
        columns.renameKey(old_name, new_name);

        # now rename column in any constraints
        # check primary key
        if (!primaryKey.empty() && primaryKey.hasKey(old_name))
            primaryKey.renameKey(old_name, new_name);

        # check foreign constraints
        foreach AbstractForeignConstraint fk in (foreignConstraints.iterator()) {
            if (fk.hasKey(old_name))
                fk.renameKey(old_name, new_name);
        }

        # check unique constraints
        foreach AbstractConstraint ac in (constraints.iterator()) {
            if (!(ac instanceof AbstractUniqueConstraint))
                continue;
            AbstractUniqueConstraint uc = cast<AbstractUniqueConstraint>(ac);
            if (uc.hasKey(old_name))
                uc.renameKey(old_name, new_name);
        }

        # FIXME: check constraints are not updated

        return c;
    }

    private validateOptionsIntern(string err, hash<auto> ropt, reference<hash<auto>> opt) {
        AbstractSqlUtilBase::validateOptionsIntern(err, ropt, \opt, name);
    }

    private validateOptionsIntern(string err, hash<auto> ropt, reference<hash<auto>> opt, string tag) {
        AbstractSqlUtilBase::validateOptionsIntern(err, ropt, \opt, tag);
    }

    private execSql(softlist lsql) {
        #map printf("%s;\n", $1), lsql;
        if (inDb) {
            on_success ds.commit();
            on_error ds.rollback();
            map ds.execRaw($1), lsql;
        }
        else
            manual = True;
    }

    #! adds a primary key to the table; if the table is already known to be in the database, then it is added in the database also immediately; otherwise it is only added internally and can be created when create() is called for example
    /** @par Example:
        @code{.py}
string sql;
AbstractPrimaryKey pk = table.addPrimaryKey("pk_mytable", "id", ("index_tablespace": "index1"), \sql);
printf("%s;\n", sql);
        @endcode

        In case the table is already in the database, this method commits the transaction on success and rolls back the transaction if there's an error.

        @param pkname the name of the new primary key constraint
        @param cols a single column name or a list of columns that make up the primary key
        @param opt a hash of options for the new primary key; each driver may implement its own options; for common options, see @ref SqlUtil::AbstractTable::ConstraintOptions
        @param sql an optional reference to a string to retrieve the SQL used to modify the table (only executed if the table is already in the database)

        @return an AbstractPrimaryKey object corresponding to the primary key created

        @throw PRIMARY-KEY-ERROR the table already has a primary key or invalid columns or options passed

        @see inDb() for a method that tells if the table is already in the database or not
    */
    AbstractPrimaryKey addPrimaryKey(string pkname, softlist cols, *hash<auto> opt, *reference<string> sql) {
        l.lock();
        on_exit l.unlock();

        return addPrimaryKeyUnlocked(pkname, cols, opt, \sql);
    }

    #! returns the SQL that can be used to add a primary key to the table
    /** @par Example:
        @code{.py}
string sql = table.getAddPrimaryKeySql("pk_mytable", "id", ("index_tablespace": "index1"), opt);
printf("%s;\n", sql);
        @endcode

        In case the table is already in the database, this method commits the transaction on success and rolls back the transaction if there's an error.

        @param pkname the name of the new primary key constraint
        @param cols a single column name or a list of columns that make up the primary key
        @param pkopt a hash of options for the new primary key; each driver may implement its own options; for common options, see @ref SqlUtil::AbstractTable::ConstraintOptions
        @param opt a hash of options for the SQL string; see @ref SqlUtil::AbstractTable::AlignTableOptions for common options; each driver can support additional driver-specific options

        @return the SQL that can be used to add a primary key to the table

        @throw OPTION-ERROR invalid or unsupported option passed
        @throw PRIMARY-KEY-ERROR the table already has a primary key or invalid columns or options passed

        @see inDb() for a method that tells if the table is already in the database or not

        @note if the @ref sql_callback_executed "sql_callback_executed option" is @ref True "True" in \a opt, then the changes are also effected in the current object, if not, then they are not (see @ref sql_callback_executed for more information)
    */
    string getAddPrimaryKeySql(string pkname, softlist cols, *hash pkopt, *hash<auto> opt) {
        validateOptionsIntern("OPTION-ERROR", getAlignTableOptions(), \opt);
        l.lock();
        on_exit l.unlock();

        string sql;
        AbstractPrimaryKey pk = addPrimaryKeyUnlockedIntern(pkname, cols, pkopt, \sql);
        on_success if (opt.sql_callback_executed) {
            setPrimaryKeyUnlocked(pk);
            manual = True;
        }
        return AbstractDatabase::doCallback(opt, sql, AbstractDatabase::AC_Add, "primary key", pkname, getSqlName());
    }

    private setPrimaryKeyUnlocked(AbstractPrimaryKey pk) {
        primaryKey = pk;
    }

    private AbstractPrimaryKey addPrimaryKeyUnlocked(string pkname, softlist cols, *hash<auto> opt, *reference<string> sql) {
        AbstractPrimaryKey pk = addPrimaryKeyUnlockedIntern(pkname, cols, opt, \sql);
        execSql(sql);
        setPrimaryKeyUnlocked(pk);
        return pk;
    }

    private AbstractPrimaryKey addPrimaryKeyUnlockedIntern(string pkname, softlist cols, *hash<auto> opt, *reference<string> sql) {
        getColumnsUnlocked();
        if (!manual)
            getIndexesUnlocked();

        # load constraints if needed, verify unique constraint name, validate/process options
        checkUniqueConstraintNameValidateOptions("PRIMARY-KEY-ERROR", pkname, getConstraintOptions(), \opt);

        if (!primaryKey.empty())
            throw "PRIMARY-KEY-ERROR", sprintf("%s add primary key %y: already has primary key %y with columns: %y", getDesc(), pkname, primaryKey.getName(), primaryKey.keys());

        if (!cols)
            throw "PRIMARY-KEY-ERROR", sprintf("%s add primary key %y: no column names passed to %s::addPrimaryKey()", getDesc(), pkname, self.className());

        hash<auto> ch;
        while (cols) {
            auto v = shift cols;
            if (v.typeCode() != NT_STRING)
                throw "PRIMARY-KEY-ERROR", sprintf("%s add primary key %y: %y argument passed as column name to %s::addPrimaryKey(); expecting \"string\" (value: %y)", getDesc(), pkname, v.type(), self.className(), v);
            if (!columns.hasKey(v))
                throw "PRIMARY-KEY-ERROR", sprintf("%s add primary key %y: table has no column named %y", getDesc(), pkname, v);
            ch{v} = columns{v};
        }

        # get the new primary key object
        AbstractPrimaryKey pk = addPrimaryKeyImpl(pkname, ch, opt);

        # find matching index for unique constraint
        pk.findMatchingIndex(indexes);

        # add pk to table if the table already exists in the DB
        sql = pk.getCreateSql(getSqlName());

        return pk;
    }

    #! gets a list of SQL strings to drop all constraints and indexes with the given column name; if the column does not exist then an empty list is returned
    /** @par Example:
        @code{.py}
list l = table.getDropAllConstraintsAndIndexesOnColumnSql("status");
        @endcode

        @param cname the name of the column
        @param opt a hash of options for the SQL string; see @ref SqlUtil::AbstractTable::AlignTableOptions for common options; each driver can support additional driver-specific options

        @throw OPTION-ERROR invalid or unsupported option passed

        @note
        - this method retrieves current primary key definition from the database if not already loaded or defined
        - if the @ref sql_callback_executed "sql_callback_executed option" is @ref True "True" in \a opt, then the changes are also effected in the current object, if not, then they are not (see @ref sql_callback_executed for more information)

        @see inDb() for a method that tells if the table is already in the database or not
    */
    list<auto> getDropAllConstraintsAndIndexesOnColumnSql(string cname, *hash<auto> opt) {
        validateOptionsIntern("OPTION-ERROR", getAlignTableOptions(), \opt);

        l.lock();
        on_exit l.unlock();

        return getDropAllConstraintsAndIndexesOnColumnSqlUnlocked(cname, opt);
    }

    private list<auto> getDropAllConstraintsAndIndexesOnColumnSqlUnlocked(string cname, *hash<auto> opt) {
        getAllConstraintsUnlocked();
        getIndexesUnlocked();

        list lst = ();

        if (primaryKey && primaryKey.hasColumn(cname)) {
            on_success if (opt.sql_callback_executed)
                remove primaryKey;

            lst += AbstractDatabase::doCallback(opt, primaryKey.getDropSql(getSqlName()), AbstractDatabase::AC_Drop, "primary key", primaryKey.getName(), getSqlName());
        }

        foreach AbstractConstraint c in (constraints.iterator()) {
            if (c.hasColumn(cname)) {
                on_success if (opt.sql_callback_executed) {
                    constraints.take(c.getName());
                    # if it's a unique constraint, remove any index with the same name
                    if (c instanceof AbstractUniqueConstraint && constraintsLinkedToIndexesImpl())
                        indexes.take(c.getName());
                }

                lst += AbstractDatabase::doCallback(opt, c.getDropSql(getSqlName()), AbstractDatabase::AC_Drop, "constraint", c.getName(), getSqlName());
            }
        }

        foreach AbstractForeignConstraint fk in (foreignConstraints.iterator()) {
            if (fk.hasColumn(cname)) {
                on_success if (opt.sql_callback_executed)
                    foreignConstraints.take(fk.getName());

                lst += AbstractDatabase::doCallback(opt, fk.getDropSql(getSqlName()), AbstractDatabase::AC_Drop,
                    "foreign constraint", fk.getName(), getSqlName());
            }
        }

        foreach AbstractIndex ix in (indexes.iterator()) {
            if (ix.hasColumn(cname)) {
                on_success if (opt.sql_callback_executed)
                    indexes.take(ix.name);

                lst += AbstractDatabase::doCallback(opt, ix.getDropSql(getSqlName()), AbstractDatabase::AC_Drop, "index", ix.name, getSqlName());
            }
        }

        return lst;
    }

    #! gets a list of SQL strings that can be used to drop the primary key from the table
    /** @par Example:
        @code{.py}
list l = table.getDropPrimaryKeySql();
        @endcode

        @param opt a hash of options for the SQL string; see @ref SqlUtil::AbstractTable::AlignTableOptions for common options; each driver can support additional driver-specific options

        @return a list of SQL strings that can be used to drop the primary key from the table

        @throw OPTION-ERROR invalid or unsupported option passed
        @throw PRIMARY-KEY-ERROR the table has no primary key

        @note
        - this method retrieves current primary key definition from the database if not already loaded or defined
        - if there are known foreign contraints on the primary key, SQL for dropping those constraints is also returned
        - if the @ref sql_callback_executed "sql_callback_executed option" is @ref True "True" in \a opt, then the changes are also effected in the current object, if not, then they are not (see @ref sql_callback_executed for more information)

        @see inDb() for a method that tells if the table is already in the database or not
    */
    list<auto> getDropPrimaryKeySql(*hash<auto> opt) {
        validateOptionsIntern("OPTION-ERROR", getAlignTableOptions(), \opt);

        l.lock();
        on_exit l.unlock();

        getPrimaryKeyUnlocked();
        if (primaryKey.empty())
            throw "PRIMARY-KEY-ERROR", sprintf("%s: has no primary key", getDesc());

        on_success if (opt.sql_callback_executed)
            remove primaryKey;

        list l = ();
        map l += AbstractDatabase::doCallback(opt, $1.fk.getDropSql($1.table), AbstractDatabase::AC_Drop, "foreign constraint", $1.fk.getName(), $1.table), primaryKey.getSourceConstraintIterator();

        l += AbstractDatabase::doCallback(opt, primaryKey.getDropSql(getSqlName()), AbstractDatabase::AC_Drop, "primary key", primaryKey.getName(), name);
        return l;
    }

    #! drops the primary key from the table; if the table is known to be in the database already, then it is also dropped from the database immediately; otherwise it is only removed internally
    /** @par Example:
        @code{.py}
table.dropPrimaryKey();
        @endcode

        In case the table is already in the database, this method commits the transaction on success and rolls back the transaction if there's an error.
        @param lsql an optional reference to a string to retrieve the SQL used to modify the table (only executed if the table is already in the database)

        @return the primary key dropped from the table

        @throw PRIMARY-KEY-ERROR no primary key exists

        @note
        - this method retrieves the primary key definition from the database if none has already been defined
        - if there are known foreign contraints on the primary key, SQL for dropping those constraints is also included

        @see inDb() for a method that tells if the table is already in the database or not
    */
    AbstractPrimaryKey dropPrimaryKey(*reference lsql) {
        l.lock();
        on_exit l.unlock();
        getPrimaryKeyUnlocked();
        if (primaryKey.empty())
            throw "PRIMARY-KEY-ERROR", sprintf("%s: has no primary key", getDesc());

        lsql = ();
        map execSql(lsql += $1.fk.getDropSql($1.table)), primaryKey.getSourceConstraintIterator();

        # drop pk from table if the table already exists in the DB
        execSql(lsql += primaryKey.getDropSql(getSqlName()));

        return remove primaryKey;
    }

    #! adds a unique constraint to the table; if the table is known to be in the database already, then the constraint is added to the database also immediately; otherwise it is only added internally and can be created when create() is called for example
    /** @par Example:
        @code{.py}
string sql;
AbstractUniqueConstraint c = table.addUniqueConstraint("uk_mytable", "name", ("index_tablespace": "index1"), \sql);
printf("%s;\n", sql);
        @endcode

        In case the table is already in the database, this method commits the transaction on success and rolls back the transaction if there's an error.

        @param cname the name of the new unique constraint
        @param cols a single column name or a list of columns that make up the unique constraint
        @param opt a hash of options for the new unique constraint; each driver may implement its own options; for common options, see @ref SqlUtil::AbstractTable::ConstraintOptions
        @param sql an optional reference to a string to retrieve the SQL used to modify the table (only executed if the table is already in the database)

        @return an AbstractUniqueConstraint object corresponding to the unique constraint created

        @throw UNIQUE-CONSTRAINT-ERROR the table already has a constraint with the given name or invalid columns passed
        @throw OPTION-ERROR invalid or unsupported option passed

        @see inDb() for a method that tells if the table is already in the database or not
    */
    AbstractUniqueConstraint addUniqueConstraint(string cname, softlist cols, *hash<auto> opt, *reference<string> sql) {
        l.lock();
        on_exit l.unlock();
        return addUniqueConstraintUnlocked(cname, cols, opt, \sql);
    }

    #! returns an SQL string that can be used to add a unique constraint to the table
    /** @par Example:
        @code{.py}
string sql = table.getAddUniqueConstraintSql("uk_mytable", "name", ("index_tablespace": "index1"));
printf("%s;\n", sql);
        @endcode

        @param cname the name of the new unique constraint
        @param cols a single column name or a list of columns that make up the unique constraint
        @param ukopt a hash of options for the new unique constraint; each driver may implement its own options; for common options, see @ref SqlUtil::AbstractTable::ConstraintOptions
        @param opt a hash of options for the SQL string; see @ref SqlUtil::AbstractTable::AlignTableOptions for common options; each driver can support additional driver-specific options

        @return an SQL string that can be used to add a unique constraint to the table

        @throw OPTION-ERROR invalid or unsupported option passed
        @throw UNIQUE-CONSTRAINT-ERROR the table already has a constraint with the given name or invalid columns passed

        @see inDb() for a method that tells if the table is already in the database or not

        @note if the @ref sql_callback_executed "sql_callback_executed option" is @ref True "True" in \a opt, then the changes are also effected in the current object, if not, then they are not (see @ref sql_callback_executed for more information)
    */
    string getAddUniqueConstraintSql(string cname, softlist cols, *hash ukopt, *hash<auto> opt) {
        validateOptionsIntern("OPTION-ERROR", getAlignTableOptions(), \opt);

        l.lock();
        on_exit l.unlock();

        string sql;
        AbstractUniqueConstraint uk = addUniqueConstraintUnlockedIntern(cname, cols, ukopt, \sql);
        on_success if (opt.sql_callback_executed) {
            # set as constraint for the table
            if (!constraints)
                constraints = new Constraints();
            constraints.add(cname, uk);
            manual = True;
        }
        return AbstractDatabase::doCallback(opt, sql, AbstractDatabase::AC_Add, "unique constraint", cname, getSqlName());
    }

    private AbstractUniqueConstraint addUniqueConstraintUnlocked(string cname, softlist cols, *hash<auto> opt, *reference<string> sql) {
        AbstractUniqueConstraint uk = addUniqueConstraintUnlockedIntern(cname, cols, opt, \sql);
        # add foreign constraint to table in the DB if the table already exists in the DB
        execSql(sql);
        # set as constraint for the table
        if (!constraints)
            constraints = new Constraints();
        constraints.add(cname, uk);
        return uk;
    }

    private AbstractUniqueConstraint addUniqueConstraintUnlockedIntern(string cname, softlist cols, *hash<auto> opt, *reference<string> sql) {
        getColumnsUnlocked();
        if (!manual)
            getIndexesUnlocked();

        # load constraints if needed, verify unique constraint name, validate/process options
        checkUniqueConstraintNameValidateOptions("UNIQUE-CONSTRAINT-ERROR", cname, getConstraintOptions(), \opt);

        if (!cols)
            throw "UNIQUE-CONSTRAINT-ERROR", sprintf("%s add unique constraint %y: no column names passed to %s::addUniqueConstraint()", getDesc(), cname, self.className());

        hash<auto> ch;
        while (cols) {
            auto v = shift cols;
            if (v.typeCode() != NT_STRING)
                throw "PRIMARY-KEY-ERROR", sprintf("%s add unique constraint %y: %y argument passed as column name to %s::addUniqueConstraint(); expecting \"string\" (value: %y)", getDesc(), cname, v.type(), self.className(), v);
            if (!columns.hasKey(v))
                throw "PRIMARY-KEY-ERROR", sprintf("%s add unique constraint %y: table has no column named %y", getDesc(), cname, v);
            ch{v} = columns{v};
        }

        # get the new constraint object
        AbstractUniqueConstraint c = addUniqueConstraintImpl(cname, ch, opt);

        # find matching index for unique constraint
        c.findMatchingIndex(indexes);

        # add foreign constraint to table in the DB if the table already exists in the DB
        sql = c.getCreateSql(getSqlName());

        # return the new constraint
        return c;
    }

    #! adds an index to the table; if the table is already known to be in the database, then it is added in the database also immediately; otherwise it is only added internally and can be created when create() is called for example
    /** @par Example:
        @code{.py}
string sql;
AbstractIndex ix = table.addIndex("uk_mytable_name", True, "name", ("index_tablespace": "index1"), \sql);
printf("%s;\n", sql);
        @endcode

        In case the table is already in the database, this method commits the transaction on success and rolls back the transaction if there's an error.

        @param iname the name of the new index
        @param unique a flag to tell if the new index should be unique or not
        @param cols a single column name or a list of columns that make up the index
        @param opt a hash of options for the new index; each driver may implement its own options; for common options, see @ref SqlUtil::AbstractTable::IndexOptions
        @param sql an optional reference to a string to retrieve the SQL used to modify the table (only executed if the table is already in the database)

        @return an AbstractIndex object corresponding to the index created

        @throw INDEX-ERROR the table already has an index with the given name or invalid columns or options were passed

        @see inDb() for a method that tells if the table is already in the database or not
    */
    AbstractIndex addIndex(string iname, bool unique, softlist cols, *hash<auto> opt, *reference<string> sql) {
        l.lock();
        on_exit l.unlock();

        return addIndexUnlocked(iname, unique, cols, opt, \sql);
    }

    #! returns an SQL string that can be used to add an index to the table
    /** @par Example:
        @code{.py}
string sql = table.getAddIndexSql("uk_mytable_name", True, "name", ("index_tablespace": "index1"));
printf("%s;\n", sql);
        @endcode

        @param iname the name of the new index
        @param unique a flag to tell if the new index should be unique or not
        @param cols a single column name or a list of columns that make up the index
        @param ixopt a hash of options for the new index; each driver may implement its own options; for common options, see @ref SqlUtil::AbstractTable::IndexOptions
        @param opt a hash of options for the SQL string; see @ref SqlUtil::AbstractTable::AlignTableOptions for common options; each driver can support additional driver-specific options

        @return an SQL string that can be used to add an index to the table

        @throw OPTION-ERROR invalid or unsupported option passed
        @throw INDEX-ERROR the table already has an index with the given name or invalid columns or options were passed

        @see inDb() for a method that tells if the table is already in the database or not

        @note if the @ref sql_callback_executed "sql_callback_executed option" is @ref True "True" in \a opt, then the changes are also effected in the current object, if not, then they are not (see @ref sql_callback_executed for more information)
    */
    string getAddIndexSql(string iname, bool unique, softlist cols, *hash<auto> ixopt, *hash<auto> opt) {
        validateOptionsIntern("OPTION-ERROR", getAlignTableOptions(), \opt);

        l.lock();
        on_exit l.unlock();

        string sql;
        AbstractIndex ix = addIndexUnlockedIntern(iname, unique, cols, ixopt, \sql);
        on_success if (opt.sql_callback_executed) {
            # set as index for the table
            indexes.add(iname, ix);
            manual = True;
        }
        return AbstractDatabase::doCallback(opt, sql, AbstractDatabase::AC_Add, "index", iname, getSqlName());
    }

    private AbstractIndex addIndexUnlocked(string iname, bool unique, softlist cols, *hash<auto> opt, *reference<string> sql) {
        AbstractIndex ix = addIndexUnlockedIntern(iname, unique, cols, opt, \sql);
        execSql(sql);
        # set as index for the table
        indexes.add(iname, ix);
        if (ix.unique && uniqueIndexCreatesConstraintImpl()) {
            getConstraintsUnlocked();
            if (!constraints.hasKey(ix.name)) {
                string csql;
                AbstractUniqueConstraint uk = addUniqueConstraintUnlockedIntern(iname, cols, opt, \csql);
                uk.setIndexBase(ix.name);
                if (!constraints)
                    constraints = new Constraints();
                constraints.add(iname, uk);
                ix.setSupportingConstraint(uk);
            }
        }
        return ix;
    }

    private AbstractIndex addIndexUnlockedIntern(string iname, bool unique, softlist cols, *hash<auto> opt, *reference<string> sql) {
        getColumnsUnlocked();
        if (inDb)
            getIndexesUnlocked();

        if (!indexes)
            indexes = new Indexes();
        else if (indexes.hasKey(iname))
            throw "INDEX-ERROR", sprintf("%s: already has an index named %y with columns: %y", getDesc(), iname, indexes{iname}.columns.keys());

        if (!cols)
            throw "INDEX-ERROR", sprintf("%s: no column names passed to %s::addIndex()", getDesc(), self.className());

        hash<auto> ch;
        while (cols) {
            auto v = shift cols;
            if (v.typeCode() != NT_STRING)
                throw "INDEX-ERROR", sprintf("%s: %y argument passed as column name to %s::addIndex(); expecting \"string\" (value: %y)", getDesc(), v.type(), self.className(), v);
            # only check for column if the index is not a function index
            if (!columns.hasKey(v)) {
                # actually the original check for /\(.*\)/ in the value is not correct.
                # DBMS usually allow to specify special types of func. indexes. Like eg.
                # Oracle's "foo desc", PGSQL "collate" etc. All these constructs are
                # without () inside.
                ch{v} = {};
            }
            else
                ch{v} = columns{v};
        }

        # validate options
        hash<auto> ixopt = getIndexOptions();
        validateOptionsIntern("INDEX-ERROR", ixopt, \opt);

        # get the new index object
        AbstractIndex ix = addIndexImpl(iname, unique, ch, opt);

        # add index to table in the DB if the table already exists in the DB
        sql = ix.getCreateSql(getSqlName(), opt);

        # return the new index
        return ix;
    }

    #! renames an existing index; if the table is already known to be in the database, then the changes are effected in the database also immediately; otherwise it is only updated internally
    /** @par Example:
        @code{.py}
AbstractIndex ix = table.renameIndex("uk_table_name", "unique_table_name");
        @endcode

        @param old_name the current name of the index
        @param new_name the new name for the index
        @param sql an optional reference to a string to retrieve the SQL used to modify the table (only executed if the table is already in the database)
        @return the index object that was updated

        @throw INDEX-ERROR if the old index does not exist in the table or if the new index name already exists
    */
    AbstractIndex renameIndex(string old_name, string new_name, reference<string> sql) {
        l.lock();
        on_exit l.unlock();
        getIndexesUnlocked();

        AbstractIndex ix;

        code rename;

        if (!indexes || !indexes.hasKey(old_name))
            throw "INDEX-ERROR", sprintf("%s: has no index %y; valid index: %y", getDesc(), old_name, indexes.keys());

        ix = indexes{old_name};
        rename = sub () { indexes.renameKey(old_name, new_name); };

        # rename index in table if the table already exists in the DB
        execSql(sql = ix.getRenameSql(getSqlName(), new_name));

        ix.name = new_name;

        # rename index in container structure
        rename();

        # return the updated index
        return ix;
    }

    #! drops the given index from the table; if the table is known to be in the database already, then it is also dropped from the database immediately; otherwise it is only removed internally
    /** @par Example:
        @code{.py}
table.dropIndex("uk_mytable_name");
        @endcode

        In case the table is already in the database, this method commits the transaction on success and rolls back the transaction if there's an error.

        @param iname the name of the index to drop
        @param sql an optional reference to a string to retrieve the SQL used to modify the table (only executed if the table is already in the database)

        @return the index dropped from the table

        @throw INDEX-ERROR the given index does not exist

        @note this method retrieves all current index definitions from the database if none have already been defined

        @see inDb() for a method that tells if the table is already in the database or not
    */
    AbstractIndex dropIndex(string iname, *reference<string> sql) {
        l.lock();
        on_exit l.unlock();
        getIndexesUnlocked();
        if (!indexes.hasKey(iname))
            throw "INDEX-ERROR", sprintf("%s: has no index %y; valid indexes: %y", getDesc(), iname, indexes.keys());

        # drop index from table if the table already exists in the DB
        execSql(sql = indexes{iname}.getDropSql(getSqlName()));

        # remove from index structure and return removed index
        return indexes.take(iname);
    }

    #! gets the SQL that can be used to drop an index from the table
    /** @par Example:
        @code{.py}
string sql = table.getDropIndexSql("uk_mytable_name");
        @endcode

        @param iname the name of the index to drop
        @param opt a hash of options for the SQL string; see @ref SqlUtil::AbstractTable::AlignTableOptions for common options; each driver can support additional driver-specific options

        @return the SQL that can be used to drop an index from the table

        @throw OPTION-ERROR invalid or unsupported option passed
        @throw INDEX-ERROR the given index does not exist in the table

        @note
        - this method retrieves current index definitions from the database if not already loaded or defined
        - if the @ref sql_callback_executed "sql_callback_executed option" is @ref True "True" in \a opt, then the changes are also effected in the current object, if not, then they are not (see @ref sql_callback_executed for more information)

        @see inDb() for a method that tells if the table is already in the database or not
    */
    string getDropIndexSql(string iname, *hash<auto> opt) {
        validateOptionsIntern("OPTION-ERROR", getAlignTableOptions(), \opt);

        l.lock();
        on_exit l.unlock();

        getIndexesUnlocked();
        if (!indexes.hasKey(iname))
            throw "INDEX-ERROR", sprintf("%s: has no index %y; valid indexes: %y", getDesc(), iname, indexes.keys());

        on_success if (opt.sql_callback_executed)
            indexes.take(iname);

        return AbstractDatabase::doCallback(opt, indexes{iname}.getDropSql(getSqlName()), AbstractDatabase::AC_Drop, "index", iname, name);
    }

    #! adds a foreign constraint to the table; if the table is already known to be in the database, then it is added in the database also immediately; otherwise it is only added internally and can be created when create() is called for example
    /** @par Example:
        @code{.py}
string sql;
AbstractForeignConstraint fc = table.addForeignConstraint("fk_mytable_other_table", ("name", "version"), "other_table", NOTHING, \sql);
printf("%s;\n", sql);
        @endcode

        In case the table is already in the database, this method commits the transaction on success and rolls back the transaction if there's an error.

        @param cname the name of the new foreign constraint
        @param cols a single column name or a list of columns in the local table that make up the foreign constraint
        @param table the name of the other table that the constraint targets
        @param tcols a single column name or a list of columns in the foreign table or @ref nothing meaning that the column names are the same as in the local table; if column names are given the same number of columns must be given in the local and foreign tables
        @param opt a hash of options for the new foreign constraint; each driver may implement its own options; for common options, see @ref SqlUtil::AbstractTable::ForeignConstraintOptions
        @param sql an optional reference to a string to retrieve the SQL used to modify the table (only executed if the table is already in the database)

        @return an AbstractForeignConstraint object corresponding to the foreign constraint created

        @throw FOREIGN-CONSTRAINT-ERROR the table already has a constraint with the given name or invalid columns or options were passed

        @see inDb() for a method that tells if the table is already in the database or not
    */
    AbstractForeignConstraint addForeignConstraint(string cname, softlist cols, string table, *softlist tcols, *hash<auto> opt, *reference<string> sql) {
        l.lock();
        on_exit l.unlock();

        return addForeignConstraintUnlocked(cname, cols, table, tcols, opt, \sql);
    }

    #! returns an SQL string that can be used to add a foreign constraint to the table
    /** @par Example:
        @code{.py}
string sql = table.getAddForeignConstraintSql("fk_mytable_other_table", ("name", "version"), "other_table");
printf("%s;\n", sql);
        @endcode

        @param cname the name of the new foreign constraint
        @param cols a single column name or a list of columns in the local table that make up the foreign constraint
        @param table the name of the other table that the constraint targets
        @param tcols a single column name or a list of columns in the foreign table or @ref nothing meaning that the column names are the same as in the local table; if column names are given the same number of columns must be given in the local and foreign tables
        @param fkopt a hash of options for the new foreign constraint; each driver may implement its own options; for common options, see @ref SqlUtil::AbstractTable::ForeignConstraintOptions
        @param opt a hash of options for the SQL string; see @ref SqlUtil::AbstractTable::AlignTableOptions for common options; each driver can support additional driver-specific options

        @return an SQL string that can be used to add a foreign constraint to the table

        @throw OPTION-ERROR invalid or unsupported option passed
        @throw FOREIGN-CONSTRAINT-ERROR the table already has a constraint with the given name or invalid columns or options were passed

        @see inDb() for a method that tells if the table is already in the database or not

        @note if the @ref sql_callback_executed "sql_callback_executed option" is @ref True "True" in \a opt, then the changes are also effected in the current object, if not, then they are not (see @ref sql_callback_executed for more information)
    */
    string getAddForeignConstraintSql(string cname, softlist cols, string table, *softlist tcols, *hash fkopt, *hash<auto> opt) {
        validateOptionsIntern("OPTION-ERROR", getAlignTableOptions(), \opt);

        l.lock();
        on_exit l.unlock();

        string sql;
        AbstractForeignConstraint fk = addForeignConstraintUnlockedIntern(cname, cols, table, tcols, fkopt, \sql);
        on_success if (opt.sql_callback_executed) {
            # update cached tables with foreign constraint source info
            if (opt.table_cache.hasKey(fk.target.table))
                opt.table_cache{fk.target.table}.addSourceConstraint(getSqlName(), fk);

            # set as foreign constraint for the table
            if (!foreignConstraints)
                foreignConstraints = new ForeignConstraints();
            foreignConstraints.add(cname, fk);
            manual = True;
        }
        return AbstractDatabase::doCallback(opt, sql, AbstractDatabase::AC_Add, "foreign constraint", cname, getSqlName());
    }

    private Columns getReferencedTableColumnsUnlocked(string table, *Tables cache, string err = "FOREIGN-CONSTRAINT-ERROR") {
        if (table == name) {
            getColumnsUnlocked();
            return columns;
        }
        if (cache.hasKey(table))
            return cache{table}.describe();
        Table t(ds, table);
        if (cache) {
            #printf("*** adding %s to cache\n", name);
            cache.add(table, t.getTable());
        }
        return t.describe();
    }

    private AbstractForeignConstraint addForeignConstraintUnlocked(string cname, softlist cols, string table, *softlist tcols, *hash<auto> opt, *reference<string> sql) {
        AbstractForeignConstraint fk = addForeignConstraintUnlockedIntern(cname, cols, table, tcols, opt, \sql);
        execSql(sql);

        # update cached tables with foreign constraint source info
        if (opt.table_cache.hasKey(fk.target.table))
            opt.table_cache{fk.target.table}.addSourceConstraint(getSqlName(), fk);

        # set as foreign constraint for the table
        if (!foreignConstraints)
            foreignConstraints = new ForeignConstraints();
        foreignConstraints.add(cname, fk);
        return fk;
    }

    private AbstractForeignConstraint addForeignConstraintUnlockedIntern(string cname, softlist cols, string table, *softlist tcols, *hash<auto> opt, *reference<string> sql) {
        getColumnsUnlocked();
        # load constraints if needed, verify unique constraint name, validate/process options
        checkUniqueConstraintNameValidateOptions("FOREIGN-CONSTRAINT-ERROR", cname, getForeignConstraintOptions(), \opt);

        if (!cols)
            throw "FOREIGN-CONSTRAINT-ERROR", sprintf("%y: no column names passed to %s::addForeignConstraint()", getDesc(), self.className());

        hash<auto> ch;
        while (cols) {
            auto v = shift cols;
            if (v.typeCode() != NT_STRING)
                throw "FOREIGN-CONSTRAINT-ERROR", sprintf("%y: %y argument passed as column name to %s::addForeignConstraint(); expecting \"string\" (value: %y)", getDesc(), v.type(), self.className(), v);
            if (!columns.hasKey(v))
                throw "FOREIGN-CONSTRAINT-ERROR", sprintf("%y: has no column named %y", getDesc(), v);
            ch{v} = columns{v};
        }

        if (!tcols)
            tcols = ch.keys();

        # get target table description
        Columns targ_cols = getReferencedTableColumnsUnlocked(table, opt.table_cache);
        hash tch;
        while (tcols) {
            auto v = shift tcols;
            if (v.typeCode() != NT_STRING)
                throw "FOREIGN-CONSTRAINT-ERROR", sprintf("%y target %y: %y argument passed as target column name to %s::addForeignConstraint(); expecting \"string\" (value: %y)", getDesc(), table, v.type(), self.className(), v);
            if (!targ_cols.hasKey(v))
                throw "FOREIGN-CONSTRAINT-ERROR", sprintf("%y target %y: target table has no column named %y", getDesc(), table, v);
            tch{v} = targ_cols{v};
        }

        # get the new foreign constraint object
        AbstractForeignConstraint fk = addForeignConstraintImpl(cname, ch, table, tch, opt);

        sql = fk.getCreateSql(getSqlName());

        # return the new foreign constraint
        return fk;
    }

    #! drops a foreign constraint from the table; if the table is known to be in the database already, then it is also dropped from the database immediately; otherwise it is only removed internally
    /** @par Example:
        @code{.py}
table.dropForeignConstraint("uk_mytable_name");
        @endcode

        In case the table is already in the database, this method commits the transaction on success and rolls back the transaction if there's an error.

        @param cname the name of the foreign constraint to drop
        @param sql an optional reference to a string to retrieve the SQL used to modify the table (only executed if the table is already in the database)

        @return the foreign constraint dropped from the table

        @throw FOREIGN-CONSTRAINT-ERROR the given constraint does not exist

        @note this method retrieves all current foreign constraint definitions from the database if none have already been defined

        @see inDb() for a method that tells if the table is already in the database or not
    */
    AbstractForeignConstraint dropForeignConstraint(string cname, *reference<string> sql) {
        l.lock();
        on_exit l.unlock();
        getForeignConstraintsUnlocked();
        if (!foreignConstraints.hasKey(cname))
            throw "FOREIGN-CONSTRAINT-ERROR", sprintf("%s: has no foreign constraint %y; valid foreign constraints: %y", getDesc(), cname, foreignConstraints.keys());

        # drop foreign constraint from table if the table already exists in the DB
        execSql(sql = foreignConstraints{cname}.getDropSql(getSqlName()));

        # remove from foreign constraint structure and return removed foreign constraint
        return foreignConstraints.take(cname);
    }

    #! removes the named foreign constraint from the table; no SQL is executed in any case, only the named foreign constraint is removed from the table definition
    /** @par Example:
        @code{.py}
table.removeForeignConstraint("uk_mytable_name");
        @endcode

        @param cname the name of the foreign constraint to remove

        @return the foreign constraint removed from the table

        @throw FOREIGN-CONSTRAINT-ERROR the given constraint does not exist

        @note this method retrieves all current foreign constraint definitions from the database if none have already been defined

        @see inDb() for a method that tells if the table is already in the database or not
    */
    AbstractForeignConstraint removeForeignConstraint(string cname) {
        l.lock();
        on_exit l.unlock();
        getForeignConstraintsUnlocked();
        if (!foreignConstraints.hasKey(cname))
            throw "FOREIGN-CONSTRAINT-ERROR", sprintf("%s: has no foreign constraint %y; valid foreign constraints: %y", getDesc(), cname, foreignConstraints.keys());

        # remove from foreign constraint structure and return removed foreign constraint
        return foreignConstraints.take(cname);
    }

    #! adds a check constraint to the table; if the table is already known to be in the database, then it is added in the database also immediately; otherwise it is only added internally and can be created when create() is called for example
    /** @par Example:
        @code{.py}
string sql;
AbstractCheckConstraint cc = table.addCheckConstraint("check_mytable_id", "id > 10", NOTHING, \sql);
printf("%s;\n", sql);
        @endcode

        In case the table is already in the database, this method commits the transaction on success and rolls back the transaction if there's an error.

        @param cname the name of the new constraint
        @param src the source of the constraint clause
        @param opt a hash of options for the new constraint; each driver may implement its own options; for common options, see @ref SqlUtil::AbstractTable::ConstraintOptions
        @param sql an optional reference to a string to retrieve the SQL used to modify the table (only executed if the table is already in the database)

        @return an AbstractCheckConstraint object corresponding to the check constraint created

        @throw CHECK-CONSTRAINT-ERROR the table already has a constraint with the given name or invalid columns or options were passed

        @see inDb() for a method that tells if the table is already in the database or not
    */
    AbstractCheckConstraint addCheckConstraint(string cname, string src, *hash<auto> opt, *reference<string> sql) {
        l.lock();
        on_exit l.unlock();
        return addCheckConstraintUnlocked(cname, src, opt, \sql);
    }

    #! returns an SQL string that can be used to add a check constraint to the table
    /** @par Example:
        @code{.py}
string sql = table.getAddCheckConstraintSql("check_mytable_id", "id > 10");
printf("%s;\n", sql);
        @endcode

        In case the table is already in the database, this method commits the transaction on success and rolls back the transaction if there's an error.

        @param cname the name of the new constraint
        @param src the source of the constraint clause
        @param copt a hash of options for the new constraint; each driver may implement its own options; for common options, see @ref SqlUtil::AbstractTable::ConstraintOptions
        @param opt a hash of options for the SQL string; see @ref SqlUtil::AbstractTable::AlignTableOptions for common options; each driver can support additional driver-specific options

        @return an SQL string that can be used to add a check constraint to the table

        @throw OPTION-ERROR invalid or unsupported option passed
        @throw CHECK-CONSTRAINT-ERROR the table already has a constraint with the given name or invalid columns or options were passed

        @see inDb() for a method that tells if the table is already in the database or not

        @note if the @ref sql_callback_executed "sql_callback_executed option" is @ref True "True" in \a opt, then the changes are also effected in the current object, if not, then they are not (see @ref sql_callback_executed for more information)
    */
    string getAddCheckConstraintSql(string cname, string src, *hash copt, *hash<auto> opt) {
        validateOptionsIntern("OPTION-ERROR", getAlignTableOptions(), \opt);

        l.lock();
        on_exit l.unlock();

        string sql;
        AbstractCheckConstraint cc = addCheckConstraintUnlocked(cname, src, copt, \sql);
        on_success if (opt.sql_callback_executed) {
            # set as constraint for the table
            if (!constraints)
                constraints = new Constraints();
            constraints.add(cname, cc);
            manual = True;
        }
        return AbstractDatabase::doCallback(opt, sql, AbstractDatabase::AC_Add, "check constraint", cname, getSqlName());
    }

    private AbstractCheckConstraint addCheckConstraintUnlocked(string cname, string src, *hash<auto> opt, *reference<string> sql) {
        AbstractCheckConstraint cc = addCheckConstraintUnlockedIntern(cname, src, opt, \sql);
        execSql(sql);

        # set as constraint for the table
        if (!constraints)
            constraints = new Constraints();
        constraints.add(cname, cc);

        # return the new constraint
        return cc;
    }

    private AbstractCheckConstraint addCheckConstraintUnlockedIntern(string cname, string src, *hash<auto> opt, *reference<string> sql) {
        # load constraints if needed, verify unique constraint name, validate/process options
        checkUniqueConstraintNameValidateOptions("CHECK-CONSTRAINT-ERROR", cname, getConstraintOptions(), \opt);

        # get the new constraint object
        AbstractCheckConstraint cc = addCheckConstraintImpl(cname, src, opt);

        # get the sql creation string
        sql = cc.getCreateSql(getSqlName());

        # return the new constraint
        return cc;
    }

    #! renames an existing constraint; this can be any constraint on the table, a primary key, a foreign key constraint, or a generic constraint; if the table is already known to be in the database, then the changes are effected in the database also immediately; otherwise it is only updated internally
    /** @par Example:
        @code{.py}
AbstractConstraint con = table.renameConstraint("pk_table_name", "pkey_table_name");
        @endcode

        @param old_name the current name of the constraint
        @param new_name the new name for the constraint
        @param lsql an optional reference to a list of strings to retrieve the SQL used to modify the constraint (only executed if the table is already in the database)
        @return the constraint object that was updated

        @throw CONSTRAINT-ERROR if the old constraint does not exist in the table or if the new constraint name already exists
    */
    AbstractConstraint renameConstraint(string old_name, string new_name, reference lsql) {
        l.lock();
        on_exit l.unlock();
        getAllConstraintsUnlocked();

        AbstractConstraint c;

        code rename;

        if (constraints && constraints.hasKey(old_name)) {
            c = constraints{old_name};
            rename = sub () { constraints.renameKey(old_name, new_name); };
        } else {
            if (inDb) {
                getPrimaryKeyUnlocked();
                if (!primaryKey.empty() && primaryKey.getName() == old_name) {
                    c = primaryKey;
                    # no need to update any container if the primary key is being updated
                    rename = sub () { };
                } else {
                    getForeignConstraintsUnlocked();
                    if (foreignConstraints && foreignConstraints.hasKey(old_name)) {
                        c = foreignConstraints{old_name};
                        rename = sub () { foreignConstraints.take(old_name); };
                    }
                }
            }
        }

        if (!c) {
            list cl = ();
            # get a list of all constraint names
            cl += constraints.keys();
            if (!primaryKey.empty())
                cl += primaryKey.getName();
            if (foreignConstraints)
                cl += foreignConstraints.keys();
            throw "CONSTRAINT-ERROR", sprintf("%s: has no constraint %y; valid constraints: %y", getDesc(), old_name, cl);
        }

        # rename foreign constraint in table if the table already exists in the DB
        execSql(lsql = c.getRenameSql(getSqlName(), new_name));

        c.rename(new_name);

        # rename constraint in container structure
        rename();

        # return the updated constraint
        return c;
    }

    #! gets the SQL that can be used to drop a constraint from the table; this can be any constraint on the table, a primary key, a foreign key constraint, or a generic constraint
    /** @par Example:
        @code{.py}
string sql = table.getDropConstraintSql("uk_mytable_name");
        @endcode

        @param cname the name of the constraint to drop
        @param opt a hash of options for the SQL string; see @ref SqlUtil::AbstractTable::AlignTableOptions for common options; each driver can support additional driver-specific options

        @return the SQL that can be used to drop a constraint from the table; this can be any constraint on the table, a primary key, a foreign key constraint, or a generic constraint

        @throw OPTION-ERROR invalid or unsupported option passed
        @throw CONSTRAINT-ERROR the given constraint does not exist in the table

        @note
        - this method retrieves current constraint definitions from the database if not already loaded or defined
        - if the @ref sql_callback_executed "sql_callback_executed option" is @ref True "True" in \a opt, then the changes are also effected in the current object, if not, then they are not (see @ref sql_callback_executed for more information)

        @see inDb() for a method that tells if the table is already in the database or not
    */
    string getDropConstraintSql(string cname, *hash<auto> opt) {
        validateOptionsIntern("OPTION-ERROR", getAlignTableOptions(), \opt);

        l.lock();
        on_exit l.unlock();

        code rmv;
        AbstractConstraint c = findDropConstraintUnlocked(cname, \rmv);

        on_success if (opt.sql_callback_executed)
            rmv();

        return AbstractDatabase::doCallback(opt, c.getDropSql(getSqlName()), AbstractDatabase::AC_Drop, "constraint", c.getName(), name);
    }

    #! gets the SQL that can be used to drop a constraint from the table if it exists, otherwise returns @ref nothing; this can be any constraint on the table, a primary key, a foreign key constraint, or a generic constraint
    /** @par Example:
        @code{.py}
*string sql = table.getDropConstraintIfExistsSql("uk_mytable_name");
        @endcode

        @param cname the name of the constraint to drop
        @param opt a hash of options for the SQL string; see @ref SqlUtil::AbstractTable::AlignTableOptions for common options; each driver can support additional driver-specific options
        @param cref an optional reference to an AbstractConstraint object that will return the constraint dropped

        @return the SQL that can be used to drop a constraint from the table if it exists, otherwise returns @ref nothing; this can be any constraint on the table, a primary key, a foreign key constraint, or a generic constraint

        @throw OPTION-ERROR invalid or unsupported option passed

        @note
        - this method retrieves current constraint definitions from the database if not already loaded or defined
        - if the @ref sql_callback_executed "sql_callback_executed option" is @ref True "True" in \a opt, then the changes are also effected in the current object, if not, then they are not (see @ref sql_callback_executed for more information)

        @see inDb() for a method that tells if the table is already in the database or not
    */
    *string getDropConstraintIfExistsSql(string cname, *hash<auto> opt, *reference<AbstractConstraint> cref) {
        validateOptionsIntern("OPTION-ERROR", getAlignTableOptions(), \opt);

        l.lock();
        on_exit l.unlock();

        code rmv;
        AbstractConstraint c;
        try {
            c = findDropConstraintUnlocked(cname, \rmv);
            cref = c;
        }
        catch (hash<ExceptionInfo> ex) {
            if (ex.err == "CONSTRAINT-ERROR")
                return;
            rethrow;
        }

        on_success if (opt.sql_callback_executed)
            rmv();

        return AbstractDatabase::doCallback(opt, c.getDropSql(getSqlName()), AbstractDatabase::AC_Drop, "constraint", c.getName(), name);
    }

    private AbstractConstraint findDropConstraintUnlocked(string cname, reference<code> rmv) {
        getAllConstraintsUnlocked();

        AbstractConstraint c;

        if (constraints && constraints.hasKey(cname)) {
            c = constraints{cname};
            rmv = sub () {
                constraints.take(cname);
                # if it's a unique constraint, remove any index with the same name
                if (c instanceof AbstractUniqueConstraint && constraintsLinkedToIndexesImpl())
                    indexes.take(cname);
            };
        } else if (!primaryKey.empty() && primaryKey.getName() == cname) {
            c = primaryKey;
            rmv = sub () {
                remove self.primaryKey;
                # remove any index with the same name
                if (constraintsLinkedToIndexesImpl())
                    indexes.take(cname);
            };
        } else if (foreignConstraints && foreignConstraints.hasKey(cname)) {
            c = foreignConstraints{cname};
            rmv = sub () {
                foreignConstraints.take(cname);
            };
        }

        if (!c) {
            list cl = ();
            # get a list of all constraint names
            cl += constraints.keys();
            if (!primaryKey.empty())
                cl += primaryKey.getName();
            if (foreignConstraints)
                cl += foreignConstraints.keys();
            throw "CONSTRAINT-ERROR", sprintf("%s: has no constraint %y; valid constraints: %y", getDesc(), cname, cl);
        }

        return c;
    }

    #! drops a constraint from the table; this can be any constraint on the table, a primary key, a foreign key constraint, or a generic constraint; if the table is known to be in the database already, then it is also dropped from the database immediately; otherwise it is only removed internally
    /** @par Example:
        @code{.py}
table.dropConstraint("uk_mytable_name");
        @endcode

        In case the table is already in the database, this method commits the transaction on success and rolls back the transaction if there's an error.

        @param cname the name of the constraint to drop
        @param sql an optional reference to a string to retrieve the SQL used to modify the table (only executed if the table is already in the database)

        @return the constraint dropped from the table

        @throw CONSTRAINT-ERROR the given constraint does not exist in the table

        @note this method retrieves current constraint definitions from the database if not already loaded or defined

        @see inDb() for a method that tells if the table is already in the database or not
    */
    AbstractConstraint dropConstraint(string cname, *reference<string> sql) {
        l.lock();
        on_exit l.unlock();

        code rmv;
        AbstractConstraint c = findDropConstraintUnlocked(cname, \rmv);

        # drop foreign constraint from table if the table already exists in the DB
        execSql(sql = c.getDropSql(getSqlName()));

        # remove from foreign constraint structure and return removed foreign constraint
        rmv();
        return c;
    }

    #! adds a trigger to the table; if the table is already known to be in the database, then it is added in the database also immediately; otherwise it is only added internally and can be created when create() is called for example
    /** @par Example:
        @code{.py}
string sql;
AbstractTrigger trig = table.addTrigger("trig_mytable", "", NOTHING, \sql);
printf("%s;\n", sql);
        @endcode

        In case the table is already in the database, this method commits the transaction on success and rolls back the transaction if there's an error.

        @param tname the name of the new trigger
        @param src the source of the trigger
        @param opt a hash of options for the new trigger; each driver may implement its own options; for common options, see @ref SqlUtil::AbstractTable::TriggerOptions
        @param lsql an optional reference to a list of strings to retrieve the SQL used to modify the table (only executed if the table is already in the database)

        @return an AbstractTrigger object corresponding to the trigger created

        @throw TRIGGER-ERROR the table already has a trigger with the given name or invalid options were passed

        @see inDb() for a method that tells if the table is already in the database or not
    */
    AbstractTrigger addTrigger(string tname, string src, *hash<auto> opt, *reference lsql) {
        l.lock();
        on_exit l.unlock();

        return addTriggerUnlocked(tname, src, opt, \lsql);
    }

    #! returns a list of SQL strings that can be used to add a trigger to the table
    /** @par Example:
        @code{.py}
string sql = table.getAddTriggerSql("trig_mytable", trigger_src);
printf("%s;\n", sql);
        @endcode

        In case the table is already in the database, this method commits the transaction on success and rolls back the transaction if there's an error.

        @param tname the name of the new trigger
        @param src the source of the trigger
        @param topt a hash of options for the new trigger; each driver may implement its own options; for common options, see @ref SqlUtil::AbstractTable::TriggerOptions
        @param opt a hash of options for the SQL string; see @ref SqlUtil::AbstractTable::AlignTableOptions for common options; each driver can support additional driver-specific options

        @return a list of SQL strings that can be used to add a trigger to the table

        @throw OPTION-ERROR invalid or unsupported option passed
        @throw TRIGGER-ERROR the table already has a trigger with the given name or invalid options were passed

        @see inDb() for a method that tells if the table is already in the database or not

        @note if the @ref sql_callback_executed "sql_callback_executed option" is @ref True "True" in \a opt, then the changes are also effected in the current object, if not, then they are not (see @ref sql_callback_executed for more information)
    */
    list<auto> getAddTriggerSql(string tname, string src, *hash topt, *hash<auto> opt) {
        validateOptionsIntern("OPTION-ERROR", getAlignTableOptions(), \opt);

        l.lock();
        on_exit l.unlock();

        list lsql;
        AbstractTrigger trig = addTriggerUnlockedIntern(tname, src, topt, \lsql);
        on_success if (opt.sql_callback_executed) {
            # store trigger in trigger container
            if (!triggers)
                triggers = new Triggers();

            triggers.add(tname, trig);
            manual = True;
        }
        return AbstractDatabase::doCallback(opt, lsql, AbstractDatabase::AC_Add, "trigger", tname, getSqlName());
    }

    private AbstractTrigger addTriggerUnlocked(string tname, string src, *hash<auto> opt, *reference lsql) {
        AbstractTrigger trig = addTriggerUnlockedIntern(tname, src, opt, \lsql);
        execSql(lsql);

        # store trigger in trigger container
        if (!triggers)
            triggers = new Triggers();

        triggers.add(tname, trig);

        # return the new trigger
        return trig;
    }

    private AbstractTrigger addTriggerUnlockedIntern(string tname, string src, *hash<auto> opt, *reference lsql) {
        # load triggers if needed
        if (!manual)
            getTriggersUnlocked();

        # verify unique trigger name
        if (triggers && triggers.hasKey(tname))
            throw "TRIGGER-ERROR", sprintf("%y: already has a trigger named %y, drop the trigger before adding it", getDesc(), tname);

        # validate/process options
        validateOptionsIntern("TRIGGER-ERROR", getTriggerOptions(), \opt);

        # get the new constraint object
        AbstractTrigger trig = addTriggerImpl(tname, src, opt);

        lsql = trig.getCreateSql(getSqlName());

        # return the new trigger
        return trig;
    }

    #! drops the given trigger from the table; if the table is known to be in the database already, then it is also dropped from the database immediately; otherwise it is only removed internally
    /** @par Example:
        @code{.py}
table.dropTrigger("trig_mytable");
        @endcode

        In case the table is already in the database, this method commits the transaction on success and rolls back the transaction if there's an error.

        @param tname the name of the trigger to drop
        @param sql an optional reference to a string to retrieve the SQL used to modify the database (only executed if the table is already in the database)

        @return the trigger dropped from the table

        @throw TRIGGER-ERROR the given trigger does not exist

        @note this method retrieves all current trigger definitions from the database if none have already been defined

        @see inDb() for a method that tells if the table is already in the database or not
    */
    AbstractTrigger dropTrigger(string tname, *reference<string> sql) {
        l.lock();
        on_exit l.unlock();
        getTriggersUnlocked();
        if (!triggers.hasKey(tname))
            throw "TRIGGER-ERROR", sprintf("%s: has no trigger %y; valid triggers: %y", getDesc(), tname, triggers.keys());

        # drop trigger from table if the table already exists in the DB
        execSql(sql = triggers{tname}.getDropSql(getSqlName()));

        # remove from trigger structure and return removed trigger
        return triggers.take(tname);
    }

    #! returns SQL that can be used to drop the given trigger from the table
    /** @par Example:
        @code{.py}
string sql = table.getDropTriggerSql("trig_mytable");
        @endcode

        @param tname the name of the trigger to drop
        @param opt a hash of options for the SQL string; see @ref SqlUtil::AbstractTable::AlignTableOptions for common options; each driver can support additional driver-specific options

        @return the SQL that can be used to drop the given trigger from the table

        @throw OPTION-ERROR invalid or unsupported option passed
        @throw TRIGGER-ERROR the given trigger does not exist

        @note
        - this method retrieves all current trigger definitions from the database if none have already been defined
        - if the @ref sql_callback_executed "sql_callback_executed option" is @ref True "True" in \a opt, then the changes are also effected in the current object, if not, then they are not (see @ref sql_callback_executed for more information)

        @see inDb() for a method that tells if the table is already in the database or not
    */
    list<auto> getDropTriggerSql(string tname, *hash<auto> opt) {
        validateOptionsIntern("OPTION-ERROR", getAlignTableOptions(), \opt);

        l.lock();
        on_exit l.unlock();

        getTriggersUnlocked();
        if (!triggers.hasKey(tname))
            throw "TRIGGER-ERROR", sprintf("%s: has no trigger %y; valid triggers: %y", getDesc(), tname, triggers.keys());

        # remove from trigger structure if sql is executed
        on_success if (opt.sql_callback_executed)
            triggers.take(tname);

        # drop trigger from table if the table already exists in the DB
        return AbstractDatabase::doCallback(opt, triggers{tname}.getDropSql(getSqlName()), AbstractDatabase::AC_Drop, "trigger", tname, getSqlName());
    }

    private getAllConstraintsUnlocked(*hash<auto> opt) {
        if (manual)
            return;

        getPrimaryKeyUnlocked();
        getForeignConstraintsUnlocked(opt);
        getConstraintsUnlocked();
    }

    private checkUniqueConstraintName(string err, string cname) {
        getAllConstraintsUnlocked();
        if (foreignConstraints.hasKey(cname))
            throw err, printf("%y: already has a foreign constraint named %y with columns: %y", name, cname, foreignConstraints{cname}.columns.keys());

        if (constraints.hasKey(cname))
            throw err, printf("%y: already has a constraint named %y", name, cname);

        if (!primaryKey.empty() && primaryKey.getName() == cname)
            throw err, printf("%y: already has a primary key constraint named %y with columns: %y", name, cname, primaryKey.keys());
    }

    private checkUniqueConstraintNameValidateOptions(string err, string cname, hash<auto> ropt, reference<hash> opt) {
        checkUniqueConstraintName(err, cname);
        # validate options
        validateOptionsIntern(err, ropt, \opt);
    }

    #! validates column options
    private validateColumnOptions(string cname, reference<hash> opt, bool nullable) {
        hash copt = getColumnDescOptions();

        # check valid options and option value types
        validateOptionsIntern("COLUMN-ERROR", copt, \opt, sprintf("%s.%s", name, cname));

        hash otm = getTypeMapImpl();
        hash th;

        # first try to handle non-registered type (like eg. Oracle's objects)
        if (opt.native_type && !otm.hasKey(opt.native_type) && !opt.qore_type) {
            opt.qore_type = "any";
        } else {
            # issue #4400: handle "or nothing" types
            if (opt.qore_type && opt.qore_type[0] == "*") {
                if (!exists opt.notnull) {
                    opt.notnull = False;
                }
                splice opt.qore_type, 0, 1;
            }
            # get native_type from registered maps
            if (!opt.native_type) {
                hash<auto> qtm = getQoreTypeMapImpl();
                opt.native_type = qtm{opt.qore_type};
                if (!opt.native_type)
                    throw "COLUMN-ERROR", sprintf("%s column %y: qore type %y cannot be mapped to a column type "
                        "(valid values: %y)", getDesc(), cname, opt.qore_type, qtm.keys());
            }

            # get native type description
            th = otm{opt.native_type};

            # reset qore type from native type
            opt.qore_type = th.qore;
            opt.local_def = th;
        }

        if (exists opt.size) {
            if (!th.size)
                throw "COLUMN-ERROR", sprintf("%s column %y: size %y given but type %y does not take a size "
                    "attribute", getDesc(), cname, opt.size, opt.native_type);
            if (th.size_range && (opt.size < th.size_range[0] || opt.size > th.size_range[1]))
                throw "COLUMN-ERROR", sprintf("%s column %y: size %y is not valid for type %y; must be between %d "
                    "and %d inclusive", getDesc(), cname, opt.size, opt.native_type, th.size_range[0],
                    th.size_range[1]);
        } else {
            if (th.default_size) {
                opt.size = th.default_size;
            } else {
                if (th.size == SZ_MAND) {
                    throw "COLUMN-ERROR", sprintf("%s column %y: type %y requires a size argument but none was supplied "
                        "(options: %y)", getDesc(), cname, opt.native_type, opt);
                }
                opt.size = 0;
            }
        }

        if (th.size == SZ_NUM) {
            opt.precision = opt.size;
            opt.size = 0;
            if (exists opt.scale) {
                if (th.scale_range && (opt.scale < th.scale_range[0] || opt.scale > th.scale_range[1]))
                    throw "COLUMN-ERROR", sprintf("%s column %y: scale %y is not valid for type %y; must be between "
                        "%d and %d inclusive", getDesc(), cname, opt.scale, opt.native_type, th.scale_range[0],
                        th.scale_range[1]);
            } else {
                opt.scale = th.default_scale ?? 0;
            }
        } else if (opt.scale) {
            throw "COLUMN-ERROR", sprintf("%s column %y: scale %y is not valid for type %y; it is not a numeric type",
                getDesc(), cname, opt.scale, opt.native_type);
        } else {
            opt.precision = opt.scale = 0;
        }

        # check type of default value (must match column type)
        if (exists opt.default_value && !opt.default_value_native) {
            checkValue(cname, "default_value", \opt.default_value, opt.qore_type);
            opt.default_value = getSqlValue(opt.default_value);
        }
    }

    #! drops a column from the table
    /** @par Example:
        @code{.py}
table.dropColumn("notes_2");
        @endcode

        In case the table is already in the database, this method commits the transaction on success and rolls back the transaction if there's an error.

        @param cname the name of the column to drop
        @param lsql an optional reference to a list of strings to retrieve the SQL used to modify the table (only executed if the table is already in the database)

        @return the AbstractColumn object representing the column dropped from the table

        @throw COLUMN-ERROR the named column is not present in the table

        @note this method retrieves the table definition from the database if none has already been defined

        @see inDb() for a method that tells if the table is already in the database or not
    */
    AbstractColumn dropColumn(string cname, *reference lsql) {
        l.lock();
        on_exit l.unlock();
        getColumnsUnlocked();
        if (!columns.hasKey(cname))
            throw "COLUMN-ERROR", sprintf("%s column %y: no such column (valid columns: %y)", getDesc(), cname,
                columns.keys());

        code sc;
        if (inDb) {
            sc = sub (string str) {
                ds.execRaw(str);
            };
        }

        # if there are any indexes or constraints on this column, remove them from the table first
        lsql = getDropAllConstraintsAndIndexesOnColumnSql(cname, ("sql_callback": sc, "sql_callback_executed": True));

        # drop column from table if the table already exists in the DB
        execSql(lsql += columns{cname}.getDropSql(getSqlName()));
        return columns.take(cname);
    }

    #! returns the SQL that can be used to drop a column from the table
    /** @par Example:
        @code{.py}
string sql = table.getDropColumnSql("notes_2");
        @endcode

        @param cname the name of the column to drop
        @param opt a hash of options for the SQL string; see @ref SqlUtil::AbstractTable::AlignTableOptions for common options; each driver can support additional driver-specific options

        @return the SQL that can be used to drop a column from the table

        @throw OPTION-ERROR invalid or unsupported option passed
        @throw COLUMN-ERROR the named column is not present in the table

        @note
        - this method retrieves the column definitions from the database if none have already been defined
        - if the @ref sql_callback_executed "sql_callback_executed option" is @ref True "True" in \a opt, then the changes are also effected in the current object, if not, then they are not (see @ref sql_callback_executed for more information)

        @see inDb() for a method that tells if the table is already in the database or not
    */
    list<auto> getDropColumnSql(string cname, *hash<auto> opt) {
        validateOptionsIntern("OPTION-ERROR", getAlignTableOptions(), \opt);

        l.lock();
        on_exit l.unlock();
        getColumnsUnlocked();
        if (!columns.hasKey(cname))
            throw "COLUMN-ERROR", sprintf("%s column %y: no such column (valid columns: %y)", getDesc(), cname, columns.keys());

        on_success if (opt.sql_callback_executed)
            columns.take(cname);

        list lst = ();
        lst += getDropAllConstraintsAndIndexesOnColumnSqlUnlocked(cname, opt);
        lst += AbstractDatabase::doCallback(opt, columns{cname}.getDropSql(getSqlName()), AbstractDatabase::AC_Drop, "column", cname, name);
        return lst;
    }

    #! inserts a row into the table; the transaction is committed if successful, if an error occurs, it is rolled back
    /** @par Example:
        @code{.py}
table.insertCommit(row);
        @endcode

        @param row a hash representing the row to insert; hash values can also be set with @ref sql_iop_funcs to insert values based on SQL operations to be used directly in the insert statement

        @return in case the \c "returning" \c AbstractTable::InsertOptions "insert option" is used, a hash of return values is returned, otherwise @ref nothing is returned

        @throw COLUMN-ERROR an unknown column was referenced in the hash to be inserted
    */
    *hash<auto> insertCommit(hash<auto> row) {
        on_success ds.commit();
        on_error ds.rollback();

        return insertIntern(row);
    }

    #! @ref SqlUtil::AbstractTable::insertCommit() variant
    /**
        @param row a hash representing the row to insert; hash values can also be set with @ref sql_iop_funcs to insert values based on SQL operations to be used directly in the insert statement
        @param sql an optional reference to a string to return the SQL generated for the insert statement
    */
    *hash<auto> insertCommit(hash<auto> row, reference<string> sql) {
        on_success ds.commit();
        on_error ds.rollback();

        return insertIntern(row, \sql);
    }

    #! @ref SqlUtil::AbstractTable::insertCommit() variant
    /**
        @param row a hash representing the row to insert; hash values can also be set with @ref sql_iop_funcs to insert values based on SQL operations to be used directly in the insert statement
        @param opt optional insert options; see @ref AbstractTable::InsertOptions for more info
    */
    *hash<auto> insertCommit(hash<auto> row, hash<auto> opt) {
        on_success ds.commit();
        on_error ds.rollback();

        return insertIntern(row, NOTHING, opt);
    }

    #! @ref SqlUtil::AbstractTable::insertCommit() variant
    /**
        @param row a hash representing the row to insert; hash values can also be set with @ref sql_iop_funcs to insert values based on SQL operations to be used directly in the insert statement
        @param sql an optional reference to a string to return the SQL generated for the insert statement
        @param opt optional insert options; see @ref AbstractTable::InsertOptions for more info
    */
    *hash<auto> insertCommit(hash<auto> row, reference<string> sql, hash<auto> opt) {
        on_success ds.commit();
        on_error ds.rollback();

        return insertIntern(row, \sql, opt);
    }

    #! inserts a row into the table without any transaction management; a transaction will be in progress after this method is successfully executed
    /** @ingroup inserts
        @par Example:
        @code{.py}
table.insert(row);
        @endcode

        @param row a hash representing the row to insert; hash values can also be set with @ref sql_iop_funcs to insert values based on SQL operations to be used directly in the insert statement

        @return in case the \c "returning" \c AbstractTable::InsertOptions "insert option" is used, a hash of return values is returned, otherwise @ref nothing is returned

        @throw COLUMN-ERROR an unknown column was referenced in the hash to be inserted
    */
    *hash<auto> insert(hash<auto> row) {
        return insertIntern(row);
    }

    #! @ref SqlUtil::AbstractTable::insert() variant
    /**
        @param row a hash representing the row to insert; hash values can also be set with @ref sql_iop_funcs to insert values based on SQL operations to be used directly in the insert statement
        @param sql an optional reference to a string to return the SQL generated for the insert statement
    */
    *hash<auto> insert(hash<auto> row, reference<string> sql) {
        return insertIntern(row, \sql);
    }

    #! @ref SqlUtil::AbstractTable::insert() variant
    /**
        @param row a hash representing the row to insert; hash values can also be set with @ref sql_iop_funcs to insert values based on SQL operations to be used directly in the insert statement
        @param opt optional insert options; see @ref AbstractTable::InsertOptions for more info
    */
    *hash<auto> insert(hash<auto> row, hash<auto> opt) {
        return insertIntern(row, NOTHING, opt);
    }

    #! @ref SqlUtil::AbstractTable::insert() variant
    /**
        @param row a hash representing the row to insert; hash values can also be set with @ref sql_iop_funcs to insert values based on SQL operations to be used directly in the insert statement
        @param sql an optional reference to a string to return the SQL generated for the insert statement
        @param opt optional insert options; see @ref AbstractTable::InsertOptions for more info
    */
    *hash<auto> insert(hash<auto> row, reference<string> sql, hash<auto> opt) {
        return insertIntern(row, \sql, opt);
    }

    #! Inserts a row and returns the result and also the SQL used
    /** @param row a hash representing the row to insert; hash values can also be set with @ref sql_iop_funcs to insert
        values based on SQL operations to be used directly in the insert statement
        @param opt optional insert options; see @ref AbstractTable::InsertOptions for more info

        @return a hash giving the result of the operation and the SQL used for it; the \c result key is assigned to
        the integer number of rows inserted

        @since SqlUtil 1.7
    */
    hash<SqlResultInfo> insertWithInfo(hash<auto> row, *hash<auto> opt) {
        hash<SqlResultInfo> rv();
        rv.result = insertIntern(row, \rv.sql, opt, \rv.args);
        return rv;
    }

    #! A legacy wrapper for @ref SqlUtil::AbstractTable::insert()
    deprecated *hash<auto> insertNoCommit(hash<auto> row, *reference<string> sql, *hash<auto> opt) {
        return insertIntern(row, \sql, opt);
    }

    #! A legacy wrapper for @ref SqlUtil::AbstractTable::insert()
    deprecated *hash<auto> insertNoCommit(hash<auto> row, hash<auto> opt) {
        return insertIntern(row, NOTHING, opt);
    }

    private *hash<auto> insertIntern(hash<auto> row, *reference<string> sql, *hash<auto> opt,
            *reference<softlist<auto>> args) {
        # check data callback options if any
        validateOptionsIntern("OPTION-ERROR", getInsertOptions(), \opt);

        # issue #3352: do not execute runtime DML while holding the lock
        if (!columns) {
            l.lock();
            on_exit l.unlock();

            getColumnsUnlocked();
        }
        sql = sprintf("insert into %s (", getSqlName());
        foreach string k in (keys row) {
            if (!columns.hasKey(k))
                throw "COLUMN-ERROR", sprintf("%s column %y is not a valid column (valid columns: %y)", getDesc(), k,
                    columns.keys());
        }
        hash vh = getPlaceholdersAndValues(row);

        sql += (foldl $1 + "," + $2, (map getColumnSqlName($1), keys row));
        sql += ") values (";
        sql += (foldl $1 + "," + $2, vh.placeholders);
        sql += ")";

        args = vh.values;

        # check for a bulk insert in case the driver does not support array binding
        if (!hasArrayBind()) {
            int size = 0;
            foreach auto arg in (args) {
                if (arg.typeCode() == NT_LIST) {
                    size = arg.size();
                    break;
                }
            }
            if (size) {
                hash<auto> rv;
                for (int i = 0; i < size; ++i) {
                    # get arg list for row
                    list<auto> targs = map $1.typeCode() == NT_LIST ? $1[i] : $1, args;
                    if (opt.returning) {
                        if (!rv) {
                            hash<auto> h = doReturningImpl(opt, \sql, targs);
                            map rv{$1.key} += ($1.value,), h.pairIterator();
                        } else {
                            hash<auto> h = execData(opt, sql, targs);
                            map rv{$1.key} += $1.value, h.pairIterator();
                        }
                    } else {
                        execData(opt, sql, targs);
                    }
                }
                return rv;
            }
        }

        if (opt.returning)
            return doReturningImpl(opt, \sql, args);

        execData(opt, sql, args);
    }

    private hash<auto> getPlaceholdersAndValues(hash<auto> row) {
        hash<auto> im;
        # placeholder list
        list phl = ();
        # value list
        list vl = ();

        HashIterator i(row);
        while (i.next()) {
            string key = i.getKey();
            auto v = i.getValue();
            string ph = "%v";
            if (v.typeCode() == NT_LIST)
                v = v[0];
            if (v."_iop".typeCode() == NT_STRING) {
                string iop = v."_iop";
                if (!exists im)
                    im = getInsertOperatorMap();
                *hash cmd = im{iop};
                if (!cmd)
                    throw "INSERT-OPERATOR-ERROR", sprintf("%s: unknown insert operator %y: expecting one of: %y", getDesc(), iop, im.keys());
                v = v.arg;
                if (cmd.arg) {
                    string at = v.type();
                    switch (cmd.arg) {
                        default: {
                            if (at != cmd.arg)
                                throw "INSERT-OPERATOR-ERROR", sprintf("%s: invalid argument to insert operator %y; got type %y, expecting %y", getDesc(), iop, at, cmd.arg);
                        }
                    }
                }

                if (cmd.code)
                    v = cmd.code(key, v);
                if (cmd.immediate) {
                    # make sure only identifiers are included here since the string is inserted directly into the query
                    if (v =~ /[^a-z\._0-9]/i)
                        throw "INSERT-OPERATOR-ERROR", sprintf("%s: immediate value to insert operator %y is %y which contains illegal characters", getDesc(), iop, v);
                    phl += v;
                    continue;
                }
                if (cmd.placeholder)
                    ph = cmd.placeholder;
                phl += ph;
                push vl, v;
                continue;
            }
            phl += ph;
            push vl, i.getValue();
        }

        return (
            "placeholders": phl,
            "values": vl,
        );
    }

    #! returns @ref True "True" if the current database driver supports the \c "returning" clause in insert statements, @ref False "False" if not
    /** @return @ref True "True" if the current database driver supports the \c "returning" clause in insert statements, @ref False "False" if not
        @since %SqlUtil 1.3
    */
    bool hasReturning() {
        return hasReturningImpl();
    }

    #! inserts rows into a table based on a select statement from another table (which must be using the same datasource as the current table); the transaction is committed if successful, if an error occurs, it is rolled back
    /** @par Example:
        @code{.py}
int rows = table.insertFromSelectCommit(("id", "name", "created"), source_table, (("columns": ("id", "name", "created"), "where": ("type": "CUSTOMER"))));
        @endcode

        @param cols the list of column names to use to insert in the current table
        @param source the source table for the select statement
        @param sh a hash of conditions for the select statement; see @ref select_option_hash "select option hash" for information about this argument
        @param sql an optional reference to a string to return the SQL generated for the select statement
        @param opt optional SQL data operation callback options; see @ref AbstractTable::SqlDataCallbackOptions for more inf

        @return the number of rows inserted

        @throw OPTION-ERROR invalid or unsupported option
        @throw COLUMN-ERROR unknown or invalid column in insert list
        @throw SELECT-ERROR \c 'offset' supplied without \c 'orderby' or \c 'limit', \c 'orderby' with \c 'limit' and \c 'offset' does not match any unique constraint

        @note this method does not take insert options because it is executed entirely in the database server; use insertFromIterator() or insertFromIteratorCommit() to insert arbitrary data with insert options
    */
    int insertFromSelectCommit(list cols, AbstractTable source, hash<auto> sh, reference<string> sql, hash<auto> opt) {
        on_success ds.commit();
        on_error ds.rollback();

        return insertFromSelectIntern(cols, source, sh, \sql);
    }

    #! @ref SqlUtil::AbstractTable::insertFromSelectCommit() variant
    int insertFromSelectCommit(list cols, AbstractTable source) {
        on_success ds.commit();
        on_error ds.rollback();

        return insertFromSelectIntern(cols, source);
    }

    #! @ref SqlUtil::AbstractTable::insertFromSelectCommit() variant
    int insertFromSelectCommit(list cols, AbstractTable source, hash<auto> sh) {
        on_success ds.commit();
        on_error ds.rollback();

        return insertFromSelectIntern(cols, source, sh);
    }

    #! @ref SqlUtil::AbstractTable::insertFromSelectCommit() variant
    int insertFromSelectCommit(list cols, AbstractTable source, hash<auto> sh, reference<string> sql) {
        on_success ds.commit();
        on_error ds.rollback();

        return insertFromSelectIntern(cols, source, sh, \sql);
    }

    #! @ref SqlUtil::AbstractTable::insertFromSelectCommit() variant
    int insertFromSelectCommit(list cols, AbstractTable source, hash<auto> sh, hash<auto> opt) {
        on_success ds.commit();
        on_error ds.rollback();

        return insertFromSelectIntern(cols, source, sh, NOTHING, opt);
    }

    #! inserts rows into a table based on a select statement from another table (which must be using the same datasource as the current table); a transaction will be in progress after this method is successfully executed
    /** @par Example:
        @code{.py}
int rows = table.insertFromSelect(("id", "name", "created"), source_table, (("columns": ("id", "name", "created"), "where": ("type": "CUSTOMER"))));
        @endcode

        @param cols the list of column names to use to insert in the current table
        @param source the source table for the select statement
        @param sh a hash of conditions for the select statement; see @ref select_option_hash "select option hash" for information about this argument
        @param sql an optional reference to a string to return the SQL generated for the select statement
        @param opt optional SQL data operation callback options; see @ref AbstractTable::SqlDataCallbackOptions for more inf

        @return the number of rows inserted

        @throw OPTION-ERROR invalid or unsupported option
        @throw COLUMN-ERROR unknown or invalid column in insert list
        @throw SELECT-ERROR \c 'offset' supplied without \c 'orderby' or \c 'limit', \c 'orderby' with \c 'limit' and \c 'offset' does not match any unique constraint

        @note this method does not take insert options because it is executed entirely in the database server; use insertFromIterator() or insertFromIteratorCommit() to insert arbitrary data with insert options
    */
    int insertFromSelect(list cols, AbstractTable source, hash<auto> sh, reference<string> sql, hash<auto> opt) {
        return insertFromSelectIntern(cols, source, sh, \sql, opt);
    }

    #! @ref SqlUtil::AbstractTable::insertFromSelectCommit() variant
    int insertFromSelect(list cols, AbstractTable source) {
        return insertFromSelectIntern(cols, source);
    }

    #! @ref SqlUtil::AbstractTable::insertFromSelectCommit() variant
    int insertFromSelect(list cols, AbstractTable source, hash<auto> sh) {
        return insertFromSelectIntern(cols, source, sh);
    }

    #! @ref SqlUtil::AbstractTable::insertFromSelectCommit() variant
    int insertFromSelect(list cols, AbstractTable source, hash<auto> sh, reference<string> sql) {
        return insertFromSelectIntern(cols, source, sh, \sql);
    }

    #! @ref SqlUtil::AbstractTable::insertFromSelectCommit() variant
    int insertFromSelect(list cols, AbstractTable source, hash<auto> sh, hash<auto> opt) {
        return insertFromSelectIntern(cols, source, sh, NOTHING, opt);
    }

    #! inserts rows into a table based on a select statement from another table (which must be using the same datasource as the current table)
    /**
        @param cols the list of column names to use to insert in the current table
        @param source the source table for the select statement
        @param select_hash a hash of conditions for the select statement; see
        @ref select_option_hash "select option hash" for information about this argument
        @param opt optional SQL data operation callback options; see @ref AbstractTable::SqlDataCallbackOptions for
        more info

        @return a hash giving the result of the operation and the SQL used for it; the \c result key will hold the
        integer number of rows inserted

        @since SqlUtil 1.7
    */
    hash<SqlResultInfo> insertFromSelectWithInfo(list<auto> cols, AbstractTable source, hash<auto> select_hash,
            *hash<auto> opt) {
        hash<SqlResultInfo> rv();
        rv.result = insertFromSelectIntern(cols, source, select_hash, \rv.sql, opt, \rv.args);
        return rv;
    }

    #! A legacy @ref SqlUtil::AbstractTable::insertFromSelect() wrapper
    deprecated int insertFromSelectNoCommit(list cols, AbstractTable source, *hash<auto> sh, *reference<string> sql,
            *hash<auto> opt) {
        return insertFromSelectIntern(cols, source, sh, \sql, opt);
    }

    private int insertFromSelectIntern(list cols, AbstractTable source, *hash<auto> sh, *reference<string> sql,
            *hash<auto> opt, *reference<softlist<auto>> args) {
        string ssql = source.getSelectSql(sh, \args);

        # issue #3352: do not execute runtime DML while holding the lock
        if (!columns) {
            l.lock();
            on_exit l.unlock();

            getColumnsUnlocked();
        }

        sql = sprintf("insert into %s (", getSqlName());

        foreach string k in (cols) {
            if (!columns.hasKey(k))
                throw "COLUMN-ERROR", sprintf("%s column %y is not a valid column (valid columns: %y)", getDesc(), k, cols);
        }

        sql += (foldl $1 + "," + $2, getColumnSqlNames(cols));
        sql += ") " + ssql;

        return execData(opt, sql, args);
    }

    #! this method inserts data from the given iterator argument (whose @ref Qore::AbstractIterator::getValue() "getValue()" method must return a hash giving row values) into the current table; the transaction is committed if successful, if an error occurs then it is rolled back
    /** @par Example:
        @code{.py}
int rows = table.insertFromIteratorCommit(i);
        @endcode

        @param i the @ref Qore::AbstractIterator argument that will provide the data to insert into the current table; the @ref Qore::AbstractIterator::getValue() "getValue()" method of this object must return a hash giving row values (for example, the @ref Qore::SQL::AbstractSQLStatement class is well-suited to be used here); note that the iterator is assumed to return the same columns in every element
        @param opt a hash of options for the insert operation; see @ref SqlUtil::AbstractTable::InsertFromIteratorOptions for common options; each driver can support additional driver-specific options

        @return the number of rows inserted

        @throw OPTION-ERROR invalid or unsupported option
        @throw COLUMN-ERROR an unknown column was referenced in the hash to be inserted

        @see
        - insertFromIterator()
        - upsertFromIterator()
        - upsertFromIteratorCommit()
    */
    int insertFromIteratorCommit(Qore::AbstractIterator i, *hash<auto> opt) {
        on_success ds.commit();
        on_error ds.rollback();

        return insertFromIteratorIntern(i, opt);
    }

    #! this method inserts data from the given iterator argument (whose @ref Qore::AbstractIterator::getValue() "getValue()" method must return a hash giving row values) into the current table; no transaction management is performed with this method
    /** @par Example:
        @code{.py}
int rows = table.insertFromIterator(i);
        @endcode

        @param i the @ref Qore::AbstractIterator argument that will provide the data to insert into the current table; the @ref Qore::AbstractIterator::getValue() "getValue()" method of this object must return a hash giving row values (for example, the @ref Qore::SQL::AbstractSQLStatement class is well-suited to be used here); note that the iterator is assumed to return the same columns in the same order in every element
        @param opt a hash of options for the insert operation; see @ref SqlUtil::AbstractTable::InsertFromIteratorOptions for common options; each driver can support additional driver-specific options; note that this method ignores any \c "commit_block" option

        @return the number of rows inserted

        @throw OPTION-ERROR invalid or unsupported option
        @throw COLUMN-ERROR an unknown column was referenced in the hash to be inserted

        @see
        - insertFromIterator()
        - upsertFromIterator()
        - upsertFromIteratorCommit()
    */
    int insertFromIterator(Qore::AbstractIterator i, *hash<auto> opt) {
        return insertFromIteratorIntern(i, opt - "commit_block");
    }

    #! A legacy @ref SqlUtil::AbstractTable::insertFromIterator() wrapper
    deprecated int insertFromIteratorNoCommit(Qore::AbstractIterator i, *hash<auto> opt) {
        return insertFromIteratorIntern(i, opt - "commit_block");
    }

    private int insertFromIteratorIntern(Qore::AbstractIterator i, *hash<auto> opt) {
        # check upsert options
        validateOptionsIntern("OPTION-ERROR", getInsertFromIteratorOptions(), \opt);

        if (!i.next())
            return 0;

        # get initial row
        hash<auto> row = i.getValue();

        # make insert statement template
        string sql = sprintf("insert into %s (%s) values (%s)", getSqlName(), (foldl $1 + "," + $2, (map getColumnSqlName($1), keys row)),
                                (foldl $1 + "," + $2, (map "%v", keys row)));
        AbstractSQLStatement stmt = ds.getSQLStatement();
        stmt.prepare(sql);

        opt.change_count = 0;
        int rc = 0;
        do {
            execData(stmt, opt, row.values());
            if (opt.info_callback)
                opt.info_callback(getSqlName(), row, UR_Inserted);
            ++rc;

            if (opt.commit_block) {
                if (++opt.change_count >= opt.commit_block) {
                    ds.commit();
                    opt.change_count = 0;
                }
            }
        } while (i.next() && (row = i.getValue()));

        return rc;
    }

    #! update or insert the data in the table according to the hash argument; the table must have a unique key to do this; the transaction is committed if successful, if an error occurs then it is rolled back
    /** @par Example:
        @code{.py}
table.upsertCommit(row);
        @endcode

        @param row a hash representing the row to insert or update
        @param upsert_strategy see @ref upsert_options for possible values for the upsert strategy
        @param opt a hash of options for the upsert operation; see @ref SqlUtil::AbstractTable::UpsertOptions for common options; each driver can support additional driver-specific options

        @return an integer code giving the result of the update; see @ref upsert_results for more information

        @throw COLUMN-ERROR an unknown column was referenced in the hash to be inserted
        @throw UPSERT-ERROR no primary key, unique constraint, or unique index for upsert; not all columns of the unique constraint/index are used in the upsert statement

        @note if upserting multiple rows; it's better to use getBulkUpsertClosure(), getUpsertClosure(), or getUpsertClosureWithValidation() and execute the closure on each row; when using this method, the overhead for setting up the upsert is made for each row which is very inefficient
    */
    int upsertCommit(hash<auto> row, int upsert_strategy = UpsertAuto, *hash<auto> opt) {
        on_success ds.commit();
        on_error ds.rollback();

        return upsert(row, upsert_strategy, opt);
    }

    #! update or insert the data in the table according to the hash argument; the table must have a unique key to do this
    /** @par Example:
        @code{.py}
table.upsert(row);
        @endcode

        @param row a hash representing the row to insert or update
        @param upsert_strategy see @ref upsert_options for possible values for the upsert strategy
        @param opt a hash of options for the upsert operation; see @ref SqlUtil::AbstractTable::UpsertOptions for common options; each driver can support additional driver-specific options

        @return an integer code giving the result of the update; see @ref upsert_results for more information

        @throw COLUMN-ERROR an unknown column was referenced in the hash to be inserted
        @throw UPSERT-ERROR no primary key, unique constraint, or unique index for upsert; not all columns of the unique constraint/index are used in the upsert statement

        @note if upserting multiple rows; it's better to use getBulkUpsertClosure(), getUpsertClosure(), or getUpsertClosureWithValidation() and execute the closure on each row; when using this method, the overhead for setting up the upsert is made for each row which is very inefficient
    */
    int upsert(hash<auto> row, int upsert_strategy = UpsertAuto, *hash<auto> opt) {
        return getUpsertClosure(row, upsert_strategy, opt)(row);
    }

    #! A legacy @ref SqlUtil::AbstractTable::upsert() wrapper
    deprecated int upsertNoCommit(hash<auto> row, int upsert_strategy = UpsertAuto) {
        return getUpsertClosure(row, upsert_strategy)(row);
    }

    #! returns a closure that can be executed given a hash argument representing a single row that will be updated or inserted in the database with the given upsert strategy; the table must have a unique key to do this; the closure returned does not check the input hash for validity
    /** @par Example:
        @code{.py}
Datasource ds("pgsql:user/pass@db%localhost");
Table table(ds, "table_name");
code upsert = table.getUpsertClosure(row, AbstractTable::UpsertSelectFirst);
on_success ds.commit();
on_error ds.rollback();
map upsert($1), row_list;
        @endcode

        @param row a hash representing an example row to insert or update; every row passed to the upsert closure
        returned must have the same keys in the same order
        @param upsert_strategy see @ref upsert_options for possible values for the upsert strategy
        @param opt a hash of options for the upsert operation; see @ref SqlUtil::AbstractTable::UpsertOptions for
        common options; each driver can support additional driver-specific options

        @return a closure that can be executed given a hash argument representing a single row that will be updated or
        inserted in the database with the given upsert strategy; the closure returned does not check the input hash
        for validity; the closure has the following signature:
        @code{.py} int sub upsert(hash<auto> row) {} @endcode
        The return value of the closure is an integer code giving the result of the update; see @ref upsert_results
        for more information

        @throw COLUMN-ERROR an unknown column was referenced in the hash to be inserted
        @throw UPSERT-ERROR no primary key, unique constraint, or unique index for upsert; not all columns of the
        unique constraint/index are used in the upsert statement

        @note the row values passed to the closure for upserting are not checked if they match the example row passed
        to the getUpsertClosure() method; passing non-conforming data will cause errors; see
        @ref SqlUtil::AbstractTable::getUpsertClosureWithValidation() for a similar method that returns a validating
        closure; the closure returned by this method is faster than the one returned by
        @ref SqlUtil::AbstractTable::getUpsertClosure() since there is no validation
    */
    code<int(hash<auto>)> getUpsertClosure(hash<auto> row, int upsert_strategy = UpsertAuto, *hash<auto> opt) {
        # check upsert options
        validateOptionsIntern("OPTION-ERROR", getUpsertOptions(), \opt);

        if (upsert_strategy == UpsertAuto) {
            upsert_strategy = emptyDataIntern() ? UpsertInsertFirst : UpsertUpdateFirst;
        }

        # accommodate for bulk DML
        if (row.firstValue().typeCode() == NT_LIST) {
            row = map {$1.key : $1.value.typeCode() == NT_LIST ? $1.value[0] : $1.value}, row.pairIterator();
        }

        hash<auto> upsert_row;
        if (opt.upsert_columns) {
            hash<auto> uch = map {$1: True}, opt.upsert_columns;
            if (*hash<auto> invalid_cols = (uch - keys row)) {
                throw "UPSERT-ERROR", sprintf("%s: 'upsert_columns' contains the following columns not referenced in "
                    "the row data: %y", getDesc(), keys invalid_cols);
            }
            upsert_row = map {$1: NOTHING}, opt.upsert_columns;
        } else {
            upsert_row = row;
        }

        Columns cols = checkUpsertRow(upsert_row, \upsert_strategy);

        switch (upsert_strategy) {
            case UpsertInsertFirst:
                return getUpsertInsertFirst(cols, row, opt);

            case UpsertUpdateFirst:
                return getUpsertUpdateFirst(cols, row, opt);

            case UpsertSelectFirst:
                return getUpsertSelectFirst(cols, row, opt);

            case UpsertInsertOnly:
                return getUpsertInsertOnly(cols, row, opt);

            case UpsertUpdateOnly:
                return getUpsertUpdateOnly(cols, row, opt);
        }

        throw "UPSERT-ERROR", sprintf("%s: invalid upsert code: %d", getDesc(), upsert_strategy);
    }

    #! returns a closure that can be executed given a hash argument representing either a single row or a set of rows (where each key value is a list of column values) that will be updated or inserted in the database with the given upsert strategy; the table must have a unique key to do this; the closure returned does not check the input hash for validity
    /** @par Example:
        @code{.py}
Datasource ds("pgsql:user/pass@db%localhost");
Table table(ds, "table_name");
code upsert = table.getBulkUpsertClosure(row, AbstractTable::UpsertSelectFirst);
on_success ds.commit();
on_error ds.rollback();
upsert(row_hash);
        @endcode

        @param example_row a hash representing an example row to insert or update; every hash passed to the upsert closure returned must have the same keys in the same order
        @param upsert_strategy see @ref upsert_options for possible values for the upsert strategy
        @param opt a hash of options for the upsert operation; see @ref SqlUtil::AbstractTable::UpsertOptions for common options; each driver can support additional driver-specific options

        @return a closure that can be executed given a hash argument representing either a single row or a set of rows (where each key value is a list of column values) that will be updated or inserted in the database with the given upsert strategy; the closure returned does not check the input hash for validity; the closure has the following signature: @code{.py} int sub upsert(hash<auto> row) {} @endcode The return value of the closure is always \c SqlUtil::AbstractTable::UR_Verified; see \c upsert_results for more information

        @throw COLUMN-ERROR an unknown column was referenced in the hash to be inserted
        @throw UPSERT-ERROR no primary key, unique constraint, or unique index for upsert; not all columns of the unique constraint/index are used in the upsert statement

        @note the row values passed to the closure for upserting are not checked if they match the example row passed to the getUpsertClosure() method; passing non-conforming data will cause errors; see @ref SqlUtil::AbstractTable::getUpsertClosureWithValidation() for a similar method that returns a validating closure; the closure returned by this method is faster than the one returned by @ref SqlUtil::AbstractTable::getUpsertClosure() since there is no validation

        @see
        - upsert()
        - upsertCommit()
        - getUpsertClosure()
        - getUpsertClosureWithValidation()
    */
    code<int(hash<auto>)> getBulkUpsertClosure(hash<auto> example_row, int upsert_strategy = AbstractTable::UpsertAuto, *hash<auto> opt) {
        code<int(hash<auto>)> upsert = getUpsertClosure(example_row, upsert_strategy, opt);
        return int sub (hash<auto> h) {
            map upsert($1), h.contextIterator();
            return UR_Verified;
        };
    }

    #! returns a closure for performing bulk update operations
    /** The closure accepts two hash arguments: set data and condition data, each as a hash-of-lists
        (one list per column). Returns the number of rows affected.

        The default implementation iterates over rows using a prepared statement.
        Drivers that support more efficient bulk operations (e.g. PostgreSQL UNNEST) can override this.

        @param example_set an example single-row hash of SET column values
        @param example_cond an example single-row hash of WHERE condition values
        @return a closure that accepts (hash<auto> set_data, hash<auto> cond_data) and returns the affected row count

        @see getBulkUpsertClosure()
    */
    code<int(hash<auto>, hash<auto>)> getBulkUpdateClosure(hash<auto> example_set, hash<auto> example_cond) {
        # execute the first row to generate and prepare the SQL
        *AbstractSQLStatement stmt;
        bool first = True;
        return int sub (hash<auto> set_data, hash<auto> cond_data) {
            int uc = 0;
            if (set_data.firstValue().typeCode() == NT_LIST) {
                # hash-of-lists: iterate over rows
                foreach hash<auto> row in (set_data.contextIterator()) {
                    int row_idx = $#;
                    hash<auto> row_cond = (map {$1.key: $1.value[row_idx]}, cond_data.pairIterator());
                    if (first) {
                        string sql;
                        uc += self.update(row, row_cond, \sql);
                        stmt = self.getStatement();
                        stmt.prepare(sql);
                        first = False;
                    } else {
                        stmt.execArgs(row.values() + row_cond.values());
                        uc += stmt.affectedRows();
                    }
                }
            } else {
                # single row
                if (first) {
                    string sql;
                    uc += self.update(set_data, cond_data, \sql);
                    stmt = self.getStatement();
                    stmt.prepare(sql);
                    first = False;
                } else {
                    stmt.execArgs(set_data.values() + cond_data.values());
                    uc += stmt.affectedRows();
                }
            }
            return uc;
        };
    }

    #! returns a closure that can be executed given a hash argument representing a single row that will be updated or inserted in the database with the given upsert strategy; the table must have a unique key to do this; the closure returned checks the input hash for validity
    /** @par Example:
        @code{.py}
Datasource ds("pgsql:user/pass@db%localhost");
Table table(ds, "table_name");
code upsert = table.getUpsertClosureWithValidation(row, AbstractTable::UpsertSelectFirst);
on_success ds.commit();
on_error ds.rollback();
map upsert($1), row_list;
        @endcode

        @param example_row a hash representing an example row to insert or update; every row passed to the upsert closure returned must have the same keys in the same order or the closure returned will throw an \c UPSERT-ERROR exception
        @param upsert_strategy see @ref upsert_options for possible values for the upsert strategy
        @param opt a hash of options for the upsert operation; see @ref SqlUtil::AbstractTable::UpsertOptions for common options; each driver can support additional driver-specific options

        @return a closure that can be executed given a hash argument representing a single row that will be updated or inserted in the database with the given upsert strategy; the closure has the following signature: @code{.py} int sub upsert(hash<auto> row) {} @endcode The return value of the closure is an integer code giving the result of the update; see @ref upsert_results for more information

        @throw COLUMN-ERROR an unknown column was referenced in the hash to be inserted
        @throw UPSERT-ERROR no primary key, unique constraint, or unique index for upsert; not all columns of the unique constraint/index are used in the upsert statement

        @note the row values passed to the closure for upserting are checked if they match the example row passed to the getUpsertClosure() method; passing non-conforming data to the closure will cause the closure to throw an \c UPSERT-ERROR exception; see @ref SqlUtil::AbstractTable::getUpsertClosure() for a similar method that returns a non-validating closure; the closure returned by this method is a little slower than the one returned by @ref SqlUtil::AbstractTable::getUpsertClosure() since each row is validated
    */
    code<int(hash<auto>)> getUpsertClosureWithValidation(hash<auto> example_row, int upsert_strategy = UpsertAuto, *hash<auto> opt) {
        code<int(hash<auto>)> upsert = getUpsertClosure(example_row, upsert_strategy, opt);

        return int sub (hash<auto> row) {
            if (!row.compareKeys(example_row)) {
                throw "UPSERT-ERROR", sprintf("%s: the keys in the row passed to the upsert closure (%y) do not "
                    "match the keys passed to AbstractTable::getUpsertClosureWithValidation() (%y)", getDesc(),
                    row.keys, keys example_row);
            }
            return upsert(row);
        };
    }

    #! this method upserts or merges data from the given iterator argument (whose @ref Qore::AbstractIterator::getValue() "getValue()" method must return a hash giving row values) into the current table; the transaction is committed if successful, if an error occurs then it is rolled back
    /** @par Example:
        @code{.py}
hash h = table.upsertFromIterator(i, AbstractTable::UpsertUpdateFirst);
        @endcode

        @param i the @ref Qore::AbstractIterator argument that will provide the data to upsert or merge into the current table; the @ref Qore::AbstractIterator::getValue() "getValue()" method of this object must return a hash giving row values (for example, the @ref Qore::SQL::SQLStatement class is well-suited to be used here)
        @param upsert_strategy see @ref upsert_options for possible values for the upsert strategy
        @param opt a hash of options for the upsert operation; see @ref SqlUtil::AbstractTable::UpsertOptions for common options; each driver can support additional driver-specific options

        @return @ref nothing if no actions were taken or a hash with the following keys assigned to numeric values indicating the number of rows processed (keys correspond to \c SqlUtil::AbstractTable::UpsertResultDescriptionMap keys):
        - \c "inserted": the number of rows inserted
        - \c "verified": the number of rows updated unconditionally; note that this key is returned with all upsert strategy codes other than @ref SqlUtil::AbstractTable::UpsertSelectFirst instead of \c "updated"
        - \c "updated": the number of rows updated; note that this key is only returned if \a upsert_strategy is @ref SqlUtil::AbstractTable::UpsertSelectFirst, otherwise updated rows are reported as \c "verified" since rows are updated unconditionally with other the upsert strategy codes
        - \c "unchanged": the number of rows unchanged; this key can only be returned if \a upsert_strategy is @ref SqlUtil::AbstractTable::UpsertSelectFirst, @ref SqlUtil::AbstractTable::UpsertInsertOnly, or @ref SqlUtil::AbstractTable::UpsertUpdateOnly
        - \c "deleted": the number of rows deleted; this can only be returned if \c SqlUtil::AbstractTable::UpsertOptions "upsert option" \c delete_others is @ref True "True"

        @throw OPTION-ERROR invalid or unsupported option
        @throw COLUMN-ERROR an unknown column was referenced in the hash to be inserted
        @throw UPSERT-ERROR no primary key, unique constraint, or unique index for upsert; not all columns of the unique constraint/index are used in the upsert statement

        @note if \c SqlUtil::AbstractTable::UpsertOptions "upsert option" \c delete_others is @ref True "True", then a hash of primary key values in the input data is built as the input data is iterated.  After iterating, if the row count of the table and the input data matches, then nothing more is done, otherwise, every row of the table is iterated and compared to the primary key hash; if a row does not match a primary key value, then it is deleted.  This operation is only executed if \c delete_others is @ref True "True" and is expensive for large data sets.

        @see
        - upsertFromIteratorCommit()
        - upsertFromSelect()
        - upsertFromSelectCommit()
        - upsert()
        - upsertCommit()
        - getUpsertClosure()
        - insertFromIteratorCommit()
        - insertFromIterator()
    */
    *hash<string, int> upsertFromIteratorCommit(Qore::AbstractIterator i,
            int upsert_strategy = AbstractTable::UpsertAuto, *hash<auto> opt) {
        on_success ds.commit();
        on_error ds.rollback();

        return upsertFromIteratorIntern(i, upsert_strategy, opt);
    }

    #! this method upserts or merges data from the given iterator argument (whose @ref Qore::AbstractIterator::getValue() "getValue()" method must return a hash giving row values) into the current table; no transaction management is performed with this method
    /** @par Example:
        @code{.py}
hash h = table.upsertFromIterator(i, AbstractTable::UpsertUpdateFirst);
        @endcode

        @param i the @ref Qore::AbstractIterator argument that will provide the data to upsert or merge into the current table; the @ref Qore::AbstractIterator::getValue() "getValue()" method of this object must return a hash giving row values (for example, the @ref Qore::SQL::SQLStatement class is well-suited to be used here)
        @param upsert_strategy see @ref upsert_options for possible values for the upsert strategy
        @param opt a hash of options for the upsert operation; see @ref SqlUtil::AbstractTable::UpsertOptions for common options; each driver can support additional driver-specific options; note that this method ignores any \c "commit_block" option

        @return @ref nothing if no actions were taken or a hash with the following keys assigned to numeric values indicating the number of rows processed (keys correspond to \c SqlUtil::AbstractTable::UpsertResultDescriptionMap keys):
        - \c "inserted": the number of rows inserted
        - \c "verified": the number of rows updated unconditionally; note that this key is returned with all upsert strategy codes other than @ref SqlUtil::AbstractTable::UpsertSelectFirst instead of \c "updated"
        - \c "updated": the number of rows updated; note that this key is only returned if \a upsert_strategy is @ref SqlUtil::AbstractTable::UpsertSelectFirst, otherwise updated rows are reported as \c "verified" since rows are updated unconditionally with other the upsert strategy codes
        - \c "unchanged": the number of rows unchanged; this key can only be returned if \a upsert_strategy is @ref SqlUtil::AbstractTable::UpsertSelectFirst, @ref SqlUtil::AbstractTable::UpsertInsertOnly,  or @ref SqlUtil::AbstractTable::UpsertUpdateOnly
        - \c "deleted": the number of rows deleted; this can only be returned if \c SqlUtil::AbstractTable::UpsertOptions "upsert option" \c delete_others is @ref True "True"

        @throw OPTION-ERROR invalid or unsupported option
        @throw COLUMN-ERROR an unknown column was referenced in the hash to be inserted
        @throw UPSERT-ERROR no primary key, unique constraint, or unique index for upsert; not all columns of the unique constraint/index are used in the upsert statement

        @note if \c SqlUtil::AbstractTable::UpsertOptions "upsert option" \c delete_others is @ref True "True", then a hash of primary key values in the input data is built as the input data is iterated.  After iterating, if the row count of the table and the input data matches, then nothing more is done, otherwise, every row of the table is iterated and compared to the primary key hash; if a row does not match a primary key value, then it is deleted.  This operation is only executed if \c delete_others is @ref True "True" and is expensive for large data sets.

        @see
        - upsertFromIteratorCommit()
        - upsertFromSelect()
        - upsertFromSelectCommit()
        - upsert()
        - upsertCommit()
        - getUpsertClosure()
        - insertFromIteratorCommit()
        - insertFromIterator()
    */
    *hash<string, int> upsertFromIterator(Qore::AbstractIterator i, int upsert_strategy = AbstractTable::UpsertAuto,
            *hash<auto> opt) {
        return upsertFromIteratorIntern(i, upsert_strategy, opt - "commit_block");
    }

    #! A legacy SqlUtil::AbstractTable::upsertFromIterator() wrapper
    deprecated *hash<string, int> upsertFromIteratorNoCommit(Qore::AbstractIterator i,
            int upsert_strategy = AbstractTable::UpsertAuto, *hash<auto> opt) {
        return upsertFromIteratorIntern(i, upsert_strategy, opt - "commit_block");
    }

    private *hash<string, int> upsertFromIteratorIntern(Qore::AbstractIterator i,
            int upsert_strategy = AbstractTable::UpsertAuto, *hash<auto> opt) {
        # check upsert options
        validateOptionsIntern("OPTION-ERROR", getUpsertOptions(), \opt);

        # try to get initial row
        if (!i.next()) {
            return;
        }

        # result hash
        hash<string, int> rh;

        # primary key value hash for "delete_others"
        hash<string, bool> pkh;

        hash<auto> row = i.getValue();
        code upsert = getUpsertClosure(row, upsert_strategy, opt);

        opt.change_count = 0;
        do {
            int code = upsert(row);
            ++rh{AbstractTable::UpsertResultMap{code}};
            if (opt.info_callback) {
                opt.info_callback(getSqlName(), row, code);
            }

            if (code != UR_Unchanged && opt.commit_block) {
                if (++opt.change_count >= opt.commit_block) {
                    ds.commit();
                    opt.change_count = 0;
                }
            }

            # save primary key value in primary key hash if "delete_others" is set
            if (opt.delete_others) {
                pkh{foldl $1 + "-" + $2, (map row.$1.toString(), primaryKey.keyIterator())} = True;
            }
        } while (i.next() && (row = i.getValue()));

        if (opt.delete_others) {
            rh += doDeleteOthersIntern(pkh, opt);
        }

        return rh;
    }

    private *hash<string, int> doDeleteOthersIntern(hash<auto> pkh, *hash<auto> opt) {
        hash<string, int> rh;

        # iterate through the table to find rows that do not belong and delete them
        list<string> pkl = primaryKey.keys();
        foreach hash<auto> row in (getStatement()) {
            string k = foldl $1 + "-" + $2, (map row.$1.toString(), pkl);
            if (pkh{k})
                continue;

            hash<auto> h = row{pkl};
            self.del(h);

            ++rh.deleted;
            if (opt.info_callback)
                opt.info_callback(getSqlName(), row, UR_Deleted);
            if (opt.commit_block) {
                if (++opt.change_count >= opt.commit_block) {
                    ds.commit();
                    opt.change_count = 0;
                }
            }
        }

        return rh;
    }

    #! Upserts or merges data from the given foreign table and @ref select_option_hash "select option hash" into the current table
    /** @par Example:
        @code{.py}
hash<auto> h = table.upsertFromSelectCommit(table2, ("where": ("account_type": "CUSTOMER")),
    AbstractTable::UpsertUpdateFirst);
        @endcode

        The transaction is committed if successful, if an error occurs then it is rolled back.

        The table argument does not need to be in the same database as the current table; it can also
        be in a different database server or a database server of a different type (you can use this method to upsert
        or merge data to or from any database supported by SqlUtil).

        @param t the table for the source data; this does not need to be in the same database as the target (the
        current table), nor does it need to be the same database type
        @param sh a hash of conditions for the select statement; see @ref select_option_hash "select option hash" for
        information about this argument
        @param upsert_strategy see @ref upsert_options for possible values for the upsert strategy
        @param opt a hash of options for the upsert operation; see @ref SqlUtil::AbstractTable::UpsertOptions for
        common options; each driver can support additional driver-specific options

        @return @ref nothing if no actions were taken or a hash with the following keys assigned to numeric values
        indicating the number of rows processed (keys correspond to
        @ref SqlUtil::AbstractTable::UpsertResultDescriptionMap keys):
        - \c "inserted": the number of rows inserted
        - \c "verified": the number of rows updated unconditionally; note that this key is returned with all upsert
          strategy codes other than @ref SqlUtil::AbstractTable::UpsertSelectFirst instead of \c "updated"
        - \c "updated": the number of rows updated; note that this key is only returned if \a upsert_strategy is
          @ref SqlUtil::AbstractTable::UpsertSelectFirst, otherwise updated rows are reported as \c "verified" since
          rows are updated unconditionally with other the upsert strategy codes
        - \c "unchanged": the number of rows unchanged; this key can only be returned if \a upsert_strategy is
          @ref SqlUtil::AbstractTable::UpsertSelectFirst, @ref SqlUtil::AbstractTable::UpsertInsertOnly, or
          @ref SqlUtil::AbstractTable::UpsertUpdateOnly
        - \c "deleted": the number of rows deleted; this can only be returned if
          \c SqlUtil::AbstractTable::UpsertOptions "upsert option" \c delete_others is @ref True "True"

        @throw OPTION-ERROR invalid or unsupported option
        @throw COLUMN-ERROR an unknown column was referenced in the hash to be inserted
        @throw UPSERT-ERROR no primary key, unique constraint, or unique index for upsert; not all columns of the
        unique constraint/index are used in the upsert statement

        @note
        - if \c SqlUtil::AbstractTable::UpsertOptions "upsert option" \c delete_others is @ref True "True", then a
          hash of primary key values in the input data is built as the input data is iterated.  After iterating, if
          the row count of the table and the input data matches, then nothing more is done, otherwise, every row of
          the table is iterated and compared to the primary key hash; if a row does not match a primary key value,
          then it is deleted.  This operation is only executed if \c delete_others is @ref True "True" and is
          expensive for large data sets.
        - this method also calls commit() or rollback() on the source table to release the transaction lock on the
          underlying source table datasource as an @ref Qore::SQL::AbstractSQLStatement "AbstractSQLStatement" object
          is used to pipeline the data to the upsert code; use upsertFromSelect() to do manual transaction management
          instead
        - unlike insertFromSelect() and insertFromSelectCommit(), this method processes arbitrary input data and
          accepts @ref SqlUtil::AbstractTable::UpsertOptions "upsert options"

        @see
        - upsertFromSelect()
        - upsertFromIterator()
        - upsertFromIteratorCommit()
        - upsert()
        - upsertCommit()
        - getUpsertClosure()
    */
    *hash<string, int> upsertFromSelectCommit(AbstractTable t, *hash<auto> sh,
            int upsert_strategy = AbstractTable::UpsertAuto, *hash<auto> opt) {
        on_success { ds.commit(); t.commit(); }
        on_error { ds.rollback(); t.rollback(); }

        return upsertFromSelect(t, sh, upsert_strategy, opt);
    }

    #! @ref SqlUtil::AbstractTable::upsertFromSelectCommit() variant
    *hash<string, int> upsertFromSelectCommit(Table t, *hash<auto> sh,
            int upsert_strategy = AbstractTable::UpsertAuto, *hash<auto> opt) {
        on_success { ds.commit(); t.commit(); }
        on_error { ds.rollback(); t.rollback(); }

        return upsertFromIteratorIntern(t.getStatement(sh), upsert_strategy, opt);
    }

    #! this method upserts or merges data from the given foreign table and @ref select_option_hash "select option hash" into the current table; no transaction management is performed with this method
    /** @par Example:
        @code{.py}
on_success { table.commit(); table2.commit(); }
on_error { table.rollback(); table2.rollback(); }
hash h = table.upsertFromSelect(table2, ("where": ("account_type": "CUSTOMER")), AbstractTable::UpsertUpdateFirst);
        @endcode

        The table argument does not need to be in the same database as the current table; it can also
        be in a different database server or a database server of a different type (you can use this method to upsert or
        merge data to or from any database supported by SqlUtil).

        @param t the table for the source data; this does not need to be in the same database as the target (the current table), nor does it need to be the same database type
        @param sh a hash of conditions for the select statement; see @ref select_option_hash "select option hash" for information about this argument
        @param upsert_strategy see @ref upsert_options for possible values for the upsert strategy
        @param opt a hash of options for the upsert operation; see @ref SqlUtil::AbstractTable::UpsertOptions for common options; each driver can support additional driver-specific options; note that this method ignores any \c "commit_block" option

        @return @ref nothing if no actions were taken or a hash with the following keys assigned to numeric values indicating the number of rows processed (keys correspond to \c SqlUtil::AbstractTable::UpsertResultDescriptionMap keys):
        - \c "inserted": the number of rows inserted
        - \c "verified": the number of rows updated unconditionally; note that this key is returned with all upsert strategy codes other than @ref SqlUtil::AbstractTable::UpsertSelectFirst instead of \c "updated"
        - \c "updated": the number of rows updated; note that this key is only returned if \a upsert_strategy is @ref SqlUtil::AbstractTable::UpsertSelectFirst, otherwise updated rows are reported as \c "verified" since rows are updated unconditionally with other the upsert strategy codes
        - \c "unchanged": the number of rows unchanged; this key can only be returned if \a upsert_strategy is @ref SqlUtil::AbstractTable::UpsertSelectFirst, @ref SqlUtil::AbstractTable::UpsertInsertOnly, or @ref SqlUtil::AbstractTable::UpsertUpdateOnly
        - \c "deleted": the number of rows deleted; this can only be returned if \c SqlUtil::AbstractTable::UpsertOptions "upsert option" \c delete_others is @ref True "True"

        @throw OPTION-ERROR invalid or unsupported option
        @throw COLUMN-ERROR an unknown column was referenced in the hash to be inserted
        @throw UPSERT-ERROR no primary key, unique constraint, or unique index for upsert; not all columns of the unique constraint/index are used in the upsert statement

        @note
        - if \c SqlUtil::AbstractTable::UpsertOptions "upsert option" \c delete_others is @ref True "True", then a hash of primary key values in the input data is built as the input data is iterated.  After iterating, if the row count of the table and the input data matches, then nothing more is done, otherwise, every row of the table is iterated and compared to the primary key hash; if a row does not match a primary key value, then it is deleted.  This operation is only executed if \c delete_others is @ref True "True" and is expensive for large data sets.
        - this method uses an @ref Qore::SQL::AbstractSQLStatement "AbstractSQLStatement" object to pipeline the select data to the upsert code; to release the transaction lock acquired by the @ref Qore::SQL::AbstractSQLStatement "AbstractSQLStatement" object, a commit() or rollback() action must be executed on the underlying datasource object as in the example above
        - unlike insertFromSelect() and insertFromSelectCommit(), this method processes arbitrary input data and accepts @ref SqlUtil::AbstractTable::UpsertOptions "upsert options"

        @see
        - upsertFromSelect()
        - upsertFromIterator()
        - upsertFromIteratorCommit()
        - upsert()
        - upsertCommit()
        - getUpsertClosure()
    */
    *hash<string, int> upsertFromSelect(AbstractTable t, *hash<auto> sh,
            int upsert_strategy = AbstractTable::UpsertAuto, *hash<auto> opt) {
        return upsertFromIteratorIntern(t.getStatement(sh), upsert_strategy, opt - "commit_block");
    }

    #! A legacy @ref SqlUtil::AbstractTable::upsertFromSelect() wrapper
    deprecated *hash<string, int> upsertFromSelectNoCommit(AbstractTable t, *hash<auto> sh,
            int upsert_strategy = AbstractTable::UpsertAuto, *hash<auto> opt) {
        return upsertFromIteratorIntern(t.getStatement(sh), upsert_strategy, opt - "commit_block");
    }

    #! @ref SqlUtil::AbstractTable::upsertFromSelect() variant
    deprecated *hash<string, int> upsertFromSelect(Table t, *hash<auto> sh,
            int upsert_strategy = AbstractTable::UpsertAuto, *hash<auto> opt) {
        return upsertFromIteratorIntern(t.getStatement(sh), upsert_strategy, opt - "commit_block");
    }

    #! A legacy @ref SqlUtil::AbstractTable::upsertFromSelect() wrapper
    deprecated *hash<string, int> upsertFromSelectNoCommit(Table t, *hash<auto> sh,
            int upsert_strategy = AbstractTable::UpsertAuto, *hash<auto> opt) {
        return upsertFromIteratorIntern(t.getStatement(sh), upsert_strategy, opt - "commit_block");
    }

    #! returns the number of rows in the table
    /** @par Example:
        @code{.py}
int cnt = table.rowCount();
        @endcode

        @return the number of rows in the table

        @note to see if the table is empty or not, use emptyData() as this is much faster than rowCount()

        @see emptyData()
    */
    softint rowCount() {
        return ds.selectRow("select count(1) cnt from %s", getSqlName()).cnt;
    }

    #! returns an @ref Qore::SQL::AbstractSQLStatement "AbstractSQLStatement" object that will iterate the results of a select statement matching the arguments
    /** @par Example:
        @code{.py}
AbstractSQLStatement i = table.getRowIterator(sh);
        @endcode

        @param sh a hash of conditions for the select statement; see @ref select_option_hash "select option hash" for information about this argument
        @param sql an optional reference to a string to return the SQL generated for the select statement
        @param opt optional SQL data operation callback options; see @ref AbstractTable::SqlDataCallbackOptions for more info

        @return an @ref Qore::SQL::AbstractSQLStatement "AbstractSQLStatement" object that will iterate the results of a select statement matching the arguments

        @throw OPTION-ERROR invalid or unsupported select option
        @throw SELECT-ERROR \c 'offset' supplied without \c 'orderby' or \c 'limit', \c 'orderby' with \c 'limit' and \c 'offset' does not match any unique constraint

        @note
        - if \c "offset" is supplied and no \c "orderby" is supplied, then if any primary key exists, the primary key columns will be used for the \c "orderby" option automatically
        - the @ref Qore::SQL::AbstractSQLStatement "AbstractSQLStatement" object created by a successful call to this method acquires a thread resource for the underlying @ref Qore::SQL::AbstractDatasource "AbstractDatasource" object that must be released by calling @ref Qore::SQL::AbstractDatasource::commit() "commit()" or @ref Qore::SQL::AbstractDatasource::rollback() "rollback()", even if the statement does not acquire any database locks

        @deprecated use getStatement() instead; this method is a legacy alias
    */
    Qore::SQL::AbstractSQLStatement getRowIterator(*hash<auto> sh, *reference<string> sql, *hash<auto> opt) {
        return getStatementIntern(sh, \sql, opt);
    }

    #! returns an @ref Qore::SQL::AbstractSQLStatement "AbstractSQLStatement" object that will iterate the results of a select statement matching the arguments without executing the statement itself; the statement is only prepared
    /** @par Example:
        @code{.py}
AbstractSQLStatement i = table.getRowIteratorNoExec(sh);
        @endcode

        @param sh a hash of conditions for the select statement; see @ref select_option_hash "select option hash" for information about this argument
        @param sql an optional reference to a string to return the SQL generated for the select statement
        @param opt optional SQL data operation callback options; see @ref AbstractTable::SqlDataCallbackOptions for more info

        @return an @ref Qore::SQL::AbstractSQLStatement "AbstractSQLStatement" object that will iterate the results of a select statement matching the arguments

        @throw OPTION-ERROR invalid or unsupported select option
        @throw SELECT-ERROR \c 'offset' supplied without \c 'orderby' or \c 'limit', \c 'orderby' with \c 'limit' and \c 'offset' does not match any unique constraint

        @note
        - if \c "offset" is supplied and no \c "orderby" is supplied, then if any primary key exists, the primary key columns will be used for the \c "orderby" option automatically
        - no transaction thread resources / locks are acquired with this method as the SQL statement is not executed

        @see getRowIterator()

        @deprecated use getStatementNoExec() instead; this method is a legacy alias
    */
    Qore::SQL::AbstractSQLStatement getRowIteratorNoExec(*hash<auto> sh, *reference<string> sql, *hash<auto> opt) {
        return getStatementIntern(sh, \sql, opt, True);
    }

    #! returns an @ref Qore::SQL::AbstractSQLStatement "AbstractSQLStatement" object that will iterate the results of a select statement matching the arguments
    /** @par Example:
        @code{.py}
AbstractSQLStatement i = table.getRowIterator();
        @endcode

        @param sh a hash of conditions for the select statement; see @ref select_option_hash "select option hash" for information about this argument
        @param opt optional SQL data operation callback options; see @ref AbstractTable::SqlDataCallbackOptions for more info

        @return an @ref Qore::SQL::AbstractSQLStatement "AbstractSQLStatement" object that will iterate the results of a select statement matching the arguments

        @throw OPTION-ERROR invalid or unsupported select option
        @throw SELECT-ERROR \c 'offset' supplied without \c 'orderby' or \c 'limit', \c 'orderby' with \c 'limit' and \c 'offset' does not match any unique constraint

        @note
        - if \c "offset" is supplied and no \c "orderby" is supplied, then if any primary key exists, the primary key columns will be used for the \c "orderby" option automatically
        - the @ref Qore::SQL::AbstractSQLStatement "AbstractSQLStatement" object created by a successful call to this method acquires a thread resource for the underlying @ref Qore::SQL::AbstractDatasource "AbstractDatasource" object that must be released by calling @ref Qore::SQL::AbstractDatasource::commit() "commit()" or @ref Qore::SQL::AbstractDatasource::rollback() "rollback()", even if the statement does not acquire any database locks

        @deprecated use getStatement() instead; this method is a legacy alias
    */
    Qore::SQL::AbstractSQLStatement getRowIterator(*hash<auto> sh, *hash<auto> opt) {
        return getStatementIntern(sh, NOTHING, opt);
    }

    #! returns an @ref Qore::SQL::AbstractSQLStatement "AbstractSQLStatement" object that will iterate the results of a select statement matching the arguments
    /** @par Example:
        @code{.py}
AbstractSQLStatement i = table.getStatement(sh);
        @endcode

        @param sh a hash of conditions for the select statement; see @ref select_option_hash "select option hash" for information about this argument
        @param sql an optional reference to a string to return the SQL generated for the select statement
        @param opt optional SQL data operation callback options; see @ref AbstractTable::SqlDataCallbackOptions for more info

        @return an @ref Qore::SQL::AbstractSQLStatement "AbstractSQLStatement" object that will iterate the results of a select statement matching the arguments

        @throw OPTION-ERROR invalid or unsupported select option
        @throw SELECT-ERROR \c 'offset' supplied without \c 'orderby' or \c 'limit', \c 'orderby' with \c 'limit' and \c 'offset' does not match any unique constraint

        @note
        - if \c "offset" is supplied and no \c "orderby" is supplied, then if any primary key exists, the primary key columns will be used for the \c "orderby" option automatically
        - the @ref Qore::SQL::AbstractSQLStatement "AbstractSQLStatement" object created by a successful call to this method acquires a thread resource for the underlying @ref Qore::SQL::AbstractDatasource "AbstractDatasource" object that must be released by calling @ref Qore::SQL::AbstractDatasource::commit() "commit()" or @ref Qore::SQL::AbstractDatasource::rollback() "rollback()", even if the statement does not acquire any database locks

        @see getStatementNoExec()

        @since %SqlUtil 1.5
    */
    Qore::SQL::AbstractSQLStatement getStatement(*hash<auto> sh, *reference<string> sql, *hash<auto> opt) {
        return getStatementIntern(sh, \sql, opt);
    }

    #! returns an @ref Qore::SQL::AbstractSQLStatement "AbstractSQLStatement" object that will iterate the results of a select statement matching the arguments
    /** @par Example:
        @code{.py}
AbstractSQLStatement i = table.getStatement();
        @endcode

        @param sh a hash of conditions for the select statement; see @ref select_option_hash "select option hash" for information about this argument
        @param opt optional SQL data operation callback options; see @ref AbstractTable::SqlDataCallbackOptions for more info

        @return an @ref Qore::SQL::AbstractSQLStatement "AbstractSQLStatement" object that will iterate the results of a select statement matching the arguments

        @throw OPTION-ERROR invalid or unsupported select option
        @throw SELECT-ERROR \c 'offset' supplied without \c 'orderby' or \c 'limit', \c 'orderby' with \c 'limit' and \c 'offset' does not match any unique constraint

        @note
        - if \c "offset" is supplied and no \c "orderby" is supplied, then if any primary key exists, the primary key columns will be used for the \c "orderby" option automatically
        - the @ref Qore::SQL::AbstractSQLStatement "AbstractSQLStatement" object created by a successful call to this method acquires a thread resource for the underlying @ref Qore::SQL::AbstractDatasource "AbstractDatasource" object that must be released by calling @ref Qore::SQL::AbstractDatasource::commit() "commit()" or @ref Qore::SQL::AbstractDatasource::rollback() "rollback()", even if the statement does not acquire any database locks

        @see getStatementNoExec()

        @since %SqlUtil 1.5
    */
    Qore::SQL::AbstractSQLStatement getStatement(*hash<auto> sh, *hash<auto> opt) {
        return getStatementIntern(sh, NOTHING, opt);
    }

    #! returns a result hash including an @ref Qore::SQL::AbstractSQLStatement "AbstractSQLStatement" object that will iterate the results of a select statement matching the arguments
    /** @par Example:
        @code{.py}
hash<SqlResultInfo> info = table.getStatementWithInfo(sh);
        @endcode

        @param select_hash a hash of conditions for the select statement; see
        @ref select_option_hash "select option hash" for information about this argument
        @param opt optional SQL data operation callback options; see @ref AbstractTable::SqlDataCallbackOptions for
        more info

        @return the result info hash; the \c result key will hold an
        @ref Qore::SQL::AbstractSQLStatement "AbstractSQLStatement" object that will iterate the results of a select
        statement matching the arguments

        @throw OPTION-ERROR invalid or unsupported select option
        @throw SELECT-ERROR \c 'offset' supplied without \c 'orderby' or \c 'limit', \c 'orderby' with \c 'limit' and
        \c 'offset' does not match any unique constraint

        @note
        - if \c "offset" is supplied and no \c "orderby" is supplied, then if any primary key exists, the primary key
          columns will be used for the \c "orderby" option automatically
        - the @ref Qore::SQL::AbstractSQLStatement "AbstractSQLStatement" object created by a successful call to this
          method acquires a thread resource for the underlying @ref Qore::SQL::AbstractDatasource "AbstractDatasource"
          object that must be released by calling @ref Qore::SQL::AbstractDatasource::commit() "commit()" or
          @ref Qore::SQL::AbstractDatasource::rollback() "rollback()", even if the statement does not acquire any
          database locks

        @see getStatementNoExecWithInfo()

        @since %SqlUtil 1.7
    */
    hash<SqlResultInfo> getStatementWithInfo(*hash<auto> select_hash, *hash<auto> opt) {
        hash<SqlResultInfo> rv();
        rv.result = getStatementIntern(select_hash, \rv.sql, opt, False, \rv.args);
        return rv;
    }

    #! returns an @ref Qore::SQL::AbstractSQLStatement "AbstractSQLStatement" object that will iterate the results of a select statement matching the arguments without executing the statement itself; the statement is only prepared
    /** @par Example:
        @code{.py}
AbstractSQLStatement i = table.getStatementNoExec(sh);
        @endcode

        @param sh a hash of conditions for the select statement; see @ref select_option_hash "select option hash" for information about this argument
        @param sql an optional reference to a string to return the SQL generated for the select statement
        @param opt optional SQL data operation callback options; see @ref AbstractTable::SqlDataCallbackOptions for more info

        @return an @ref Qore::SQL::AbstractSQLStatement "AbstractSQLStatement" object that will iterate the results of a select statement matching the arguments

        @throw OPTION-ERROR invalid or unsupported select option
        @throw SELECT-ERROR \c 'offset' supplied without \c 'orderby' or \c 'limit', \c 'orderby' with \c 'limit' and \c 'offset' does not match any unique constraint

        @note
        - if \c "offset" is supplied and no \c "orderby" is supplied, then if any primary key exists, the primary key columns will be used for the \c "orderby" option automatically
        - no transaction thread resources / locks are acquired with this method as the SQL statement is not executed

        @see getStatement()

        @since %SqlUtil 1.5
    */
    Qore::SQL::AbstractSQLStatement getStatementNoExec(*hash<auto> sh, *reference<string> sql, *hash<auto> opt) {
        return getStatementIntern(sh, \sql, opt, True);
    }

    #! returns a result hash including an @ref Qore::SQL::AbstractSQLStatement "AbstractSQLStatement" object that will iterate the results of a select statement matching the arguments without executing the statement itself; the statement is only prepared
    /** @par Example:
        @code{.py}
hash<SqlResultInfo> info = table.getStatementNoExecWithInfo(sh);
        @endcode

        @param select_hash a hash of conditions for the select statement; see
        @ref select_option_hash "select option hash" for information about this argument
        @param opt optional SQL data operation callback options; see @ref AbstractTable::SqlDataCallbackOptions for
        more info

        @return the result info hash; the \c result key will hold an
        @ref Qore::SQL::AbstractSQLStatement "AbstractSQLStatement" object that will iterate the results of a select
        statement matching the arguments

        @throw OPTION-ERROR invalid or unsupported select option
        @throw SELECT-ERROR \c 'offset' supplied without \c 'orderby' or \c 'limit', \c 'orderby' with \c 'limit' and
        \c 'offset' does not match any unique constraint

        @note
        - if \c "offset" is supplied and no \c "orderby" is supplied, then if any primary key exists, the primary key
          columns will be used for the \c "orderby" option automatically
        - no transaction thread resources / locks are acquired with this method as the SQL statement is not executed

        @see getStatementNoExecWithInfo()

        @since %SqlUtil 1.7
    */
    hash<SqlResultInfo> getStatementNoExecWithInfo(*hash<auto> select_hash, *hash<auto> opt) {
        hash<SqlResultInfo> rv();
        rv.result = getStatementIntern(select_hash, \rv.sql, opt, True, \rv.args);
        return rv;
    }

    #! returns an @ref Qore::SQL::AbstractSQLStatement "AbstractSQLStatement" object that will iterate the results of a select statement matching the arguments without executing the statement itself; the statement is only prepared
    /** @par Example:
        @code{.py}
AbstractSQLStatement i = table.getStatement();
        @endcode

        @param sh a hash of conditions for the select statement; see @ref select_option_hash "select option hash" for information about this argument
        @param opt optional SQL data operation callback options; see @ref AbstractTable::SqlDataCallbackOptions for more info

        @return an @ref Qore::SQL::AbstractSQLStatement "AbstractSQLStatement" object that will iterate the results of a select statement matching the arguments

        @throw OPTION-ERROR invalid or unsupported select option
        @throw SELECT-ERROR \c 'offset' supplied without \c 'orderby' or \c 'limit', \c 'orderby' with \c 'limit' and \c 'offset' does not match any unique constraint

        @note
        - if \c "offset" is supplied and no \c "orderby" is supplied, then if any primary key exists, the primary key columns will be used for the \c "orderby" option automatically
        - no transaction thread resources / locks are acquired with this method as the SQL statement is not executed

        @see getStatement()

        @since %SqlUtil 1.5
        */
    Qore::SQL::AbstractSQLStatement getStatementNoExec(*hash<auto> sh, *hash<auto> opt) {
        return getStatementIntern(sh, NOTHING, opt, True);
    }

    private Qore::SQL::AbstractSQLStatement getStatementIntern(*hash<auto> sh, *reference<string> sql,
            *hash<auto> opt, *bool no_exec, *reference<softlist<auto>> args) {
        validateOptionsIntern("OPTION-ERROR", getSelectOptions(), \sh);

        sql = getSelectSqlStringIntern(sh, \args, opt);

        AbstractSQLStatement stmt = ds.getSQLStatement();
        stmt.prepare(sql);
        if (no_exec) {
            if (args) {
                stmt.bindArgs(args);
            }
        } else {
            execData(stmt, opt, args);
        }

        return stmt;
    }

    #! returns a hash representing the row in the table that matches the argument hash
    /** @par Example:
        @code{.py}
*hash<auto> h = table.selectRow(sh);
        @endcode

        If more than one row would be returned an exception is raised

        @param sh a hash of conditions for the select statement; see @ref select_option_hash "select option hash" for information about this argument
        @param sql an optional reference to a string to return the SQL generated for the select statement
        @param opt optional SQL data operation callback options; see @ref AbstractTable::SqlDataCallbackOptions for more info

        @return a hash representing the row in the table that matches the argument hash; if more than one row would be returned an exception is raised

        @throw OPTION-ERROR invalid or unsupported select option
        @throw SELECT-ERROR \c 'offset' supplied without \c 'orderby' or \c 'limit', \c 'orderby' with \c 'limit' and \c 'offset' does not match any unique constraint
        @throw DBI-SELECT-ROW-ERROR more than 1 row retrieved from the server

        @note
        - if \c "offset" is supplied and no \c "orderby" is supplied, then if any primary key exists, the primary key columns will be used for the \c "orderby" option automatically
        - if the \c forupdate @ref select_option_hash "select option" is used, then after a successful select operation, the calling thread will own the thread transaction lock
    */
    *hash<auto> selectRow(*hash<auto> sh, *reference<string> sql, *hash<auto> opt) {
        return selectRowIntern(sh, \sql, opt);
    }

    private *hash<auto> selectRowIntern(*hash<auto> sh, *reference<string> sql, *hash<auto> opt,
            *reference<softlist<auto>> args) {
        validateOptionsIntern("OPTION-ERROR", getSelectOptions(), \sh);
        # check data callback options if any
        validateOptionsIntern("OPTION-ERROR", getSqlDataCallbackOptions(), \opt);

        sql = getSelectSqlStringIntern(sh, \args, opt);

        if (opt.sqlarg_callback) {
            opt.sqlarg_callback(sql, args);
        }

        bool rollback_on_error = False;
        if (sh.forupdate) {
            rollback_on_error = !ds.currentThreadInTransaction();
            if (rollback_on_error)
                ds.beginTransaction();
        }
        on_error if (rollback_on_error)
            ds.rollback();
        return ds.vselectRow(sql, args);
    }

    #! returns a hash with a result representing the row in the table that matches the argument hash; if more than one row would be returned an exception is raised
    /** @par Example:
        @code{.py}
hash<SqlResultInfo> info = table.selectRowWithInfo(sh);
        @endcode

        @param select_hash a hash of conditions for the select statement; see
        @ref select_option_hash "select option hash" for information about this argument
        @param opt optional SQL data operation callback options; see @ref AbstractTable::SqlDataCallbackOptions for
        more info

        @return an info hash; the \c result key will contain a hash representing the row in the table that matches the
        argument hash; if more than one row would be returned an exception is raised

        @throw OPTION-ERROR invalid or unsupported select option
        @throw SELECT-ERROR \c 'offset' supplied without \c 'orderby' or \c 'limit', \c 'orderby' with \c 'limit' and
        \c 'offset' does not match any unique constraint
        @throw DBI-SELECT-ROW-ERROR more than 1 row retrieved from the server

        @note
        - if \c "offset" is supplied and no \c "orderby" is supplied, then if any primary key exists, the primary key
          columns will be used for the \c "orderby" option automatically
        - if the \c forupdate @ref select_option_hash "select option" is used, then after a successful select
          operation, the calling thread will own the thread transaction lock

        @since SqlUTil 1.7
    */
    hash<SqlResultInfo> selectRowWithInfo(*hash<auto> select_hash, *hash<auto> opt) {
        hash<SqlResultInfo> rv();
        rv.result = selectRowIntern(select_hash, \rv.sql, opt, \rv.args);
        return rv;
    }

    #! returns a list of hashes representing the rows in the table that match the argument hash
    /** @par Example:
        @code{.py}
*list<auto> l = table.selectRows(sh);
        @endcode

        @param sh a hash of conditions for the select statement; see @ref select_option_hash "select option hash" for information about this argument
        @param sql an optional reference to a string to return the SQL generated for the select statement
        @param opt optional SQL data operation callback options; see @ref AbstractTable::SqlDataCallbackOptions for more info

        @return a list of hashes representing the rows in the table that match the argument hash

        @throw OPTION-ERROR invalid or unsupported select option
        @throw SELECT-ERROR \c 'offset' supplied without \c 'orderby' or \c 'limit', \c 'orderby' with \c 'limit' and \c 'offset' does not match any unique constraint

        @note
        - if \c "offset" is supplied and no \c "orderby" is supplied, then if any primary key exists, the primary key
          columns will be used for the \c "orderby" option automatically
        - if the \c forupdate @ref select_option_hash "select option" is used, then after a successful select
          operation, the calling thread will own the thread transaction lock
    */
    *list<auto> selectRows(*hash<auto> sh, *reference<string> sql, *hash<auto> opt) {
        return selectRowsIntern(sh, \sql, opt);
    }

    private *list<auto> selectRowsIntern(*hash<auto> sh, *reference<string> sql, *hash<auto> opt,
            *reference<softlist<auto>> args) {
        validateOptionsIntern("OPTION-ERROR", getSelectOptions(), \sh);
        # check data callback options if any
        validateOptionsIntern("OPTION-ERROR", getSqlDataCallbackOptions(), \opt);

        sql = getSelectSqlStringIntern(sh, \args, opt);

        if (opt.sqlarg_callback)
            opt.sqlarg_callback(sql, args);

        bool rollback_on_error = False;
        if (sh.forupdate) {
            rollback_on_error = !ds.currentThreadInTransaction();
            if (rollback_on_error)
                ds.beginTransaction();
        }
        on_error if (rollback_on_error)
            ds.rollback();

        return ds.vselectRows(sql, args);
    }

    #! returns a hash with a result key assigned to a list of hashes representing the rows in the table that match the argument hash
    /** @par Example:
        @code{.py}
hash<SqlResultInfo> info = table.selectRowsWithInfo(sh);
        @endcode

        @param select_hash a hash of conditions for the select statement; see
        @ref select_option_hash "select option hash" for information about this argument
        @param opt optional SQL data operation callback options; see @ref AbstractTable::SqlDataCallbackOptions for
        more info

        @return an info hash; the \c result key will contain a list of hashes representing the rows in the table that
        match the argument hash

        @throw OPTION-ERROR invalid or unsupported select option
        @throw SELECT-ERROR \c 'offset' supplied without \c 'orderby' or \c 'limit', \c 'orderby' with \c 'limit' and
        \c 'offset' does not match any unique constraint

        @note
        - if \c "offset" is supplied and no \c "orderby" is supplied, then if any primary key exists, the primary key
          columns will be used for the \c "orderby" option automatically
        - if the \c forupdate @ref select_option_hash "select option" is used, then after a successful select
          operation, the calling thread will own the thread transaction lock

        @since Qorus 1.7
    */
    hash<SqlResultInfo> selectRowsWithInfo(*hash<auto> select_hash, *hash<auto> opt) {
        hash<SqlResultInfo> rv();
        rv.result = selectRowsIntern(select_hash, \rv.sql, opt, \rv.args);
        return rv;
    }

    #! returns a hash of lists representing the columns and rows in the table that match the argument hash
    /** @par Example:
        @code{.py}
*hash h = table.select(sh);
        @endcode

        @param sh a hash of conditions for the select statement; see @ref select_option_hash "select option hash" for information about this argument
        @param sql an optional reference to a string to return the SQL generated for the select statement
        @param opt optional SQL data operation callback options; see @ref AbstractTable::SqlDataCallbackOptions for more info

        @return a hash of lists representing the columns and rows in the table that match the argument hash

        @throw OPTION-ERROR invalid or unsupported select option
        @throw SELECT-ERROR \c 'offset' supplied without \c 'orderby' or \c 'limit', \c 'orderby' with \c 'limit' and
        \c 'offset' does not match any unique constraint

        @note
        - if \c "offset" is supplied and no \c "orderby" is supplied, then if any primary key exists, the primary key columns will be used for the \c "orderby" option automatically
        - if the \c forupdate @ref select_option_hash "select option" is used, then after a successful select operation, the calling thread will own the thread transaction lock
    */
    *hash<auto> select(*hash<auto> sh, *reference<string> sql, *hash<auto> opt) {
        return selectIntern(sh, \sql, opt);
    }

    private *hash<auto> selectIntern(*hash<auto> sh, *reference<string> sql, *hash<auto> opt, *reference<softlist<auto>> args) {
        validateOptionsIntern("OPTION-ERROR", getSelectOptions(), \sh);

        sql = getSelectSqlStringIntern(sh, \args, opt);

        bool rollback_on_error = False;
        if (sh.forupdate) {
            rollback_on_error = !ds.currentThreadInTransaction();
            if (rollback_on_error)
                ds.beginTransaction();
        }
        on_error if (rollback_on_error)
            ds.rollback();

        return ds.vselect(sql, args);
    }

    #! returns a hash with a \c result key assigned to a hash of lists representing the columns and rows in the table that match the argument hash
    /** @par Example:
        @code{.py}
hash<SqlResultInfo> info = table.selectWithInfo(sh);
        @endcode

        @param select_hash a hash of conditions for the select statement; see
        @ref select_option_hash "select option hash" for information about this argument
        @param opt optional SQL data operation callback options; see @ref AbstractTable::SqlDataCallbackOptions for
        more info

        @return a hash with the \c result key assigned to a hash of lists representing the columns and rows in the
        table that match the argument hash

        @throw OPTION-ERROR invalid or unsupported select option
        @throw SELECT-ERROR \c 'offset' supplied without \c 'orderby' or \c 'limit', \c 'orderby' with \c 'limit' and
        \c 'offset' does not match any unique constraint

        @note
        - if \c "offset" is supplied and no \c "orderby" is supplied, then if any primary key exists, the primary key
          columns will be used for the \c "orderby" option automatically
        - if the \c forupdate @ref select_option_hash "select option" is used, then after a successful select
          operation, the calling thread will own the thread transaction lock

        @since Qorus 1.7
    */
    hash<SqlResultInfo> selectWithInfo(*hash<auto> select_hash, *hash<auto> opt) {
        hash<SqlResultInfo> rv();
        rv.result = selectIntern(select_hash, \rv.sql, opt, \rv.args);
        return rv;
    }

    #! returns a hash representing the row in the table that matches the argument hash; if more than one row would be returned an exception is raised
    /** @par Example:
        @code{.py}
*hash h = table.selectRow(sh);
        @endcode

        @param sh a hash of conditions for the select statement; see @ref select_option_hash "select option hash" for information about this argument
        @param opt optional SQL data operation callback options; see @ref AbstractTable::SqlDataCallbackOptions for more info

        @return a hash representing the row in the table that matches the argument hash; if more than one row would be returned an exception is raised

        @throw OPTION-ERROR invalid or unsupported select option
        @throw SELECT-ERROR \c 'offset' supplied without \c 'orderby' or \c 'limit', \c 'orderby' with \c 'limit' and \c 'offset' does not match any unique constraint
        @throw DBI-SELECT-ROW-ERROR more than 1 row retrieved from the server

        @note
        - if \c "offset" is supplied and no \c "orderby" is supplied, then if any primary key exists, the primary key columns will be used for the \c "orderby" option automatically
        - if the \c forupdate @ref select_option_hash "select option" is used, then after a successful select operation, the calling thread will own the thread transaction lock
    */
    *hash<auto> selectRow(*hash<auto> sh, *hash<auto> opt) {
        return selectRowIntern(sh, NOTHING, opt);
    }

    #! returns a list of hashes representing the rows in the table that match the argument hash
    /** @par Example:
        @code{.py}
*list l = table.selectRows(sh);
        @endcode

        @param sh a hash of conditions for the select statement; see @ref select_option_hash "select option hash" for information about this argument
        @param opt optional SQL data operation callback options; see @ref AbstractTable::SqlDataCallbackOptions for more info

        @return a list of hashes representing the rows in the table that match the argument hash

        @throw OPTION-ERROR invalid or unsupported select option
        @throw SELECT-ERROR \c 'offset' supplied without \c 'orderby' or \c 'limit', \c 'orderby' with \c 'limit' and \c 'offset' does not match any unique constraint

        @note
        - if \c "offset" is supplied and no \c "orderby" is supplied, then if any primary key exists, the primary key columns will be used for the \c "orderby" option automatically
        - if the \c forupdate @ref select_option_hash "select option" is used, then after a successful select operation, the calling thread will own the thread transaction lock
    */
    *list<auto> selectRows(*hash<auto> sh, *hash<auto> opt) {
        return selectRowsIntern(sh, NOTHING, opt);
    }

    #! returns a hash of lists representing the columns and rows in the table that match the argument hash
    /** @par Example:
        @code{.py}
*hash h = table.select(sh);
        @endcode

        @param sh a hash of conditions for the select statement; see @ref select_option_hash "select option hash" for information about this argument
        @param opt optional SQL data operation callback options; see @ref AbstractTable::SqlDataCallbackOptions for more info

        @return a hash of lists representing the columns and rows in the table that match the argument hash

        @throw OPTION-ERROR invalid or unsupported select option
        @throw SELECT-ERROR \c 'offset' supplied without \c 'orderby' or \c 'limit', \c 'orderby' with \c 'limit' and \c 'offset' does not match any unique constraint

        @note
        - if \c "offset" is supplied and no \c "orderby" is supplied, then if any primary key exists, the primary key columns will be used for the \c "orderby" option automatically
        - if the \c forupdate @ref select_option_hash "select option" is used, then after a successful select operation, the calling thread will own the thread transaction lock
    */
    *hash<auto> select(*hash<auto> sh, *hash<auto> opt) {
        validateOptionsIntern("OPTION-ERROR", getSelectOptions(), \sh);
        # check data callback options if any
        validateOptionsIntern("OPTION-ERROR", getSqlDataCallbackOptions(), \opt);

        list<auto> args;
        string sql = getSelectSqlStringIntern(sh, \args, opt);

        if (opt.sqlarg_callback)
            opt.sqlarg_callback(sql, args);

        bool rollback_on_error = False;
        if (sh.forupdate) {
            rollback_on_error = !ds.currentThreadInTransaction();
            if (rollback_on_error)
                ds.beginTransaction();
        }
        on_error if (rollback_on_error)
            ds.rollback();

        return ds.vselect(sql, args);
    }

    #! returns the SQL string to be executed corresponding to the argument hash with an output parameter for the select bind arguments
    /** @par Example:
        @code{.py}
list<auto> args;
string sql = table.getSelectSql(sh, \args);
        @endcode

        @param sh a hash of conditions for the select statement; see @ref select_option_hash "select option hash" for information about this argument
        @param args an optional reference to a list for any bind arguments

        @return the SQL that can be used to execute a select on the database corresponding to the arguments; the select bind arguments are returned in an output parameter

        @throw OPTION-ERROR invalid or unsupported select option
        @throw SELECT-ERROR \c 'offset' supplied without \c 'orderby' or \c 'limit', \c 'orderby' with \c 'limit' and \c 'offset' does not match any unique constraint

        @note
        - the select string is specific to the arguments passed (for example @ref null arguments will have clauses like 'is null')
        - if \c "offset" is supplied and no \c "orderby" is supplied, then if any primary key exists, the primary key columns will be used for the \c "orderby" option automatically
    */
    string getSelectSql(*hash<auto> sh, *reference<list<auto>> args) {
        validateOptionsIntern("OPTION-ERROR", getSelectOptions(), \sh);
        args = ();
        return getSelectSqlStringIntern(sh, \args);
    }

    bool testMatchAnyUnique(softlist<auto> cols) {
        if (matchAnyUnique(cols)) {
            return True;
        }
        foreach AbstractIndex ix in (indexes.iterator()) {
            if (ix.unique && ix.columns.partialMatchKeys(cols)) {
                return True;
            }
        }
        return False;
    }

    *AbstractUniqueConstraint matchAnyUnique(softlist<auto> cols) {
        if (!manual) {
            getPrimaryKeyUnlocked();
        }

        if (!primaryKey.empty() && primaryKey.partialMatchKeys(cols)) {
            return primaryKey;
        }

        if (!manual) {
            getConstraintsUnlocked();
        }

        foreach AbstractConstraint c in (constraints.iterator()) {
            if (c instanceof AbstractUniqueConstraint) {
                AbstractUniqueConstraint uk = c;
                if (uk.partialMatchKeys(cols))
                    return uk;
            }
        }
    }

    private string getSelectSqlStringIntern(*hash<auto> qh, reference<list<auto>> args, *hash<auto> opt) {
        if (!columns) {
            l.lock();
            on_exit l.unlock();
            getColumnsUnlocked();
        }

        # we first get column & table info in case we are referencing a table through a synonym
        # so the getSelectSqlName() will refer to the proper schema owner
        return getSelectSqlStringIntern(qh, getSelectSqlName(qh), \args, NOTHING, opt);
    }

    # column & table information must be retrieved before calling this method
    private string getSelectSqlStringIntern(*hash<auto> qh, string from, reference<list<auto>> args,
            *hash<string, bool> subquery_column_map, *hash<auto> opt, *hash<auto> pseudo_column_map) {
        hash<QueryInfo> info = <QueryInfo>{
            "table": self,
            # make sure query_hash stays "hash<auto>"
            "query_hash": {} + qh,
            "subquery_column_map": subquery_column_map,
            "query_options": opt,
            "pseudo_column_map": pseudo_column_map ?? ({} + getPseudoColumnHash()),
            "expression_map": getExpressionMap(),

            # backwards-compatibility
            "where_operator_map": getWhereOperatorMap(),
            "column_operator_map": getColumnOperatorMap(),
            "args": args ?? (),
        };
        on_success args = info.args;

        if (exists info.query_hash.offset) {
            if (!info.query_hash.orderby) {
                # use primary key by default
                getPrimaryKeyUnlocked();
                if (!primaryKey.empty()) {
                    info.query_hash.orderby = primaryKey.keys();
                } else {
                    throw "SELECT-ERROR", sprintf("%s: select 'offset' supplied (%d) but no 'orderby' and the table "
                        "has no primary key", getDesc(), info.query_hash.offset);
                }
            }
            if (!info.query_hash.limit) {
                throw "SELECT-ERROR", sprintf("%s: select 'offset' supplied (%d) without limit (%y)", getDesc(),
                    info.query_hash.offset, info.query_hash{"offset", "limit", "orderby"});
            }
        }
        # do not overwrite args; could be used in a subquery
        if (!exists args) {
            args = ();
        }

        # resolve any table names in joins first
        map info.query_hash.join{$1.key}.table = getSubtableFromString($1.value.table, opt),
            info.query_hash.join.pairIterator(), $1.value.table.typeCode() == NT_STRING;

        # make a hash of aliases to tables when using joins
        info.join_map = cast<hash<string, AbstractTable>>(
            map {$1.alias ?? $1.table.getName() : $1.table}, info.query_hash.join.iterator()
        );

        # issue #1909: add an alias for the main table if present
        if (info.query_hash.alias) {
            if (info.join_map{info.query_hash.alias}) {
                throw "SELECT-ERROR", sprintf("alias %y provided for the main table is also used in a join clause; "
                    "known aliases in joins: %y", info.query_hash.alias,
                    (map {$1.key: $1.value.getName()}, info.join_map.pairIterator()));
            }
            info.join_map{info.query_hash.alias} = self;
        }

        # if we have a "superquery" argument, then we need to track which columns are in this subquery in
        # subquery_column_map

        string cstr;
        # make query
        list<auto> coll = ();
        if (info.query_hash.columns) {
            # column expressions
            list<auto> ce = ();
            foreach auto cv in (info.query_hash.columns) {
                switch (cv.typeCode()) {
                    case NT_STRING: {
                        string cn = getColumnNameIntern(info, cv);
                        if (info.query_hash.superquery) {
                            subquery_column_map{cv} = True;
                        }
                        ce += cn;
                        break;
                    }

                    case NT_HASH: {
                        if ((cv instanceof hash<DataProviderExpression>)
                            || (cv instanceof hash<DataProviderFieldReference>)) {
                            ce += getExpressionArg(\info, ER_Field, cv, AbstractDataProviderTypeMap."any");
                        } else {
                            if (!cv.cop)
                                throw "SELECT-ERROR", sprintf("%s: missing column operator in \"columns\" hash element "
                                    "%d/%d; the \"columns\" select option must be assigned to a list of strings giving "
                                    "the column names or a list of column operator hashes as returned from column "
                                    "operator functions; this element is missing hash key \"cop\" giving the column "
                                    "operator; keys provided: %y", getDesc(), $# + 1, info.query_hash.columns.lsize(),
                                    cv.keys());
                            try {
                                string val = doColumnOperatorIntern(\info, cv);
                                ce += val;
                            } catch (hash<ExceptionInfo> ex) {
                                # rethrow exception with additional contextual information
                                rethrow ex.err, sprintf("%s: %s: in \"columns\" hash element %d/%d: %s", get_ex_pos(ex),
                                    getName(), $# + 1, info.query_hash.columns.lsize(), ex.desc), ex.arg;
                            }
                            if (info.query_hash.superquery && cv.cop == COP_AS && cv.arg) {
                                subquery_column_map{cv.arg} = True;
                            }
                        }
                        break;
                    }

                    default:
                        throw "SELECT-ERROR", sprintf("%s: don't know how to parse argument type %y (value: %y) in "
                            "column list, expecting \"string\" or \"hash\"", getDesc(), cv.type(), cv);
                }
            }
            cstr = foldl $1 + ", " + $2, ce.iterator();
            coll = ce;
        } else {
            cstr = "*";
        }

        string sql = sprintf("select %s from %s", cstr, getFromIntern(from, info.query_hash));
        if (info.query_hash.alias) {
            sql += " " + info.query_hash.alias;
        }

        # process join specifications
        foreach auto js in (info.query_hash.join.iterator()) {
            hash jh;
            if (js.typeCode() == NT_HASH)
                jh = js;
            else if (js instanceof AbstractTable)
                jh.table = js;
            else if (js instanceof Table)
                jh.table = js.getTable();
            else
                throw "SELECT-ERROR", sprintf("%s: don't know how to process join element of type %y; expecting a "
                    "join hash, an AbstractTable object, or a Table object", getDesc(), js.type());

            AbstractTable t = jh.table;
            t.describe();

            string an = jh.alias ? jh.alias : t.getSqlName();

            AbstractTable jt;
            if (jh.ta) {
                if (!info.join_map{jh.ta})
                    throw "SELECT-ERROR", sprintf("%s: can't join with unknown table/alias %y; known tables/aliases: "
                        "%y", getDesc(), jh.ta, (getName(),) + keys info.join_map);
                jt = info.join_map{jh.ta};
            } else {
                jt = self;
            }

            # set join table name
            string jtn;

            if (jt != self) {
                if (!jt.primaryKey) {
                    jt.getPrimaryKey();
                }
                if (!jt.foreignConstraints) {
                    jt.getForeignConstraints();
                }
                jtn = jh.ta;
            } else {
                if (!primaryKey) {
                    getPrimaryKey();
                }
                if (!foreignConstraints) {
                    getForeignConstraints();
                }
                jtn = info.query_hash.alias ?? getSqlName();
            }

            # create "join on" clause
            # the join clause expression list
            list<string> jel = ();
            if (jh.jcols) {
                if (jh.jcols.typeCode() != NT_HASH)
                    throw "SELECT-ERROR", sprintf("%s: join clause error; expecting a hash assigned to the \"jcols\" "
                        "key; got %y instead", getDesc(), jh.jcols.type());
                HashIterator i(jh.jcols);
                while (i.next()) {
                    string key = i.getKey();
                    string value = i.getValue();
                    if (!jt.columns.hasKey(key))
                        throw "SELECT-ERROR", sprintf("%s: cannot join on unknown column %y.%y; known columns: %y",
                            getDesc(), jt.getSqlName(), key, jt.columns.keys());
                    if (!t.describe().hasKey(value))
                        throw "SELECT-ERROR", sprintf("%s: cannot join on unknown column %y.%y; known columns: %y",
                            getDesc(), t.getSqlName(), value, t.columns.keys());
                    jel += sprintf("%s.%s = %s.%s", jtn, key, an, value);
                }
            } else {
                *hash fkh;
                if (!jt.primaryKey.empty()) {
                    # see if there is a foreign constraint from table -> pk of self
                    fkh = t.getForeignConstraints().findConstraintOn(jt.name, jt.primaryKey.keys());
                    if (fkh) {
                        AbstractIterator pki = jt.primaryKey.keyIterator();
                        AbstractIterator fki = fkh.keyIterator();
                        while (pki.next() && fki.next())
                            jel += sprintf("%s.%s = %s.%s", jtn, pki.getValue(), an, fki.getValue());
                    }
                }
                if (!fkh) {
                    # see if there is a foreign constraint in self -> pk of table
                    if (!jt.foreignConstraints.empty()) {
                        AbstractPrimaryKey pk = t.getPrimaryKey();
                        if (!pk.empty()) {
                            fkh = jt.foreignConstraints.findConstraintOn(t.getName(), pk.keys());
                            if (fkh) {
                                AbstractIterator fki = fkh.keyIterator();
                                AbstractIterator pki = pk.keyIterator();
                                while (pki.next() && fki.next())
                                    jel += sprintf("%s.%s = %s.%s", jtn, fki.getValue(), an, pki.getValue());
                            }
                        }
                    }

                    if (!fkh)
                        throw "SELECT-ERROR", sprintf("%s: cannot perform a %s join from %s on %s without a column "
                            "specification since neither table has a foreign constraint on the primary key of the "
                            "other table", getDesc(), jh.jop, jt.getSqlName(), t.getSqlName());
                }
            }

            # process any additional join conditions
            if (jh.cond) {
                hash<QueryInfo> join_info = <QueryInfo>{
                    "pseudo_column_map": info.pseudo_column_map,
                    # include reference data
                    "expression_map": info.expression_map,

                    # backwards-compatibility
                    "where_operator_map": info.where_operator_map,
                    "column_operator_map": info.column_operator_map,

                    "args": info.args,
                };
                on_success info.args = join_info.args;
                jel += jt.getWhereClauseListIntern(\join_info, jh.cond, an);
            }

            sql += sprintf(" %s join %s%s on (%s)", JopMap{jh.jop}, t.getSelectSqlName(jh.opt),
                jh.alias ? (" " + jh.alias) : "", (foldl $1 + " and " + $2, jel));
        }

        getSelectWhereSqlIntern(\info, \sql);

        if (info.query_hash.groupby) {
            list ce = getGroupByListUnlocked(info, coll);
            sql += " group by " + (foldl $1 + ", " + $2, ce.iterator());
        }

        if (info.query_hash.having) {
            list<auto> hl = ();

            HashIterator i(info.query_hash.having);
            while (i.next()) {
                string key = i.getKey();
                auto value = i.getValue();
                if (value.typeCode() != NT_LIST) {
                    throw "SELECT-ERROR", sprintf("%s: expecting a list<auto> argument to the \"having\" condition "
                        "for column %y; got type %y instead (value: %y)", getDesc(), key, value.type(), value);
                }
                if (value[0].typeCode() != NT_STRING) {
                    throw "SELECT-ERROR", sprintf("%s: expecting a string argument to the \"having\" condition in "
                        "the first position giving the column operator code for column %y; got type %y instead "
                        "(value: %y)", getDesc(), key, value[0].type(), value[0]);
                }
                *hash<auto> psch_copy = info.pseudo_column_map;
                string hstr = doColumnOperatorIntern(\info, value[0], NOTHING, getColumnExpressionIntern(\info, key));
                if (psch_copy != info.pseudo_column_map) {
                    throw "HAVING-ERROR", sprintf("%s: cannot use column operator functions in the having clause "
                        "with column alias specifications", getDesc());
                }
                hl += doWhereExpressionIntern(\info, hstr, value[1]);
            }
            if (hl) {
                sql += " having " + (foldl $1 + " and " + $2, hl);
            }
        }

        if (info.query_hash.orderby) {
            # process minuses to generate the "orderbydesc" keys
            foreach auto k in (\info.query_hash.orderby) {
                if (k.typeCode() == NT_STRING && k =~ /^-/) {
                    k = k.substr(1);
                    info.query_hash.orderbydesc{k} = True;
                }
            }
            if (info.query_hash.offset || info.query_hash.limit) {
                if (!info.query_hash.orderby) {
                    # use primary key by default
                    getPrimaryKeyUnlocked();
                    if (!primaryKey.empty()) {
                        info.query_hash.orderby = primaryKey.keys();
                    } else {
                        throw "SELECT-ERROR", sprintf("%s: select %y supplied but no 'orderby' and the "
                            "table has no primary key", getDesc(), info.query_hash{"offset", "limit"});
                    }
                } else if (!testMatchAnyUnique(info.query_hash.orderby)) {
                    throw "SELECT-ERROR", sprintf("%s: 'orderby %y' with 'offset %d', 'limit %d' does not match "
                        "any unique constraint", getDesc(), info.query_hash.orderby, info.query_hash.offset,
                        info.query_hash.limit);
                }

                doSelectOrderByWithOffsetSqlInternImpl(\info, \sql, coll);
            } else {
                doSelectOrderBySqlIntern(info, \sql, coll);
            }
        } else if (info.query_hash.limit) {
            doSelectLimitOnlyInternImpl(\info, \sql);
        }

        #printf("SQL: %s\nargs: %y\n", sql, args);

        if (info.query_hash.forupdate && !info.query_hash.superquery) {
            doForUpdate(\sql);
        }

        # issue #4206: splice comments and hints into the SQL query string last
        if (info.query_hash.comment || info.query_hash.hint) {
            string comment;
            if (info.query_hash.comment) {
                comment = sprintf("/* %s */ ", replace(info.query_hash.comment, "*/", ""));
            }
            if (info.query_hash.hint) {
                comment += sprintf("/*+ %s */ ", replace(info.query_hash.hint, "*/", ""));
            }
            # splice comment / hint test right before the " from " text in the query
            int i = sql.find(" from ");
            splice sql, i + 1, 0, comment;
        }

        if (info.query_hash.superquery) {
            validateOptionsIntern("OPTION-ERROR", getSelectOptions(), \info.query_hash.superquery);
            return getSelectSqlStringIntern(info.query_hash.superquery, sprintf("(%s) subquery", sql), \info.args,
                subquery_column_map, opt, (map {$1: True}, keys info.pseudo_column_map));
        }

        return sql;
    }

    #! Returns the given table from the argument, using any \c "tablecode" option if present
    AbstractTable getSubtableFromString(string table, *hash<auto> opt) {
        return opt.tablecode ? opt.tablecode(ds, table) : new Table(ds, table).getTable();
    }

    string getExpressionArg(reference<hash<QueryInfo>> info, int role, auto arg,
            AbstractDataProviderType expected_type) {
        if (arg instanceof hash<DataProviderFieldReference>) {
            # FIXME: check type
            return getColumnNameIntern(info, arg.field);
        }
        if (arg instanceof hash<DataProviderExpression>) {
            if (!arg.exp) {
                throw "EXPRESSION-ERROR", sprintf("expression %y is invalid; missing the \"exp\" key", arg);
            }
            *hash<auto> exp = info.expression_map{arg.exp} ?? info.expression_map{OperatorAliasMap{arg.exp}};
            if (!exp) {
                throw "EXPRESSION-ERROR", sprintf("expression %y does not have an implementation in module %y; known "
                    "expressions: %y", arg.exp, self.className(), keys info.expression_map);
            }
            on_error rethrow $1.err, sprintf("%s (while processing expression %y)", $1.desc, arg.exp);
            AbstractDataProvider::verifyExpression(role, exp.exp, LC_All, expected_type,
                cast<hash<DataProviderExpression>>(arg));
            return exp.code(\info, role, arg.args);
        }

        info.args += arg;
        return "%v";
    }

    private string getFromIntern(string from, *hash<auto> qh) {
        return from;
    }

    private list<auto> getGroupByListUnlocked(hash<QueryInfo> info, list<auto> coll) {
        return getGroupOrderByListIntern(info, "groupby", coll);
    }

    private list<auto> getOrderByListIntern(hash<QueryInfo> info, list<auto> coll) {
        return getGroupOrderByListIntern(info, "orderby", coll);
    }

    private list<auto> getGroupOrderByListIntern(hash<QueryInfo> info, string key, list<auto> coll) {
        list<auto> ce = ();

        code do_int = sub (int cv) {
            if (!coll) {
                if (cv > columns.size())
                    throw "SELECT-ERROR", sprintf("%s %d: %s does not support numeric column offsets for the %s "
                        "expression, and the table has %d column%s", key, cv, ds.getDriverName(), key, columns.size(),
                        columns.size() == 1 ? "" : "s");
                ce += columns.keys()[cv - 1];
            } else {
                if (cv > coll.size())
                    throw "SELECT-ERROR", sprintf("%s %d: %s does not support numeric column offsets for the %s "
                        "expression, and %d column%s %s given in the select argument", key, cv, ds.getDriverName(),
                        key, coll.size(), coll.size() == 1 ? "" : "s", coll.size() == 1 ? "was" : "were");
                string val = coll[cv - 1];
                val =~ s/ as .*$//;
                ce += val;
            }
        };

        foreach auto cv in (info.query_hash{key}) {
            switch (cv.typeCode()) {
                case NT_STRING: {
                    if (cv.toInt() == cv)
                        do_int(cv.toInt());
                    else
                        ce += getColumnNameIntern(info, cv);
                    break;
                }

                case NT_HASH: {
                    ce += doColumnOperatorIntern(\info, cv);
                    break;
                }

                case NT_INT: {
                    do_int(cv);
                    break;
                }

                default:
                    throw "SELECT-ERROR", sprintf("%s: don't know how to parse argument type %y (value: %y) in %s "
                        "list, expecting \"string\" or \"hash\"", getDesc(), cv.type(), cv, key);
            }
        }
        return ce;
    }

    private doForUpdate(reference<string> sql) {
        sql += " for update";
    }

    private string getSelectSqlName(*hash<auto> qh) {
        return getSqlName();
    }

    private string getColumnExpressionIntern(reference<hash<QueryInfo>> info, auto cvc) {
        switch (cvc.typeCode()) {
            case NT_HASH:
                return doColumnOperatorIntern(\info, cvc);
            case NT_STRING:
                return getColumnNameIntern(info, cvc);
        }
        throw "SELECT-ERROR", sprintf("%s: column operator hash %y has an invalid \"column\" key; expecting a string "
            "column specification or a column operator description hash", getDesc(), cvc);
    }

    private string doColumnOperatorIntern(reference<hash<QueryInfo>> info, hash<auto> cvc) {
        return doColumnOperatorIntern(\info, cvc.cop, cvc.arg,
            (cvc.column ? getColumnExpressionIntern(\info, cvc.column) : NOTHING));
    }

    private string doColumnOperatorIntern(reference<hash<QueryInfo>> info, auto cop, auto arg, *string cve) {
        if (cop.typeCode() != NT_STRING) {
            throw "SELECT-ERROR", sprintf("%s: invalid column operator code %y; use column operator functions to "
                "create valid column specifications with supported column operators", getDesc(), cop);
        }

        *hash<auto> cmd = info.column_operator_map{cop};
        if (!cmd) {
            throw "SELECT-ERROR", sprintf("%s: unknown column operator %y: expecting one of: %y", getDesc(), cop,
                keys info.column_operator_map);
        }

        if (cmd.arg) {
            string at = arg.type();
            switch (cmd.arg) {
                default: {
                    if (at != cmd.arg)
                        throw "SELECT-ERROR", sprintf("%s: invalid argument to column operator %y; got type %y, "
                            "expecting %y", getDesc(), cop, at, cmd.arg);
                }
            }
        }

        if (cmd.sqlvalue) {
            arg = getSqlValue(arg);
        } else if (cmd.argcolumn) {
            if (!cmd.argoptional || arg) {
                # issue #2555: allow the use of literal with other column operators
                if (arg.cop == COP_VALUE) {
                    arg = arg.arg.toString();
                } else {
                    if (arg.typeCode() != NT_STRING) {
                        throw "SELECT-ERROR", sprintf("%s: expecting a string column designation for column "
                            "operator %y; got type %y instead (value: %y)", getDesc(), cop, arg.type(), arg);
                    }
                    arg = getColumnNameIntern(info, arg);
                }
            }
        } else if (cmd.columnargs) {
            if (arg.args.typeCode() != NT_LIST) {
                throw "SELECT-ERROR", sprintf("%s: expecting a list of column designators for column operator %y; got "
                    "type %y instead", getDesc(), cop, arg.args.type());
            }
            foreach auto narg in (\arg.args) {
                switch (narg.typeCode()) {
                    case NT_STRING: narg = getColumnNameIntern(info, narg); break;
                    case NT_HASH: narg = doColumnOperatorIntern(\info, narg); break;
                    # HACK: for COP_OVER. We need to handle situation when arg can be NOTHING
                    #       but also we have to validate non-NOTHING args to real columns.
                    case NT_NOTHING: if (cmd.columnargs_ignore_nothings) continue;
                    default: throw "COLUMN-ERROR", sprintf("%s: expecting a string column designator or a column "
                        "operator hash; got %y in position %d/%d instead", getDesc(), narg, $#, arg.args.lsize());
                }
            }
        }

        if (!exists cve && !cmd.nocolumn && !cmd.columnargs) {
            throw "SELECT-ERROR", sprintf("%s: column operator %y requires a column argument name but none was "
                "provided", getDesc(), cop);
        }

        if (cmd.withalias) {
            # get escaped AS alias only when required (a reserved word)
            switch (arg.typeCode()) {
                case NT_STRING: arg = getColumnSqlName(arg); break;
                case NT_HASH: arg.as = getColumnSqlName(arg.as); break;
            }
            return cmd.code(cve, arg, \info.pseudo_column_map);
        } else {
            return cmd.code(cve, arg);
        }
    }

    private string getColumnNameIntern(hash<QueryInfo> info, string column_name) {
        # remove any disposable unique prefix
        column_name =~ s/^[0-9]+://;
        # return the name if it's a "technical name" (starts with "_")
        if (column_name =~ /^_/) {
            return column_name;
        }

        bool use_prefix = info.join_map.toBool();

        # see if we have a table.column spec
        *string table_prefix = (column_name =~ x/(\w+)\..+/)[0];
        if (table_prefix) {
            if (!info.join_map{table_prefix}) {
                throw "COLUMN-ERROR", sprintf("%s: query references unknown alias %y in %y; known aliases in this "
                    "query: %y", getDesc(), table_prefix, column_name, keys info.join_map);
            }
            # remove table alias from column name
            splice column_name, 0, table_prefix.size() + 1;

            if (column_name != "*" && !info.join_map{table_prefix}.describe().hasKey(column_name)
                && !info.pseudo_column_map{column_name})
                throw "COLUMN-ERROR", sprintf("%s: query references unknown column \"%s.%s\" (%y is aliased to "
                    "table %y); known columns: %y", getDesc(), table_prefix, column_name, table_prefix,
                    info.join_map{table_prefix}.getSqlName(),
                    keys info.join_map{table_prefix}.describe());
        } else if (info.pseudo_column_map{column_name}) {
            if (info.pseudo_column_map{column_name}.typeCode() == NT_STRING) {
                # substitute aliases with their actual definition
                column_name = info.pseudo_column_map{column_name};
            }
            # issue #4214: use table prefixes in a join for pseudocolumns
            return use_prefix ? sprintf("%s.%s", getSqlName(), column_name) : column_name;
        } else if (info.subquery_column_map) {
            if (!info.subquery_column_map{column_name} && !info.pseudo_column_map{column_name}) {
                throw "COLUMN-ERROR", sprintf("%s: query references unknown column %y; known columns from subquery: "
                    "%y", getDesc(), column_name, keys info.subquery_column_map);
            }
        } else {
            table_prefix = getSqlName();
            if (column_name == "*") {
                use_prefix = asteriskRequiresPrefix();
            } else if (!columns.hasKey(column_name) && !info.pseudo_column_map{column_name}) {
                throw "COLUMN-ERROR", sprintf("%s: query references unknown column \"%s.%s\"; known columns: %y; "
                    "known column aliases: %y", getDesc(), table_prefix, column_name, columns.keys(),
                    keys info.pseudo_column_map);
            }
        }

        # ensure that reserved words are properly quoted
        column_name = getColumnSqlName(column_name);

        return use_prefix ? sprintf("%s.%s", table_prefix, column_name) : column_name;
    }

    #! returns @ref True if the database requires a wildcard \c "*" to be prefixed with the table name when it appears with other column arguments in a select statement
    private bool asteriskRequiresPrefix() {
        return False;
    }

    private getSelectWhereSqlIntern(reference<hash<QueryInfo>> info, reference<string> sql) {
        if (info.query_hash."where") {
            sql += getWhereClauseIntern(\info, info.query_hash."where");
        }
    }

    private *string getWhereClause(*hash<auto> cond, reference<list<auto>> args, *string cprefix,
            *hash<string, AbstractTable> join_map) {
        hash<QueryInfo> info = <QueryInfo>{
            "table": self,
            "query_hash": {
                "where": cond,
            },
            "args": args ?? (),

            "join_map": join_map,
            "pseudo_column_map": {} + getPseudoColumnHash(),
            "expression_map": getExpressionMap(),

            # backwards-compatibility
            "where_operator_map": getWhereOperatorMap(),
            "column_operator_map": getColumnOperatorMap(),
        };
        on_success args = info.args;
        return getWhereClauseIntern(\info, info.query_hash."where", cprefix);
    }

    private *string getWhereExpressionIntern(reference<hash<QueryInfo>> info, hash<DataProviderExpression> cond,
            *string cprefix) {
        string sql = getExpressionIntern(\info, ER_Search, cond, AbstractDataProviderTypeMap."bool");
        if (sql) {
            return " where " + sql;
        }
    }

    private *string getExpressionIntern(reference<hash<QueryInfo>> info, int role,
            hash<DataProviderExpression> cond, AbstractDataProviderType expected_type) {
        *hash<auto> exp = info.expression_map{cond.exp};
        if (!exp) {
            throw "SQLUTIL-EXPRESSION-ERROR", sprintf("expression references unknown expression %y; supported "
                "expressions: %y", cond.exp, keys info.expression_map);
        }

        AbstractDataProvider::verifyExpression(role, exp.exp, LC_All, expected_type, cond);
        return exp.code(\info, role, cond.args);
    }

    private *string getWhereClauseIntern(reference<hash<QueryInfo>> info, *hash<auto> cond, *string cprefix) {
        if (!cond) {
            return;
        }
        if (!columns) {
            l.lock();
            on_exit l.unlock();
            getColumnsUnlocked();
        }
        if (cond instanceof hash<DataProviderExpression>) {
            return getWhereExpressionIntern(\info, cast<hash<DataProviderExpression>>(cond), cprefix);
        }
        if (!exists info.args) {
            info.args = ();
        }
        *list<string> wl;
        # for backwards-compatibility, return an "or" clause of all the "and" clauses for a list of hashes
        if (cond instanceof list) {
            foreach auto c in (cond) {
                *list<string> owl;
                if (c.typeCode() != NT_HASH) {
                    throw "WHERE-ERROR", sprintf("expected \"hash\" in \"where\" list; got type %y instead",
                        c.fullType());
                }
                owl = getWhereClauseListIntern(\info, c, cprefix);
                if (owl) {
                    wl += "(" + (foldl $1 + " and " + $2, owl) + ")";
                }
            }
            if (wl) {
                return " where " + (foldl $1 + " or " + $2, wl);
            }
        } else {
            wl = getWhereClauseListIntern(\info, cond, cprefix);
            if (wl) {
                return " where " + (foldl $1 + " and " + $2, wl);
            }
        }
    }

    private *list<string> getWhereClauseListIntern(reference<hash<QueryInfo>> info, *hash<auto> cond, *string cprefix) {
        if (!cond) {
            return;
        }

        return
            map doWhereExpressionIntern(\info, cprefix
                ? sprintf("%s.%s", cprefix, $1.key)
                : getColumnNameIntern(info, $1.key), $1.value),
            cond.pairIterator();
    }

    private string doWhereExpressionIntern(reference<hash<QueryInfo>> info, string cn, auto we) {
        int wt = we.typeCode();
        if (wt == NT_HASH) {
            if (we.hasKey("cop")) {
                *hash<auto> psm_copy = info.pseudo_column_map;
                string val = doColumnOperatorIntern(\info, we);
                if (psm_copy != info.pseudo_column_map) {
                    throw "WHERE-ERROR", sprintf("%s: cannot use column operator function %y in the where clause "
                        "with column alias specifications", getDesc(), we.cop);
                }
                return sprintf("%s = %s", cn, val);
            }

            if (!we.op) {
                throw "WHERE-ERROR", sprintf("%s: argument to key %y is a hash without an \"op\" key: %y", getDesc(),
                    cn, we);
            }

            *hash<auto> cmd = info.where_operator_map{we.op};
            if (!cmd) {
                throw "WHERE-ERROR", sprintf("%s: operator hash for key %y has unknown operator %y: expecting one "
                    "of: %y (operator hash: %y)", getDesc(), cn, we.op, keys info.where_operator_map, we);
            }

            if (cmd.recursive) {
                cn = doWhereExpressionIntern(\info, cn, we.arg);
                delete we.arg;
            } else if (cmd.argcolumn) {
                if (!we.arg || we.arg.typeCode() != NT_STRING)
                    throw "WHERE-ERROR", sprintf("%s: operator hash for key %y expects a string column name; got "
                        "type %y instead", getDesc(), cn, we.arg.type());
                we.arg = getColumnNameIntern(info, we.arg);
            }

            return cmd.code(\info, cn, we.arg);
        }
        if (we === NULL || !exists we) {
            return sprintf("%s is null", cn);
        }
        info.args += we;
        return sprintf("%s = %v", cn);
    }

    string getOrClause(reference<hash<QueryInfo>> info, list<auto> arglist) {
        list<string> l = ();
        foreach hash<auto> h in (arglist) {
            string str = getOrClause(\info, h);
            if (str) {
                l += str;
            }
        }
        return l ? ("(" + (foldl $1 + " or " + $2, l) + ")") : "";
    }

    string getOrClause(reference<hash<QueryInfo>> info, hash<auto> arg) {
        *list<string> wl = getWhereClauseListIntern(\info, arg);
        return wl ? ("(" + (foldl $1 + " and " + $2, wl) + ")") : "";
    }

    private doSelectOrderBySqlIntern(hash<QueryInfo> info, reference<string> sql, list<auto> coll) {
        list<auto> ce = getOrderByListIntern(info, coll);
        sql += " order by " + (foldl $1 + ", " + $2, (
            map $1 + (info.query_hash.desc || info.query_hash.orderbydesc.$1 ? " desc" : ""), ce
        ));
    }

    #! deletes rows in the table matching the condition and returns the count of rows deleted; the transaction is committed if successful, if an error occurs then it is rolled back
    /** @par Example:
        @code{.py}
int dcnt = table.delCommit(("name": name));
        @endcode

        @param cond a hash of conditions for the where clause; see @ref where_clauses for more information
        @param sql an optional reference to a string to return the SQL generated for the select statement
        @param opt optional SQL data operation callback options; see @ref AbstractTable::SqlDataCallbackOptions for
        more info

        @return the count of rows deleted

        @throw WHERE-ERROR unknown operator or invalid arguments given in the cond hash for the where clause

        @see delete(*hash)
        */
    int delCommit(hash<auto> cond, reference<string> sql, hash<auto> opt) {
        on_success ds.commit();
        on_error ds.rollback();
        return delIntern(cond, \sql, opt);
    }

    #! @ref SqlUtil::AbstractTable::delCommit() variant
    int delCommit(hash<auto> cond, hash<auto> opt) {
        on_success ds.commit();
        on_error ds.rollback();
        return delIntern(cond, NOTHING, opt);
    }

    #! @ref SqlUtil::AbstractTable::delCommit() variant
    int delCommit(hash<auto> cond, reference<string> sql) {
        on_success ds.commit();
        on_error ds.rollback();
        return delIntern(cond, \sql);
    }

    #! @ref SqlUtil::AbstractTable::delCommit() variant
    int delCommit(hash<auto> cond) {
        on_success ds.commit();
        on_error ds.rollback();
        return delIntern(cond);
    }

    #! @ref SqlUtil::AbstractTable::delCommit() variant
    int delCommit() {
        on_success ds.commit();
        on_error ds.rollback();

        return delIntern();
    }

    #! deletes rows in the table matching the condition and returns the count of rows deleted; no transaction management is performed with this method
    /** @par Example:
        @code{.py}
int dcnt = table.del(("name": name));
        @endcode

        @param cond a hash of conditions for the where clause; see @ref where_clauses for more information
        @param sql an optional reference to a string to return the SQL generated for the select statement
        @param opt optional SQL data operation callback options; see @ref AbstractTable::SqlDataCallbackOptions for more info

        @return the count of rows deleted

        @throw WHERE-ERROR unknown operator or invalid arguments given in the cond hash for the where clause

        @see delCommit(*hash)
    */
    int del(hash<auto> cond, reference<string> sql, hash<auto> opt) {
        return delIntern(cond, \sql, opt);
    }

    #! @ref SqlUtil::AbstractTable::del() variant
    int del(hash<auto> cond, hash<auto> opt) {
        return delIntern(cond, NOTHING, opt);
    }

    #! @ref SqlUtil::AbstractTable::del() variant
    int del(hash<auto> cond, reference<string> sql) {
        return delIntern(cond, \sql);
    }

    #! @ref SqlUtil::AbstractTable::del() variant
    int del(hash<auto> cond) {
        return delIntern(cond);
    }

    #! @ref SqlUtil::AbstractTable::del() variant
    int del() {
        return delIntern();
    }

    #! deletes rows in the table matching the condition and returns the count of rows deleted; no transaction management is performed with this method
    /** @par Example:
        @code{.py}
hash<SqlResultInfo> info = table.delWithInfo({"name": name});
        @endcode

        @param cond a hash of conditions for the where clause; see @ref where_clauses for more information
        @param opt optional SQL data operation callback options; see @ref AbstractTable::SqlDataCallbackOptions for more info

        @return a hash where the \c result key is the count of rows deleted

        @throw WHERE-ERROR unknown operator or invalid arguments given in the cond hash for the where clause

        @since SqlUtil 1.7
    */
    hash<SqlResultInfo> delWithInfo(hash<auto> cond, *hash<auto> opt) {
        hash<SqlResultInfo> rv();
        rv.result = delIntern(cond, \rv.sql, opt, \rv.args);
        return rv;
    }

    #! A legacy @ref SqlUtil::AbstractTable::del() wrapper
    deprecated int delNoCommit(*hash<auto> cond, *reference<string> sql) {
        return del(cond, \sql);
    }

    private int delIntern(*hash<auto> cond, *reference<string> sql, *hash<auto> opt, *reference<softlist<auto>> args) {
        # make query
        sql = sprintf("delete from %s", getSqlName());
        if (cond) {
            sql += getWhereClause(cond, \args);
        }

        return execData(opt, sql, args);
    }

    #! updates rows in the table matching an optional condition and returns the count of rows updated; the transaction is committed if successful, if an error occurs then it is rolled back
    /** @par Example:
        @code{.py}
int ucnt = table.updateCommit(("id": id), ("name": name));
        @endcode

        @param set the hash of values to set, key values are column names, hash values are the values to assign to
        those columns, field references (@ref DataProviderFieldReference), data provider expressions
        (@ref DataProvider::DataProviderExpressionInfo), or update operators (see @ref sql_uop_funcs)
        @param cond a hash of conditions for the where clause; see @ref where_clauses for more information
        @param sql an optional reference to a string to return the SQL generated for the select statement
        @param opt optional SQL data operation callback options; see @ref AbstractTable::SqlDataCallbackOptions for more info

        @return the count of rows updated

        @throw UPDATE-ERROR the set hash is empty
        @throw WHERE-ERROR unknown operator or invalid arguments given in the cond hash for the where clause

        @see update()
    */
    int updateCommit(hash<auto> set, hash<auto> cond, reference<string> sql, hash<auto> opt) {
        on_success ds.commit();
        on_error ds.rollback();
        return updateIntern(set, cond, \sql, opt);
    }

    #! A @ref SqlUtil::AbstractTable::updateCommit() variant
    int updateCommit(hash<auto> set, hash<auto> cond, reference<string> sql) {
        on_success ds.commit();
        on_error ds.rollback();
        return updateIntern(set, cond, \sql);
    }

    #! A @ref SqlUtil::AbstractTable::updateCommit() variant
    int updateCommit(hash<auto> set, hash<auto> cond, hash<auto> opt) {
        on_success ds.commit();
        on_error ds.rollback();
        return updateIntern(set, cond, NOTHING, opt);
    }

    #! A @ref SqlUtil::AbstractTable::updateCommit() variant
    int updateCommit(hash<auto> set, hash<auto> cond) {
        on_success ds.commit();
        on_error ds.rollback();
        return updateIntern(set, cond);
    }

    #! A @ref SqlUtil::AbstractTable::updateCommit() variant
    int updateCommit(hash<auto> set) {
        on_success ds.commit();
        on_error ds.rollback();
        return updateIntern(set);
    }

    #! updates rows in the table matching an optional condition and returns the count of rows updated; no transaction management is performed with this method
    /** @par Example:
        @code{.py}
int ucnt = table.update(("id": id), ("name": name));
        @endcode

        @param set the hash of values to set, key values are column names, hash values are the values to assign to
        those columns, field references (@ref DataProviderFieldReference), data provider expressions
        (@ref DataProvider::DataProviderExpressionInfo), or update operators (see @ref sql_uop_funcs)
        @param cond a hash of conditions for the where clause; see @ref where_clauses for more information
        @param sql an optional reference to a string to return the SQL generated for the select statement
        @param opt optional SQL data operation callback options; see @ref AbstractTable::SqlDataCallbackOptions for more info

        @return the count of rows updated

        @throw UPDATE-ERROR the set hash is empty
        @throw WHERE-ERROR unknown operator or invalid arguments given in the cond hash for the where clause

        @see updateCommit()
    */
    int update(hash<auto> set, hash<auto> cond, reference<string> sql, hash<auto> opt) {
        return updateIntern(set, cond, \sql, opt);
    }

    #! A @ref SqlUtil::AbstractTable::update() variant
    int update(hash<auto> set, hash<auto> cond, reference<string> sql) {
        return updateIntern(set, cond, \sql);
    }

    #! A @ref SqlUtil::AbstractTable::update() variant
    int update(hash<auto> set, hash<auto> cond, hash<auto> opt) {
        return updateIntern(set, cond, NOTHING, opt);
    }

    #! A @ref SqlUtil::AbstractTable::update() variant
    int update(hash<auto> set, hash<auto> cond) {
        return updateIntern(set, cond);
    }

    #! A @ref SqlUtil::AbstractTable::update() variant
    int update(hash<auto> set) {
        return updateIntern(set);
    }

    #! updates rows in the table matching an optional condition and returns an info hash with the count of rows updated; no transaction management is performed with this method
    /** @par Example:
        @code{.py}
hash<SqlResultInfo> info = table.updateWithInfo({"id": id}, {"name": name});
        @endcode

        @param set the hash of values to set, key values are column names, hash values are the values to assign to
        those columns, field references (@ref DataProviderFieldReference), data provider expressions
        (@ref DataProvider::DataProviderExpressionInfo), or update operators (see @ref sql_uop_funcs)
        @param cond a hash of conditions for the where clause; see @ref where_clauses for more information
        @param opt optional SQL data operation callback options; see @ref AbstractTable::SqlDataCallbackOptions for
        more info

        @return an info hash where the \c result key is assigned to the count of rows updated

        @throw UPDATE-ERROR the set hash is empty
        @throw WHERE-ERROR unknown operator or invalid arguments given in the cond hash for the where clause

        @since SqlUtil 1.7
    */
    hash<SqlResultInfo> updateWithInfo(hash<auto> set, hash<auto> cond, *hash<auto> opt) {
        hash<SqlResultInfo> rv();
        rv.result = updateIntern(set, cond, \rv.sql, opt, \rv.args);
        return rv;
    }

    #! Returns the SQL for the given update parameters
    /** @param set the hash of values to set, key values are column names, hash values are the values to assign to
        those columns, field references (@ref DataProviderFieldReference), data provider expressions
        (@ref DataProvider::DataProviderExpressionInfo), or update operators (see @ref sql_uop_funcs)

        @param cond the update condition

        @return a hash with the following keys:
        - \c sql: the SQL for the given update parameters
        - \c args: the arguments for the SQL statement

        @throw UPDATE-ERROR set hash is empty; invalid operators or update expressions
        @throw COLUMN-ERROR invalid column reference

        @since %SqlUtil 1.6
    */
    hash<SqlCommandInfo> getUpdateSql(hash<auto> set, *hash<auto> cond) {
        if (!set) {
            throw "UPDATE-ERROR", sprintf("%s: the set hash is empty", getDesc());
        }

        # make query
        string sql = sprintf("update %s set ", getSqlName());

        hash<QueryInfo> qinfo = <QueryInfo>{
            "table": self,
            "args": (),

            "query_hash": {},
            "pseudo_column_map": {} + getPseudoColumnHash(),
            "expression_map": getExpressionMap(),

            # backwards-compatibility
            "where_operator_map": getWhereOperatorMap(),
            "column_operator_map": getColumnOperatorMap(),
        };

        list<string> sl = ();
        HashIterator i(set);
        while (i.next()) {
            string key = i.getKey();
            auto value = i.getValue();
            string exp = sprintf("%s = ", getColumnSqlName(key));
            if (value instanceof hash<DataProviderFieldReference>) {
                exp += getColumnNameIntern(qinfo, value.field);
            } else if (value instanceof hash<DataProviderExpression>) {
                exp += getExpressionIntern(\qinfo, ER_Field, value, getColumnDataType(key));
            } else if (value.uop) {
                exp += getUpdateExpression(key, cast<hash<UpdateOperatorInfo>>(value));
            } else {
                exp += "%v";
                qinfo.args += value;
            }
            sl += exp;
        }

        sql += foldl $1 + ", " + $2, sl;

        if (cond) {
            sql += getWhereClauseIntern(\qinfo, cond);
        }

        return <SqlCommandInfo>{
            "sql": sql,
            "args": qinfo.args,
        };
    }

    #! A legacy @ref SqlUtil::AbstractTable::update() wrapper
    deprecated int updateNoCommit(hash<auto> set, *hash<auto> cond, *reference<string> sql) {
        return updateIntern(set, cond, \sql);
    }

    #! A legacy @ref SqlUtil::AbstractTable::update() wrapper
    deprecated int updateNoCommit(hash<auto> set, *hash<auto> cond, *hash<auto> opt) {
        return updateIntern(set, cond, NOTHING, opt);
    }

    private int updateIntern(hash<auto> set, *hash<auto> cond, *reference<string> sql, *hash<auto> opt,
            *reference<softlist<auto>> args) {
        hash<SqlCommandInfo> update_hash = getUpdateSql(set, cond);
        sql = update_hash.sql;
        args = update_hash.args;
        #printf("update_hash: %y\n", update_hash);
        return execData(opt, update_hash.sql, update_hash.args);
    }

    private string getUpdateExpression(string col, hash<UpdateOperatorInfo> uh) {
        hash um = getUpdateOperatorMap();

        *hash cmd = um{uh.uop};
        if (!cmd)
            throw "UPDATE-ERROR", sprintf("%s: unknown update operator %y in %y: expecting one of: %y", getDesc(),
                uh.uop, uh, um.keys());

        if (cmd.arg) {
            string at = uh.arg.type();
            switch (cmd.arg) {
                default: {
                    if (at != cmd.arg)
                        throw "UPDATE-ERROR", sprintf("%s: invalid argument to update operator %y; got type %y, "
                            "expecting %y", getDesc(), uh.uop, at, cmd.arg);
                }
            }
        }

        if (cmd.sqlvalue)
            uh.arg = getSqlValue(uh.arg);
        else if (cmd.argcolumn) {
            if (!cmd.argoptional || uh.arg) {
                if (uh.arg.typeCode() != NT_STRING) {
                    throw "UPDATE-ERROR", sprintf("%s: expecting a string column designation for update operator %y; "
                        "got type %y instead", getDesc(), uh.uop, uh.arg.type());
                }
                uh.arg = getColumnNameIntern(<QueryInfo>{}, uh.arg);
            }
        } else if (cmd.columnargs) {
            if (uh.arg.typeCode() != NT_LIST) {
                throw "UPDATE-ERROR", sprintf("%s: expecting a list of column designators for update operator %y; "
                    "got type %y instead", getDesc(), uh.uop, uh.arg.type());
            }
            foreach auto narg in (\uh.arg) {
                switch (narg.typeCode()) {
                    case NT_STRING: narg = getColumnNameIntern(<QueryInfo>{}, narg); break;
                    case NT_HASH: {
                        hash<QueryInfo> info();
                        narg = doColumnOperatorIntern(\info, narg);
                        break;
                    }
                    default: throw "COLUMN-ERROR", sprintf("%s: expecting a string column designator or a update "
                        "operator hash; got %y in position %d/%d instead", getDesc(), narg, $#, uh.arg.lsize());
                }
            }
        }

        string arg = (uh.nest ? getUpdateExpression(col, cast<hash<UpdateOperatorInfo>>(uh.nest)) : col);

        on_error printf("uh: %y\n", uh);
        return cmd.code(arg, uh.arg);
    }

    private bool emptyDataIntern() {
        list<auto> args;
        string sql = getSelectSql({"limit": 1}, \args);
        return boolean(ds.vselectRow(sql, args));
    }

    private Columns checkUpsertRow(hash<auto> row, reference<int> upsert_strategy) {
        return getAllUpsertColumns(row).firstValue();
    }

    private code<int(hash<auto>)> getUpsertInsertFirst(Columns cols, hash<auto> example_row, *hash<auto> opt) {
        string insert_sql = getUpsertInsertSql(example_row);
        list updc = ();
        string update_sql = getUpsertUpdateSql(example_row, cols, \updc, opt);

        return int sub (hash<auto> row) {
            if (tryInsertImpl(insert_sql, row))
                return UR_Inserted;

            tryUpdate(update_sql, row, cols, updc);
            return UR_Verified;
        };
    }

    private code<int(hash<auto>)> getUpsertUpdateFirst(Columns cols, hash<auto> example_row, *hash<auto> opt) {
        list updc = ();
        string update_sql = getUpsertUpdateSql(example_row, cols, \updc, opt);
        string insert_sql = getUpsertInsertSql(example_row);

        return int sub (hash<auto> row) {
            if (tryUpdate(update_sql, row, cols, updc))
                return UR_Verified;

            ds.vexec(insert_sql, row.values());
            return UR_Inserted;
        };
    }

    private code<int(hash<auto>)> getUpsertSelectFirst(Columns cols, hash<auto> example_row, *hash<auto> opt) {
        #printf("getUpsertSelectFirst() %s: %y\n", name, cols.keys());
        list<string> updc();
        string select_sql = getUpsertSelectSql(example_row, cols, \updc);
        string insert_sql = getUpsertInsertSql(example_row);
        string update_sql = getUpsertUpdateSql(example_row, cols, \updc, opt);

        return int sub (hash<auto> row) {
            list<auto> args = row.(cols.keys()).values();
            #printf("select_sql: %s args: %y\n", select_sql, args);
            *hash dbrow = ds.vselectRow(select_sql, args);
            if (!dbrow) {
                ds.vexec(insert_sql, row.values());
                return UR_Inserted;
            }
            if (dbrow == row{updc})
                return UR_Unchanged;

            #printf("dbrow: %y row{%y}: %y\n", dbrow, updc, row{updc});

            tryUpdate(update_sql, row, cols, updc);
            return UR_Updated;
        };
    }

    private code<int(hash<auto>)> getUpsertInsertOnly(Columns cols, hash<auto> example_row, *hash<auto> opt) {
        string insert_sql = getUpsertInsertSql(example_row);

        return int sub (hash<auto> row) {
            return tryInsertImpl(insert_sql, row) ? UR_Inserted : UR_Unchanged;
        };
    }

    private code<int(hash<auto>)> getUpsertUpdateOnly(Columns cols, hash<auto> example_row, *hash<auto> opt) {
        list updc = ();
        string update_sql = getUpsertUpdateSql(example_row, cols, \updc, opt);

        return int sub (hash<auto> row) {
            return tryUpdate(update_sql, row, cols, updc) ? UR_Verified : UR_Unchanged;
        };
    }

    private Columns getUpsertColumns(reference<string> csrc) {
        hash<string, Columns> rv = getAllUpsertColumns();
        csrc = rv.firstKey();
        return rv.firstValue();
    }

    #! Returns a hash with a single value
    /** @param row if given, a row to match a unique constraint or unique index

        @return a hash where the key is a description of the column set returned in the value;
        - if \a row is given, then this method returns a column set matching \a row
        - if \a row is not given, then this method returns the first unique constraint or unique index

        @throw UPSERT-ERROR either no unique constraint or index exists or none matching \a row exists
    */
    private hash<string, Columns> getAllUpsertColumns(*hash<auto> row) {
        if (!columns) {
            l.lock();
            on_exit l.unlock();
            getColumnsUnlocked();
        }
        if (!primaryKey) {
            getPrimaryKey();
        }

        if (!primaryKey.empty()) {
            if ((row && matchUniqueColumns(primaryKey, row)) || !row) {
                return {sprintf("primary key %s", primaryKey.getName()): primaryKey};
            }
        }

        # find unique constraints
        if (!constraints) {
            getConstraints();
        }
        foreach AbstractConstraint c in (constraints.iterator()) {
            if (c instanceof AbstractUniqueConstraint) {
                AbstractUniqueConstraint uc = cast<AbstractUniqueConstraint>(c);
                if ((row && matchUniqueColumns(uc, row)) || !row) {
                    return {sprintf("unique constraint %s", uc.getName()): uc};
                }
            }
        }

        # find unique indices
        if (!indexes) {
            getIndexes();
        }
        foreach AbstractIndex ix in (indexes.iterator()) {
            if (ix.unique && ((row && matchUniqueColumns(ix.columns, row)) || !row)) {
                return {sprintf("unique index %s", ix.name): ix.columns};
            }
        }

        throw "UPSERT-ERROR", sprintf("%s: cannot perform upsert because there is no primary key, unique "
            "constraint, or unique index", getDesc());
    }

    private bool matchUniqueColumns(Columns cols, hash<auto> row) {
        if (row.size() < cols.size()) {
            return False;
        }
        # check if all keys described in the unique column set are used in the template row
        if (row{cols.keys()}.size() == cols.size()) {
            # now check the rest of the keys in the row to ensure that they are valid column names
            if (*hash<auto> invalid_columns = row - columns.keys()) {
                throw "COLUMN-ERROR", sprintf("%s upsert row references invalid columns: %y (valid columns: %y)",
                    getDesc(), keys invalid_columns, columns.keys());
            }
            return True;
        }
        return False;
    }

    private string getUpsertSelectSql(hash<auto> row, Columns cols, reference<list<string>> updc) {
        if (cols.size() != row.size()) {
            # make list of columns not in unique key
            map updc += $1, keys row, !cols.hasKey($1);
        } else {
            updc = cols.keys();
        }

        return sprintf("select %s from %s where %s",
                        (foldl $1 + ", " + $2, getColumnSqlNames(updc)),
                        getSqlName(),
                        (foldl $1 + " and " + $2, (map sprintf("%s = %v", getColumnSqlName($1)), cols.keyIterator()))
            );
    }

    private string getUpsertInsertSql(hash<auto> row) {
        string sql = sprintf("insert into %s (", getSqlName());
        sql += (foldl $1 + "," + $2, (map getColumnSqlName($1), keys row));
        sql += ") values (";
        sql += (foldl $1 + "," + $2, (map "%v", keys row));
        sql += ")";
        return sql;
    }

    private string getUpsertUpdateSql(hash<auto> row, Columns cols, reference updc, *hash<auto> opt) {
        # make list of columns not in unique key if necessary
        if (!updc) {
            # hash of columns to skip on update if applicable
            *hash suh = map {$1: True}, opt.omit_update;
            map updc += $1, keys row, !cols.hasKey($1) && !suh.$1;
        }

        # make sql
        string sql = sprintf("update %s set ", getSqlName());
        sql += foldl $1 + ", " + $2, (map sprintf("%s = %v", getColumnSqlName($1)), updc);
        sql += " where ";
        sql += foldl $1 + " and " + $2, (map sprintf("%s = %v", getColumnSqlName($1)), cols.keyIterator());

        return sql;
    }

    private softbool tryUpdate(string sql, hash<auto> row, Columns cols, list updc) {
        list<auto> args = row{updc}.values() + row{cols.keys()}.values();
        return ds.vexec(sql, args);
    }

    private checkValue(string cname, string argname, reference val, string type) {
        if (val === NULL) {
            delete val;
            return;
        }

        switch (type) {
            case Type::Int: {
                if (!val.intp())
                    throw "COLUMN-ERROR", sprintf("%s column %y: %s has type %y, expecting %y (value: %y)", name, cname, argname, val.type(), type, val);
                if (val.type() != type)
                    val = val.toInt();
                break;
            }
            case Type::Float: {
                if (!val.intp())
                    throw "COLUMN-ERROR", sprintf("%s column %y: %s has type %y, expecting %y (value: %y)", getDesc(), cname, argname, val.type(), type, val);
                if (val.type() != type)
                    val = val.toFloat();
                break;
            }
            case Type::Number: {
                if (!val.intp())
                    throw "COLUMN-ERROR", sprintf("%s column %y: %s has type %y, expecting %y (value: %y)", getDesc(), cname, argname, val.type(), type, val);
                if (val.type() != type)
                    val = number(val);
                break;
            }
            case Type::Boolean: {
                if (!val.intp())
                    throw "COLUMN-ERROR", sprintf("%s column %y: %s has type %y, expecting %y (value: %y)", getDesc(), cname, argname, val.type(), type, val);
                if (val.type() != type)
                    val = val.toBool();
                break;
            }
            case Type::String: {
                if (!val.strp())
                    throw "COLUMN-ERROR", sprintf("%s column %y: %s has type %y, expecting %y (value: %y)", getDesc(), cname, argname, val.type(), type, val);
                if (val.type() != type)
                    val = val.toString();
                break;
            }
            default: {
                if (val.type() != type)
                    throw "COLUMN-ERROR", sprintf("%s column %y: %s value for has type %y, expecting %y (value: %y)", getDesc(), cname, argname, val.type(), type, val);
                break;
            }
        }
    }

    #! returns an SQL string corresponding to the list of commands in the argument
    /** @par Example:
        @code{.py}
string sql = t.getSqlFromList(list);
        @endcode

        @param l a list of SQL commands

        @return an SQL string corresponding to the list of commands in the argument
    */
    string getSqlFromList(list<auto> l) {
        return getCreateSqlImpl(l);
    }

    #! returns a string for use in SQL queries representing the DB-specific value of the argument
    /** @par Example:
        @code{.py}
string str = table.getSqlValue(date);
        @endcode

        @param v the value to convert to an SQL value

        @return a string representing the value usable in SQL strings for the given database driver

        @throw VALUE-ERROR the given value cannot be converted to an SQL string (missing rules for value)
        */
    string getSqlValue(auto v) {
        *string str = getSqlValueImpl(v);
        if (!exists str)
            throw "VALUE-ERROR", sprintf("%s: class %s cannot convert type %y to an SQL string (value: %y)", getDesc(), self.className(), v.type(), v);

        return str;
    }

    #! returns the name of the table
    string getName() {
        return name;
    }

    #! reads in all attributes of the table from the database
    /** @par Example:
        @code{.py}
table.cache();
        @endcode

        @param opts cache options; see @ref SqlUtil::AbstractTable::CacheOptions for common options; each driver can support additional driver-specific options
    */
    cache(*hash<auto> opts) {
        l.lock();
        on_exit l.unlock();
        cacheUnlocked(opts);
    }

    #! purges the current table definition
    /** @par Example:
        @code{.py}
table.clear();
        @endcode
    */
    clear() {
        l.lock();
        on_exit l.unlock();

        delete columns;
        delete primaryKey;
        delete foreignConstraints;
        delete constraints;
        delete indexes;
        delete triggers;

        clearImpl();
    }

    #! Returns a description hash of the table
    /** @return a description hash of the table

        @return currently this method does not return information for all supported elements; it returns only:
        - \c columns
        - \c primary_key
        - \c indexes
        - \c foreign_constraints

        @since %SqlUtil 1.7.5
    */
    hash<auto> getDescriptionHash() {
        hash<auto> rv = {};

        l.lock();
        on_exit l.unlock();

        # cache the table if necessary
        if (!manual) {
            cacheUnlocked();
        }

        # get column descriptions
        rv.columns = map {$1.key: $1.value.getDescriptionHash()}, columns.pairIterator();

        # add pk desc if available
        if (!primaryKey.empty()) {
            rv.primary_key = {
                "name": primaryKey.getName(),
                "columns": primaryKey.keys(),
            };
        }

        # add indexes
        if (indexes) {
            foreach hash<auto> i in (indexes.pairIterator()) {
                hash<auto> ix = {
                    "columns": i.value.columns.keys(),
                    "unique": i.value.unique ?? False,
                };
                rv.indexes{i.key} = ix;
            }
        }

        # add foreign constraints
        if (foreignConstraints) {
            foreach hash<auto> i in (foreignConstraints.pairIterator()) {
                hash<auto> fc = {
                    "table": i.value.target.table,
                    "columns": i.value.keys(),
                    "target_columns": i.value.target.columns.keys(),
                };
                rv.foreign_constraints{i.key} = fc;
            }
        }

        return rv;
    }

    #! returns an object of class Columns describing the table
    /** @par Example:
        @code{.py}
Columns cols = table.describe();
        @endcode

        @return an object of class Columns describing the table
    */
    Columns describe() {
        if (!columns) {
            l.lock();
            on_exit l.unlock();
            getColumnsUnlocked();
        }
        return columns;
    }

    #! returns an object of class AbstractPrimaryKey describing the primary key of the table
    /** If there is no primary key then the object returned will be empty (see @ref SqlUtil::AbstractPrimaryKey::empty())

        @par Example:
        @code{.py}
        @endcode

        @return an object of class AbstractPrimaryKey describing the primary key of the table
    */
    AbstractPrimaryKey getPrimaryKey() {
        if (!primaryKey) {
            l.lock();
            on_exit l.unlock();
            getPrimaryKeyUnlocked();
        }
        return primaryKey;
    }

    #! returns the given AbstractUniqueConstraint object if defined for the table (also includes the primary key)
    /** @par Example:
        @code{.py}
my *AbstractUniqueConstraint uk = table.findUniqueConstraint("uk_table2");
        @endcode

        @param name the name of the unique constraint to find

        @return the given AbstractUniqueConstraint object if defined for the table (also includes the primary key)
    */
    *AbstractUniqueConstraint findUniqueConstraint(string name) {
        if (!primaryKey) {
            getPrimaryKey();
        }
        if (!primaryKey.empty() && primaryKey.getName() == name) {
            return primaryKey;
        }
        if (!constraints) {
            getConstraints();
        }
        return constraints.hasKey(name) && constraints{name} instanceof AbstractUniqueConstraint
            ? constraints{name}
            : NOTHING;
    }

    #! returns the given AbstractUniqueConstraint object if defined for the table (also includes the primary key)
    /** @par Example:
        @code{.py}
my *AbstractUniqueConstraint uk = table.findUniqueConstraint("uk_table2");
        @endcode

        @param name the name of the unique constraint to find

        @return the given AbstractUniqueConstraint object if defined for the table (also includes the primary key)
    */
    private *AbstractUniqueConstraint findUniqueConstraintUnlocked(string name) {
        if (!primaryKey) {
            getPrimaryKeyUnlocked();
        }
        if (!primaryKey.empty() && primaryKey.getName() == name) {
            return primaryKey;
        }
        if (!constraints) {
            getConstraintsUnlocked();
        }
        return constraints.hasKey(name) && constraints{name} instanceof AbstractUniqueConstraint
            ? constraints{name}
            : NOTHING;
    }

    #! returns an object of class Indexes describing the indexes on the table
    /** If there are no indexes on the table then the object returned will be empty (see @ref SqlUtil::Indexes::empty())

        @par Example:
        @code{.py}
Indexes ix = table.getIndexes();
        @endcode

        @return an object of class Indexes describing the indexes on the table
    */
    Indexes getIndexes() {
        if (!indexes) {
            l.lock();
            on_exit l.unlock();
            getIndexesUnlocked();
        }
        return indexes;
    }

    #! returns a ForeignConstraints object describing the foreign constraints that the table has on other tables
    ForeignConstraints getForeignConstraints(*hash<auto> opt) {
        if (!foreignConstraints) {
            l.lock();
            on_exit l.unlock();
            getForeignConstraintsUnlocked(opt);
        }
        return foreignConstraints;
    }

    #! returns a Constraints object describing the non-foreign constraints on the table
    Constraints getConstraints() {
        if (!constraints) {
            l.lock();
            on_exit l.unlock();
            getConstraintsUnlocked();
        }
        return constraints;
    }

    #! returns an object of class Triggers describing the triggers on the table
    /** If there are no triggers on the table then the object returned will be empty (see @ref SqlUtil::Triggers::empty())

        @par Example:
        @code{.py}
Triggers trig = table.getTriggers();
        @endcode

        @return an object of class Triggers describing the triggers on the table
    */
    Triggers getTriggers() {
        if (!triggers) {
            l.lock();
            on_exit l.unlock();
            getTriggersUnlocked();
        }
        return triggers;
    }

    #! returns an SQL string that could be used to rename the table in the database
    /** @par Example:
        @code{.py}
string sql = table.getRenameSql("new_name");
        @endcode

        @param new_name the new name for the table
        @param opt a hash of options for the SQL string; see @ref SqlUtil::AbstractTable::AlignTableOptions for
        common options; each driver can support additional driver-specific options

        @return an SQL string that could be used to rename the table in the database

        @throw OPTION-ERROR invalid or unsupported option passed

        @note If the @ref sql_callback_executed "sql_callback_executed option key" is @ref True "True",
        this method also renames the object internally (see @ref sql_callback_executed for more information),
        additionally if the \c "db_table_cache" option key is assigned to a @ref SqlUtil::Tables "Tables"
        argument, then the table cache is also updated with the name change in this case.

        @see
        - rename()
        - inDb() for a method that tells if the table is already in the database or not
        */
    string getRenameSql(string new_name, *hash<auto> opt) {
        validateOptionsIntern("OPTION-ERROR", getAlignTableOptions(), \opt);

        l.lock();
        on_exit l.unlock();
        on_success if (opt.sql_callback_executed)
            doRenameIntern(new_name, opt.db_table_cache);

        return AbstractDatabase::doCallback(opt, getRenameSqlImpl(new_name), AbstractDatabase::AC_Rename, "table",
            name, NOTHING, new_name);
    }

    #! returns an SQL string that could be used to create the table and all known properties of the table
    /** @par Example:
        @code{.py}
printf("%s\n", table.getCreateSql());
        @endcode

        @param opt a hash of options for the SQL creation string

        @return an SQL string that could be used to create the table and all known properties of the table
    */
    string getCreateSqlString(*hash<auto> opt) {
        # convert table alignment omission options to a hash and check
        if (opt.omit) opt.omit = getCheckOmissionOptions(opt.omit, "CREATE-TABLE-ERROR");

        l.lock();
        on_exit l.unlock();

        if (!manual)
            cacheUnlocked(opt);

        return getCreateSqlImpl(getCreateSqlUnlocked(opt));
    }

    #! returns a list of SQL strings that could be used to create the table and all known properties of the table
    /** @par Example:
        @code{.py}
map printf("$1;\n", $1), table.getCreateSql();
        @endcode

        @param opt a hash of options for the table, index, and constraint creation strings; see @ref TableCreationOptions for common options; each driver can support additional driver-specific options

        @return a list of SQL strings that could be used to create the table and all known properties of the table
    */
    list<auto> getCreateSql(*hash<auto> opt) {
        # convert table alignment omission options to a hash and check
        if (opt.omit) opt.omit = getCheckOmissionOptions(opt.omit, "CREATE-TABLE-ERROR");

        l.lock();
        on_exit l.unlock();
        if (!manual)
            cacheUnlocked(opt);
        return getCreateSqlUnlocked(opt, False);
    }

    #! returns an SQL string that could be used to create the basic table structure without indexes and constraints
    /** @par Example:
        @code{.py}
string sql = table.getCreateTableSql();
        @endcode

        @param opt a hash of options for the table creation string; see @ref TableCreationOptions for common options; each driver can support additional driver-specific options

        @return an SQL string that could be used to create the basic table structure without indexes and constraints

        @throw OPTION-ERROR invalid or unsupported option passed
    */
    string getCreateTableSql(*hash<auto> opt) {
        # convert table alignment omission options to a hash and check
        if (opt.omit) opt.omit = getCheckOmissionOptions(opt.omit, "CREATE-TABLE-ERROR");

        validateOptionsIntern("OPTION-ERROR", getTableCreationOptions(), \opt);
        l.lock();
        on_exit l.unlock();
        return getCreateTableSqlUnlocked(opt);
    }

    #! returns @ref True "True" if the table exists in the database, @ref False "False" if not
    /** this method will actively check the database if the table is defined there in case no configuration from the database has already been cached for the object

        @see AbstractTable::inDb()
    */
    bool checkExistence() {
        l.lock();
        on_exit l.unlock();
        if (inDb)
            return True;
        return checkExistenceImpl();
    }

    private *hash<string, bool> getCheckOmissionOptions(*softlist<softstring> ol, string err) {
        # convert table alignment omission options to a hash and check
        if (!ol)
            return;

        hash<string, bool> oh;
        foreach string ostr in (ol) {
            if (!TableOmissionOptions{ostr})
                throw err, sprintf("omission option %y is unknown; known omission options: %y", ostr, TableOmissionOptions.keys());
            oh{ostr} = True;
        }
        return oh;
    }

    #! returns a list of SQL strings required to align the table to the table given as an argument
    /** if the tables are identical then an empty list is returned

        @par Example:
        @code{.py}
list<auto> l = table.getAlignSql(table2);
        @endcode

        @param t the template table that the current table will be compared to
        @param opt a hash of options for the SQL creation string; see @ref SqlUtil::AbstractTable::AlignTableOptions
        for common options; each driver can support additional driver-specific options

        @return a list of SQL strings required to align the structure and configuration of the current table with that
        of the AbstractTable argument; if the tables are identical then an empty list is returned

        @throw ALIGN-TABLE-ERROR the argument must be of the same class as the current object; template table has no
        columns
        @throw OPTION-ERROR invalid or unsupported option passed

        @note if the @ref sql_callback_executed "sql_callback_executed option" is @ref True "True" in \a opt, then the
        changes are also effected in the current object, if not, then they are not (see @ref sql_callback_executed for
        more information)
    */
    list<auto> getAlignSql(AbstractTable t, *hash<auto> opt) {
        if (self.className() != t.className())
            throw "ALIGN-TABLE-ERROR", sprintf("cannot align %s of class %y with %s of class %y", getDesc(),
                self.className(), t.getDesc(), t.className());

        hash<auto> ato = getAlignTableOptions();

        validateOptionsIntern("OPTION-ERROR", ato, \opt);

        # convert table alignment omission options to a hash and check
        opt.omit = getCheckOmissionOptions(opt.omit, "ALIGN-TABLE-ERROR");

        l.lock();
        on_exit l.unlock();
        # lock "template" table
        t.l.lock();
        on_exit t.l.unlock();

        # cache tables if not already cached
        if (!t.manual) {
            t.cacheUnlocked(opt);
        }

        if (!manual) {
            # if the table does not exist, then just return the create table strings for the template table
            if (!checkExistenceImpl())
                return t.getCreateSqlUnlocked(opt, False);

            # use db_table_cache for table_cache when caching DB objects
            *hash<auto> db_opt = {} + opt;
            db_opt.table_cache = remove db_opt.db_table_cache;
            cacheUnlocked(db_opt);
        }

        if (!t.columns || t.columns.empty()) {
            throw "ALIGN-TABLE-ERROR", sprintf("cannot align %s with %s: %y has no columns", getDesc(), t.getDesc(),
                t.name);
        }

        if (!columns) {
            columns = new Columns();
        }

        AbstractTable this = opt.sql_callback_executed ? self : self.copy();

        return this.getAlignSqlUnlocked(t, opt);
    }

    private list<auto> getAlignSqlUnlocked(AbstractTable t, *hash<auto> opt) {
        list<auto> l = ();

        # check name
        if (name != t.name) {
            l += AbstractDatabase::doCallback(opt, getRenameSql(t.name), AbstractDatabase::AC_Rename, "table", name,
                NOTHING, t.name);
            name = t.name;
        }

        bool uixc = uniqueIndexCreatesConstraintImpl();

        # process column_map: rename columns
        HashIterator i(opt.column_map);
        while (i.next()) {
            string key = i.getKey();
            if (!columns.hasKey(key))
                continue;
            string value = i.getValue();
            #printf("%s rename %s -> %s cols: %y\n", name, key, value, columns.keys());
            AbstractColumn c = columns{key};
            l += AbstractDatabase::doCallback(opt, c.getRenameSql(self, value), AbstractDatabase::AC_Rename,
                "column", c.name, name, value);
            columns.renameKey(key, value);
            c.name = value;

            # rename column in indexes
            foreach AbstractIndex ix in (indexes.iterator()) {
                if (ix.columns.hasKey(key)) {
                    #printf("%s.%s: col %y %N\n", getName(), ix.name, key, ix.columns);
                    AbstractColumn rc = ix.columns{key};
                    rc.name = value;
                    ix.columns.renameKey(key, value);
                }
            }
            # rename column in pk
            if (primaryKey.hasKey(key)) {
                AbstractColumn rc = primaryKey{key};
                rc.name = value;
                primaryKey.renameKey(key, value);
            }
            # rename column in unique constraints
            foreach AbstractConstraint ct in (constraints.iterator()) {
                if (ct instanceof AbstractUniqueConstraint) {
                    AbstractUniqueConstraint uk = ct;
                    if (uk.hasKey(key)) {
                        AbstractColumn rc = uk{key};
                        rc.name = value;
                        uk.renameKey(key, value);
                    }
                }
            }
            # rename column in foreign constraints
            foreach AbstractForeignConstraint fk in (foreignConstraints.iterator()) {
                if (fk.hasKey(key)) {
                    AbstractColumn rc = fk{key};
                    rc.name = value;
                    fk.renameKey(key, value);
                }
            }
        }

        # before modifying columns, rename indexes constraints and drop any excess indexes and constraints
        # process index_map: rename indexes
        i = new HashIterator(opt.index_map);
        while (i.next()) {
            string key = i.getKey();
            if (indexes.hasKey(key)) {
                AbstractIndex ix = indexes{key};
                string value = i.getValue();
                l += AbstractDatabase::doCallback(opt, ix.getRenameSql(getSqlName(), value),
                    AbstractDatabase::AC_Rename, "index", ix.name, name, value);
                renameIndexUnlocked(ix, value);
            }
        }

        if (!opt.omit.indexes) {
            # check for indexes with different names than in the template and rename them automatically
            foreach AbstractIndex ix in (indexes.iterator()) {
                if (!t.indexes.hasKey(ix.name)
                    && t.indexes
                    && (*AbstractIndex other = t.indexes.findEqual(ix))
                    && !indexes.hasKey(other.name)) {
                    # rename index
                    l += AbstractDatabase::doCallback(opt, ix.getRenameSql(getSqlName(), other.name),
                        AbstractDatabase::AC_Rename, "index", ix.name, name, other.name);
                    renameIndexUnlocked(ix, other.name);
                }
            }

            # check for indexes not in the template and drop them
            foreach AbstractIndex ix in (indexes.iterator()) {
                if (!t.indexes.hasKey(ix.name)) {
                    # make sure there is no unique constraint or primary key for the unique index if the "index"
                    # option is supported in which case update the constraint to be based on the index and do not drop
                    # the index
                    if (ix.unique && !t.primaryKey.empty() && t.primaryKey.matchKeys(ix.columns)) {
                        if (t.primaryKey.setIndexBase(ix.name)) {
                            ix.setSupportingConstraint(primaryKey);
                            continue;
                        }
                    } else {
                        # see if a unique or fk constraint with the same name and columns exists
                        foreach AbstractColumnConstraint csc in (getAllSupportingConstraints(ix.name)) {
                            if (csc.matchKeys(ix.columns) && csc.setIndexBase(ix.name)) {
                                ix.setSupportingConstraint(csc);
                                continue;
                            }
                        }
                        /*
                        *AbstractColumnSupportingConstraint csc = getSupportingConstraint(ix.name);
                        if (csc && csc.matchKeys(ix.columns) && csc.setIndexBase(ix.name)) {
                            ix.setSupportingConstraint(csc);
                            continue;
                        }
                        */
                    }
                    # see if the index is supporting a constraint that has to be dropped first
                    foreach AbstractColumnConstraint csc in (ix.getAllSupportingConstraints()) {
                        if (constraintsLinkedToIndexesImpl()) {
                            code rmv;
                            AbstractConstraint c = findDropConstraintUnlocked(csc.getName(), \rmv);

                            l += AbstractDatabase::doCallback(opt, c.getDropSql(getSqlName()),
                                AbstractDatabase::AC_Drop, "constraint", c.getName(), name);
                            rmv();
                        } else {
                            l += AbstractDatabase::doCallback(opt, ix.getDropSql(getSqlName()),
                                AbstractDatabase::AC_Drop, "index", ix.name, name);
                        }
                    }
                    /*
                    *AbstractColumnSupportingConstraint csc = ix.getSupportingConstraint();
                    if (csc && constraintsLinkedToIndexesImpl()) {
                        code rmv;
                        AbstractConstraint c = findDropConstraintUnlocked(csc.getName(), \rmv);

                        l += AbstractDatabase::doCallback(opt, c.getDropSql(getSqlName()), AbstractDatabase::AC_Drop,
                            "constraint", c.getName(), name);
                        rmv();
                    } else
                        l += AbstractDatabase::doCallback(opt, ix.getDropSql(getSqlName()), AbstractDatabase::AC_Drop,
                            "index", ix.name, name);
                    */
                    if (uixc) {
                        *AbstractUniqueConstraint c = ix.getSupportingConstraint();
                        if (c && constraints.hasKey(c.getName()))
                            constraints.take(c.getName());
                    }
                    indexes.tryTake(ix.name);
                }
            }
        }

        # process constraint_map: rename constraints
        i = new HashIterator(opt.constraint_map);
        while (i.next()) {
            string key = i.getKey();
            string value = i.getValue();
            *AbstractConstraint c;
            if (foreignConstraints.hasKey(key)) {
                c = foreignConstraints{key};
                foreignConstraints.renameKey(key, value);
            }
            else if (constraints.hasKey(key)) {
                c = constraints{key};
                constraints.renameKey(key, value);
            }
            else if (!primaryKey.empty() && primaryKey.getName() == key)
                c = primaryKey;

            if (c) {
                l += AbstractDatabase::doCallback(opt, c.getRenameSql(getSqlName(), value),
                    AbstractDatabase::AC_Rename, "constraint", c.getName(), name, value);
                c.rename(value);
            }
        }

        # check for unique constraints with different names than in the template and rename them automatically
        foreach AbstractConstraint c in (constraints.iterator()) {
            if (!(c instanceof AbstractUniqueConstraint))
                continue;
            AbstractUniqueConstraint uk = c;
            if (!t.constraints.hasKey(uk.getName())
                && t.constraints
                && (*AbstractUniqueConstraint other = t.constraints.findEqualUniqueConstraint(uk))
                && !constraints.hasKey(other.getName())) {
                # rename constraint
                l += AbstractDatabase::doCallback(opt, uk.getRenameSql(getSqlName(), other.getName()),
                    AbstractDatabase::AC_Rename, "constraint", uk.getName(), name, other.getName());
                constraints.renameKey(uk.getName(), other.getName());
                uk.rename(other.getName());
            }
        }

        if (!opt.omit.foreign_constraints) {
            # check for foreign constraints with different names than in the template and rename them automatically
            foreach AbstractForeignConstraint fk in (foreignConstraints.iterator()) {
                if (!t.foreignConstraints.hasKey(fk.getName())
                    && t.foreignConstraints
                    && (*AbstractForeignConstraint other = t.foreignConstraints.findEqual(fk))
                    && !foreignConstraints.hasKey(other.getName())) {
                    # rename constraint
                    l += AbstractDatabase::doCallback(opt, fk.getRenameSql(getSqlName(), other.getName()),
                        AbstractDatabase::AC_Rename, "constraint", fk.getName(), name, other.getName());
                    foreignConstraints.renameKey(fk.getName(), other.getName());
                    fk.rename(other.getName());
                }
            }

            # check for foreign constraints not in the template and drop them
            foreach AbstractForeignConstraint c in (foreignConstraints.iterator()) {
                if (!t.foreignConstraints.hasKey(c.getName())) {
                    l += AbstractDatabase::doCallback(opt, c.getDropSql(getSqlName()), AbstractDatabase::AC_Drop,
                        "foreign constraint", c.getName(), name);
                    foreignConstraints.take(c.getName());
                }
            }
        }

        # check for other constraints not in the template and drop them
        foreach AbstractConstraint c in (constraints.iterator()) {
            if (!t.constraints.hasKey(c.getName())) {
                l += AbstractDatabase::doCallback(opt, c.getDropSql(getSqlName()), AbstractDatabase::AC_Drop,
                    "constraint", c.getName(), name);
                constraints.take(c.getName());
            }
        }

        # see if we need to drop the primary key
        if (!t.primaryKey.empty()) {
            if (!primaryKey.empty()) {
                if (!primaryKey.equal(t.primaryKey)) {
                    # see if pk has the same definition but a different name; if so, it will be renamed later
                    if (primaryKey.getName() == t.primaryKey.getName() || primaryKey.matchKeys(t.primaryKey)) {
                        #printf("pk1: %N\nXXXXXXX\npk2: %N\n", primaryKey, t.primaryKey);
                        l += AbstractDatabase::doCallback(opt, primaryKey.getDropSql(getSqlName()),
                            AbstractDatabase::AC_Drop, "primary key", t.primaryKey.getName(), name);
                        # clear supporting constraint from index in the template since we've dropped the PK constraint
                        *string pk_index = t.primaryKey.getIndex();
                        if (pk_index && t.indexes.hasKey(pk_index))
                            t.indexes{pk_index}.setSupportingConstraint();
                        if (constraintsLinkedToIndexesImpl() && indexes.hasKey(primaryKey.getName()))
                            t.primaryKey.clearIndex();

                        delete primaryKey;
                    }
                }
            } else {
                # there is no PK in the table, so we have to remove the link from the index to the PK in the template
                *string pk_index = t.primaryKey.getIndex();
                if (pk_index && t.indexes.hasKey(pk_index))
                    t.indexes{pk_index}.setSupportingConstraint();
                if (constraintsLinkedToIndexesImpl() && indexes.hasKey(t.primaryKey.getName()))
                    t.primaryKey.clearIndex();
            }
        } else if (!primaryKey.empty()) {
            l += AbstractDatabase::doCallback(opt, primaryKey.getDropSql(getSqlName()), AbstractDatabase::AC_Drop,
                "primary key", primaryKey.getName(), name);
            delete primaryKey;
        }

        # check for columns in the template that are not present or different in the current table
        foreach AbstractColumn c in (t.columns.iterator()) {
            if (!columns.hasKey(c.name)) {
                # add column to table
                l += AbstractDatabase::doCallback(opt, c.getAddColumnSql(self), AbstractDatabase::AC_Add, "column",
                    c.name, name);
            } else {
                # get any delta SQL
                list<auto> cl = columns{c.name}.getModifySql(self, c);
                if (cl) {
                    on_error rethrow $1.err, sprintf("%s (%y table: %y desc: %y)", $1.desc, c.name,
                        columns{c.name}{"native_type", "qore_type"}, c.native_type{"native_type", "qore_type"});
                    l += AbstractDatabase::doCallback(opt, cl, AbstractDatabase::AC_Modify, "column", c.name, name,
                        NOTHING, c.getNativeTypeString());
                } else
                    AbstractDatabase::doOkCallback(opt, AbstractDatabase::AC_Unchanged, "column", c.name, name);
            }
        }

        # check for columns not in the template and drop them
        foreach AbstractColumn c in (columns.iterator()) {
            if (!t.columns.hasKey(c.name)) {
                l += getDropAllConstraintsAndIndexesOnColumnSqlUnlocked(c.name, opt);
                l += AbstractDatabase::doCallback(opt, c.getDropSql(getSqlName()), AbstractDatabase::AC_Drop,
                    "column", c.name, name);
            }
        }

        if (!opt.omit.indexes) {
            # check for indexes in the template that are not present or different in the current table
            foreach AbstractIndex ix in (t.indexes.iterator()) {
                if (!indexes.hasKey(ix.name)) {
                    # add index to table
                    l += AbstractDatabase::doCallback(opt, ix.getCreateSql(getSqlName(), opt),
                        AbstractDatabase::AC_Add, "index", ix.name, name);
                } else if (!ix.equal(indexes{ix.name})) {
                    l += AbstractDatabase::doCallback(opt, ix.getRecreateSql(ds, getSqlName(), opt),
                        AbstractDatabase::AC_Recreate, "index", ix.name, name);
                } else
                    AbstractDatabase::doOkCallback(opt, AbstractDatabase::AC_Unchanged, "index", ix.name, name);
            }
        }

        # check for primary key differences
        if (!t.primaryKey.empty()) {
            if (!primaryKey.empty()) {
                # we know the 2 primary key definitions have equal column definitions as it's checked above
                # see if pk has a different name
                if (primaryKey.getName() != t.primaryKey.getName()) {
                    l += AbstractDatabase::doCallback(opt, primaryKey.getRenameSql(getSqlName(),
                        t.primaryKey.getName()), AbstractDatabase::AC_Rename, "constraint", primaryKey.getName(),
                        name, t.primaryKey.getName());
                    primaryKey.rename(t.primaryKey.getName());
                }
            } else {
                l += AbstractDatabase::doCallback(opt, t.primaryKey.getCreateSql(getSqlName(), opt),
                    AbstractDatabase::AC_Add, "primary key", t.primaryKey.getName(), name);
            }
        }

        if (!opt.omit.foreign_constraints) {
            # check for foreign constraints in the template that are not present or different in the current table
            foreach AbstractForeignConstraint c in (t.foreignConstraints.iterator()) {
                if (!foreignConstraints.hasKey(c.getName())) {
                    # add constraint to table
                    l += AbstractDatabase::doCallback(opt, c.getCreateSql(getSqlName(), opt),
                        AbstractDatabase::AC_Add, "foreign constraint", c.getName(), name);
                } else if (!c.equal(foreignConstraints{c.getName()})) {
                    # drop current constraint
                    l += AbstractDatabase::doCallback(opt, foreignConstraints{c.getName()}.getDropSql(getSqlName()),
                        AbstractDatabase::AC_Drop, "foreign constraint", c.getName(), name);
                    # add new constraint to table
                    l += AbstractDatabase::doCallback(opt, c.getCreateSql(getSqlName(), opt),
                        AbstractDatabase::AC_Add, "foreign constraint", c.getName(), name);
                } else {
                    AbstractDatabase::doOkCallback(opt, AbstractDatabase::AC_Unchanged, "foreign constraint",
                        c.getName(), name);
                }
            }
        }

        # check for other constraints in the template that are not present or different in the current table
        foreach AbstractConstraint c in (t.constraints.iterator()) {
            if (!constraints.hasKey(c.getName())) {
                # make sure the constraint wasn't implicitly added with a unique index
                if (uixc && (c instanceof AbstractUniqueConstraint)) {
                    AbstractUniqueConstraint uk = c;
                    if (t.indexes.hasKey(uk.getName()) && uk.matchKeys(t.indexes{uk.getName()}.columns.getHash()))
                        continue;
                }
                # add constraint to table
                l += AbstractDatabase::doCallback(opt, c.getCreateSql(getSqlName(), opt), AbstractDatabase::AC_Add,
                    "constraint", c.getName(), name);
            } else if (!c.equal(constraints{c.getName()})) {
                    # drop current constraint
                l += AbstractDatabase::doCallback(opt, constraints{c.getName()}.getDropSql(getSqlName()),
                    AbstractDatabase::AC_Drop, "constraint", c.getName(), name);
                if (constraints{c.getName()} instanceof AbstractUniqueConstraint && constraintsLinkedToIndexesImpl()
                    && indexes.hasKey(c.getName()))
                    cast<AbstractUniqueConstraint>(c).clearIndex();

                # add new constraint to table
                l += AbstractDatabase::doCallback(opt, c.getCreateSql(getSqlName(), opt), AbstractDatabase::AC_Add,
                    "constraint", c.getName(), name);
            } else
                AbstractDatabase::doOkCallback(opt, AbstractDatabase::AC_Unchanged, "constraint", c.getName(), name);
        }

        # check for table-specific alignment actions before triggers
        {
            *list fl = getAlignSqlImpl(t, opt);
            if (fl)
                l += fl;
        }

        if (!opt.omit.triggers) {
            # check for triggers not in the template and drop them
            # do this before adding for dbs like mysql that cannot support multiple triggers with the same action on the same table
            foreach AbstractTrigger trig in (triggers.iterator()) {
                if (!t.triggers.hasKey(trig.name)) {
                    l += AbstractDatabase::doCallback(opt, trig.getDropSql(getSqlName()), AbstractDatabase::AC_Drop,
                        "trigger", trig.name, name);
                }
            }

            # check for triggers in the template that are not present or different in the current table
            foreach AbstractTrigger trig in (t.triggers.iterator()) {
                if (!triggers.hasKey(trig.name)) {
                    # add trigger to table
                    l += AbstractDatabase::doCallback(opt, trig.getCreateSql(getSqlName(), opt), AbstractDatabase::AC_Add,
                        "trigger", trig.name, name);
                } else if (!trig.equal(triggers{trig.name})) {
                    # drop current trigger
                    l += AbstractDatabase::doCallback(opt, triggers{trig.name}.getDropSql(getSqlName()),
                        AbstractDatabase::AC_Drop, "trigger", trig.name, name);
                    # add new trigger to table
                    l += AbstractDatabase::doCallback(opt, trig.getCreateSql(getSqlName(), opt),
                        AbstractDatabase::AC_Add, "trigger", trig.name, name);
                } else
                    AbstractDatabase::doOkCallback(opt, AbstractDatabase::AC_Unchanged, "trigger", trig.name, name);
            }
        }

        return l;
    }

    private *AbstractColumnSupportingConstraint getSupportingConstraint(string ixname) {
        if (constraints.hasKey(ixname) && constraints{ixname} instanceof AbstractColumnSupportingConstraint)
            return constraints{ixname};
    }

    private *list<AbstractColumnConstraint> getAllSupportingConstraints(string ixname) {
        list<AbstractColumnConstraint> rv;
        if (constraints.hasKey(ixname) && constraints{ixname} instanceof AbstractColumnSupportingConstraint) {
            rv += constraints{ixname};
        }
        if (foreignConstraints.hasKey(ixname)) {
            rv += foreignConstraints{ixname};
        }
        return rv;
    }

    private renameIndexUnlocked(AbstractIndex ix, string new_name) {
        string old_name = ix.name;
        indexes.renameKey(old_name, new_name);
        ix.name = new_name;

        # if constraints are linked to indexes, then rename any matching constraint as well
        if (constraintsLinkedToIndexesImpl()) {
            if (!primaryKey.empty() && primaryKey.getName() == old_name) {
                primaryKey.rename(new_name);
            } else {
                foreach AbstractConstraint uk in (constraints.iterator()) {
                    if (uk instanceof AbstractUniqueConstraint && uk.getName() == old_name) {
                        uk.rename(new_name);
                        constraints.renameKey(old_name, new_name);
                        break;
                    }
                }
            }
        }
    }

    #! accepts an AbstractTable argument and returns an SQL string that could be executed to align the structure and configuration of the current table with that of the argument; if the tables are identical then an empty string is returned
    /** @par Example:
        @code{.py}
string sql = table.getAlignSqlString(table2);
        @endcode

        @param t the template table that the current table will be compared to
        @param opt a hash of options for the SQL creation string; see @ref SqlUtil::AbstractTable::AlignTableOptions for common options; each driver can support additional driver-specific options

        @return an SQL string that could be executed to align the structure and configuration of the current table with that of the argument; if the tables are identical then an empty string is returned

        @throw ALIGN-TABLE-ERROR the argument must be of the same class as the current object; template table has no columns
        @throw OPTION-ERROR invalid or unsupported option passed
    */
    string getAlignSqlString(AbstractTable t, *hash<auto> opt) {
        return getCreateSqlImpl(getAlignSql(t, opt));
    }

    #! returns a list of SQL strings that could be used to create indexes on the table or @ref nothing if there are no indexes on the table
    /** @par Example:
        @code{.py}
*list l = table.getCreateIndexesSql();
        @endcode

        @param opt a hash of options for the index creation string; see @ref SqlUtil::AbstractTable::IndexOptions for common options; each driver can support additional driver-specific options
        @param cache read in data from the database for uncached properties of the table

        @return a list of SQL strings that could be used to create indexes on the table or @ref nothing if there are no indexes on the table

        @throw OPTION-ERROR invalid or unsupported option passed
    */
    *list<auto> getCreateIndexesSql(*hash<auto> opt, bool cache = True) {
        validateOptionsIntern("OPTION-ERROR", getIndexOptions(), \opt);
        l.lock();
        on_exit l.unlock();
        return getCreateIndexesSqlUnlocked(opt, cache);
    }

    #! returns an SQL string that could be used to create the primary key on the table
    /** @par Example:
        @code{.py}
*string sql = table.getCreatePrimaryKeySql();
        @endcode

        @param opt a hash of options for the primary key creation string; see @ref SqlUtil::AbstractTable::ConstraintOptions for common options; each driver can support additional driver-specific options
        @param cache read in data from the database for uncached properties of the table

        @return an SQL string that could be used to create the primary key on the table or @ref nothing if there is no primary key on the table

        @throw OPTION-ERROR invalid or unsupported option passed
    */
    *string getCreatePrimaryKeySql(*hash<auto> opt, bool cache = True) {
        validateOptionsIntern("OPTION-ERROR", getConstraintOptions(), \opt);
        l.lock();
        on_exit l.unlock();
        return getCreatePrimaryKeySqlUnlocked(opt, cache);
    }

    #! returns a list of SQL strings that could be used to create foreign constraints on the table or @ref nothing if there are no foreign constraints on the table
    /** @par Example:
        @code{.py}
*list l = table.getCreateForeignConstraintsSql();
        @endcode

        @param opt a hash of options for the foreign constraint creation string; see @ref SqlUtil::AbstractTable::ForeignConstraintOptions for common options; each driver can support additional driver-specific options
        @param cache read in data from the database for uncached properties of the table

        @return a list of SQL strings that could be used to create foreign constraints on the table or @ref nothing if there are no foreign constraints on the table

        @throw OPTION-ERROR invalid or unsupported option passed
    */
    *list<auto> getCreateForeignConstraintsSql(*hash<auto> opt, bool cache = True) {
        validateOptionsIntern("OPTION-ERROR", getForeignConstraintOptions(), \opt);
        l.lock();
        on_exit l.unlock();
        return getCreateForeignConstraintsSqlUnlocked(opt, cache);
    }

    #! returns a list of SQL strings that could be used to create non-foreign constraints on the table or @ref nothing if there are no non-foreign constraints on the table
    /** @par Example:
        @code{.py}
*list l = table.getCreateConstraintsSql();
        @endcode

        @param opt a hash of options for the non-foreign constraint creation string; see @ref SqlUtil::AbstractTable::ConstraintOptions for common options; each driver can support additional driver-specific options
        @param cache read in data from the database for uncached properties of the table

        @return a list of SQL strings that could be used to create non-foreign constraints on the table or @ref nothing if there are no non-foreign constraints on the table

        @see getCreateForeignConstraintsSql()

        @throw OPTION-ERROR invalid or unsupported option passed
    */
    *list<auto> getCreateConstraintsSql(*hash<auto> opt, bool cache = True) {
        validateOptionsIntern("OPTION-ERROR", getConstraintOptions(), \opt);
        l.lock();
        on_exit l.unlock();
        return getCreateConstraintsSql(opt, cache);
    }

    #! returns a list of SQL strings that could be used to create other table attributes (such as comments, if supported and present) or @ref nothing if there are none
    /** @par Example:
        @code{.py}
*list l = table.getCreateMiscSql();
        @endcode

        @param opt a hash of options for the SQL creation string; see @ref TableCreationOptions for common options; each driver can support additional driver-specific options
        @param cache read in data from the database for uncached properties of the table

        @return a list of SQL strings that could be used to create other table attributes (such as comments, if supported and present) or @ref nothing if there are none

        @throw OPTION-ERROR invalid or unsupported option passed
    */
    *list<auto> getCreateMiscSql(*hash<auto> opt, bool cache = True) {
        validateOptionsIntern("OPTION-ERROR", getTableCreationOptions(), \opt);
        l.lock();
        on_exit l.unlock();
        return getCreateMiscSqlUnlocked(opt, cache);
    }

    #! returns a list of SQL strings that could be used to create triggers on the table or @ref nothing if there are no triggers on the table
    /** @par Example:
        @code{.py}
*list l = table.getCreateTriggersSql();
        @endcode

        @param opt a hash of options for the trigger creation strings; see @ref TableCreationOptions for common options; each driver can support additional driver-specific options
        @param cache read in data from the database for uncached properties of the table

        @return a list of SQL strings that could be used to create triggers on the table or @ref nothing if there are no triggers on the table

        @throw OPTION-ERROR invalid or unsupported option passed

        @see getCreateTriggersSql()
    */
    *list<auto> getCreateTriggersSql(*hash<auto> opt, bool cache = True) {
        validateOptionsIntern("OPTION-ERROR", getTableCreationOptions(), \opt);
        l.lock();
        on_exit l.unlock();
        return getCreateTriggersSqlUnlocked(opt, cache);
    }

    #! finds a row in the table with the given primary key value; if no row matches the primary key value passed then @ref nothing is returned
    /** @par Example:
        @code{.py}
*hash<auto> row = table.find(id);
        @endcode

        @throw PRIMARY-KEY-ERROR the table has no primary key or the primary key has more than one column
        */
    *hash find(auto id) {
        string cname = getPrimaryKeyColumn();

        return ds.selectRow("select * from %s where %s = %v", getSqlName(), getColumnSqlName(cname), id);
    }

    #! finds rows in the table with the given primary key values; if no row matches any primary key value passed then @ref nothing is returned
    /** @par Example:
        @code{.py}
*list rows = table.find(list);
        @endcode

        @param ids the list of primary key IDs to find; if the list is empty then @ref nothing is returned

        @return a list of hashes of rows matching the primary key IDs passed or @ref nothing if no row matches any primary key value passed

        @throw PRIMARY-KEY-ERROR the table has no primary key or the primary key has more than one column
        */
    *list find(list<auto> ids) {
        string cname = getPrimaryKeyColumn();

        if (!ids)
            return;

        list<auto> args;
        hash<auto> cond{cname} = op_in(ids);
        string sql = getSelectSqlStringIntern(("where": cond), \args);

        return ds.vselectRows(sql, args);
    }

    private string getPrimaryKeyColumn() {
        if (!primaryKey) {
            l.lock();
            on_exit l.unlock();
            getPrimaryKeyUnlocked();
        }
        if (primaryKey.empty())
            throw "PRIMARY-KEY-ERROR", sprintf("%s: has no primary key", getDesc());
        if (primaryKey.size() > 1)
            throw "PRIMARY-KEY-ERROR", sprintf("%s: has a primary key with %d columns (%y); this method can only be "
                "called on tables with a primary key with one column", getDesc(), primaryKey.size(), primaryKey.keys());
        return primaryKey.firstKey();
    }

    #! finds a row in the table with the given primary key value given as a hash; if no row matches the primary key value passed then @ref nothing is returned
    /** @par Example:
        @code{.py}
*hash<auto> row = table.find(("account_type": type, "name": name));
        @endcode

        @param row a hash giving the primary key value to find; other columns may also appear in the hash, however at least all columns of the primary key must be present

        @return a hash of the row value matching the primary key value passed or @ref nothing if no row matches the primary key value passed

        @throw PRIMARY-KEY-ERROR the table has no primary key or the the hash passed does not contain all columns of the primary key

        @note a table with a primary key with a single column can also be used with this method; just pass a hash with one key
        */
    *hash<auto> find(hash<auto> row) {
        Columns cols;
        if (!primaryKey) {
            l.lock();
            on_exit l.unlock();
            getPrimaryKeyUnlocked();
        }
        if (primaryKey.empty())
            throw "PRIMARY-KEY-ERROR", sprintf("%s: has no primary key", getDesc());
        cols = primaryKey;

        # make sure all columns of the primary key are included in the row
        foreach string k in (cols.keyIterator()) {
            if (!row.hasKey(k))
                throw "PRIMARY-KEY-ERROR", sprintf("%s: row passed is missing column %y of primary key (%y)", getDesc(), k, cols.keys());
        }

        # make query
        string sql = sprintf("select * from %s where ", getSqlName());
        sql += foldl $1 + " and " + $2, (map sprintf("%s = %v", getColumnSqlName($1)), keys row);
        return ds.vselectRow(sql, row.values());
    }

    #! finds a single row in the table that match the row condition passed; multiple rows may match, but only one row will be returned from the database; if no row matches the condition hash passed then @ref nothing is returned
    /** @par Example:
        @code{.py}
*hash<auto> row = table.findSingle(h);
        @endcode

        @param cond a hash giving the column values to find; see @ref where_clauses for the format of this argument

        @return a hash representing a single row in the table with the given column values; multiple rows may match, but only one row will be returned from the database; if no row matches the condition hash passed then @ref nothing is returned

        @throw WHERE-ERROR unknown operator or invalid arguments given in the cond hash for the where clause

        @note this is equivalent to calling selectRow() with \c where = \c cond and \c limit = 1
        */
    *hash<auto> findSingle(*hash<auto> cond) {
        return selectRows(("where": cond, "limit": 1))[0];
    }

    #! finds all rows in the table with the given column values; a list of hashes is returned representing the rows returned
    /** @par Example:
        @code{.py}
*list<auto> rows = table.findAll(h);
        @endcode

        @param cond a hash giving the column values to find; see @ref where_clauses for the format of this argument

        @return a list of hashes is returned representing the rows returned

        @throw WHERE-ERROR unknown operator or invalid arguments given in the cond hash for the where clause

        @note this is equivalent to calling selectRows() with \c where = \c cond
        */
    *list<auto> findAll(*hash<auto> cond) {
        return selectRows(("where": cond));
    }

    #! returns a descriptive string of the datasource (without the password) and the table name (with a possible qualifier for schema, etc)
    /** Used in exception descriptions

        @since SqlUtil 1.3
    */
    string getDesc() {
        return sprintf("%s: %s %s", AbstractSqlUtilBase::makeDatasourceDesc(ds), getBaseType(), getSqlName());
    }

    #! returns the base type of the underlying object (normally \"table\", some DB-specific implementations may support others like \c "view")
    string getBaseType() {
        return "table";
    }

    #! returns the name of the table to be used in SQL (with a possible qualifier for schema, etc)
    string getSqlName() {
        return name;
    }

    #! returns the column name for use in SQL strings; subclasses can return a special string in case the column name is a reserved word
    string getColumnSqlName(string col) {
        return col;
    }

    #! returns a list of column names for use in SQL strings; subclasses can process the argument list in case a column name is a reserved word
    list<auto> getColumnSqlNames(softlist cols) {
        return cols;
    }

    #! returns @ref True if the DB treats empty strings as @ref NULL, @ref False if not; by default this method returns @ref False
    /** @since SqlUtil 1.3
    */
    bool bindEmptyStringsAsNull() {
        return False;
    }

    #! returns a record description for the table
    /** @return the record type for the table

        @since SqlUtil 1.6
    */
    *hash<string, AbstractDataField> getRecordType() {
        return map {
            $1.name.lwr(): getColumnDataField($1),
        }, describe().iterator();
    }

    #! returns a field object for the given column
    /** @param column_name the column name
        @param options data type options
        @param append_desc an optional string to append to the field's description

        @return a field object for the given column

        @throw COLUMN-ERROR unknown column

        @since SqlUtil 1.6
    */
    AbstractDataField getColumnDataField(string column_name, *hash<auto> options, *string append_desc) {
        *Columns columns = describe();
        *AbstractColumn column = columns{column_name};
        if (!column) {
            throw "COLUMN-ERROR", sprintf("column %y is unknown; valid columns: %y", column_name, columns.keys());
        }
        return getColumnDataField(column, options, append_desc);
    }

    #! returns a field object for the given column
    /** @param column the column
        @param options data type options
        @param append_desc an optional string to append to the field's description

        @return a field object for the given column

        @since SqlUtil 1.6
    */
    AbstractDataField getColumnDataField(AbstractColumn column, *hash<SqlUtilDataTypeOptionInfo> options, *string append_desc) {
        AbstractDataProviderType type = getDbType(column, options);

        string desc = column.getNativeTypeString();
        if (!column.nullable) {
            desc += " NOT NULL";
        }
        if (column.comment) {
            desc += "; " + column.comment;
        }
        if (append_desc) {
            desc += "; " + append_desc;
        }

        return new SqlUtilColumnField(column.name.lwr(), desc, type, column.def_val);
    }

    #! returns the data type for the given column
    /** @param column_name the column name
        @param options data type options

        @return the data type for the given column

        @throw COLUMN-ERROR unknown column

        @since SqlUtil 1.6
    */
    AbstractDataProviderType getColumnDataType(string column_name, *hash<SqlUtilDataTypeOptionInfo> options) {
        *Columns columns = describe();
        *AbstractColumn column = columns{column_name};
        if (!column) {
            throw "COLUMN-ERROR", sprintf("column %y is unknown; valid columns: %y", column_name, columns.keys());
        }

        return getDbType(column, options);
    }

    #! returns the DB type for the given column type
    /** @since SqlUtil 1.6
    */
    AbstractDataProviderType getDbType(AbstractColumn column, *hash<SqlUtilDataTypeOptionInfo> options) {
        string name = column.native_type;
        if (column.size) {
            name += column.scale ? sprintf("(%d,%d)", column.size, column.scale) : sprintf("(%d)", column.size);
        }

        return getDbType(name, column.qore_type, exists column.def_val ? True : column.nullable,
            column.size ? column.size : -1, options);
    }

    #! returns the DB type for the given column type
    /** @since SqlUtil 1.6
    */
    AbstractDataProviderType getDbType(string native_type, *string qore_type, bool nullable, int max_size = -1,
            *hash<SqlUtilDataTypeOptionInfo> options) {
        hash<auto> type_options;
        if (nullable && options.mandatory) {
            nullable = False;
        }
        if (nullable && native_type[0] != "*") {
            native_type = "*" + native_type;
        }
        if (options.maxlen && (max_size < 0 || max_size > options.maxlen)) {
            type_options."string.max_size_chars" = options.maxlen;
        }
        if (!qore_type) {
            qore_type = getTypeMapImpl(){native_type};
            if (!qore_type) {
                return new SqlUtilDbSpecificDataType(native_type, nullable, type_options);
            }
        }
        switch (qore_type) {
            case "number":
                return getNumericType(native_type, nullable, {"number.format": options.number_format});

            case "integer":
                return new SqlUtilIntType(native_type, nullable, {"number.format": options.number_format});

            case "float":
                return new SqlUtilFloatType(native_type, nullable, {"number.format": options.number_format});

            case "date":
                type_options = {
                    "date.format": options.date_format,
                    "date.output_timezone": options.db_timezone,
                    "date.input_timezone": options.data_timezone,
                };
                return new SqlUtilDateType(native_type, nullable, type_options);

            case "string": {
                if (max_size > 0 && !type_options."string.max_size_chars") {
                    type_options."string.max_size_chars" = max_size;
                }
                return new SqlUtilStringType(native_type, nullable, type_options);
            }

            default:
                return new SqlUtilColumnDataType(native_type, (nullable ? "*" : "") + qore_type, type_options);
        }
    }

    #! returns the type for number / numeric columns for the database so that data conversions can be handled properly
    /** @since SqlUtil 1.6
    */
    AbstractDataProviderType getNumericType(string type_name, bool nullable, *hash<auto> options) {
        return getNumericTypeImpl(type_name, nullable, options);
    }

    #! get DB-specific savepoint helper
    /** @param savepoint the savepoint string, if not given a unique savepoint string will be generated

        @since SqlUtil 1.6
    */
    AbstractSavepointHelper getSavepointHelper(*string savepoint) {
        return getSavepointHelperImpl(savepoint);
    }

    #! Returns the expression map for this database server
    /** override in subclasses to return driver-specific options

        @since SqlUtil 1.8
    */
    hash<auto> getExpressionMap() {
        return DefaultExpressionMap;
    }

    #! returns the "where" operator map for this object
    /** override in subclasses to return driver-specific options
    */
    hash<auto> getWhereOperatorMap() {
        return DefaultOpMap;
    }

    #! returns @ref True "True" if the underlying DB driver supports bulk DML operations
    abstract bool hasArrayBind();

    #! transforms bind arguments for bulk DML operations
    /** Override in subclasses to apply driver-specific transformations to bind values
        before they are passed to a prepared statement for bulk execution.

        The default implementation returns the hash values unchanged.

        @param data the hash of column data (column name → values; lists for array columns, scalars for constants)
        @return the transformed bind arguments as a flat list

        @since %Qore 2.1
    */
    list<auto> transformBulkBindArgs(hash<auto> data) {
        return data.values();
    }

    #! returns the table options for this driver
    /** override in subclasses to return driver-specific options
    */
    private hash<auto> getTableOptions() {
        return TableOptions;
    }

    #! return the foreign constraint options for this driver
    /** override in subclasses to return driver-specific options
    */
    private hash<auto> getForeignConstraintOptions() {
        return ForeignConstraintOptions;
    }

    #! returns the constraint options for this driver
    /** override in subclasses to return driver-specific options
    */
    private hash<auto> getConstraintOptions() {
        return ConstraintOptions;
    }

    #! returns the cache options for this driver
    /** override in subclasses to return driver-specific options
    */
    private hash<auto> getCacheOptions() {
        return CacheOptions;
    }

    #! returns the table creation options for this driver
    /** override in subclasses to return driver-specific options
    */
    private hash<auto> getTableCreationOptions() {
        return TableCreationOptions;
    }

    #! returns the align table options for this driver
    /** override in subclasses to return driver-specific options
    */
    private hash<auto> getAlignTableOptions() {
        return AlignTableOptions;
    }

    #! returns the table description hash<auto> options for this driver
    /** override in subclasses to return driver-specific options
    */
    private hash<auto> getTableDescriptionHashOptions() {
        return TableDescriptionHashOptions;
    }

    #! returns the column options for this driver
    /** override in subclasses to return driver-specific options
    */
    private hash<auto> getColumnOptions() {
        return ColumnOptions;
    }

    #! returns the column description options for this driver
    /** override in subclasses to return driver-specific options
    */
    private hash<auto> getColumnDescOptions() {
        return ColumnDescOptions;
    }

    #! returns the table column description options for this driver
    /** override in subclasses to return driver-specific options
    */
    private hash<auto> getTableColumnDescOptions() {
        return getColumnDescOptions() + AdditionalColumnDescOptions;
    }

    #! returns the index options for this driver
    /** override in subclasses to return driver-specific options
    */
    private hash<auto> getIndexOptions() {
        return IndexOptions;
    }

    #! returns the trigger options for this driver
    /** override in subclasses to return driver-specific options
    */
    private hash<auto> getTriggerOptions() {
        return TriggerOptions;
    }

    #! returns the select options for this driver
    /** override in subclasses to return driver-specific options
    */
    private hash<auto> getSelectOptions() {
        return SelectOptions;
    }

    #! returns the upsert options for this driver
    /** override in subclasses to return driver-specific options
    */
    private hash<auto> getUpsertOptions() {
        return UpsertOptions;
    }

    #! returns the insert options for this driver
    /** override in subclasses to return driver-specific options
    */
    private hash<auto> getInsertOptions() {
        return InsertOptions;
    }

    #! returns the insert from iterator options for this driver
    /** override in subclasses to return driver-specific options
    */
    private hash<auto> getInsertFromIteratorOptions() {
        return InsertFromIteratorOptions;
    }

    #! returns the sql data operation callback options for this driver
    /** override in subclasses to return driver-specific options
    */
    private hash<auto> getSqlDataCallbackOptions() {
        return SqlDataCallbackOptions;
    }

    #! returns the column operator map for this object
    /** subclasses should to implement getColumnOperatorMapImpl() to return driver-specific options
    */
    private hash<auto> getColumnOperatorMap() {
        return DefaultCopMap + getColumnOperatorMapImpl() + m_customCopMap;
    }

    #! Reimplement in subclasses to provide driver specific column operators
    private *hash<auto> getColumnOperatorMapImpl() {
    }

    #! register custom user column operator for this table object
    /** This method allows to register custom operators for select
        statements.

        @param name a string with operator name. It has to be unique in the driver
        @param operator an operator has as expected by @ref SqlUtil::make_cop() function

        @throw CUSTOM-OPERATOR-ERROR in case when user tries to register
                already existing operator with \c name or if the \c operator
                hash does not contain \c "code" key/value.

        @code{.py}
# t is a AbstractTable/Table object
# custom operator hash
hash<auto> to_char = (
"code" : string sub(string arg1, auto arg) {
                return sprintf("to_char(%s, '%s')", arg1, arg);
            },
);
# operator registration
t.addCustomCopOperator("to_char", to_char);
# example usage
hash<auto> sh = (
"columns" : cop_as(SqlUtil::make_cop("to_char", "d", "yyyymmddhh24miss"), "string_fmt_date"),
"limit" : 1,
);
# output
string sql;
on_exit printf("SQL> %s\n", sql);
*list<auto> res = t.selectRows(sh, \sql);
# result (from Oracle)
# list: (1 element)
#  [0]=hash: (3 members)
#    string_fmt_date : "20171020071854"
# SQL> select to_char(d, 'yyyymmddhh24miss') as string_fmt_date from test_schema.t
        @endcode
    */
    addCustomCopOperator(string name, hash<auto> operator) {
        hash<auto> ops = getColumnOperatorMap();
        if (ops.hasKey(name)) {
            throw "CUSTOM-OPERATOR-ERROR", sprintf("Operator '%s' is already registered as: %y", name, ops{name});
        }

        # input hash validations
        if (!operator.hasKey("code")) {
            throw "CUSTOM-OPERATOR-ERROR", "Operator hash must contain key 'code'";
        } else if (!operator."code".callp()) {
            throw "CUSTOM-OPERATOR-ERROR", sprintf("Operator hash must contain key 'code' with a callable value; got "
                "type: %s", operator."code".type());
        }

        m_customCopMap{name} = operator;
    }

    #! Returns True if the exception was raised because of a duplicate row / key error
    /** @since SqlUtil 1.7.3
    */
    bool isDuplicateRowError(hash<ExceptionInfo> ex) {
        return isDuplicateRowErrorImpl(ex);
    }

    #! returns the insert operator map for this object
    /** override in subclasses to return driver-specific options
    */
    private hash<auto> getInsertOperatorMap() {
        return DefaultIopMap;
    }

    #! returns the update operator map for this object
    /** override in subclasses to return driver-specific options
    */
    private hash<auto> getUpdateOperatorMap() {
        hash<auto> uom = getRawUpdateOperatorMap();
        return getColumnOperatorMap().(uom.keys())
            + (map {$1.key: $1.value}, uom.pairIterator(), $1.value.typeCode() == NT_HASH);
    }

    #! returns the raw (default) update operator map for this object
    /** override in subclasses to return driver-specific options
    */
    private hash<auto> getRawUpdateOperatorMap() {
        return DefaultUopMap;
    }

    #! returns a hash of valid pseudocolumns
    /** override in subclasses to return driver-specific pseudocolumns; by default this method returns @ref nothing
    */
    private *hash<auto> getPseudoColumnHash() {
    }

    private string getCreateTableSqlUnlocked(*hash<auto> opt) {
        getColumnsUnlocked();
        return AbstractDatabase::doCallback(opt, getCreateTableSqlImpl(opt), AbstractDatabase::AC_Create, "table", name);
    }

    private *list<auto> getCreateIndexesSqlUnlocked(*hash<auto> opt, bool cache = True) {
        if (cache)
            getIndexesUnlocked();
        if (!indexes)
            return;

        return map AbstractDatabase::doCallback(opt, $1.getCreateSql(name, opt), AbstractDatabase::AC_Add, "index", $1.name, name), indexes.iterator();
    }

    private *string getCreatePrimaryKeySqlUnlocked(*hash<auto> opt, bool cache = True) {
        if (cache)
            getPrimaryKeyUnlocked();
        if (primaryKey.empty())
            return;
        return AbstractDatabase::doCallback(opt, primaryKey.getCreateSql(name, opt), AbstractDatabase::AC_Add, "primary key", primaryKey.getName(), name);
    }

    private *list<auto> getCreateConstraintsSqlUnlocked(*hash<auto> opt, bool cache = True) {
        if (cache)
            getConstraintsUnlocked();
        if (constraints.empty())
            return;
        bool ix = uniqueIndexCreatesConstraintImpl();
        return map AbstractDatabase::doCallback(opt, $1.getCreateSql(name, opt), AbstractDatabase::AC_Add, "constraint", $1.getName(), name), constraints.iterator(), !ix || !$1.getIndex();
    }

    private *list<auto> getCreateForeignConstraintsSqlUnlocked(*hash<auto> opt, bool cache = True) {
        if (cache)
            getForeignConstraintsUnlocked(opt);
        if (foreignConstraints.empty())
            return;
        return map AbstractDatabase::doCallback(opt, $1.getCreateSql(name, opt), AbstractDatabase::AC_Add, "foreign constraint", $1.getName(), name), foreignConstraints.iterator();
    }

    private *list<auto> getCreateMiscSqlUnlocked(*hash<auto> opt, bool cache = True) {
        return getCreateMiscSqlImpl(opt, cache);
    }

    private *list<auto> getCreateTriggersSqlUnlocked(*hash<auto> opt, bool cache = True) {
        if (cache)
            getTriggersUnlocked();
        if (triggers.empty())
            return;

        list<auto> l = ();
        map l += AbstractDatabase::doCallback(opt, $1.getCreateSql(name, opt), AbstractDatabase::AC_Add, "trigger", $1.name, name), triggers.iterator();
        return l;
    }

    private list<auto> getCreateSqlUnlocked(*hash<auto> opt, bool cache = True) {
        # make sure driver options take precedence over generic options
        if (opt) {
            string dn = getDriverName();
            AbstractDatabase::checkDriverOptions(\opt, dn);
        }

        # mark in options that the entire table is being created
        opt.create_table_all = True;
        list<auto> l += getCreateTableSqlUnlocked(opt);
        *list tl;
        if (!opt.omit.indexes) {
            tl = getCreateIndexesSqlUnlocked(opt, cache);
            if (tl) l += tl;
        }
        *string sql = getCreatePrimaryKeySqlUnlocked(opt, cache); if (sql) l += sql;

        if (!opt.omit.foreign_constraints) {
            tl = getCreateForeignConstraintsSqlUnlocked(opt, cache);
            if (tl) l += tl;
        }
        tl = getCreateConstraintsSqlUnlocked(opt, cache); if (tl) l += tl;
        tl = getCreateMiscSqlUnlocked(opt, cache); if (tl) l += tl;
        if (!opt.omit.triggers) {
            tl = getCreateTriggersSqlUnlocked(opt, cache);
            if (tl) l += tl;
        }

        return l;
    }

    private cacheUnlocked(*hash<auto> opt) {
        getColumnsUnlocked();
        getPrimaryKeyUnlocked();
        getIndexesUnlocked();
        getForeignConstraintsUnlocked(opt);
        getConstraintsUnlocked();
        getTriggersUnlocked();
    }

    private auto execData(*hash<auto> opt, string sql, *list<auto> args) {
        if (opt.sqlarg_callback)
            opt.sqlarg_callback(sql, args);
        return ds.vexec(sql, args);
    }

    private execData(AbstractSQLStatement stmt, *hash<auto> opt, *list<auto> args) {
        if (opt.sqlarg_callback)
            opt.sqlarg_callback(stmt.getSQL(), args);
        on_error rethrow $1.err, sprintf("%s (SQL: %s)", $1.desc, stmt.getSQL());
        stmt.execArgs(args);
    }

    static AbstractTable getTable(AbstractDatasource nds, string nname, *hash<auto> opts) {
        string drv;
        string mn = AbstractDatabase::loadModule(nds, \drv);

        # try to get AbstractTable object
        try {
            return call_function(mn + "::" + "get_table", nds, nname, opts);
        } catch (hash<ExceptionInfo> ex) {
            rethrow "TABLE-DRIVER-ERROR", sprintf("%s: error retrieving AbstractTable object for driver %y: %s: %s",
                get_ex_pos(ex), drv, ex.err, ex.desc);
        }
    }

    static AbstractTable getTable(string dsstr, string nname, *hash<auto> opts) {
        Datasource nds(dsstr);
        return AbstractTable::getTable(nds, nname, opts);
    }

    static AbstractTable getTable(hash<auto> dsh, string nname, *hash<auto> opts) {
        Datasource nds(dsh);
        return AbstractTable::getTable(nds, nname, opts);
    }

    private getColumnsUnlocked() {
        if (columns)
            return;
        if (manual) {
            columns = new Columns();
            return;
        }
        columns = describeImpl();
        inDb = True;
    }

    private getPrimaryKeyUnlocked() {
        if (primaryKey || manual)
            return;
        getColumnsUnlocked();
        primaryKey = getPrimaryKeyImpl();

        # get indexes
        if (!manual && !indexes)
            indexes = getIndexesImpl();

        if (!primaryKey.empty())
            primaryKey.findMatchingIndex(indexes);

        inDb = True;
    }

    # also loads primary key and constraints (for unique constraints)
    private getIndexesUnlocked() {
        if (indexes)
            return;
        if (manual) {
            indexes = new Indexes();
            return;
        }
        # make sure columns are loaded
        getColumnsUnlocked();
        if (!manual) {
            # make sure any primary key is loaded
            getPrimaryKeyUnlocked();
            # make sure constraints are also loaded
            getConstraintsUnlocked();
        }
        indexes = getIndexesImpl();
        inDb = True;
    }

    private getForeignConstraintsUnlocked(*hash<auto> opt) {
        if (foreignConstraints) {
            return;
        }
        if (manual) {
            foreignConstraints = new ForeignConstraints();
            return;
        }

        getColumnsUnlocked();
        foreignConstraints = getForeignConstraintsImpl(opt);

        # update cached tables with foreign constraint source info
        if (opt.table_cache) {
            foreach AbstractForeignConstraint fk in (foreignConstraints.iterator()) {
                if (!opt.table_cache.hasKey(fk.target.table)) {
                    Table t(ds, fk.target.table);
                    opt.table_cache.add(fk.target.table, t.getTable());
                    #printf("ERROR: missing %y in table cache (adding fk %y to %y)\n", fk.target.table, fk.getName(),
                    #    getName());
                }
                opt.table_cache{fk.target.table}.addSourceConstraint(getSqlName(), fk);
            }
        }
        inDb = True;
    }

    private addSourceConstraint(string table_name, AbstractForeignConstraint fk) {
        bool lock = !l.lockOwner();
        if (lock) l.lock();
        on_exit if (lock) l.unlock();

        getAllConstraintsUnlocked();

        # find unique constraint with the given columns
        if (!primaryKey.empty() && primaryKey.matchKeys(fk.target.columns)) {
            primaryKey.addSourceConstraint(table_name, fk);
            return;
        }
        foreach AbstractConstraint c in (constraints.iterator()) {
            if (!(c instanceof AbstractUniqueConstraint))
                continue;
            AbstractUniqueConstraint uk = cast<AbstractUniqueConstraint>(c);
            if (uk.matchKeys(fk.target.columns)) {
                uk.addSourceConstraint(table_name, fk);
                return;
            }
        }
    }

    private getConstraintsUnlocked() {
        if (constraints) {
            return;
        }
        if (manual) {
            constraints = new Constraints();
            return;
        }
        getColumnsUnlocked();
        constraints = getConstraintsImpl();
        # get indexes
        if (!manual && !indexes) {
            indexes = getIndexesImpl();
        }

        foreach AbstractConstraint c in (constraints.iterator()) {
            if (!(c instanceof AbstractUniqueConstraint)) {
                continue;
            }
            AbstractUniqueConstraint uk = cast<AbstractUniqueConstraint>(c);
            uk.findMatchingIndex(indexes);
        }

        inDb = True;
    }

    private getTriggersUnlocked() {
        if (triggers)
            return;
        if (manual) {
            triggers = new Triggers();
            return;
        }
        triggers = getTriggersImpl();
        inDb = True;
    }

    #! returns @ref True "True" if the current database driver supports the \c "returning" clause in insert statements, @ref False "False" if not
    private bool hasReturningImpl() {
        return True;
    }

    private softlist<auto> getDropSqlImpl() {
        return sprintf("drop table %s", getSqlName());
    }

    private string getTruncateSqlImpl() {
        return sprintf("truncate table %s", getSqlName());
    }

    #! tries to execute a command so that if an error occurs the current transaction status is not lost
    private auto tryExecArgsImpl(string sql, *softlist<auto> args) {
        return ds.vexec(sql, args);
    }

    #! tries to execute a command so that if an error occurs the current transaction status is not lost
    private auto tryExecRawImpl(string sql) {
        return ds.execRaw(sql);
    }

    #! clears any driver-specific table information
    private clearImpl() {
    }

    private preSetupTableImpl(reference desc, *hash<auto> opt) {
    }

    #! returns the type for number / numeric columns for the database so that data conversions can be handled properly
    /** @since SqlUtil 1.6
    */
    private abstract AbstractDataProviderType getNumericTypeImpl(string type_name, bool nullable, *hash<auto> options);

    private abstract *hash<auto> doReturningImpl(hash<auto> opt, reference<string> sql, list<auto> args);

    private abstract bool emptyImpl();

    #! returns a string for use in SQL queries representing the DB-specific value of the argument; returns NOTHING if the type cannot be converted to an SQL string
    private abstract *string getSqlValueImpl(auto v);

    #! returns @ref True "True" if the table exists in the DB, @ref False "False" if not
    /** this method will only be called if inDb is @ref False "False" and
        should set inDb = True if it returns @ref True "True"
        */
    private abstract bool checkExistenceImpl();

    #! returns @ref True "True" if the database support tablespaces
    private abstract bool supportsTablespacesImpl();

    #! returns @ref True "True" if the database links constraints to indexes (ie dropping the constraint drops the index, etc)
    private abstract bool constraintsLinkedToIndexesImpl();

    #! returns @ref True "True" if the database automatically creates a unique constraint when a unique index is created (ex: mysql)
    private abstract bool uniqueIndexCreatesConstraintImpl();

    private abstract setupTableImpl(hash<auto> desc, *hash<auto> opt);

    private abstract Columns describeImpl();
    private abstract AbstractPrimaryKey getPrimaryKeyImpl();
    #! Always called with the lock held
    private abstract Indexes getIndexesImpl();
    private abstract ForeignConstraints getForeignConstraintsImpl(*hash<auto> opt);
    private abstract Constraints getConstraintsImpl();
    private abstract Triggers getTriggersImpl();

    private abstract string getCreateTableSqlImpl(*hash<auto> opt);
    private abstract *list<auto> getCreateMiscSqlImpl(*hash<auto> opt, bool cache);
    private abstract string getCreateSqlImpl(list<auto> l);
    private abstract string getRenameSqlImpl(string new_name);
    private abstract *list<auto> getAlignSqlImpl(AbstractTable t, *hash<auto> opt);

    private abstract AbstractColumn addColumnImpl(string cname, hash<auto> opt, bool nullable = True);
    private abstract AbstractPrimaryKey addPrimaryKeyImpl(string cname, hash<auto> ch, *hash<auto> opt);
    private abstract AbstractIndex addIndexImpl(string iname, bool enabled, hash<auto> ch, *hash<auto> opt);
    private abstract AbstractForeignConstraint addForeignConstraintImpl(string cname, hash<auto> ch, string table, hash<auto> tch, *hash<auto> opt);
    private abstract AbstractCheckConstraint addCheckConstraintImpl(string cname, string src, *hash<auto> opt);
    private abstract AbstractUniqueConstraint addUniqueConstraintImpl(string cname, hash<auto> ch, *hash<auto> opt);

    private abstract AbstractTrigger addTriggerImpl(string tname, string src, *hash<auto> opt);

    #! tries to insert a row, if there is a duplicate key, then it returns @ref False "False", if successful, returns @ref True "True"
    private abstract bool tryInsertImpl(string sql, hash<auto> row);

    #! returns the qore type -> column type map
    private abstract hash<auto> getQoreTypeMapImpl();

    #! returns the type name -> type description hash
    private abstract hash<auto> getTypeMapImpl();

    #! processes a string for use in SQL select statements when there is an "order by" and "offset" argument
    private abstract doSelectOrderByWithOffsetSqlInternImpl(reference<hash<QueryInfo>> info, reference<string> sql,
        list<auto> coll);

    #! processes a string for use in SQL select statements when there is a "limit" argument, but no "orderby" or "offset" arguments
    private abstract doSelectLimitOnlyInternImpl(reference<hash<QueryInfo>> info, reference<string> sql);

    #! db-specific copy actions
    private abstract copyImpl(AbstractTable old);

    #! get DB-specific savepoint helper
    /** @param savepoint the savepoint string, if not given a unique savepoint string will be generated

        @since SqlUtil 1.6
    */
    private abstract AbstractSavepointHelper getSavepointHelperImpl(*string savepoint);

    #! Returns True if the exception was raised because of a duplicate row / key error
    /** @since SqlUtil 1.7.3
    */
    private abstract bool isDuplicateRowErrorImpl(hash<ExceptionInfo> ex);
}
}
