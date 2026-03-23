# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore BulkInsertOperation class definition

/*  BulkInsertOperation.qc Copyright 2015 - 2024 Qore Technologies, s.r.o.

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

#! the BulkSqlUtil namespace. All classes used in the BulkSqlUtil module should be inside this namespace
public namespace BulkSqlUtil {
    #! base class for bulk DML insert operations
    /** This class assists with bulk inserts into a target @ref SqlUtil::AbstractTable "table".

        @par Submitting Data
        To use this class, queue data in the form of a hash (a single row or a set of rows) or a list of rows
        by calling the queueData() method.\n\n
        The queueData() method queues data to be written to the database; the queue is flush()ed
        automatically when \c block_size rows have been queued.

        @par Retrieving Data From Inserts
        It is possible to use @ref sql_iop_funcs in the hashes submitted with queueData(); in this case the
        BulkInsertOperation class assumes that every row has the same operations as in the first row.
        Output data can then be processed by using the \c rowcode option in the constructor() or by calling
        setRowCode().\n\n
        In case @ref sql_iop_funcs are used and a \c rowcode option is set, then the SQL DML query for inserts
        is creating using the \c "returning" @ref SqlUtil::AbstractTable::InsertOptions "insert option", therefore
        the DBI driver in this case must support this option as well.

        @par Flushing and Discarding Data
        Each call to flush() (whether implicit or explicit) will cause a single call to be made to
        the dataserver; all queued rows are sent in a single bulk DML call, which allows for efficient
        processing of large amounts of data.\n\n
        A call to flush() must be made before committing the transaction to ensure that any remaining
        rows in the internal queue have been written to the database.  Because the destructor() will
        throw an exception if any data is left in the internal queue when the object is destroyed, a call
        to discard() must be made prior to the destruction of the object in case of errors.

        @code{.py}
# single commit and rollback
on_success ds.commit();
on_error ds.rollback();
{
    BulkInsertOperation op1(table1);
    BulkInsertOperation op2(table2);

    # each operation needs to be flushed or discarded individually
    on_success {
        op1.flush();
        op2.flush();
    }
    on_error {
        op1.discard();
        op2.discard();
    }

    # data is queued and flushed automatically when the buffer is full
    map op1.queueData($1), data1.iterator();
    map op2.queueData($1), data2.iterator();
}
        @endcode

        @note Each bulk DML object must be manually flush()ed before committing or manually
        discard()ed before rolling back to ensure that all data is managed properly in the same
        transaction and to ensure that no exception is thrown in the destructor().
        See the example above for more information.
    */
    public class BulkInsertOperation inherits BulkSqlUtil::AbstractBulkOperation {
        private {
            #! statement for DML
            AbstractSQLStatement stmt;

            #! per-row @ref closure or @ref call_reference for inserts
            *code rowcode;

            #! hash of "returning" arguments
            hash static_ret_expr;
        }

        #! creates the object from the supplied arguments
        /** @param target the target table object
            @param opts an optional hash of options for the object as follows:
            - \c "info_log": an optional info logging callback; must accept a string format specifier and sprintf()-
              style arguments
            - \c "block_size": the number of rows executed at once (default: 1000)
            - \c "rowcode": a per-row @ref closure or @ref call_reference for batch inserts; this must take a single
              hash argument and will be called for every row after a bulk insert; the hash argument representing the
              row inserted will also contain any output values if applicable (for example if @ref sql_iop_funcs are
              used in the row hashes submitted to queueData())

            @see setRowCode()
        */
        constructor(SqlUtil::Table target, *hash opts) : AbstractBulkOperation("insert", target, opts) {
        }

        #! creates the object from the supplied arguments
        /** @param target the target table object
            @param opts an optional hash of options for the object as follows:
            - \c "info_log": an optional info logging callback; must accept a string format specifier and sprintf()-
              style arguments
            - \c "block_size": the number of rows executed at once (default: 1000)
            - \c "rowcode": a per-row @ref closure or @ref call_reference for batch inserts; this must take a single
              hash argument and will be called for every row after a bulk insert; the hash argument representing the
              row inserted will also contain any output values if applicable (for example if @ref sql_iop_funcs are
              used in the row hashes submitted to queueData())

            @see setRowCode()
        */
        constructor(SqlUtil::AbstractTable target, *hash opts) : AbstractBulkOperation("insert", target, opts) {
        }

        #! sets a @ref closure "closure" or @ref call_reference "call reference" that will be called when data has been sent to the database and all output data is available; must accept a hash argument that represents the data written to the database including any output arguments. This code will be reset, once the transaction is commited.
        /** @par Example:
            @code{.py}
# single commit and rollback
on_success ds.commit();
on_error ds.rollback();
code rowcode = sub (hash row) {
    # process row data
};
inserter.setRowCode(rowcode);
{
    # each operation needs to be flushed or discarded individually
    on_success inserter.flush();
    on_error inserter.discard();

    # data is queued and flushed automatically when the buffer is full
    map inserter.queueData($1), data.iterator();
}
            @endcode

            @param rowc a @ref closure "closure" or @ref call_reference "call reference" that will be called when data
            has been sent to the database and all output data is available; must accept a hash argument that
            represents the data written to the database including any output arguments

            @note
            - the per-row @ref closure "closure" or @ref call_reference "call reference" can also be set by using the
              \c "rowcode" option in the constructor()
            - if this method is not called before the first row is queued then output values will not be retrieved;
              the initial query is built when the template row is queued and output values are only retrieved if a
              \c rowcode @ref closure "closure" or @ref call_reference "call reference" is set beforehand
        */
        setRowCode(*code rowc) {
            rowcode = rowc;
        }

        #! common constructor initialization
        private init(*hash opts) {
            if (opts.rowcode)
                rowcode = opts.rowcode;
            AbstractBulkOperation::init(opts);
        }

        #! sets up the block buffer given the initial template hash of lists for inserting
        private setupInitialRowColumns(hash row) {
            setupStaticRowValues(\row);
            AbstractBulkOperation::setupInitialRowColumns(row);
        }

        #! sets up support for "returning" insert options for any possible rowcode member
        private setupInitialRow(hash row) {
            setupStaticRowValues(\row);
            AbstractBulkOperation::setupInitialRow(row);
        }

        private setupStaticRowValues(reference<hash> row) {
            foreach hash<auto> h in (row.pairIterator()) {
                if (h.value.typeCode() == NT_HASH) {
                    ret_args += h.key;
                    static_ret_expr.(h.key) = h.value;
                    # remove the hash from the row
                    delete row.(h.key);
                }
            }
            if (static_ret_expr)
                cval_keys += static_ret_expr.keys();
        }

        #! inserts internally-queued queued data in the database with bulk DML operations
        /**
            This method sets up the SQL DML query used for inserts when row is queued.
            Output values are only retrieved if @ref sql_iop_funcs are used and a
            \c rowcode @ref closure "closure" or @ref call_reference "call reference"
            has been set beforehand in the constructor() or by calling setRowCode() and
            the underlying DBI driver supports the \c "returning"
            @ref SqlUtil::AbstractTable::InsertOptions "insert option".
        */
        private flushImpl() {
            *hash rh;
            if (!stmt) {
                string sql;
                # insert the data
                rh = table.insert(hbuf + cval + static_ret_expr, \sql, rowcode ? ("returning": ret_args) : NOTHING);
                # create the statement for future inserts
                stmt = table.getDatasource().getSQLStatement();
                stmt.prepare(sql);
            } else {
                # execute the statement on the args
                if (table.hasArrayBind()) {
                    stmt.execArgs(table.transformBulkBindArgs(hbuf + cval + static_ret_expr));
                    rh = stmt.getOutput();
                } else {
                    softlist args = (hbuf + cval + static_ret_expr).values();
                    int size = 0;
                    foreach auto arg in (args) {
                        if (arg.typeCode() == NT_LIST) {
                            size = arg.size();
                            break;
                        }
                    }
                    if (size) {
                        for (int i = 0; i < size; ++i) {
                            # get arg list for row
                            list<auto> targs = map $1.typeCode() == NT_LIST ? $1[i] : $1, args;
                            stmt.execArgs(targs);
                        }
                    }
                }
            }

            # call rowcode if it exists
            if (rowcode) {
                map rowcode($1), (hbuf + rh).contextIterator();
            }
        }
    } # BulkInsertOperation class
} # BulkSqlUtil namespace
