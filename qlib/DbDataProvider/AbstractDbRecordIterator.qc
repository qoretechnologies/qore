# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AbstractDbRecordIterator class definition

/** AbstractDbRecordIterator.qc Copyright 2019 - 2024 Qore Technologies, s.r.o.

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
public class AbstractDbRecordIterator inherits AbstractDataProviderRecordIterator {
    private {
        #! the statement being iterated
        AbstractSQLStatement stmt;

        #! release the transaction with a rollback in the destructor?
        bool release_transaction;
    }

    #! creates the iterator
    /** @param release_transaction release the transaction with a rollback in the destructor?
        @param stmt the SQL statement to iterate
        @param search_options search options
    */
    constructor(bool release_transaction, AbstractSQLStatement stmt, *hash<auto> search_options)
            : AbstractDataProviderRecordIterator(search_options.requires_result,
                search_options.max_records) {
        self.release_transaction = release_transaction;
        self.stmt = stmt;
    }

    #! Rolls back the transaction autoomatically if applicable
    /** Rolls back the transaction if a transaction lock was acquired in the constructor and keepTransactionLock()
        was not called
    */
    destructor() {
        if (release_transaction) {
            stmt.rollback();
        }
    }

    #! Returns @ref True if the iterator is valid
    /**
        @return @ref True if the iterator is valid
    */
    bool valid() {
        return stmt ? stmt.valid() : False;
    }

    #! Returns a single record if the iterator is valid
    /** @throw INVALID-ITERATOR the iterator is not pointing at a valid element
    */
    hash<auto> getValue() {
        if (!stmt) {
            throw "INVALID-ITERATOR", "This object was invalidated when a bulk iterator was created from it";
        }
        return stmt.getValue();
    }

    #! Ensures that no rollback is performed when the object is destroyed
    keepTransactionLock() {
        release_transaction = False;
    }

    #! Returns the record description, if available
    /** @return the record type of the query results
    */
    *hash<string, AbstractDataField> getRecordType() {
        return DbDataProvider::getRecordTypeFromDescribeHash(stmt.describe());
    }

    #! Returns the bulk data interface
    /** @return the bulk data interface

        @note This object cannot be used after this call
    */
    *AbstractDataProviderBulkRecordInterface getBulkApi(*int block_size = DefaultBlockSize) {
        return new DbSelectBulkRecordInterface(block_size, remove release_transaction, remove stmt);
    }

    #! Increments the row pointer when retrieving rows from a select statement
    /**
        @return @ref True if there is a row to retrieve, @ref False if not (no more rows to be retrieved)

        @note Exceptions could be thrown by the DBI driver when the statement is executed; see the relevant DBI driver
        docs for more information
    */
    private bool nextImpl() {
        return stmt.next();
    }
}
}
