# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DbTableRecordIterator class definition

/** DbTableRecordIterator.qc Copyright 2019 - 2024 Qore Technologies, s.r.o.

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
public class DbTableRecordIterator inherits AbstractDbRecordIterator {
    private {
        #! The table being iterated
        AbstractTable table;
    }

    #! creates the iterator
    /** @param table the table to iteratate
        @param search_options search options; assumed to have already been processed for validity before this call
    */
    constructor(AbstractTable table, *hash<auto> where_cond, *hash<auto> search_options)
        : AbstractDbRecordIterator(!table.getDatasource().currentThreadInTransaction() && !search_options.forupdate,
            table.getStatement(search_options - ("requires_result", "max_records") + {"where": where_cond}),
            search_options) {
        self.table = table;
    }

    #! Returns the value of the given field in the current record, if the iterator is valid
    /** @param key the name of the field

        @return the value of the given field in the current record, if the iterator is valid

        @throw FIELD-ERROR invalid or unknown field name
    */
    auto memberGate(string key) {
        return doMemberGate(key);
    }

    #! Returns the record description, if available
    /** @return the record type of the iterator
    */
    *hash<string, AbstractDataField> getRecordType() {
        table.getRecordType();
    }
}
}
