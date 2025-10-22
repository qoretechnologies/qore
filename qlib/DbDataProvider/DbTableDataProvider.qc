# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DbTableDataProvider class definition

/** DbTableDataProvider.qc Copyright 2019 - 2025 Qore Technologies, s.r.o.

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
#! Defines a data provider based on a single SQL table
public class DbTableDataProvider inherits DbDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "DbTableDataProvider",
            "supports_read": True,
            "supports_create": True,
            "supports_update": True,
            "supports_upsert": True,
            "supports_delete": True,
            "supports_native_search": True,
            "supports_bulk_read": True,
            "supports_bulk_create": True,
            "supports_bulk_upsert": True,
            "supports_children": False,
            "supports_add_field": True,
            "supports_update_field": True,
            "supports_delete_field": True,
            "supports_search_expressions": True,
            "constructor_options": ConstructorOptions,
            "search_options": SearchOptions,
            "create_options": CreateOptions,
            "upsert_options": UpsertOptions,
            "expressions": (AbstractDataProvider::GenericExpressions - DP_SEARCH_OP_REGEX) + {
                OP_LIKE: <DataProviderExpressionInfo>{
                    "type": DET_Operator,
                    "display_name": "like",
                    "name": OP_LIKE,
                    "symbol": "like",
                    "short_desc": "The SQL 'like' operator",
                    "desc": "The SQL 'like' operator",
                    "args": (DataProviderSignatureStringType, DataProviderSignatureStringValueType),
                    "return_type": AbstractDataProviderTypeMap."bool",
                },
            },
            "transaction_management": True,
            "has_record": True,
            "mapper_keys": MapperKeyInfo,
        };

        #! Mapper runtime key info; see details below
        /** The DbTableDataProvider class supports the following create option:
            - \c sequence: names the DB sequence that will be used to populate the field
            - \c sequence_currval: names the DB sequence that will be used to populate the field; the current value of
              the sequence is used; will not increment the sequence
        */
        const MapperKeyInfo = Mapper::MapperKeyInfo + {
            "sequence": <MapperRuntimeKeyInfo>{
                "desc": "names the DB sequence that will be used to populate the field",
                "value_type": "string",
                "unique_roles": "*",
                "returns_type": "int",
            },
            "sequence_currval": <MapperRuntimeKeyInfo>{
                "desc": "names the DB sequence that will be used to populate the field; the current value of the "
                    "sequence is used; will not increment the sequence",
                "value_type": "string",
                "unique_roles": "*",
                "returns_type": "int",
            },
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
                "desc": "The datasource connection string or an `AbstractDatasource` object; in case a connection "
                    "string is given, then the \"table\" option also needs to be given as a string",
            },
            "table": <DataProviderOptionInfo>{
                "display_name": "Table",
                "short_desc": "The name of the table to use (or an 'AbstractTable' object)",
                "type": (
                    AbstractDataProviderType::get(StringType),
                    AbstractDataProviderType::get(new Type("AbstractTable")),
                ),
                "required": True,
                "desc": "The table name or table object; if a table string is provided, then the \"datasource\" "
                    "option must also be provided",
            },
        };

        #! Create options; see details below
        /** The DbTableDataProvider class supports the following create option:
            - \c returning: a list having elements of one of the two following types:
              - @ref string_type "string": column names to return the value inserted
              - @ref hash_type "hash": a hash having the following keys:
                - \c "key": (required) the column name to return
                - \c "type": (optional) the data type for the output placeholder buffer (ex:
                  @ref Qore::Type::Number "Type::Number")

        */
        const CreateOptions = {
            "returning": <DataProviderOptionInfo>{
                "display_name": "Returning",
                "short_desc": "Indication of any output values to return",
                "type": AbstractDataProviderType::get(AbstractDataProviderType::anyType),
                "desc": "A string (giving the output name) or a hash describing the return parameters for an insert; "
                    "a hash will have the following keys: `key`: (required) the column name to return, and `type` "
                    "(optional) the data type for the output placeholder buffer (ex: `number`)",
            },
        };

        #! Upsert options; see details below
        /** The DbTableDataProvider class supports the following upsert options:
            - \c upsert_strategy: an upsert strategu code; see:
              <a href="../../SqlUtil/html/group__upsert__options.html">SqlUtil upsert strategy codes</a> for more info
            - \c omit_update: allows for an asymmetrical upsert where a set of column values is inserted, but a
              smaller set is updated in case the unique key values are present in the target table; the value of this
              option should be set to the columns to omit in the update clause
        */
        const UpsertOptions = {
            "upsert_strategy": <DataProviderOptionInfo>{
                "display_name": "Upsert Strategy",
                "short_desc": "The upsert strategy to use",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "Indicates the upsert strategy to use",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "UpsertInsertFirst",
                        "desc": "Try to insert first, if it fails, try updating",
                    },
                    <AllowedValueInfo>{
                        "value": "UpsertUpdateFirst",
                        "desc": "Try to update first, if it fails, try inserting",
                    },
                    <AllowedValueInfo>{
                        "value": "UpsertSelectFirst",
                        "desc": "Try to select first, if it fails, insert, if it succeeds, update if necessary",
                    },
                    <AllowedValueInfo>{
                        "value": "UpsertInsertOnly",
                        "desc": "Only insert, never update",
                    },
                     <AllowedValueInfo>{
                        "value": "UpsertUpdateOnly",
                        "desc": "Only update, never insert",
                    },
                     <AllowedValueInfo>{
                        "value": "UpsertAuto",
                        "desc": "Use the most efficient upsert for the underlying DB",
                    },
                ),
                "default_value": "UpsertAuto",
            },
            "omit_update": <DataProviderOptionInfo>{
                "display_name": "Omit Columns In Update",
                "short_desc": "Ignore the given columns when updating",
                "type": AbstractDataProviderType::get(new Type("softlist<string>")),
                "desc": "Allows for an asymmetrical upsert where the complete set of column values is inserted, but "
                    "a smaller set is updated in case the unique key values are present in the target table; the "
                    "value of this option should be set to the columns to omit in the update clause",
            },
        };

        #! Search options; see details below
        /** The DbTableDataProvider class supports the following search options:
            - \c columns: column argument for the select expression; see
              <a href="../../SqlUtil/html/sql_operations.html#select_option_columns">SqlUtil select option columns</a>
              for more information on this argument
            - \c limit: the maximum number of records to return; see
              <a href="../../SqlUtil/html/sql_operations.html#select_option_limit">SqlUtil select option limit</a>
              for more information on this argument
            - \c offset: the offset number in records to return; see
              <a href="../../SqlUtil/html/sql_operations.html#select_option_offset">SqlUtil select option offset</a>
              for more information on this argument
            - \c groupby: group by argument for the select expression; see
              <a href="../../SqlUtil/html/sql_operations.html#select_option_groupby">SqlUtil select option groupby</a>
              for more information on this argument
            - \c having: having argument for the select expression; see
              <a href="../../SqlUtil/html/sql_operations.html#select_option_having">SqlUtil select option having</a>
              for more information on this argument
            - \c orderby: order by argument for the select expression; see
              <a href="../../SqlUtil/html/sql_operations.html#select_option_ordeby">SqlUtil select option orderby</a>
              for more information on this argument
            - \c forupdate: uses FOR UPDATE with the query to lock records selected; see
              <a href="../../SqlUtil/html/sql_operations.html#select_option_forupdate">SqlUtil select option forupdate</a>
              for more information on this argument
        */
        const SearchOptions = {
            "columns": <DataProviderOptionInfo>{
                "display_name": "Columns",
                "short_desc": "Columns to include in the output of the query",
                "type": AbstractDataProviderType::get(AbstractDataProviderType::anyType),
                "desc": "Columns to include in the output of the query",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "The maximum number of records to return",
                "type": AbstractDataProviderType::get(IntType),
                "desc": "The maximum number of records to return",
            },
            "offset": <DataProviderOptionInfo>{
                "display_name": "Offset",
                "short_desc": "The offset in the record list for records to return",
                "type": AbstractDataProviderType::get(IntType),
                "desc": "The offset in the record list for records to return",
            },
            "groupby": <DataProviderOptionInfo>{
                "display_name": "Group By",
                "short_desc": "Field(s) to group results by",
                "type": new QoreListDataType(new Type("softlist<auto>")),
                "desc": "Field(s) to group results by in case there are aggregate expressions in the query; the "
                    "`GROUPBY` argument for the query expression",
            },
            "having": <DataProviderOptionInfo>{
                "display_name": "Having Clause",
                "short_desc": "A 'having' clause for the select expression",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "A `having` clause for the select expression",
            },
            "orderby": <DataProviderOptionInfo>{
                "display_name": "Order By",
                "short_desc": "Fields to order the query result by",
                "type": new QoreListDataType(new Type("softlist<string>")),
                "desc": "Fields to order the query result by; the `ORDERBY` argument for the query expression",
            },
            "forupdate": <DataProviderOptionInfo>{
                "display_name": "For Update",
                "short_desc": "Uses 'FOR UPDATE' with the query to lock records selected",
                "type": AbstractDataProviderType::get(SoftBoolType),
                "desc": "Uses `FOR UPDATE` with the query to lock records selected",
            },
        };

        #! Maps SqlUtil @ref upsert_results to @ref db_provider_upsert_results
        const DbUpsertMap = {
            AbstractTable::UR_Inserted: UpsertResultInserted,
            AbstractTable::UR_Verified: UpsertResultVerified,
            AbstractTable::UR_Updated: UpsertResultUpdated,
            AbstractTable::UR_Unchanged: UpsertResultUnchanged,
            AbstractTable::UR_Deleted: UpsertResultDeleted,
        };
    }

    private {
        #! the table
        AbstractTable table;

        #! lock for "db"
        Mutex db_lock();
    }

    #! Creates the object
    constructor(AbstractTable table, *LoggerInterface logger) : DbDataProviderBase(logger) {
        self.table = table;
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        if (copts.table instanceof AbstractTable) {
            if (copts.datasource instanceof AbstractDatasource && copts.datasource != copts.table.getDatasource()) {
                error("CONSTRUCTOR-ERROR", "option \"table\" is based on a different datasource than the one "
                    "supplied in the \"datasource\" option");
            }
            self.table = copts.table;
            return;
        }
        if (!copts.datasource) {
            error("CONSTRUCTOR-ERROR", "option \"datasource\" is required if no \"table\" option providing "
                "an AbstractTable object is provided");
        }
        table = DbDataProvider::getTable(copts.datasource, copts.table);
    }

    #! Returns the data provider name
    string getName() {
        return table.getName();
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("Record-based data provider for db table `%s`; supports create, read/search, update, delete, "
            "upsert, and bulk operations", table.getSqlName());
    }

    #! Returns data provider info
    hash<DataProviderInfo> getInfo() {
        *hash<string, hash<DataProviderExpressionInfo>> expression_info =
            cast<hash<string, hash<DataProviderExpressionInfo>>>(
                map {$1.key: $1.value.exp}, table.getExpressionMap().pairIterator()
            );

        hash<DataProviderInfo> rv = AbstractDataProvider::getInfo();
        if (expression_info) {
            rv.expressions = expression_info;
        }
        return rv;
    }

    #! Begins a transaction in the datasource underlying the table
    /**
        @see:
        - commit()
        - rollback()

        @since DbDataProvider 1.0.1
    */
    beginTransaction() {
        table.beginTransaction();
    }

    #! Commits data written to the data provider
    /**
        @see:
        - beginTransaction()
        - rollback()
    */
    commit() {
        table.commit();
    }

    #! Rolls back data written to the data provider
    /**
        @see:
        - beginTransaction()
        - commit()
    */
    rollback() {
        table.rollback();
    }

    #! Returns a bulk insert operation object for the data provider
    /** @param block_size the block size for the bulk operation; if not given, the default block size of 1000 rows is
        used

        @return a bulk insert operation object for the data provider
    */
    AbstractDataProviderBulkOperation getBulkInserter(*int block_size) {
        return new DbTableBulkInserter(self, table, block_size);
    }

    #! Returns a bulk upsert operation object for the data provider
    /** @param block_size the block size for the bulk operation; if not given, the default block size of 1000 rows is
        used

        @return a bulk upsert operation object for the data provider
    */
    AbstractDataProviderBulkOperation getBulkUpserter(*int block_size) {
        return new DbTableBulkUpserter(self, table, block_size);
    }

    #! Returns custom data mapper runtime keys
    /** @return custom data mapper runtime keys
    */
    *hash<string, hash<MapperRuntimeKeyInfo>> getMapperRuntimeKeys() {
        # make sure the database object is in place
        if (!db) {
            db_lock.lock();
            on_exit db_lock.unlock();

            if (!db) {
                db = AbstractDatabase::getDatabase(table.getDatasource());
            }
        }
        return DbDataProviderBase::getMapperRuntimeKeys();
    }

    #! Returns the description of the record type, if any
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return table.getRecordType();
    }

    #! Writes the given record to the data provider
    /** @param rec a hash representing a single input record
        @param create_options the create options (see @ref CreateOptions) after processing by validateCreateOptions()

        @return the data written to the data provider plus any output (returning) variables

        @note this API will commit the row to the database unless the datasource was already in a transaction before
        this call
    */
    private *hash<auto> createRecordImpl(hash<auto> rec, *hash<auto> create_options) {
        DbTableTransactionHelper db(table);
        on_error {
            if (db.isDuplicateRowError($1)) {
                rethrow "DUPLICATE-RECORD", sprintf("%s: %s", $1.err, $1.desc), $1.arg;
            }
        }
        *hash<auto> returning = db.insert(rec, create_options);
        return rec + returning;
    }

    #! Creates the given records in the data provider
    /** @param recs a hash representing multiple input records, where the keys are record names and values are either
        lists of record values or a single value that applies to all records
        @param create_options the create options after processing by validateCreateOptions()

        @return the data written to the data provider for each record
    */
    private *list<hash<auto>> createRecordsBulkImpl(hash<auto> recs, *hash<auto> create_options) {
        DbTableTransactionHelper db(table);
        hash<auto> rh;
        if (create_options.returning) {
            create_options.rowcode = sub (hash<auto> h) {
                rh = h;
            };
        }
        BulkInsertOperation bulk(table, create_options + {
            "info_log": sub (string fmt, ...) {
                logArgs(LoggerLevel::LevelInfo, fmt, argv);
            },
        });
        on_error bulk.discard();
        bulk.queueData(recs);
        bulk.flush();
        return map $1, (recs + rh).contextIterator();
    }

    #! Upserts the given record to the data provider
    /** @param rec a hash representing a single input record
        @param upsert_options the upsert options (see @ref UpsertOptions) after processing by validateUpsertOptions()

        @return see @ref db_provider_upsert_results for possible values

        @note this API will commit the row to the database unless the datasource was already in a transaction before
        this call
    */
    string upsertRecordImpl(hash<auto> rec, *hash<auto> upsert_options) {
        DbTableTransactionHelper db(table);
        return DbUpsertMap{db.upsert(rec,
            AbstractTable::UpsertStrategyDescriptionMap{remove upsert_options.upsert_strategy}
            ?? AbstractTable::UpsertAuto, upsert_options)};
    }

    #! Upserts the given records in the data provider
    /** @param recs a hash representing multiple input records, where the keys are record names and values are either
        lists of record values or a single value that applies to all records to be upserted
        @param upsert_options the upsert options; will be processed by validateUpsertOptions()

        @return an array of upsert results corresponding to each record in order; see @ref db_provider_upsert_results
        for possible values
    */
    private *list<string> upsertRecordsBulkImpl(hash<auto> recs, *hash<auto> upsert_options) {
        DbTableTransactionHelper db(table);
        BulkUpsertOperation bulk(table, upsert_options + {
            "info_log": sub (string fmt, ...) {
                logArgs(LoggerLevel::LevelInfo, fmt, argv);
            },
        });
        on_error bulk.discard();
        bulk.queueData(recs);
        bulk.flush();
        return map UpsertResultVerified, xrange(recs.firstValue().lsize());
    }

    #! Returns a single record matching the search options
    /** @param where_cond the search criteria; will be processed by processFieldValues()
        @param search_options the search options (see @ref SearchOptions) after processing by validateSearchOptions()

        @throw MULTIPLE-RECORDS-ERROR multiple records found
    */
    private *hash<auto> searchSingleRecordImpl(*hash<auto> where_cond, *hash<auto> search_options) {
        try {
            return table.selectRow(search_options + {"where": where_cond});
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "DBI-SELECT-ROW-ERROR") {
                rethrow "MULTIPLE-RECORDS-ERROR", ex.desc, ex.arg;
            }
            rethrow;
        }
    }

    #! Returns an iterator for zero or more records matching the search options
    /** @param block_size the number of records in a read block; must be a positive number
        @param where_cond the search criteria; will be processed by processFieldValues()
        @param search_options the search options (see @ref SearchOptions); will be processed by validateSearchOptions()

        @throw INVALID-BLOCK-SIZE the block size must be a positive number
        @throw INVALID-OPERATION the data provider does not support reading
    */
    private AbstractDataProviderBulkRecordInterface searchRecordsBulkImpl(int block_size = 1000,
            *hash<auto> where_cond, *hash<auto> search_options) {
        return new DbTableBulkRecordInterface(block_size, table, where_cond, search_options);
    }

    #! Returns an iterator for zero or more records matching the search options
    /** @param where_cond the search criteria; will be processed by processFieldValues()
        @param search_options the search options (see @ref SearchOptions) after processing by validateSearchOptions()

        @note this method acquires a transaction lock if one is not already acquired; the iterator returned performs a
        rollback in the destructor if a new transaction lock was acquired by this call and
        @ref SqlUtil::DbTableRecordIterator::keepTransactionLock() "DbTableRecordIterator::keepTransactionLock()" is
        not called
    */
    private DbTableRecordIterator searchRecordsImpl(*hash<auto> where_cond, *hash<auto> search_options) {
        return new DbTableRecordIterator(table, where_cond, search_options);
    }

    #! Updates a single record matching the search options
    /** @param set the hash of field data to set
        @param where_cond a hash for identifying the record to be update
        @param search_options the search options (see @ref SearchOptions) after processing by validateSearchOptions()

        @throw UPDATE-SINGLE-RECORD-ERROR if the update would update more than one record

        @note this API will commit the row to the database unless the datasource was already in a transaction before
        this call
    */
    private bool updateSingleRecordImpl(hash<auto> set, hash<auto> where_cond, *hash<auto> search_options) {
        int rows = updateRecordsImpl(set, where_cond, search_options);
        if (rows > 1) {
            bool in_trans = table.getDatasource().currentThreadInTransaction();
            throw "UPDATE-SINGLE-RECORD-ERROR", sprintf("%d record(s) were updated; %ssearch args: %y (set: %y)", rows,
                (!in_trans ? "the update has been rolled back; " : ""), search_options, set);
        }
        return rows.toBool();
    }

    #! Updates zero or more records matching the search options
    /** @param set the hash of field data to set
        @param where_cond a hash for identifying the record(s) to be updated
        @param search_options the search options (see @ref SearchOptions) after processing by validateSearchOptions()

        @return the number of records updated

        @note this API will commit the row to the database unless the datasource was already in a transaction before
        this call
    */
    private int updateRecordsImpl(hash<auto> set, *hash<auto> where_cond, *hash<auto> search_options) {
        DbTableTransactionHelper db(table);
        return db.update(set, where_cond);
    }

    #! Deletes zero or more records
    /**
        @param where_cond a hash for identifying the record(s) to be deleted
        @param search_options the search options (see @ref SearchOptions) after processing by validateSearchOptions()

        @return the number of records deleted

        @throw INVALID-OPERATION the data provider does not support record updating

        @note this API will commit the row to the database unless the datasource was already in a transaction before
        this call
    */
    private int deleteRecordsImpl(*hash<auto> where_cond, *hash<auto> search_options) {
        DbTableTransactionHelper db(table);
        return db.del(where_cond);
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Creates a new field
    /**
        @param field the field to add
        @param field_add_options the options for adding the new field

        @return the new child data provider

        @since DataProvider 2.0
    */
    private addFieldImpl(AbstractDataField field, *hash<auto> field_add_options) {
        hash<auto> table_desc = table.getDescriptionHash();
        if (table_desc.columns{field.getName()}) {
            throw "ADD-FIELD-ERROR", sprintf("field %y already exists in %y", field.getName(), table.getName());
        }

        if (!db) {
            db = AbstractDatabase::getDatabase(table.getDatasource());
        }
        hash<GenericColumnInfo> col = DbDataProvider::getColumnDesc(db, field, field_add_options);
        table_desc.columns{field.getName()} = col;

        alignTable(table_desc);
    }

    #! Updates an existing field
    /** @param name the name of the current field to update
        @param field the new definition of the field
        @param field_update_options the options for adding the new field

        @since DataProvider 2.0
    */
    private updateFieldImpl(string name, AbstractDataField field, *hash<auto> field_update_options) {
        hash<auto> table_desc = table.getDescriptionHash();
        if (!table_desc.columns{name}) {
            throw "UPDATE-FIELD-ERROR", sprintf("old field %y does not exist in %y; known fields: %y", name,
                table.getName(), keys table_desc.columns);
        }

        if (!db) {
            db = AbstractDatabase::getDatabase(table.getDatasource());
        }
        hash<GenericColumnInfo> col = DbDataProvider::getColumnDesc(db, field, field_update_options);
        hash<auto> align_opts;
        if (name != field.getName()) {
            if (table_desc.columns{field.getName()}) {
                throw "UPDATE-FIELD-ERROR", sprintf("new field %y alread exists in %y", field.getName(),
                    table.getName());
            }
            remove table_desc.columns{name};
            align_opts.column_map{name} = field.getName();
        }
        table_desc.columns{field.getName()} = col;

        alignTable(table_desc);
    }

    #! Deletes an existing field
    /** @param name the name of the field to delete
        @param field_delete_options the options for deleting the new field

        @since DataProvider 2.0
    */
    private deleteFieldImpl(string name, *hash<auto> field_delete_options) {
        hash<auto> table_desc = table.getDescriptionHash();
        if (!table_desc.columns{name}) {
            throw "DELETE-FIELD-ERROR", sprintf("field %y does not exist in %y; known fields: %y", name,
                table.getName(), keys table_desc.columns);
        }

        if (!db) {
            db = AbstractDatabase::getDatabase(table.getDatasource());
        }

        if (*list<auto> sqll = table.getDropColumnSql(name)) {
            execDdlSql(sqll);
        }
    }

    #! Align table with the given description
    private alignTable(hash<auto> table_desc) {
        AbstractTable new_table = AbstractTable::getTable(table.getDatasource(), table.getName());
        new_table.setupTable(table_desc);

        if (*list<auto> sqll = table.getAlignSql(new_table)) {
            execDdlSql(sqll);
        }
    }

    #! Executes the given DDL SQL string list
    private execDdlSql(list<auto> sqll) {
        AbstractDatasource ds = table.getDatasource();
        {
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
            # execute alignment
            string sql;
            on_error {
                error("DbTableDataProvider %y DDL: %s: %s: SQL: %s", table.getName(), $1.err, $1.desc, sql);
            }
            map ds.execRaw(sql = $1), sqll;
            table.clear();
        }
        info("DbTableDataProvider %y DDL: %s", table.getName(), (foldl $1 + "\n" + $2, sqll));
    }
}
}
