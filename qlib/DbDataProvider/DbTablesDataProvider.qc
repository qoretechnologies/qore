# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DbTablesDataProvider class definition

/** DbTablesDataProvider.qc Copyright 2019 - 2025 Qore Technologies, s.r.o.

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
#! The database tables data provider class, provides tables as children
/** Generic post-SQL search is provided just for completeness sake, but record filtering should be done in the \c sql
    search option and not in the generic post-SQL generic data provider search for performance reasons.
*/
public class DbTablesDataProvider inherits DbDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "tables",
            "type": "DbTablesDataProvider",
            "supports_children": True,
            "constructor_options": ConstructorOptions,
            "supports_child_create": True,
            "supports_child_delete": True,
            "children_can_support_records": True,
            "children_can_support_transactions": True,
            "children_identical": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Child provider creation options
        const ChildCreateOptions = {
            "primary_key": <DataProviderOptionInfo>{
                "display_name": "Primary Key",
                "short_desc": "Sets the list of column names for the primary key",
                "type": AbstractDataProviderType::get("softlist<string>"),
                "desc": "Sets the list of column names for the primary key for the table; use `primary_key_name` to "
                    "set the name of the primary key constraint manually, otherwise a suitable name will be "
                    "automatically generated",
            },
            "primary_key_name": <DataProviderOptionInfo>{
                "display_name": "Primary Key Name",
                "short_desc": "The name of the primary key constraint",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The name of the primary key constraint; only used if `primary_key` is set",
            },
            "indexes": <DataProviderOptionInfo>{
                "display_name": "Indexes",
                "short_desc": "Sets a hash of indexes keyed by index name",
                "type": new DbIndexesDataType(),
                "desc": "Sets a hash of indexes keyed by index name",
            },
            "foreign_constraints": <DataProviderOptionInfo>{
                "display_name": "Foreign Constraints",
                "short_desc": "Sets a hash of foreign constraints keyed by foreign constraint name",
                "type": new DbForeignConstraintsDataType(),
                "desc": "Sets a hash of foreign constraints keyed by foreign constraint name",
            },
        };
    }

    #! creates the object
    constructor(AbstractDatasource ds, *hash<auto> opts) : DbDataProviderBase(ds, opts) {
    }

    #! creates the object
    constructor(AbstractDatabase db, *LoggerInterface logger) : DbDataProviderBase(db) {
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
        return sprintf("Data provider for database `%s`; use the search API with the `sql` and `args` arguments to "
            "execute record-based queries", db.getDatasourceDesc());
    }

    #! Return data provider summary info
    *list<hash<DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        return map cast<hash<DataProviderSummaryInfo>>(DbTableDataProvider::ProviderInfo{DataProviderSummaryInfoKeys}
            + {
            "name": $1,
            "desc": sprintf("Record-based data provider for db table `%s`; supports create, read/search, update, delete, "
                "upsert, and bulk operations", $1),
            "type": "DbTableDataProvider",
            "supports_children": False,
            "children_can_support_records": False,
        }), getChildProviderNames();
    }

    #! Returns a list of child data provider names, if any
    /** @return a list of child data provider names, if any
    */
    private *list<string> getChildProviderNamesImpl() {
        return db.listTables();
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    /** @return the given child provider or @ref nothing if the given child is unknown

        @throw CHILD-PROVIDER-ERROR error acquiring child provider

        @see getChildProviderEx()
    */
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *AbstractTable table;
        try {
            table = db.getTable(name);
            if (!table) {
                return;
            }
        } catch (hash<ExceptionInfo> ex) {
            rethrow "CHILD-PROVIDER-ERROR", sprintf("table %y cannot be acquired from %y: %s: %s", name, getName(),
                ex.err, ex.desc), ex.arg;
        }
        return new DbTableDataProvider(table, logger);
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Creates a new child data provider and returns it after adding as a child
    /** @param name the name of the new child data provider
        @param fields the fields for the new child data provider
        @param child_create_options the options for creating the new child data provider

        @return the new child data provider
    */
    private AbstractDataProvider createChildProviderImpl(string name, hash<string, AbstractDataField> fields,
            *hash<auto> child_create_options) {
        hash<auto> table_desc = makeTableDesc(name, fields, child_create_options);
        AbstractTable table = db.makeTable(name, table_desc);
        DbTableTransactionHelper th(table);
        th.create();
        info("DbDataProvider %y DDL: %s", name, table.getCreateSqlString());
        return new DbTableDataProvider(table, logger);
    }

    #! Deletes a child data provider
    /** @param name the name of the new child data provider
        @param child_delete_options the options for deleting a child data provider

        @throw DELETE-CHILD-PROVIDER-ERROR error dropping the child data provider
    */
    private deleteChildProviderImpl(string name, *hash<auto> child_delete_options) {
        *AbstractTable t = db.getTable(name);
        if (!t) {
            throw "NO-CHILD", sprintf("%s has no child %y", getName(), name);
        }
        DbTableTransactionHelper th(t);
        info("DbDataProvider %y DDL: %s", name, (foldl $1 + "\n" + $2, t.getDropSql()));
        th.drop();
    }
}
}
