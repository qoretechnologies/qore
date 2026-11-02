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

        #! the name of the view as the database lists it
        string relation_name;
    }

    #! Creates the object from a table object for the view
    /** @param db the database
        @param table the SqlUtil table object for the view, used for reading only
        @param logger an optional logger
        @param name the name of the view as the database lists it; if not given, the name of \a table

        @note the caller must check that \a table names a view of the kind of this provider; use the
        @ref DbDataProvider::DbViewsDataProvider "DbViewsDataProvider" to acquire a view provider by name
    */
    constructor(AbstractDatabase db, AbstractTable table, *LoggerInterface logger, *string name)
            : DbDataProviderBase(db, logger) {
        self.table = table;
        relation_name = name ?? table.getName();
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
        relation_name = copts.view;
    }

    #! Returns the data provider name
    string getName() {
        return relation_name;
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
    /** @return the names of the views in the database, as
        @ref SqlUtil::AbstractDatabase::listViews() "AbstractDatabase::listViews()" lists them
    */
    list<string> getRelationNames() {
        return db.listViews();
    }

    #! Returns the kind of relation read by this provider
    string getRelationKind() {
        return "view";
    }

    #! Returns the search condition for a where hash, which may be a SqlUtil where clause
    /** A where hash with a SqlUtil operator (see @ref sql_op_funcs "SqlUtil operator functions", such as
        @ref SqlUtil::op_gt() "op_gt()", and @ref SqlUtil::wop_or() "wop_or()") is a SqlUtil where clause: it is
        evaluated by SqlUtil, with SqlUtil's semantics, such as @ref SqlUtil::op_ne() "op_ne()" matching null values
        too; only its plain values are converted to the types of their fields.  Any other search condition is
        processed as by any data provider.

        @param where_cond the search condition
        @param search_options the search options

        @return the search condition to give to the view

        @throw INVALID-FIELD a key of a SqlUtil where clause names no field of the view

        @since DbDataProvider 2.5
    */
    *hash<auto> getSearchExpression(*hash<auto> where_cond, *hash<auto> search_options) {
        if (DbTableReadHelper::isSqlUtilWhere(where_cond)) {
            return DbTableReadHelper::processSqlUtilWhere(getName(), table.getRecordType(), where_cond);
        }
        return AbstractDataProvider::getSearchExpression(where_cond, search_options);
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

#! Defines a data provider based on a single materialized view: its records are read-only, and a request refreshes it
/** The provider reads the materialized view's stored data as
    @ref DbDataProvider::DbViewDataProvider "DbViewDataProvider" reads a view; reading does not refresh it.  Its records cannot be written.

    A request (@ref DataProvider::AbstractDataProvider::doRequest() "doRequest()") refreshes the materialized view
    with @ref SqlUtil::AbstractDatabase::refreshMaterializedView() "AbstractDatabase::refreshMaterializedView()"; the
    request type has the refresh options of the database (see getRequestType()):
    - PostgreSQL: \c concurrent
    - Oracle: \c method and \c atomic_refresh

    @par Example:
    @code{.py}
DbDataProvider db(ds);
AbstractDataProvider mv = db.getChildProviderPath("materialized-views/mv_sales_by_region");
hash<auto> rv = mv.doRequest({"concurrent": True});
    @endcode

    @since DbDataProvider 2.5
*/
public class DbMaterializedViewDataProvider inherits DbViewDataProvider {
    public {
        #! Provider info
        const ProviderInfo = cast<hash<DataProviderInfo>>(DbViewDataProvider::ProviderInfo + {
            "type": "DbMaterializedViewDataProvider",
            "constructor_options": ConstructorOptions,
            # a request refreshes the materialized view
            "supports_request": True,
        });

        #! Request type: the refresh options of all databases; see getRequestType() for those of the database
        const RequestType = DbMaterializedViewRefreshRequestType;

        #! Response type
        const ResponseType = DbMaterializedViewRefreshResponseType;

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
    constructor(AbstractDatabase db, AbstractTable table, *LoggerInterface logger, *string name)
            : DbViewDataProvider(db, table, logger, name) {
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

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("Record-based data provider for db materialized view `%s`; supports read/search and bulk "
            "read operations on its stored data, which is read-only, and refreshing it with a request",
            table.getSqlName());
    }

    #! Refreshes the materialized view
    /** @param req the refresh options of the database; see getRequestType()
        @param request_options the request options after processing by validateRequestOptions()

        @return a hash with the name of the materialized view refreshed and the options used; see
        @ref DbDataProvider::DbMaterializedViewRefreshResponseType "DbMaterializedViewRefreshResponseType"

        @throw MATERIALIZED-VIEW-ERROR the materialized view cannot be refreshed; see
        @ref SqlUtil::AbstractDatabase::refreshMaterializedView() "AbstractDatabase::refreshMaterializedView()"
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        # the request is validated against the request type, a hash of the database's refresh options
        *hash<auto> opts = req.typeCode() == NT_HASH ? req : NOTHING;
        db.refreshMaterializedView(relation_name, opts);
        return {
            "materialized_view": relation_name,
            "options": opts ?? {},
        };
    }

    #! Returns the request type: the refresh options of the database
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new DbMaterializedViewRefreshRequestType(keys db.getMaterializedViewRefreshOptions());
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns the options of an action of this provider: for a request, only the refresh options of the database
    /** The \c refresh-materialized-view action is registered with the refresh options of all databases; the options
        of the action for this provider are those of its database

        @param action the action
        @param options the option values given so far

        @return the action options
    */
    private *hash<string, hash<ActionOptionInfo>> tryGetActionOptionsForRequestWithOptionsImpl(
            hash<DataProviderActionInfo> action, *hash<auto> options) {
        *hash<string, hash<ActionOptionInfo>> rv = DbViewDataProvider::tryGetActionOptionsForRequestWithOptionsImpl(
            action, options);
        hash<string, bool> db_opts = map {$1: True}, keys db.getMaterializedViewRefreshOptions();
        return map {$1.key: $1.value}, rv.pairIterator(),
            db_opts{$1.key} || !DbMaterializedViewRefreshRequestType::Fields.hasKey($1.key);
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! The request type of a refresh of a materialized view: the refresh options of the database
/** The fields are the options of
    @ref SqlUtil::AbstractDatabase::refreshMaterializedView() "AbstractDatabase::refreshMaterializedView()"; the type of a materialized view's provider has only those of its
    database.

    @since DbDataProvider 2.5
*/
public class DbMaterializedViewRefreshRequestType inherits HashDataType {
    public {
        #! The fields of all databases
        const Fields = {
            "concurrent": {
                "display_name": "Concurrent Refresh",
                "short_desc": "Refresh without blocking readers (PostgreSQL)",
                "type": AbstractDataProviderTypeMap."*bool",
                "desc": "PostgreSQL only: if `True`, the materialized view is refreshed with "
                    "`REFRESH MATERIALIZED VIEW CONCURRENTLY`, which does not block readers.\n\n"
                    "**Requirements:** the materialized view must have a unique index that uses only column names "
                    "and has no `WHERE` clause, and it must already be populated.\n\n"
                    "If `False` (the default), the refresh locks the materialized view against reads until it "
                    "completes.",
                "example_value": True,
            },
            "method": {
                "display_name": "Refresh Method",
                "short_desc": "How the stored data is refreshed (Oracle)",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Oracle only: the refresh method of `DBMS_MVIEW.REFRESH`; the default is `force`.",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "force",
                        "display_name": "Force",
                        "short_desc": "Fast refresh if possible, otherwise complete",
                        "desc": "A fast refresh if it is possible, otherwise a complete refresh",
                    },
                    <AllowedValueInfo>{
                        "value": "complete",
                        "display_name": "Complete",
                        "short_desc": "Recompute all of the stored data",
                        "desc": "The stored data is recomputed from the defining query",
                    },
                    <AllowedValueInfo>{
                        "value": "fast",
                        "display_name": "Fast",
                        "short_desc": "Apply the changes recorded since the last refresh",
                        "desc": "The changes recorded in the materialized view logs since the last refresh are "
                            "applied; the materialized view must support fast refresh",
                    },
                ),
                "example_value": "force",
            },
            "atomic_refresh": {
                "display_name": "Atomic Refresh",
                "short_desc": "Refresh in a single transaction (Oracle)",
                "type": AbstractDataProviderTypeMap."*bool",
                "desc": "Oracle only: if `True` (the default), the materialized view is refreshed in a single "
                    "transaction, so readers see the previous data until the refresh completes.\n\n"
                    "If `False`, a complete refresh may truncate the materialized view first, which is faster, but "
                    "readers see it empty until the refresh completes.",
                "example_value": True,
            },
        };
    }

    #! Creates the type with the given fields, or all fields
    /** @param field_names the names of the fields; if not given, all fields; names that have no field are ignored
    */
    constructor(*list<string> field_names) : HashDataType("DbMaterializedViewRefreshRequest") {
        addQoreFields(exists field_names ? Fields{field_names} : Fields);
    }
}

#! The request type of a refresh with the fields of all databases
public const DbMaterializedViewRefreshRequestType = new DbMaterializedViewRefreshRequestType();

#! The response type of a refresh of a materialized view
/** @since DbDataProvider 2.5
*/
public class DbMaterializedViewRefreshResponseType inherits HashDataType {
    public {
        #! The fields of the response
        const Fields = {
            "materialized_view": {
                "display_name": "Materialized View",
                "short_desc": "The name of the materialized view refreshed",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The name of the materialized view refreshed, as the database lists it",
                "example_value": "mv_sales_by_region",
            },
            "options": {
                "display_name": "Refresh Options",
                "short_desc": "The refresh options used",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "The refresh options given in the request; options not given take their default values",
                "example_value": {"concurrent": True},
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("DbMaterializedViewRefreshResponse") {
        addQoreFields(Fields);
    }
}

#! The response type of a refresh
public const DbMaterializedViewRefreshResponseType = new DbMaterializedViewRefreshResponseType();
}
