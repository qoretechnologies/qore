# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DbViewDataProvider class definition

/** DbViewDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the DbDataProvider module
public namespace DbDataProvider {
#! Defines a read-only data provider based on a single database view
/** The provider reads the view with the same search conditions, expressions and search options as
    @ref DbDataProvider::DbTableDataProvider "DbTableDataProvider", except the \c forupdate and \c partition options,
    which do not apply to a view.  Records cannot be created, updated, upserted or deleted, and the fields of the
    view cannot be changed; the provider's capabilities advertise reading only.

    The record type is the description of the view's columns from the database.

    @par Example:
    @code{.py}
DbDataProvider db(ds);
AbstractDataProvider view = db.getChildProviderPath("views/v_orders");
AbstractDataProviderRecordIterator i = view.searchRecords({"status": "open"});
    @endcode

    @since DbDataProvider 2.5
*/
public class DbViewDataProvider inherits DbDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "DbViewDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "supports_bulk_read": True,
            "bulk_read_shapes": (DataProviderShape::BufferColumns, DataProviderShape::HashOfLists),
            "supports_children": False,
            "supports_search_expressions": True,
            "constructor_options": ConstructorOptions,
            "search_options": SearchOptions,
            "expressions": DbTableDataProvider::ProviderInfo.expressions,
            "transaction_management": False,
            "has_record": True,
        };

        #! Constructor options
        const ConstructorOptions = {
            "datasource": <DataProviderOptionInfo>{
                "display_name": "Datasource",
                "short_desc": "The datasource connection string to use (or an 'AbstractDatasource' object)",
                "type": (
                    AbstractDataProviderType::get(StringType),
                    AbstractDataProviderType::get(new Type("AbstractDatasource")),
                ),
                "desc": "The datasource connection string or an `AbstractDatasource` object",
                "required": True,
            },
            "view": <DataProviderOptionInfo>{
                "display_name": "View",
                "short_desc": "The name of the view to read",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The name of the view to read, as the database lists it",
                "required": True,
            },
        };

        #! Search options; the search options of @ref DbDataProvider::DbTableDataProvider "DbTableDataProvider" that apply to a view
        /** The \c forupdate and \c partition search options of tables do not apply to a view
        */
        const SearchOptions = DbTableDataProvider::SearchOptions - ("forupdate", "partition");
    }

    private {
        #! the table object used to read the view
        AbstractTable table;
    }

    #! Creates the object from a table object for the view
    /** @param db the database
        @param table the SqlUtil table object for the view, used for reading only
        @param logger an optional logger

        @note the caller must check that \a table names a view of the kind of this provider; use the
        @ref DbDataProvider::DbViewsDataProvider "DbViewsDataProvider" to acquire a view provider by name
    */
    constructor(AbstractDatabase db, AbstractTable table, *LoggerInterface logger) : DbDataProviderBase(db, logger) {
        self.table = table;
    }

    #! Creates the object from constructor options
    /** @throw CONSTRUCTOR-ERROR missing or invalid options
        @throw VIEW-ERROR the view does not exist
    */
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        AbstractDatasource ds = copts.datasource instanceof AbstractDatasource
            ? copts.datasource
            : DbDataProviderBase::getDatasource(copts.datasource);
        db = AbstractDatabase::getDatabase(ds);
        if (!inlist(copts.view, getRelationNames())) {
            throw "VIEW-ERROR", sprintf("%s %y does not exist in %y", getRelationKind(), copts.view,
                db.getDatasourceDesc());
        }
        table = AbstractTable::getTable(ds, copts.view);
    }

    #! Returns the data provider name
    string getName() {
        return table.getName();
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("Read-only record-based data provider for db %s `%s`; supports read/search and bulk read "
            "operations", getRelationKind(), table.getSqlName());
    }

    #! Returns data provider info
    hash<DataProviderInfo> getInfo() {
        hash<DataProviderInfo> rv = AbstractDataProvider::getInfo();
        rv.expressions = DbTableReadHelper::getTableExpressionInfo(table);
        return rv;
    }

    #! Returns expression metadata for the current database implementation
    /** @param options provider options; currently unused because expression support is determined by the active
        database implementation

        @return expression metadata annotated with native pushdown capability
    */
    *hash<string, hash<DataProviderExpressionInfo>> getExpressionsWithOptions(*hash<auto> options) {
        return DbTableReadHelper::getTableExpressionInfo(table);
    }

    #! Returns @ref False; the provider only reads, so it does not need transaction management
    bool requiresTransactionManagement() {
        return False;
    }

    #! Returns the names of the relations of the kind of this provider in the database
    /** @return the names of the views in the database, as @ref SqlUtil::AbstractDatabase::listViews()
        "AbstractDatabase::listViews()" lists them
    */
    list<string> getRelationNames() {
        return db.listViews();
    }

    #! Returns the kind of relation read by this provider
    string getRelationKind() {
        return "view";
    }

    #! Returns the description of the record type, if any
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return table.getRecordType();
    }

    #! Returns a single record matching the search options
    /** @param where_cond the search criteria; will be processed by processFieldValues()
        @param search_options the search options (see @ref SearchOptions) after processing by validateSearchOptions()

        @throw MULTIPLE-RECORDS-ERROR multiple records found
    */
    private *hash<auto> searchSingleRecordImpl(*hash<auto> where_cond, *hash<auto> search_options) {
        return DbTableReadHelper::searchSingleRecord(table, where_cond, search_options);
    }

    #! Returns a bulk record interface for zero or more records matching the search options
    /** @param block_size the number of records in a read block; must be a positive number
        @param where_cond the search criteria; will be processed by processFieldValues()
        @param search_options the search options (see @ref SearchOptions); will be processed by validateSearchOptions()
    */
    private AbstractDataProviderBulkRecordInterface searchRecordsBulkImpl(int block_size = 1000,
            *hash<auto> where_cond, *hash<auto> search_options) {
        return DbTableReadHelper::getBulkReadInterface(table, block_size, where_cond, search_options);
    }

    #! Returns an iterator for zero or more records matching the search options
    /** @param where_cond the search criteria; will be processed by processFieldValues()
        @param search_options the search options (see @ref SearchOptions) after processing by validateSearchOptions()

        @note the iterator releases any transaction lock that it acquires when the end of the data is reached or when
        it is destroyed
    */
    private AbstractDataProviderRecordIterator<hash<auto>> searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        return DbTableReadHelper::getReadIterator(table, where_cond, search_options);
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns @ref True if the current thread is in a transaction
    private bool currentThreadInTransactionImpl() {
        return table.getDatasource().currentThreadInTransaction();
    }
}

