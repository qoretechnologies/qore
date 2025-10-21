# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AbstractBulkOperation class definition

/*  AbstractBulkOperation.qc Copyright 2015 - 2024 Qore Technologies, s.r.o.

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
    #! base class for bulk DML operations
    /** This is an abstract base class for bulk DML operations; this class provides the majority of the
        API support for bulk DML operations for the concrete child classes that inherit it.

        @par Submitting Data
        To use this class's API, queue data in the form of a hash (a single row or a set of rows) or a list of rows
        by calling the queueData() method.\n\n
        The queueData() method queues data to be written to the database; the queue is flush()ed
        automatically when \c block_size rows have been queued.

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

        @note
        - Each bulk DML object must be manually flush()ed before committing or manually
          discard()ed before rolling back to ensure that all data is managed properly in the same
          transaction and to ensure that no exception is thrown in the destructor().
          See the example above for more information.
        - If the underlying driver does not support bulk operations, then such support is
          emulated with single SQL operations; in such cases performance will be reduced.  Call
          @ref SqlUtil::AbstractTable::hasArrayBind() to check at runtime if the driver supports
          bulk SQL operations.
    */
    public class AbstractBulkOperation {
        public {
            #! option keys for this object
            const OptionKeys = {
                "block_size": sprintf("the row block size used for bulk DML / batch operations; default: %y",
                    OptionDefaults.block_size),
                "info_log": "a call reference / closure for informational logging",
            };

            #! default option values
            const OptionDefaults = {
                "block_size": 1000,
            };
        }

        private {
            #! the target table object
            SqlUtil::AbstractTable table;

            #! bulk operation block size
            softint block_size;

            #! buffer for bulk operations
            hash hbuf;

            #! "constant" row values; must be equal in all calls to queueData
            hash cval;

            #! "constant" row value keys
            list cval_keys;

            #! an optional info logging callback; must accept a sprintf()-style format specifier and optional arguments
            *code info_log;

            #! row count
            int row_count = 0;

            #! operation name
            string opname;

            #! list of "returning" columns
            list ret_args = ();
        }

        #! creates the object from the supplied arguments
        /** @param name the name of the operation
            @param target the target table object
            @param opts an optional hash of options for the object as follows:
            - \c "block_size": the number of rows executed at once (default: 1000)
            - \c "info_log": an optional info logging callback; must accept a string format specifier and sprintf()-
              style arguments
        */
        constructor(string name, SqlUtil::Table target, *hash opts) {
            opname = name;
            table = target.getTable();
            init(opts);
        }

        #! creates the object from the supplied arguments
        /** @param name the name of the operation
            @param target the target table object
            @param opts an optional hash of options for the object as follows:
            - \c "block_size": the number of rows executed at once (default: 1000)
            - \c "info_log": an optional info logging callback; must accept a string format specifier and sprintf()-
              style arguments
        */
        constructor(string name, SqlUtil::AbstractTable target, *hash opts) {
            opname = name;
            table = target;
            init(opts);
        }

        #! throws an exception if there is data pending in the internal row data cache; make sure to call flush() or discard() before destroying the object
        /** @throw BLOCK-ERROR there is unflushed data in the internal row data cache; make sure to call flush() or
            discard() before destroying the object
        */
        destructor() {
            if (hbuf.firstValue())
                throw "BLOCK-ERROR", sprintf("there %s still %d row%s of data in the internal cache; make sure to call %s::flush() or %s::discard() before destroying the object to flush all data to the database", hbuf.firstValue().size() == 1 ? "is" : "are", hbuf.firstValue().size(), hbuf.firstValue().size() == 1 ? "" : "s", self.className(), self.className());
        }

        #! queues row data in the block buffer; the block buffer is flushed to the DB if the buffer size reaches the limit defined by the \c block_size option; does not commit the transaction
        /** @par Example:
            @code{.py}
# single commit and rollback
on_success ds.commit();
on_error ds.rollback();
{
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

            @param data the input record or record set in case a hash of lists is passed; each hash represents a row
            (keys are column names and values are column values); when inserting, @ref sql_iop_funcs can also be used.
            If at least one hash value is a list, then any non-hash (indicating an
            @ref sql_iop_funcs "insert opertor hash") and non-list values will be assumed to be constant values for
            every row and therefore future calls of this method (and overloaded variants) will ignore any values given
            for such keys and use the values given in the first call.

            @note
            - the first row passed is taken as a template row; every other row must always have the same keys in the
              same order, otherwise the results are unpredictable
            - if any @ref sql_iop_funcs are used, then they are assumed to be identical in every row
            - make sure to call flush() before committing the transaction or discard() before rolling back the
              transaction or destroying the object when using this method
            - flush() or discard() needs to be executed individually for each bulk operation object used in the block
              whereas the DB transaction needs to be committed or rolled back once per datasource

            @see
            - flush()
            - discard()
        */
        queueData(hash data) {
            # prepare buffer hash if necessary
            if (!hbuf)
                setupInitialRowColumns(data);

            # remove "returning" arguments
            data -= ret_args;

            # remove constant args
            if (cval_keys)
                data -= cval_keys;

            # if we are working with bulk data where the row count would cause the buffer limit to be exceeded
            if (data.firstValue().typeCode() == NT_LIST) {
                while (True) {
                    int ds = data.firstValue().lsize();
                    int cs = hbuf.firstValue().lsize();
                    if ((ds + cs) < block_size)
                        break;
                    int ns = block_size - cs;
                    # add on rows until we get to the block size
                    map hbuf.$1 += (extract data.$1, 0, ns), keys hbuf;
                    flushIntern();
                }
            }

            map hbuf{$1.key} += $1.value, data.pairIterator();
            # return nothing if nothing needs to be flushed
            if (hbuf.firstValue().size() < block_size)
                return;

            # return all target data
            flushIntern();
        }

        #! queues row data in the block buffer; the block buffer is flushed to the DB if the buffer size reaches the limit defined by the \c block_size option; does not commit the transaction
        /** @par Example:
            @code{.py}
# single commit and rollback
on_success ds.commit();
on_error ds.rollback();
{
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

            @param l a list of hashes representing the input row data; each hash represents a row (keys are column names and values are column values); when inserting, @ref sql_iop_funcs can also be used

            @note
            - the first row passed is taken as a template row; every other row must always have the same keys in the same order, otherwise the results are unpredictable
            - if any @ref sql_iop_funcs are used, then they are assumed to be identical in every row
            - make sure to call flush() before committing the transaction or discard() before rolling back the transaction or destroying the object when using this method
            - flush() or discard() needs to be executed individually for each bulk operation object used in the block whereas the DB transaction needs to be committed or rolled back once per datasource

            @see
            - flush()
            - discard()
        */
        queueData(list<auto> l) {
            if (l.empty())
                return;

            # prepare buffer hash if necessary
            if (!hbuf)
                setupInitialRow(l[0]);

            # if we are working with bulk data where the row count would cause the buffer limit to be exceeded
            while (True) {
                int ds = l.size();
                int cs = hbuf.firstValue().lsize();
                if ((ds + cs) < block_size)
                    break;
                int ns = block_size - cs;
                # remove and process rows to add
                foreach hash row in (extract l, 0, ns) {
                    # add row data to block buffer
                    map hbuf.$1 += row.$1, keys hbuf;
                }

                flushIntern();
            }

            foreach hash<auto> row in (l) {
                # add row data to block buffer
                map hbuf.$1 += row.$1, keys hbuf;
            }

            # return nothing if nothing needs to be flushed
            if (hbuf.firstValue().size() < block_size)
                return;

            # return all target data
            flushIntern();
        }

        #! returns the current size of the cache as a number of rows
        /** @return the current size of the cache as a number of rows

            @since %BulkSqlUtil 1.2
        */
        int size() {
            return hbuf.firstValue().size();
        }

        #! Returns the operation name
        string getOperationName() {
            return opname;
        }

        #! Returns the block size
        int getBlockSize() {
            return block_size;
        }

        #! common constructor initialization
        private init(*hash<auto> opts) {
            block_size = opts.block_size ?? OptionDefaults.block_size;

            if (block_size < 1)
                throw "BULK-SQL-OPERATION-ERROR", sprintf("the block_size option is set to %d; this value must "
                    "be >= 1", block_size);

            if (opts.info_log)
                info_log = opts.info_log;
        }

        #! sets up the block buffer given the initial template hash of lists for inserting
        private setupInitialRowColumns(hash<auto> row) {
            # ensure we have at least one list of columns values
            bool has_list;
            foreach auto val in (row.iterator()) {
                if (val.typeCode() == NT_LIST) {
                    has_list = True;
                    break;
                }
            }

            # do not include constant values in the template row
            if (has_list) {
                cval = map {$1: remove row.$1}, keys row, row.$1.typeCode() != NT_LIST;
                if (cval)
                    cval_keys = cval.keys();
            }

            setupInitialRow(row);
        }

        #! sets up the block buffer given the initial template row for inserting
        private setupInitialRow(hash<auto> row) {
            map hbuf.$1 = (), keys row;
        }

        #! flushes any remaining batched data to the database; this method should always be called before committing the transaction or destroying the object
        /** @par Example:
            @code{.py}
# single commit and rollback
on_success ds.commit();
on_error ds.rollback();
{
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

            @note
            - make sure to call flush() before committing the transaction or discard() before rolling back the
              transaction or destroying the object when using this method
            - flush() or discard() needs to be executed individually for each bulk operation object used in the block
              whereas the DB transaction needs to be committed or rolled back once per datasource

            @see
            - queueData()
            - discard()
        */
        flush() {
            if (hbuf.firstValue())
                flushIntern();
        }

        #! flushes queued data to the database
        private flushIntern() {
            # flush data to the DB; implemented in subclasses
            flushImpl();
            # update row count
            int bs = hbuf.firstValue().lsize();
            row_count += bs;
            if (info_log)
                info_log("%s (%s): %d row%s flushed (total %d)", table.getSqlName(), opname, bs, bs == 1 ? "" : "s",
                    row_count);
            # reset internal buffer
            map hbuf.$1 = (), keys hbuf;
        }

        #! discards any buffered batched data; this method should be called before destroying the object if an error occurs
        /** @par Example:
            @code{.py}
# single commit and rollback
on_success ds.commit();
on_error ds.rollback();
{
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

            @note
            - make sure to call flush() before committing the transaction or discard() before rolling back the
              transaction or destroying the object when using this method
            - flush() or discard() needs to be executed individually for each bulk operation object used in the block
              whereas the DB transaction needs to be committed or rolled back once per datasource

            @see
            - queueData()
            - flush()
        */
        discard() {
            delete hbuf;
        }

        #! flushes any queued data and commits the transaction
        nothing commit() {
            flush();
            table.commit();
        }

        #! discards any queued data and rolls back the transaction
        nothing rollback() {
            discard();
            table.rollback();
        }

        #! returns the table name
        string getTableName() {
            return table.getSqlName();
        }

        #! returns the underlying SqlUtil::AbstractTable object
        SqlUtil::AbstractTable getTable() {
            return table;
        }

        #! returns the @ref Qore::SQL::AbstractDatasource "AbstractDatasource" object associated with this object
        Qore::SQL::AbstractDatasource getDatasource() {
            return table.getDatasource();
        }

        #! returns the affected row count
        int getRowCount() {
            return row_count;
        }

        #! flushes queued data to the database
        abstract private flushImpl();
    }
}
