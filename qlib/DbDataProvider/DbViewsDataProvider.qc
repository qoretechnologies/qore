# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DbViewsDataProvider class definition

/** DbViewsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! The database views data provider class, provides views as read-only children
/** The children are the views that @ref SqlUtil::AbstractDatabase::listViews() "AbstractDatabase::listViews()"
    lists; each child is a @ref DbDataProvider::DbViewDataProvider "DbViewDataProvider".  This provider is the
    \c views child of @ref DbDataProvider::DbDataProvider "DbDataProvider".

    @since DbDataProvider 2.5
*/
public class DbViewsDataProvider inherits DbDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "views",
            "type": "DbViewsDataProvider",
            "supports_children": True,
            "constructor_options": ConstructorOptions,
            "children_can_support_records": True,
            "children_identical": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    #! creates the object
    constructor(AbstractDatasource ds, *hash<auto> opts) : DbDataProviderBase(ds, opts) {
    }

    #! creates the object
    constructor(AbstractDatabase db, *LoggerInterface logger) : DbDataProviderBase(db, logger) {
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : DbDataProviderBase(options) {
    }

    #! Returns the data provider name
    string getName() {
        return db.getDatasourceDesc();
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("Read-only data providers for the views of database `%s`", db.getDatasourceDesc());
    }

    #! Returns @ref False; the children only read, so they do not need transaction management
    bool requiresTransactionManagement() {
        return False;
    }

    #! Return data provider summary info
    *list<hash<DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        return map getChildSummaryInfo($1), getChildProviderNames();
    }

    #! Returns the summary info of the child provider for the given relation
    private hash<DataProviderSummaryInfo> getChildSummaryInfo(string name) {
        return cast<hash<DataProviderSummaryInfo>>(DbViewDataProvider::ProviderInfo{DataProviderSummaryInfoKeys}
            + {
            "name": name,
            "desc": sprintf("Read-only record-based data provider for db view `%s`; supports read/search and bulk "
                "read operations", name),
            "type": "DbViewDataProvider",
            "supports_children": False,
            "children_can_support_records": False,
        });
    }

    #! Returns a list of child data provider names, if any
    /** @return the names of the views in the database
    */
    private *list<string> getChildProviderNamesImpl() {
        return db.listViews();
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    /** @return the given child provider or @ref nothing if the given child is not a view

        @throw CHILD-PROVIDER-ERROR error acquiring child provider
    */
    private *AbstractDataProvider getChildProviderImpl(string name) {
        try {
            if (!inlist(name, getChildProviderNamesImpl())) {
                return;
            }
            return makeChild(AbstractTable::getTable(db.getDatasource(), name));
        } catch (hash<ExceptionInfo> ex) {
            rethrow "CHILD-PROVIDER-ERROR", sprintf("view %y cannot be acquired from %y: %s: %s", name, getName(),
                ex.err, ex.desc), ex.arg;
        }
    }

    #! Returns the child provider for the given table object
    private AbstractDataProvider makeChild(AbstractTable table) {
        return new DbViewDataProvider(db, table, logger);
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! The database materialized views data provider class, provides materialized views as read-only children
/** The children are the materialized views that
    @ref SqlUtil::AbstractDatabase::listMaterializedViews() "AbstractDatabase::listMaterializedViews()" lists; each
    child is a @ref DbDataProvider::DbMaterializedViewDataProvider "DbMaterializedViewDataProvider".  This provider is
    the \c materialized-views child of @ref DbDataProvider::DbDataProvider "DbDataProvider", which has the child only
    if the database supports materialized views
    (@ref SqlUtil::AbstractDatabase::supportsMaterializedViews() "AbstractDatabase::supportsMaterializedViews()").

    @since DbDataProvider 2.5
*/
public class DbMaterializedViewsDataProvider inherits DbViewsDataProvider {
    public {
        #! Provider info
        const ProviderInfo = cast<hash<DataProviderInfo>>(DbViewsDataProvider::ProviderInfo + {
            "name": "materialized-views",
            "type": "DbMaterializedViewsDataProvider",
        });

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    #! creates the object
    constructor(AbstractDatasource ds, *hash<auto> opts) : DbViewsDataProvider(ds, opts) {
    }

    #! creates the object
    constructor(AbstractDatabase db, *LoggerInterface logger) : DbViewsDataProvider(db, logger) {
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : DbViewsDataProvider(options) {
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("Read-only data providers for the materialized views of database `%s`",
            db.getDatasourceDesc());
    }

    #! Returns the summary info of the child provider for the given relation
    private hash<DataProviderSummaryInfo> getChildSummaryInfo(string name) {
        return cast<hash<DataProviderSummaryInfo>>(
            DbMaterializedViewDataProvider::ProviderInfo{DataProviderSummaryInfoKeys} + {
            "name": name,
            "desc": sprintf("Read-only record-based data provider for db materialized view `%s`; supports "
                "read/search and bulk read operations", name),
            "type": "DbMaterializedViewDataProvider",
            "supports_children": False,
            "children_can_support_records": False,
        });
    }

    #! Returns a list of child data provider names, if any
    /** @return the names of the materialized views in the database; empty if the database does not support them
    */
    private *list<string> getChildProviderNamesImpl() {
        return db.supportsMaterializedViews() ? db.listMaterializedViews() : ();
    }

    #! Returns the child provider for the given table object
    private AbstractDataProvider makeChild(AbstractTable table) {
        return new DbMaterializedViewDataProvider(db, table, logger);
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
