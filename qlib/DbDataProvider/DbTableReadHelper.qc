# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DbTableReadHelper class definition

/** DbTableReadHelper.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! The reads of a relation through a SqlUtil table object, shared by the table and the view data providers
/** A search condition that is an expression is split into the part that the database evaluates and a residual part
    that is evaluated on the rows returned, which requires the \c allow_client_side_filter search option.
*/
class DbTableReadHelper {
    #! Rolls back a transaction that the current thread holds only since an operation that failed
    /** When a statement fails before the object that would release its implicit transaction exists, as when the
        constructor of an iterator fails, the transaction lock acquired by the statement must be released here.  An
        error in the rollback itself is ignored, so the caller rethrows the error of the operation.

        @param ds the datasource
        @param in_trans @ref True if the current thread held a transaction before the operation; nothing is done then
    */
    static releaseImplicitTransaction(AbstractDatasource ds, bool in_trans) {
        if (in_trans || !ds.currentThreadInTransaction()) {
            return;
        }
        try {
            ds.rollback();
        } catch (hash<ExceptionInfo> ex) {
            # the error of the failed operation is the error to report
        }
    }

    #! Returns a statement for a select on the given table, releasing any implicit transaction if it fails
    /** @param table the table
        @param select_options the select options, including any \c where clause
    */
    static AbstractSQLStatement getTableStatement(AbstractTable table, *hash<auto> select_options) {
        AbstractDatasource ds = table.getDatasource();
        bool in_trans = ds.currentThreadInTransaction();
        try {
            return table.getStatement(select_options);
        } catch (hash<ExceptionInfo> ex) {
            DbTableReadHelper::releaseImplicitTransaction(ds, in_trans);
            rethrow;
        }
    }

    #! Returns @ref True if the where hash is a SqlUtil where clause
    /** A where hash is a SqlUtil where clause if a value is a SqlUtil operator hash, as returned by the
        @ref sql_op_funcs "SqlUtil operator functions" such as @ref SqlUtil::op_gt() "op_gt()" and by
        @ref SqlUtil::wop_or() "wop_or()", or a SqlUtil column operator hash
    */
    static bool isSqlUtilWhere(*hash<auto> where_cond) {
        if (!where_cond || where_cond instanceof hash<DataProviderExpression>) {
            return False;
        }
        foreach auto value in (where_cond.iterator()) {
            if (value instanceof hash<OperatorInfo> || value instanceof hash<ColumnOperatorInfo>) {
                return True;
            }
        }
        return False;
    }

    #! Returns a SqlUtil where clause with its plain values converted to the types of their fields
    /** The where clause is evaluated by SqlUtil, with SqlUtil's semantics for its operators.  A key may have a
        disposable unique prefix (<tt>"1:id"</tt>) to give more than one condition on a column, as in SqlUtil, and the
        conditions of a @ref SqlUtil::wop_or() "wop_or()" operator are where clauses themselves.

        @param provider_name the name of the provider for error messages
        @param rec the record type of the relation
        @param where_cond the SqlUtil where clause

        @return the where clause with each plain value converted to the type of its field

        @throw INVALID-FIELD a key names no field of the relation
    */
    static hash<auto> processSqlUtilWhere(string provider_name, hash<string, AbstractDataField> rec,
            hash<auto> where_cond) {
        hash<auto> rv;
        foreach hash<auto> i in (where_cond.pairIterator()) {
            if (i.value instanceof hash<OperatorInfo> && i.value.op == OP_OR) {
                # each element of the argument of an "or" operator is a where clause
                hash<OperatorInfo> op = i.value;
                op.arg = map DbTableReadHelper::processSqlUtilWhere(provider_name, rec, $1), i.value.arg;
                rv{i.key} = op;
                continue;
            }
            # remove any disposable unique prefix, as SqlUtil does
            string field = i.key;
            field =~ s/^[0-9]+://;
            if (!rec{field}) {
                throw "INVALID-FIELD", sprintf("data provider %y: field %y is not a defined field; valid fields: %y",
                    provider_name, field, keys rec);
            }
            # a hash is an operator; SqlUtil processes its arguments
            rv{i.key} = i.value.typeCode() == NT_HASH ? i.value : rec{field}.acceptsValue(i.value);
        }
        return rv;
    }

    #! Returns an iterator for zero or more records of the relation matching the search options
    static AbstractDataProviderRecordIterator<hash<auto>> getReadIterator(AbstractTable table,
            *hash<auto> where_cond, *hash<auto> search_options) {
        return DbTableReadHelper::getReadIteratorFromSplit(table,
            DbTableReadHelper::getReadExpressionSplit(table, where_cond, search_options), search_options);
    }

