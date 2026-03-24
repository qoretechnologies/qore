# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore BulkUpsertOperation class definition

/*  BulkUpsertOperation.qc Copyright 2015 - 2024 Qore Technologies, s.r.o.

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
    #! base class for bulk DML upsert operations
    /** This class assists with bulk upsert (SQL merge) operations into a target @ref SqlUtil::AbstractTable "table".

        @par Submitting Data
        To use this class, queue data in the form of a hash (a single row or a set of rows) or a list of rows
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
    BulkUpsertOperation op1(table1);
    BulkUpsertOperation op2(table2);

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
    public class BulkUpsertOperation inherits BulkSqlUtil::AbstractBulkOperation {
        private {
            # upsert strategy to use
            int upsert_strategy;
            # upsert closure
            code upsert;
        }

        #! creates the object from the supplied arguments
        /** @param target the target table object
            @param opts an optional hash of options for the object as follows:
            - \c "block_size": the number of rows executed at once (default: 1000)
            - \c "info_log": an optional info logging callback; must accept a string format specifier and sprintf()-
              style arguments
            - \c "upsert_strategy": the upsert strategy to use; default SqlUtil::AbstractTable::UpsertAuto; see
              @ref upsert_options for possible values for the upsert strategy
        */
        constructor(SqlUtil::Table target, *hash<auto> opts) : AbstractBulkOperation("upsert", target, opts) {
        }

        #! creates the object from the supplied arguments
        /** @param target the target table object
            @param opts an optional hash of options for the object as follows:
            - \c "block_size": the number of rows executed at once (default: 1000)
            - \c "info_log": an optional info logging callback; must accept a string format specifier and sprintf()-
              style arguments
            - \c "upsert_strategy": the upsert strategy to use; default SqlUtil::AbstractTable::UpsertAuto; see
              @ref upsert_options for possible values for the upsert strategy
        */
        constructor(SqlUtil::AbstractTable target, *hash<auto> opts) : AbstractBulkOperation("upsert", target, opts) {
        }

        #! common constructor initialization
        private init(*hash<auto> opts) {
            if (opts.upsert_strategy) {
                upsert_strategy = opts.upsert_strategy.toInt();
                if (!AbstractTable::UpsertStrategyMap{upsert_strategy}) {
                    throw "BULK-UPSERT-ERROR", sprintf("invalid upsert strategy code %y, expecting one of: %y",
                        opts.upsert_strategy, AbstractTable::UpsertStrategyDescriptionMap.values());
                }
            } else {
                upsert_strategy = AbstractTable::UpsertAuto;
            }

            AbstractBulkOperation::init(opts);
        }

        #! executes bulk DML upserts in the database with internally queued data
        private flushImpl() {
            if (!upsert) {
                upsert = table.getBulkUpsertClosure(hbuf + cval, upsert_strategy);
            }

            # execute the statement on the args
            upsert(hbuf + cval);
        }
    } # BulkUpsertOperation class
} # BulkSqlUtil namespace
