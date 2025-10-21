# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AbstractDbTableBulkOperation class definition

/** AbstractDbTableBulkOperation.qc Copyright 2023 - 2025 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the DbDataProvider module
public namespace DbDataProvider {
#! Bulk operation object for tables
public class AbstractDbTableBulkOperation inherits AbstractDataProviderBulkOperation {
    private {
        #! The actual bulk insert operation object
        AbstractBulkOperation bulk;
    }

    #! Creates the object
    constructor(DbTableDataProvider provider, AbstractBulkOperation bulk)
            : AbstractDataProviderBulkOperation(provider, bulk.getOperationName(), bulk.getBlockSize()) {
        self.bulk = bulk;
    }

    #! Queues data in the buffer
    queueData(hash<auto> record) {
        bulk.queueData(record);
    }

    #! Queues data in the buffer
    queueData(list<hash<auto>> records) {
        bulk.queueData(records);
    }

    #! Flushes any remaining data to the data provider
    /** This method should always be called before committing the transaction (if the data provider supports
        transaction management) or destroying the object
    */
    flush() {
        bulk.flush();
    }

    #! Discards any buffered data
    /** This method should be called before rolling back the transaction (if the data provider supports transaction
        management) or destroying the object if an error occurs
    */
    discard() {
        bulk.discard();
    }

    #! This method is not called in this implementation
    private flushImpl() {
        # not called in this implementation
    }
}
}
