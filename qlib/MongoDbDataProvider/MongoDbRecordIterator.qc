# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MongoDbRecordIterator class definition

/** MongoDbRecordIterator.qc Copyright 2025 Qore Technologies, s.r.o.

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

%modern

public namespace MongoDbDataProvider {
#! MongoDB record iterator for iterating through collection documents
public class MongoDbRecordIterator inherits DataProvider::AbstractDataProviderRecordIterator {
    private {
        #! The MongoDB cursor
        MongoCursor cursor;

        #! Current document
        *hash<auto> current_doc;

        #! Valid flag - tracks if we're at a valid position
        bool is_valid = False;
    }

    #! Creates the iterator from a collection and search options
    /** @param coll the MongoDB collection
        @param where_cond the search filter
        @param search_options search options (limit, skip, sort, projection)
    */
    constructor(MongoCollection coll, *hash<auto> where_cond, *hash<auto> search_options) {
        hash<auto> opts;
        if (search_options.limit) {
            opts.limit = search_options.limit;
        }
        if (search_options.skip) {
            opts.skip = search_options.skip;
        }
        if (search_options.sort) {
            opts.sort = search_options.sort;
        }
        if (search_options.projection) {
            opts.projection = search_options.projection;
        }

        cursor = coll.find(where_cond ?? {}, opts);
    }

    #! Returns True if the iterator is valid
    /** @return True if the iterator is pointing at a valid record
    */
    bool valid() {
        return is_valid;
    }

    #! Moves to the next record
    /** @return True if a record is available, False if no more records
    */
    private bool nextImpl() {
        # cursor.next() returns the document directly, or NOTHING if no more documents
        *hash<auto> doc = cursor.next();
        if (doc) {
            current_doc = doc;
            is_valid = True;
            return True;
        }
        current_doc = NOTHING;
        is_valid = False;
        return False;
    }

    #! Returns the current record
    /** @return the current record as a hash
    */
    hash<auto> getValue() {
        return current_doc ?? {};
    }

    #! Returns the record type
    *AbstractDataProviderType getRecordType() {
        return AbstractDataProviderTypeMap."hash<auto>";
    }
}
}
