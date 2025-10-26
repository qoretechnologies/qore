# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AbstractDataProviderBulkRecordInterface class definition

/** AbstractDataProviderBulkRecordInterface.qc Copyright 2019 - 2024 Qore Technologies, s.r.o.

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
#! Defines the abstract class for data provider iterators; the destructor releases the iterator
public class AbstractDataProviderBulkRecordInterface {
    private {
        #! The record block size
        int block_size;

        #! Valid flag
        bool valid = True;
    }

    #! Creates the object
    /** @param block_size the record block size for the iterator
    */
    constructor(int block_size) {
        self.block_size = block_size;
    }

    #! Returns the block size
    int getBlockSize() {
        return block_size;
    }

    #! Returns True if there are more records to return
    bool valid() {
        return valid;
    }

    #! Returns a hash of lists (or constant values) according to the block size or @ref nothing if no more data is available
    /** @return a hash of lists (or constant values) according to the block size or @ref nothing if no more data is
        available

        @note
        - This call moves the internal record pointer forward, therefore multiple calls of this methods will
          return different results as long as data is available
        - A short read from getValueImpl() indicates that the final block is being delivered
        - The first hash key must be assigned to a list and cannot be assigned to a constant value
    */
    *hash<auto> getValue() {
        if (!valid) {
            return;
        }
        *hash<auto> rv = getValueImpl();
        if ((int size = rv.firstValue().size()) < block_size) {
            markInvalid();
            if (!size) {
                return;
            }
        }
        return rv;
    }

    #! Returns a standard record iterator for this bulk iterator
    /** @return a standard record iterator for this bulk iterator
    */
    AbstractDataProviderRecordIterator getRecordIterator() {
        return new DataProviderBulkRecordIterator(self);
    }

    #! Returns the record description, if available
    /** This base class method returns @ref nothing; reimplement in subclasses to provide a record type
    */
    *hash<string, AbstractDataField> getRecordType() {
    }

    #! Called when the iterator is marked as invalid
    private markInvalid() {
        valid = False;
    }

    #! Returns a hash of lists (or constant values) according to the block size
    /** @return a hash of lists (or constant values) according to the block size; returning @ref nothing or a hash
        where the first value is an empty list (or @ref nothing) indicates that there are no more records to return

        @throw INVALID-ITERATOR the iterator is not pointing at a valid element

        @note
        - This call moves the internal record pointer forward, therefore multiple calls of this methods will
          return different results as long as data is available
        - The first hash key must be assigned to a list and cannot be assigned to a constant value
    */
    abstract private *hash<auto> getValueImpl();
}
}
