# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MewsTableBulkRecordInterface class definition

/** MewsTableBulkRecordInterface.qc Copyright 2019 - 2024 Qore Technologies, s.r.o.

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

#! contains all public definitions in the MewsRestDataProvider module
public namespace MewsRestDataProvider {
#! Defines the record iterator class for table-based iterators
public class MewsTableBulkRecordInterface inherits DataProvider::AbstractDataProviderBulkRecordInterface {
    private:internal {
        #! The abstract data provider that will provide the data to be iterated
        DataProvider::AbstractDataProvider prov;

        #! The request data
        hash<auto> req;

        #! The response key containing the data to be iterated
        string key;

        #! The record type
        hash<string, AbstractDataField> record_type;

        #! Search options
        *hash<auto> search_options;

        #! Current data being iterated
        Qore::AbstractIterator i;

        #! Current cursor value
        string cursor;

        #! Done flag
        bool done;

        #! Total records retrieved
        int count = 0;
    }

    #! creates the iterator
    /** @param block_size the number of records in a read block; must be a positive number
        @param prov data provider that will provide the data to be iterated
        @param req request data
        @param key response key containing the data to be iterated
        @param record_type the record type for the data
        @param where_cond the search criteria; will be processed by processFieldValues()
        @param search_options search options; assumed to have already been processed for validity before this call
    */
    constructor(int block_size, DataProvider::AbstractDataProvider prov, hash<auto> req, string key,
            hash<string, AbstractDataField> record_type, *hash<auto> where_cond, *hash<auto> search_options)
            : AbstractDataProviderBulkRecordInterface(block_size) {
        self.prov = prov;
        self.req = req;
        self.key = key;
        self.record_type = record_type;
        self.search_options = search_options;
    }

    #! Returns a hash of lists according to the block size
    /**
        @note This call moves the internal record pointer forward, therefore multiple calls of this methods will
        return different results as long as data is available
    */
    private hash<string, list<auto>> getValueImpl() {
        if (done) {
        }

        hash<auto> req = self.req;
        if (cursor) {
            req.Limitation.Cursor = cursor;
        }
        hash<auto> res = prov.doRequest({"body": req});
        *list<auto> bdata = res.body{key};
        count += bdata.size();
        prov.debug("bulk records retrieved: %d (total: %d limit: %d cursor: %y)", bdata.size(), count,
            search_options.limit, res.body.Cursor);
        if (search_options.limit) {
            int remaining = search_options.limit - count;
            if (!remaining) {
                done = True;
            } else if (remaining < req.Limitation.Count) {
                self.req.Limitation.Count = remaining;
            }
        }
        if (!done) {
            if (bdata.size() < req.Limitation.Count || !res.body.Cursor) {
                done = True;
            } else {
                cursor = res.body.Cursor;
            }
        }
        hash<auto> rv = map {$1: ()}, keys record_type;
        foreach hash<auto> rec in (bdata) {
            map rv{$1} += rec{$1}, keys record_type;
        }
        return rv;
    }

    #! Returns the record description, if available
    /** @return the record type of the table
    */
    *hash<string, AbstractDataField> getRecordType() {
        return record_type;
    }
}
}
