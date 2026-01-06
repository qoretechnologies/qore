# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DataProviderBulkRecordIterator class definition

/** DataProviderBulkRecordIterator.qc Copyright 2019 - 2024 Qore Technologies, s.r.o.

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
%modern
# require type definitions everywhere
#! strict argument handling
# enable all warnings

#! contains all public definitions in the DataProvider module
public namespace DataProvider {
#! Defines the standard record iterator for bulk record iterators
public class DataProviderBulkRecordIterator inherits AbstractDataProviderRecordIterator {
    private {
        #! The bulk iterator object
        AbstractDataProviderBulkRecordInterface bulk;

        #! The current iterator for the current block
        HashListIterator i;

        #! Valid flag
        bool valid = True;
    }

    #! Creates the object
    constructor(AbstractDataProviderBulkRecordInterface bulk) {
        self.bulk = bulk;
    }

    #! Returns the 'requires_result' flag
    bool requiresResult() {
        return bulk.requires_result;
    }

    #! Returns the 'max_records' value
    *int getMaxRecords() {
        return bulk.max_records;
    }

    #! Returns the number of records retrieved from the data provider so far
    /** @return the number of records retrieved from the data provider so far

        @note Does not match the number of records returned by this iterator when iterating a record block
    */
    int getRecordCount() {
        return bulk.record_count;
    }

    #! Returns @ref True as the iterator supports bulk operation
    /** @return @ref True as the iterator supports bulk operation
    */
    bool supportsBulkApi() {
        return True;
    }

    #! Returns the bulk data interface
    /** @param block_size ignored in this implementation

        @return the bulk data interface
    */
    *AbstractDataProviderBulkRecordInterface getBulkApi(*int block_size) {
        return bulk;
    }

    #! returns @ref True "True" if the iterator is currently pointing at a valid element, @ref False "False" if not
    /** @return @ref True "True" if the iterator is currently pointing at a valid element, @ref False "False" if not
    */
    bool valid() {
        return exists i;
    }

    #! Moves the current position to the next element; returns @ref False if there are no more elements
    /** Once this method returns @ref False, it will always return @ref False, as the underlying bulk record iterator
        cannot be reset.

        The iterator object should not be used after this method returns @ref False

        @return @ref False if there are no more elements (in which case the iterator object is invalid and should not
        be used); @ref True if successful (meaning that the iterator object is valid)

        @throw NO-DATA no data available to return and \c requires_result is set
        @throw MAX-RECORDS-EXCEEDED the maximum number of records has been exceeded as set by \c max_records

        @par Example:
        @code{.py}
    while (i.next()) {
        printf(" + %y\n", i.getValue());
    }
        @endcode
    */
    bool next() {
        if (!valid) {
            return False;
        }
        if (i && i.next()) {
            return True;
        }
        *hash<string, auto> block = bulk.getValue();
        if (!block || !block.firstValue().lsize()) {
            remove i;
            valid = False;
            return False;
        }
        i = new HashListIterator(block);
        return i.next();
    }

    #! Retrieves the next record if the iterator is valid
    /** @return the next record if the iterator is valid

        @throw INVALID-ITERATOR the iterator is not pointing at a valid element
    */
    hash<auto> getValue() {
        if (!i) {
            throw "INVALID-ITERATOR", sprintf("Call %s::next() to advance the %y iterator before calling "
                "%s::getValue()", self.className(), bulk.className(), self.className());
        }
        return i.getValue();
    }

    #! Returns the record description, if available
    /** @return the record type of the bulk iterator
    */
    *hash<string, AbstractDataField> getRecordType() {
        return bulk.getRecordType();
    }

    #! This method is never called by this implementation
    private bool nextImpl() {
    }
}
}
