# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AbstractDataProviderBulkOperation class definition

/** AbstractDataProviderBulkOperation.qc Copyright 2023 - 2025 Qore Technologies, s.r.o.

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

# assume local scope for variables, do not use "$" signs
%new-style
# require type definitions everywhere
%require-types
#! strict argument handling
%strict-args
# enable all warnings
%enable-all-warnings

#! contains all public definitions in the DataProvider module
public namespace DataProvider {
#! Abstract bulk data operation class
public class AbstractDataProviderBulkOperation {
    public {
        #! Default record block size for bulk operations
        const DefaultBlockSize = 1000;
    }

    private {
        #! The data provider
        AbstractDataProvider provider;

        #! The operation name
        string opname;

        #! Block size
        int block_size;

        #! Record buffer
        hash<auto> buf;

        #! "constant" record values; must be equal in all calls to queueData
        hash<auto> cval;

        #! "constant" record value keys
        list<string> cval_keys;

        #! record count
        int record_count = 0;
    }

    #! Creates the object
    constructor(AbstractDataProvider provider, string opname, int block_size = DefaultBlockSize) {
        self.provider = provider;
        self.opname = opname;
        self.block_size = block_size;
    }

    #! Commits data written to the data provider
    /** Has no effect if the data provider does not support transaction management
    */
    commit() {
        provider.commit();
    }

    #! Rolls back data written to the data provider
    /** Has no effect if the data provider does not support transaction management
    */
    rollback() {
        provider.rollback();
    }

    #! Queues data in the buffer
    queueData(hash<auto> recs) {
        # prepare buffer hash if necessary
        if (!buf) {
            setupInitialRecord(recs);
        }

        # remove constant args
        if (cval_keys) {
            recs -= cval_keys;
        }

        # if we are working with bulk data where the record count would cause the buffer limit to be exceeded
        if (recs.firstValue().typeCode() == NT_LIST) {
            while (True) {
                int ds = recs.firstValue().lsize();
                int cs = buf.firstValue().lsize();
                if ((ds + cs) < block_size) {
                    break;
                }
                int ns = block_size - cs;
                # add on records until we get to the block size
                map buf.$1 += (extract recs.$1, 0, ns), keys buf;
                flushIntern();
            }
        }

        # add record data to block buffer
        map buf{$1.key} += $1.value, recs.pairIterator();
        # return if nothing needs to be flushed
        if (buf.firstValue().size() < block_size) {
            return;
        }
        # flush buffered record data
        flushIntern();
    }

    #! Queues data in the buffer
    queueData(list<hash<auto>> recs) {
        if (!recs) {
            return;
        }
        if (!buf) {
            buf = map {$1.key: ()}, keys recs[0];
        }

        # if we are working with bulk data where the record count would cause the buffer limit to be exceeded
        while (True) {
            int ds = recs.size();
            int cs = buf.firstValue().lsize();
            if ((ds + cs) < block_size) {
                break;
            }
            int ns = block_size - cs;
            # remove and process records to add
            foreach hash<auto> rec in (extract recs, 0, ns) {
                # add record data to block buffer
                map buf.$1 += rec.$1, keys buf;
            }

            flushIntern();
        }

        # add record data to block buffer
        foreach hash<auto> rec in (recs) {
            map buf.$1 += rec.$1, keys buf;
        }
        # return if nothing needs to be flushed
        if (buf.firstValue().size() < block_size) {
            return;
        }
        # flush buffered record data
        flushIntern();
    }

    #! Flushes any remaining batched data to the provider
    /** This method should always be called before committing the transaction (for data providers that support
        transaction management) or destroying the object

        @par Example:
        @code{.py}
# single commit and rollback
on_success prov.commit();
on_error prov.rollback();
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
        - flush() or discard() needs to be executed individually for each bulk operation object

        @see
        - queueData()
        - discard()
    */
    flush() {
        if (buf.firstValue()) {
            flushIntern();
        }
    }

    #! Discards any buffered batched data
    /** This method should be called before destroying the object if an error occurs

        @par Example:
        @code{.py}
# single commit and rollback
on_success prov.commit();
on_error prov.rollback();
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
        - flush() or discard() needs to be executed individually for each bulk operation object

        @see
        - queueData()
        - flush()
    */
    discard() {
        delete buf;
    }

    #! Returns the record count
    int getRecordCount() {
        return record_count;
    }

    #! flushes queued data to the database
    abstract private flushImpl();

    #! Flushes queued data to the provider
    private flushIntern() {
        # flush data to the provider; implemented in subclasses
        flushImpl();
        # update record count
        int bs = buf.firstValue().lsize();
        record_count += bs;
        provider.info("%s (%s): %d record%s flushed (total %d)", provider.getName(), opname, bs, bs == 1 ? "" : "s",
            record_count);
        # reset internal buffer
        map buf.$1 = (), keys buf;
    }

    #! sets up the block buffer given the initial template hash of lists for inserting
    private setupInitialRecord(hash<auto> recs) {
        # ensure we have at least one list of columns values
        bool has_list;
        foreach auto val in (recs.iterator()) {
            if (val.typeCode() == NT_LIST) {
                has_list = True;
                break;
            }
        }

        # do not include constant values in the template record
        if (has_list) {
            cval = map {$1.key: remove recs{$1.key}}, recs.pairIterator(), $1.value.typeCode() != NT_LIST;
            if (cval) {
                cval_keys = keys cval;
            }
        }

        buf = map {$1: ()}, keys recs;
    }
}
}
