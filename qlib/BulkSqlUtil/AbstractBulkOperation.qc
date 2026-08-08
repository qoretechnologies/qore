# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AbstractBulkOperation class definition

/*  AbstractBulkOperation.qc Copyright 2015 - 2026 Qore Technologies, s.r.o.

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
            const hash<string, string> OptionKeys = {
                "block_size": sprintf("the row block size used for bulk DML / batch operations; default: %y",
                    OptionDefaults.block_size),
                "info_log": "a call reference / closure for informational logging",
                "stream_bounds": sprintf("report the operation as a bounded stream to a datasource mutation "
                    "observer, if one is registered; default: %y", OptionDefaults.stream_bounds),
            };

            #! default option values
            const hash<string, auto> OptionDefaults = {
                "block_size": 1000,
                "stream_bounds": True,
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
            *code<nothing(string, ...)> info_log;

            #! row count
            int row_count = 0;

            #! operation name
            string opname;

            #! list of "returning" columns
            list ret_args = ();

            #! input field names from the first queued record or record block
            list<string> input_keys;

            #! True when input_keys has been initialized, including by an empty record
            bool input_template_set = False;

            #! True if this operation reports itself as a bounded stream when an observer is present
            bool stream_bounds;

            #! True once the stream begin boundary has been reported
            bool stream_started = False;

            #! total payload bytes reported to the observer so far
            int stream_consumed = 0;
        }

        #! creates the object from the supplied arguments
        /** @param name the name of the operation
            @param target the target table object
            @param opts an optional hash of options for the object as follows:
            - \c "block_size": the number of rows executed at once (default: 1000)
            - \c "info_log": an optional info logging callback; must accept a string format specifier and sprintf()-
              style arguments
            - \c "stream_bounds": report the operation as a bounded stream to a datasource mutation observer, if one
              is registered (default: @ref True "True"); see @ref bulk_bounded_streams
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
            - \c "stream_bounds": report the operation as a bounded stream to a datasource mutation observer, if one
              is registered (default: @ref True "True"); see @ref bulk_bounded_streams
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
            if (hbuf.firstValue()) {
                # the operation is being destroyed with data it never wrote, so the stream did not
                # complete; the boundary is reported before the error is raised
                streamEnd(False);
                throw "BLOCK-ERROR", sprintf("there %s still %d row%s of data in the internal cache; make sure to call %s::flush() or %s::discard() before destroying the object to flush all data to the database", hbuf.firstValue().size() == 1 ? "is" : "are", hbuf.firstValue().size(), hbuf.firstValue().size() == 1 ? "" : "s", self.className(), self.className());
            }
            # backstop for callers that commit on the datasource directly rather than through
            # commit() or endStream(); everything queued was written
            streamEnd(True);
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
            - the first row passed is taken as a template row; every other row must have the same field set; field order
              is not significant
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
            *int rows = validateColumnLengths(data);
            validateInputKeys(data);
            if (exists rows && hasBufferColumn(data)) {
                queueBufferColumns(data, rows);
                return;
            }

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
            - the first row passed is taken as a template row; every other row must have the same field set; field order
              is not significant
            - if any @ref sql_iop_funcs are used, then they are assumed to be identical in every row
            - make sure to call flush() before committing the transaction or discard() before rolling back the transaction or destroying the object when using this method
            - flush() or discard() needs to be executed individually for each bulk operation object used in the block whereas the DB transaction needs to be committed or rolled back once per datasource

            @see
            - flush()
            - discard()
        */
        queueData(list<auto> l) {
            if (l.empty()) {
                return;
            }

            list<string> block_keys;
            for (int row = 0; row < l.size(); ++row) {
                auto record = l[row];
                if (record.typeCode() != NT_HASH) {
                    throw "BULK-SQL-OPERATION-ERROR", sprintf("record %d/%d is type %y; expected a hash",
                        row + 1, l.size(), record.fullType());
                }
                if (!row) {
                    block_keys = keys record;
                } else {
                    validateFieldSet(record, block_keys, row + 1, l.size());
                }
            }
            validateInputKeys(l[0]);

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

        #! queues shaped row data in the block buffer
        /** @param data bulk row data in hash-of-lists, dense buffer-column, or ColumnarResult form

            @since %BulkSqlUtil 1.4
        */
        queueData(auto data) {
            if (data.typeCode() == NT_OBJECT && data.className() == "ColumnarResult") {
                hash<auto> columns = data.toColumnHash();
                *int rows = validateColumnLengths(columns);
                if (!exists rows || rows != data.numRows()) {
                    throw "BULK-SQL-OPERATION-ERROR", sprintf("ColumnarResult reports %d rows but its columns have "
                        "%d values", data.numRows(), rows ?? 0);
                }
                validateInputKeys(columns);
                queueBufferColumns(columns, rows);
                return;
            }
            if (data.typeCode() == NT_OBJECT && data.fullType() == "object<DataFrame>") {
                queueData(data.toColumnarResult());
                return;
            }
            queueData(cast<hash>(data));
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

            stream_bounds = exists opts.stream_bounds
                ? parse_boolean(opts.stream_bounds)
                : OptionDefaults.stream_bounds;
        }

        #! reports the begin boundary of the bounded stream if it has not been reported yet
        /** The boundary is reported lazily from the first flush, so an operation that never writes
            anything reports nothing at all.

            @throw SQL-MUTATION-REJECTED the observer refused the operation; the error code and
            description are supplied by the observer
        */
        private streamBegin() {
            if (stream_started || !stream_bounds) {
                return;
            }
            AbstractDatasource ds = table.getDatasource();
            if (!ds.hasMutationObserver()) {
                return;
            }
            # a rejection here throws, and nothing has been written yet
            ds.reportMutationStreamBegin(0);
            stream_started = True;
        }

        #! reports progress after a block has been written; throws if the observer stops the stream
        /** This is an admission point: an observer that has seen more bytes than it will admit stops
            the operation here, before the surrounding transaction can commit.

            @param bytes the payload size of the block just written
        */
        private streamProgress(int bytes) {
            if (!stream_started) {
                return;
            }
            stream_consumed += bytes;
            table.getDatasource().reportMutationStreamProgress(stream_consumed);
        }

        #! reports the end boundary of the bounded stream exactly once
        /** @param ok False if the operation did not complete successfully
        */
        private streamEnd(bool ok) {
            if (!stream_started) {
                return;
            }
            # cleared first so that the boundary can never be reported twice, even if the observer
            # raises an exception from it
            stream_started = False;
            table.getDatasource().reportMutationStreamEnd(stream_consumed, ok);
        }

        #! returns the payload size in bytes of the buffered block about to be written
        /** The measure is the serialized size of the values themselves, which is the same measure a
            producer uses to compute the \c "max_growth_bytes" of its declaration, so that declared
            and consumed byte counts are comparable.

            @return the total payload size in bytes of the current block
        */
        private int getBlockBytes() {
            int bytes = 0;
            foreach auto vals in ((hbuf + cval).iterator()) {
                if (vals.typeCode() == NT_LIST) {
                    map bytes += getValueBytes($1), vals;
                } else {
                    # a constant column value is repeated once per row in the block
                    bytes += getValueBytes(vals) * hbuf.firstValue().lsize();
                }
            }
            return bytes;
        }

        #! returns the serialized size in bytes of a single bound value
        private int getValueBytes(auto val) {
            switch (val.typeCode()) {
                case NT_NOTHING:
                case NT_NULL:
                    # the canonical representation of a NULL
                    return 2;
                case NT_STRING:
                    return val.strlen();
                case NT_BINARY:
                    return val.size();
                case NT_BOOLEAN:
                    return 1;
            }
            return sprintf("%s", val).strlen();
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

        #! Validates the field set against the first queued record or block
        private validateInputKeys(hash<auto> data, int row = 1, int rows = 1) {
            if (!input_template_set) {
                input_keys = keys data;
                input_template_set = True;
                return;
            }

            validateFieldSet(data, input_keys, row, rows);
        }

        #! Validates a record against an expected field set without modifying operation state
        private validateFieldSet(hash<auto> data, list<string> expected_keys, int row, int rows) {
            hash<string, bool> expected = map {$1: True}, expected_keys;
            hash<string, bool> actual = map {$1: True}, keys data;
            *hash<string, bool> missing = expected - keys actual;
            *hash<string, bool> extra = actual - keys expected;
            if (missing || extra) {
                throw "BULK-SQL-OPERATION-ERROR", sprintf("record %d/%d has a different field set; missing fields: "
                    "%y; extra fields: %y; expected fields: %y", row, rows, keys missing, keys extra, expected_keys);
            }
        }

        #! Validates list and buffer column lengths and returns the logical row count, if columnar
        private *int validateColumnLengths(hash<auto> data) {
            *int rows;
            foreach hash<auto> column in (data.pairIterator()) {
                int type = column.value.typeCode();
                if (type != NT_LIST && type != NT_BUFFER) {
                    continue;
                }
                int column_rows = column.value.lsize();
                if (!exists rows) {
                    rows = column_rows;
                } else if (column_rows != rows) {
                    throw "BULK-SQL-OPERATION-ERROR", sprintf("field %y has %d values but the record block has %d "
                        "records", column.key, column_rows, rows);
                }
            }
            return rows;
        }

        #! Returns True if at least one value is a dense column buffer
        private bool hasBufferColumn(hash<auto> data) {
            return any $1.typeCode() == NT_BUFFER, data.iterator();
        }

        #! Queues dense column blocks in block-sized slices to avoid materializing large packed inputs at once
        private queueBufferColumns(hash<auto> data, int rows) {
            for (int offset = 0; offset < rows; offset += block_size) {
                int count = rows - offset;
                if (count > block_size) {
                    count = block_size;
                }
                queueData(bufferColumnsToHashList(sliceBufferColumns(data, offset, count)));
            }
        }

        #! Returns a contiguous slice of dense buffer or list columns
        private hash<auto> sliceBufferColumns(hash<auto> data, int offset, int count) {
            return map {
                $1.key: $1.value.typeCode() == NT_BUFFER
                    ? $1.value.view(offset, count)
                    : ($1.value.typeCode() == NT_LIST ? $1.value[offset..(offset + count - 1)] : $1.value)
            }, data.pairIterator();
        }

        #! Converts dense buffer columns to the hash-of-lists compatibility representation
        private hash<auto> bufferColumnsToHashList(hash<auto> data) {
            return map {$1.key: $1.value.typeCode() == NT_BUFFER ? $1.value.toList() : $1.value},
                data.pairIterator();
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
            # report the stream boundary before anything is written, so that an observer that
            # refuses the operation stops it before the first row reaches the database
            streamBegin();
            int bytes = stream_started ? getBlockBytes() : 0;

            # any failure from here on ends the stream; the boundary is reported exactly once
            on_error streamEnd(False);

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

            # this is an admission point: an observer that has seen more than it will admit stops
            # the operation here, after this block and before the transaction can commit
            streamProgress(bytes);
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
            # the operation is being abandoned, so anything already written is not going to be
            # committed by it
            streamEnd(False);
        }

        #! reports the end of the bounded stream without flushing or ending the transaction
        /** Call this before committing when the transaction is committed on the datasource directly
            rather than through commit(), so that a mutation observer sees the stream's end boundary
            before the transaction outcome rather than after it.  It is a no-op when no stream is in
            progress, and calling it more than once is harmless.

            @par Example:
            @code{.py}
{
    on_success {
        op.flush();
        op.endStream();
        ds.commit();
    }
    on_error {
        op.discard();
        ds.rollback();
    }
    map op.queueData($1), data.iterator();
}
            @endcode

            @see
            - flush()
            - discard()

            @since %BulkSqlUtil 1.5
        */
        endStream() {
            streamEnd(True);
        }

        #! flushes any queued data and commits the transaction
        nothing commit() {
            flush();
            # the stream boundary is reported before the transaction outcome, so that a consumer
            # sees the operation close before the commit it belongs to
            streamEnd(True);
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
