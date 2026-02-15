# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DbSelectRecordIterator class definition

/** DbSelectRecordIterator.qc Copyright 2019 - 2026 Qore Technologies, s.r.o.

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
public class DbSelectRecordIterator inherits AbstractDbRecordIterator {
    #! Creates the iterator
    /** @param ds the datasource to use
        @param where_cond cannot be provided or a \c DBSELECT-ERROR will be thrown
        @param select_options search options; assumed to have already been processed for validity before this call;
        contains:
        - \c sql (required): the SQL query
        - \c args (optional): a list of bind arguments to \a sql

        @throw DBSELECT-ERROR where_cond must be empty
    */
    constructor(AbstractDatasource ds, *hash<auto> where_cond, hash<auto> select_options)
            : AbstractDbRecordIterator(
                !ds.currentThreadInTransaction() && !select_options.forupdate,
                DbSelectRecordIterator::prepareStatement(ds.getSQLStatement(), select_options),
                select_options
            ) {
        if (where_cond) {
            throw "DBSELECT-ERROR", sprintf("where_cond must be empty for %s; value provided: %y",
                self.className(), where_cond);
        }
    }

    #! Returns the value of the given field in the current record, if the iterator is valid
    /** @param key the name of the field

        @return the value of the given field in the current record, if the iterator is valid

        @throw FIELD-ERROR invalid or unknown field name
    */
    auto memberGate(string key) {
        return doMemberGate(key);
    }

    #! Prepares the AbstractSQLStatement object for the iterator
    private static AbstractSQLStatement prepareStatement(AbstractSQLStatement stmt, hash<auto> select_options) {
        stmt.prepare(select_options.sql);
        if (select_options.args) {
            stmt.bindArgs(select_options.args);
        }
        return stmt;
    }
}
}
