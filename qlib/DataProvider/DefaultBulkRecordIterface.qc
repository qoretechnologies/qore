# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DefaultBulkRecordIterface class definition

/** DefaultBulkRecordIterface.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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
#! Default bulk record interface class for data providers that do not support bulk read APIs
public class DefaultBulkRecordIterface inherits AbstractDataProviderBulkRecordInterface {
    private {
        AbstractDataProviderRecordIterator i;
    }

    #! Returns an iterator for zero or more records matching the search options
    /** @param block_size the record block size to use
        @param i the iterator to use
        @param requires_result if set to @ref True, at least one record must be returned by the iterator; if no
        records are available, an exception is thrown when this parameter is set to @ref True
        @param max_records the maximum number of records to return; if set, an exception is thrown when the maximum
        number of records is exceeded
    */
    constructor(int block_size, AbstractDataProviderRecordIterator i, *bool requires_result, *int max_records)
            : AbstractDataProviderBulkRecordInterface(block_size, requires_result, max_records) {
        self.i = i;
    }

    #! Returns a hash of lists according to the block size
    /** @throw INVALID-ITERATOR the iterator is not pointing at a valid element

        @note This call moves the internal record pointer forward, therefore multiple calls of this methods will
        return different results as long as data is available
    */
    private *hash<string, list<auto>> getValueImpl() {
        hash<string, list<auto>> rv;
        int cnt = 0;
        while (i.next()) {
            hash<auto> rec = i.getValue();
            if (!rv) {
                # issue #4457: make sure lists are "list<auto>" type
                rv = map {$1: ()}, keys rec;
            }
            map rv{$1.key} += $1.value, rec.pairIterator();
            if (++cnt == block_size) {
                break;
            }
        }
        return rv;
    }
}
}
