# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DbDataProvider class definition

/** DbDataProvider.qc Copyright 2019 - 2024 Qore Technologies, s.r.o.

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
#! The database data provider class, provides tables as children
/** Generic post-SQL search is provided just for completeness sake, but record filtering should be done in the \c sql
    search option and not in the generic post-SQL generic data provider search for performance reasons.
*/
public class DbDataProvider inherits DbDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "DbDataProvider",
            "supports_read": True,
            "supports_bulk_read": True,
            "supports_children": True,
            "constructor_options": ConstructorOptions,
            "search_options": SearchOptions,
            "create_options": NOTHING,
            "upsert_options": NOTHING,
            "child_create_options": ChildCreateOptions,
            "supports_search_expressions": True,
            "expressions": AbstractDataProvider::GenericExpressions,
            "transaction_management": True,
            "has_record": True,
            "record_requires_search_options": True,
            "supports_child_create": True,
            "supports_child_delete": True,
            "children_can_support_apis": True,
            "children_can_support_records": True,
            "children_can_support_transactions": True,
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
                "desc": "The datasource connection string or `AbstractDatasource` object",
                "required": True,
            },
        };

        #! Search options
        const SearchOptions = {
            "sql": <DataProviderOptionInfo>{
                "display_name": "SQL Query",
                "short_desc": "The raw SQL query for the select statement",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The raw SQL query for the select statement; see "
                    "[SQL Binding](https://qoretechnologies.com/manual/qorus/current/qore/lang/html/class_qore_1_1_s_q_l_1_1_datasource.html#sql_binding)"
                    " for more information on placeholders to use in the string for SQL binding",
                "required": True,
            },
            "args": <DataProviderOptionInfo>{
                "display_name": "Bind Args",
                "short_desc": "An optional list of bind arguments for placeholders in the SQL query",
                "type": AbstractDataProviderType::get(AutoListType),
                "desc": "Any bind arguments for the select statement; see "
                    "[SQL Binding](https://qoretechnologies.com/manual/qorus/current/qore/lang/html/class_qore_1_1_s_q_l_1_1_datasource.html#sql_binding)"
                    " for more information on bind arguments",
            },
        };

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

    private {
        #! Child map
        const ChildMap = {
            "exec-sql": Class::forName("DbDataProvider::DbExecSqlDataProvider"),
            "tables": Class::forName("DbDataProvider::DbTablesDataProvider"),
        };
    }

    #! creates the object
    constructor(AbstractDatasource ds, *hash<auto> opts) : DbDataProviderBase(ds, opts){
    }

    #! creates the object
    constructor(AbstractDatabase db) : DbDataProviderBase(db) {
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

    #! Returns the record type description from a describe hash
    /** @param describe_hash a hash as returned by
        @ref Qore::SQL::AbstractSQLStatement::describe() "AbstractSQLStatement::describe()"

        @return the record type of the query results
    */
    static *hash<string, AbstractDataField> getRecordTypeFromDescribeHash(hash<auto> describe_hash) {
        HashDataType input_data();
        foreach hash<auto> i in (describe_hash.pairIterator()) {
            # get input column name
            string key = i.key.lwr();
            string desc = i.value.native_type.lwr();
            # add DB maximum size to desc
            if (i.value.maxsize) {
                desc += sprintf("(%d)", i.value.maxsize);
            }
            #printf("i: %y\n", i);
            *string type = DataProvider::OptimalQoreDataTypeMap{TypeCodeMap{i.value.type}};
            AbstractDataProviderType data_type;
            if (!type.val() || type == "any") {
                data_type = AbstractDataProviderType::get(AutoType);
            } else {
                # maps standard types to auto types to avoid type stripping
                if (type =~ /(hash|list)$/) {
                    type += "<auto>";
                }
                data_type = AbstractDataProviderType::get(type);
            }
            input_data.addField(new QoreDataField(key, desc, data_type));
        }
        return input_data.getFields();
    }

    #! Returns the description of the record type, if any
    /** @param search_options to be included and processed by validateSearchOptions() if
        \a recordRequiresSearchOptions() is @ref True for this provider, otherwise any value provided in this argument
        is ignored
    */
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        AbstractDatasource ds = db.getDatasource();
        bool in_trans = ds.currentThreadInTransaction();
        on_error {
            if (!in_trans) {
                ds.rollback();
            }
        }
        on_success {
            if (!in_trans) {
                ds.commit();
            }
        }

        AbstractSQLStatement stmt = ds.getSQLStatement();
        stmt.prepare(search_options.sql);
        if (search_options.args) {
            stmt.bindArgs(search_options.args);
        }
        return getRecordTypeFromDescribeHash(stmt.describe());
    }

    #! Returns an iterator for zero or more records matching the search options
    /** @param block_size the number of records in a read block; must be a positive number
        @param where_cond the search criteria after processing by processFieldValues()
        @param search_options the search options after processing by validateSearchOptions()

        @throw INVALID-BLOCK-SIZE the block size must be a positive number
        @throw INVALID-OPERATION the data provider does not support reading
    */
    private AbstractDataProviderBulkRecordInterface searchRecordsBulkImpl(int block_size = 1000,
            *hash<auto> where_cond, *hash<auto> search_options) {
        return new DbSelectBulkRecordInterface(block_size, db.getDatasource(), where_cond, search_options);
    }

    #! Returns an iterator for zero or more records matching the search options
    /** @param where_cond the search criteria
        @param search_options the search options after processing by validateSearchOptions()

        @see requestSearchRecordsImpl()
    */
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond, *hash<auto> search_options) {
        return new DbSelectRecordIterator(db.getDatasource(), where_cond, search_options);
    }

    #! Return data provider summary info
    *list<hash<DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        return map $1.getConstant("ProviderSummaryInfo").getValue(), ChildMap.iterator();
    }

    #! Returns a list of child data provider names, if any
    /** @return a list of child data provider names, if any
    */
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    /** @return the given child provider or @ref nothing if the given child is unknown

        @throw CHILD-PROVIDER-ERROR error acquiring child provider

        @see getChildProviderEx()
    */
    private *AbstractDataProvider getChildProviderImpl(string name) {
        if (*Class cls = ChildMap{name}) {
            return cls.newObject(db, logger);
        }
        # for backwards compatibility, we try to get a table object
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

        @since DataProvider 2.0
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

        @since DataProvider 2.0
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