    #! Returns a bulk record interface for zero or more records of the relation matching the search options
    static AbstractDataProviderBulkRecordInterface getBulkReadInterface(AbstractTable table, int block_size,
            *hash<auto> where_cond, *hash<auto> search_options) {
        hash<auto> split = DbTableReadHelper::getReadExpressionSplit(table, where_cond, search_options);
        if (!exists split.client) {
            return new DbTableBulkRecordInterface(block_size, table, split.pushdown,
                search_options - "allow_client_side_filter");
        }
        return DbTableReadHelper::getReadIteratorFromSplit(table, split, search_options).getBulkApi(block_size);
    }

    #! Returns a single record of the relation matching the search options
    /** @throw MULTIPLE-RECORDS-ERROR multiple records found
    */
    static *hash<auto> searchSingleRecord(AbstractTable table, *hash<auto> where_cond, *hash<auto> search_options) {
        AbstractDataProviderRecordIterator<hash<auto>> i = DbTableReadHelper::getReadIterator(table, where_cond,
            search_options);
        *hash<auto> rv;
        if (i.next()) {
            rv = i.getValue();
        }
        if (i.next()) {
            throw "MULTIPLE-RECORDS-ERROR", sprintf("multiple records found for search: %y", where_cond);
        }
        return rv;
    }

    #! Splits a read predicate into native SQL and optional client-side portions
    /** @throw CLIENT-SIDE-FILTER-DISABLED the predicate has a part that the database cannot evaluate and the
        \c allow_client_side_filter search option is not set
    */
    static hash<auto> getReadExpressionSplit(AbstractTable table, *hash<auto> where_cond,
            *hash<auto> search_options) {
        if (!(where_cond instanceof hash<DataProviderExpression>)) {
            return {"pushdown": where_cond, "client": NOTHING};
        }
        # a field that SqlUtil cannot name as a column is read client-side
        hash<auto> split = AbstractDataProvider::splitPushdownableExpression(
            cast<hash<DataProviderExpression>>(where_cond), table.getExpressionMap(),
            \DbTableDataProvider::getColumnReference());
        if (exists split.client && !search_options.allow_client_side_filter) {
            throw "CLIENT-SIDE-FILTER-DISABLED", sprintf(
                "operator(s) in the search predicate have no native SQL implementation for database driver %y; "
                "set search option allow_client_side_filter to True to evaluate the residual predicate on returned "
                "rows, or rewrite the predicate using native operators", table.getDatasource().getDriverName());
        }
        return split;
    }

    #! Returns a read iterator from an already split expression
    /** @throw CLIENT-SIDE-FILTER-OPTIONS-UNSUPPORTED a search option cannot be combined with a client-side filter
    */
    static AbstractDataProviderRecordIterator<hash<auto>> getReadIteratorFromSplit(AbstractTable table,
            hash<auto> split, *hash<auto> search_options) {
        hash<auto> public_options = search_options - "allow_client_side_filter";
        if (!exists split.client) {
            return new DbTableRecordIterator(table, split.pushdown, public_options);
        }

        if (exists public_options.groupby || exists public_options.having) {
            throw "CLIENT-SIDE-FILTER-OPTIONS-UNSUPPORTED",
                "groupby and having search options cannot be combined with a client-side residual filter";
        }
        if (exists public_options.columns) {
            foreach auto column in (public_options.columns) {
                if (column.typeCode() != NT_STRING) {
                    throw "CLIENT-SIDE-FILTER-OPTIONS-UNSUPPORTED",
                        "computed column expressions cannot be combined with a client-side residual filter";
                }
            }
        }

        # Paging, projections, and result-count constraints must apply after the residual predicate. Query all
        # columns so the client expression has every field it may reference.
        hash<auto> native_options = public_options - ("columns", "limit", "offset", "requires_result", "max_records");
        DbTableRecordIterator native_iter(table, split.pushdown, native_options);
        return new DefaultRecordIterator<hash<auto>>(native_iter, split.client, public_options,
            table.getRecordType(), NOTHING, NOTHING);
    }

    #! Returns expression info annotated with native pushdown capability
    static hash<string, hash<DataProviderExpressionInfo>> getTableExpressionInfo(AbstractTable table) {
        return cast<hash<string, hash<DataProviderExpressionInfo>>>(
            map {
                $1.key: cast<hash<DataProviderExpressionInfo>>(
                    $1.value.exp + {"supports_pushdown": exists $1.value.code})
            }, table.getExpressionMap().pairIterator()
        );
    }
}
}
