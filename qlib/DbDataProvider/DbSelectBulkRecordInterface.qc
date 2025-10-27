# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DbSelectBulkRecordInterface class definition

/** DbSelectBulkRecordInterface.qc Copyright 2019 - 2024 Qore Technologies, s.r.o.

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
#! Defines the record iterator class for Table-based iterators
public class DbSelectBulkRecordInterface inherits AbstractDataProviderBulkRecordInterface {
    private:internal {
        #! the statement being iterated
        AbstractSQLStatement stmt;

        #! release the transaction with a rollback in the destructor?
        bool release_transaction;
    }

    #! creates the iterator
    /** @param block_size the number of records in a read block; must be a positive number
        @param ds the datasource to use
        @param where_cond must be empty; an exception will be thrown if search conditions are passed to this constructor
        @param select_options search options; assumed to have already been processed for validity before this call; contains:
        - \c sql (required): the SQL query
        - \c args (optional): a list of bind arguments to \a sql

        @throw DBSELECT-BULK-ERROR where_cond must be empty
    */
    constructor(int block_size, AbstractDatasource ds, *hash<auto> where_cond, hash<auto> select_options)
            : AbstractDataProviderBulkRecordInterface(block_size, select_options.requires_result,
                select_options.max_records) {
        if (where_cond) {
            throw "DBSELECT-BULK-ERROR", sprintf("where_cond must be empty for %s; value provided: %y",
                self.className(), where_cond);
        }
        # release the transaction with a rollback in the destructor by default if we do not already have a transaction
        # lock
        release_transaction = !ds.currentThreadInTransaction();
        stmt = ds.getSQLStatement();
        stmt.prepare(select_options.sql);
        if (select_options.args) {
            stmt.bindArgs(select_options.args);
        }
    }

    #! Creates the iterator
    /** @param block_size The number of records in a read block; must be a positive number
        @param release_transaction Should the transaction be release in the destructor
        @param stmt The SQL statement to iterate; must be already prepared
    */
    constructor(int block_size, bool release_transaction, AbstractSQLStatement stmt) {
        self.block_size = block_size;
        self.release_transaction = release_transaction;
        self.stmt = stmt;
    }

    #! rolls back the transaction if a transaction lock was acquired in the constructor and keepTransactionLock() was not called
    destructor() {
        if (release_transaction) {
            stmt.rollback();
        }
    }

    #! Ensures that no rollback is performed when the object is destroyed
    keepTransactionLock() {
        release_transaction = False;
    }

    #! Returns a hash of lists according to the block size
    /** @throw INVALID-ITERATOR the iterator is not pointing at a valid element

        @note This call moves the internal record pointer forward, therefore multiple calls of this methods will
        return different results as long as data is available
    */
    private hash<string, list<auto>> getValueImpl() {
        return stmt.fetchColumns(block_size);
    }

    #! Returns the record description, if available
    /** @return the record type of the query results
    */
    *hash<string, AbstractDataField> getRecordType() {
        return DbDataProvider::getRecordTypeFromDescribeHash(stmt.describe());
    }
}
}