#! Defines a read-only data provider based on a single materialized view
/** The provider reads the materialized view's stored data as @ref DbDataProvider::DbViewDataProvider
    "DbViewDataProvider" reads a view; the materialized view is not refreshed.

    @since DbDataProvider 2.5
*/
public class DbMaterializedViewDataProvider inherits DbViewDataProvider {
    public {
        #! Provider info
        const ProviderInfo = cast<hash<DataProviderInfo>>(DbViewDataProvider::ProviderInfo + {
            "type": "DbMaterializedViewDataProvider",
            "constructor_options": ConstructorOptions,
        });

        #! Constructor options
        const ConstructorOptions = DbViewDataProvider::ConstructorOptions + {
            "view": <DataProviderOptionInfo>{
                "display_name": "Materialized View",
                "short_desc": "The name of the materialized view to read",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The name of the materialized view to read, as the database lists it",
                "required": True,
            },
        };
    }

    #! Creates the object from a table object for the materialized view
    /** @param db the database
        @param table the SqlUtil table object for the materialized view, used for reading only
        @param logger an optional logger
    */
    constructor(AbstractDatabase db, AbstractTable table, *LoggerInterface logger)
            : DbViewDataProvider(db, table, logger) {
    }

    #! Creates the object from constructor options
    /** @throw CONSTRUCTOR-ERROR missing or invalid options
        @throw VIEW-ERROR the materialized view does not exist
    */
    constructor(*hash<auto> options) : DbViewDataProvider(options) {
    }

    #! Returns the names of the materialized views in the database
    /** @return the names of the materialized views in the database, as
        @ref SqlUtil::AbstractDatabase::listMaterializedViews() "AbstractDatabase::listMaterializedViews()" lists
        them; empty if the database does not support materialized views
    */
    list<string> getRelationNames() {
        return db.supportsMaterializedViews() ? db.listMaterializedViews() : ();
    }

    #! Returns the kind of relation read by this provider
    string getRelationKind() {
        return "materialized view";
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
