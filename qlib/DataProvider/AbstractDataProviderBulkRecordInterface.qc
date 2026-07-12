# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AbstractDataProviderBulkRecordInterface class definition

/** AbstractDataProviderBulkRecordInterface.qc Copyright 2019 - 2026 Qore Technologies, s.r.o.

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
#! Defines the abstract class for data provider iterators; the destructor releases the iterator
public class AbstractDataProviderBulkRecordInterface {
    public {
        #! Record count
        int record_count = 0;

        #! Requires result
        bool requires_result;

        #! Max records
        *int max_records;

        #! Record limit
        *int limit;
    }

    private {
        #! The record block size
        int block_size;

        #! Valid flag
        bool valid = True;
    }

    #! Creates the object
    /** @param block_size the record block size for the iterator
        @param requires_result if set to @ref True, at least one record must be returned by the iterator; if no
        records are available, an exception is thrown when this parameter is set to @ref True
        @param max_records the maximum number of records to return; if set, an exception is thrown when the maximum
        number of records is exceeded
        @param limit an optional limit on the number of records to return

        @throw INVALID-BLOCK-SIZE a block_size is not positive

        @since DataProvider 3.3
    */
    constructor(int block_size, *bool requires_result, *int max_records, *int limit) {
        if (block_size <= 0) {
            throw "INVALID-BLOCK-SIZE", sprintf("block_size %d is invalid; the block size must be positive", block_size);
        }
        if (max_records && (block_size > (max_records + 1))) {
            block_size = max_records + 1;
        }
        if (limit) {
            if (max_records && (limit > max_records)) {
                throw "LIMIT-MAX-RECORDS-ARG-ERROR", sprintf("\"limit\" cannot be greater than \"max_records\" "
                    "(limit: %d, max_records: %d)", limit, max_records);
            }
            if (block_size > limit) {
                block_size = limit;
            }
        }
        self.block_size = block_size;
        self.requires_result = requires_result ?? False;
        self.max_records = max_records;
        self.limit = limit;
    }

    #! Returns the block size
    int getBlockSize() {
        return block_size;
    }

    #! Returns True if there are more records to return
    bool valid() {
        return valid;
    }

    #! Returns the 'requires_result' flag
    bool requiresResult() {
        return requires_result;
    }

    #! Returns the 'max_records' value
    *int getMaxRecords() {
        return max_records;
    }

    #! Returns the 'limit' value
    *int getLimit() {
        return limit;
    }

    #! Returns the number of records returned so far
    int getRecordCount() {
        return record_count;
    }

    #! Returns data in the iterator's native pipeline shape or no value if no more data is available
    /** @return a shaped record block according to the block size or @ref nothing if no more data is available

        @throw NO-DATA no data available to return and \c requires_result is set
        @throw MAX-RECORDS-EXCEEDED the maximum number of records has been exceeded as set by \c max_records

        @note
        - This call moves the internal record pointer forward, therefore multiple calls of this methods will
          return different results as long as data is available
        - A short read from getValueImpl() indicates that the final block is being delivered
        - This method may return a \c HashOfLists block, \c BufferColumns block, or \c DataFrameBlock value

        @since DataProvider 3.5
    */
    *auto getShapedValue() {
        if (!valid) {
            return;
        }
        *auto rv = getShapedValueImpl();
        DataProviderShape shape = AbstractDataProcessor::detectShape(rv);
        int size = exists rv ? AbstractDataProcessor::shapeRecordCount(rv, shape) : 0;
        if (size) {
            if (limit && ((record_count + size) > limit)) {
                size = limit - record_count;
                rv = truncateShapedValue(rv, shape, size);
            }
            record_count += size;
            if ((max_records > 0) && (record_count > max_records)) {
                throw "MAX-RECORDS-EXCEEDED", sprintf("The maximum number of records (%d) has been exceeded in the "
                    "record stream (%d records retrieved)", max_records, record_count);
            }
        } else if (!record_count && requires_result) {
            # if we are required to return at least one result, return an empty block once
            throw "NO-DATA", "No data available to return from bulk iterator but the 'requires_result' flag is set";
        }
        if (size < block_size) {
            markInvalid();
            if (!size) {
                return;
            }
        }
        return rv;
    }

    #! Returns a hash of lists (or constant values) or no value if no more data is available
    /** @return a hash of lists (or constant values) according to the block size or @ref nothing if no more data is
        available

        @throw NO-DATA no data available to return and \c requires_result is set
        @throw MAX-RECORDS-EXCEEDED the maximum number of records has been exceeded as set by \c max_records

        @note
        - This is the compatibility accessor.  Use getShapedValue() when a consumer can preserve pipeline shapes
          such as \c BufferColumns or \c DataFrameBlock.
        - This call moves the internal record pointer forward, therefore multiple calls of this methods will
          return different results as long as data is available
        - A short read from getValueImpl() indicates that the final block is being delivered
        - The first hash key must be assigned to a list and cannot be assigned to a constant value
    */
    *hash<auto> getValue() {
        *auto rv = getShapedValue();
        if (!exists rv) {
            return;
        }

        DataProviderShape shape = AbstractDataProcessor::detectShape(rv);
        switch (shape) {
            case DataProviderShape::HashOfLists:
                return rv;
            case DataProviderShape::BufferColumns:
                if (AbstractDataProcessor::isColumnarResult(rv)) {
                    return AbstractDataProcessor::bufferColumnsToHashList(rv.toColumnHash());
                }
                return AbstractDataProcessor::bufferColumnsToHashList(rv);
            case DataProviderShape::DataFrameBlock:
                return AbstractDataProcessor::bufferColumnsToHashList(rv.toColumnHash());
            case DataProviderShape::RowRecords:
                if (rv.typeCode() == NT_LIST) {
                    return AbstractDataProcessor::recordsToHashList(rv);
                }
                return AbstractDataProcessor::recordsToHashList((rv,));
        }
    }

    #! Returns the native pipeline shape produced by getShapedValue()
    /** @return the native pipeline shape produced by getShapedValue()

        @since DataProvider 3.5
    */
    DataProviderShape getValueShape() {
        return getValueShapeImpl();
    }

    #! Returns a standard record iterator for this bulk iterator
    /** @return a standard record iterator for this bulk iterator
    */
    AbstractDataProviderRecordIterator<hash<auto>> getRecordIterator() {
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

    #! Returns a shaped block truncated to the given row count
    private auto truncateShapedValue(auto rv, DataProviderShape shape, int size) {
        switch (shape) {
            case DataProviderShape::HashOfLists:
                return AbstractDataProcessor::sliceColumnBlock(rv, 0, size);
            case DataProviderShape::BufferColumns:
                if (AbstractDataProcessor::isColumnarResult(rv)) {
                    return rv.slice(offset: 0, count: size);
                }
                return AbstractDataProcessor::sliceColumnBlock(rv, 0, size);
            case DataProviderShape::DataFrameBlock:
                return rv.slice(0, size);
            case DataProviderShape::RowRecords:
                if (rv.typeCode() == NT_LIST) {
                    return size ? rv[0..(size - 1)] : ();
                }
                return size ? rv : NOTHING;
        }
        return rv;
    }

    #! Returns the native pipeline shape produced by getShapedValueImpl()
    /** This base class method returns @ref DataProvider::DataProviderShape::HashOfLists "HashOfLists"; reimplement
        in subclasses to advertise a more specific native bulk shape.
    */
    private DataProviderShape getValueShapeImpl() {
        return DataProviderShape::HashOfLists;
    }

    #! Returns a shaped record block in the iterator's native pipeline shape
    /** This base class method bridges to getValueImpl() for compatibility.
    */
    private *auto getShapedValueImpl() {
        return getValueImpl();
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
