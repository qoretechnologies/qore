# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SmartSheetDataProvider module definition

/*  SmartSheetRecordIterator.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SmartSheetDataProvider module
public namespace SmartSheetDataProvider {

#! Record iterator for Smartsheet sheet rows
/** Fetches rows from a sheet and maps cell values to column names using the record info's
    column ID-to-name mapping.
*/
public class SmartSheetRecordIterator inherits DataProvider::AbstractDataProviderRecordIterator<hash<auto>> {
    public {
        #! The number of rows requested per page when rows are filtered on the client with a limit
        /** @since SmartSheetDataProvider 1.0
        */
        const FilteredPageSize = 100;
    }

    private:internal {
        #! The REST client
        RestClientIo::RestClientIo rest;

        #! The sheet ID
        string sheet_id;

        #! Record info with column mappings
        hash<SmartSheetRecordInfo> record_info;

        #! The internal list iterator
        Qore::ListHashIterator i;

        #! Where conditions for client-side filtering
        *hash<auto> where_cond;

        #! Search options
        *hash<auto> search_options;
    }

    #! Creates the iterator
    /** @param rest the RestClientIo client
        @param sheet_id the sheet ID to fetch rows from
        @param record_info the record info with column mappings
        @param where_cond optional client-side filter conditions
        @param search_options optional search options (rowId, columns, limit)
    */
    constructor(RestClientIo::RestClientIo rest, string sheet_id,
            hash<SmartSheetRecordInfo> record_info, *hash<auto> where_cond,
            *hash<auto> search_options)
            : AbstractDataProviderRecordIterator(search_options.requires_result,
                search_options.max_records) {
        self.rest = rest;
        self.sheet_id = sheet_id;
        self.record_info = record_info;
        self.where_cond = where_cond;
        self.search_options = search_options;

        # Fetch rows from the sheet
        list<hash<auto>> records = ();

        if (search_options.rowId) {
            # Fetch a single row
            try {
                hash<auto> row = rest.restGet("sheets/" + sheet_id + "/rows/"
                    + string(search_options.rowId)).body;
                records = (mapRow(row),);
            } catch (hash<ExceptionInfo> ex) {
                if (ex.err == "REST-RESPONSE-ERROR" && ex.arg.status_code == 404) {
                    # Row not found - return empty iterator
                } else {
                    rethrow;
                }
            }
        } else if (search_options.limit && where_cond) {
            # rows are filtered on the client, so matching rows can be on any page: read pages until the limit is
            # reached or the sheet has no more rows
            records = getFilteredRows(search_options.limit);
        } else {
            # without a filter, the limit is the page size of a single request; without a limit, the whole sheet is
            # read in one request
            hash<auto> params;
            if (search_options.limit) {
                params.pageSize = search_options.limit;
            }
            hash<auto> sheet = rest.restGet("sheets/" + sheet_id
                + (params ? "?" + buildQS(params) : "")).body;
            foreach hash<auto> row in (sheet.rows) {
                hash<auto> record = mapRow(row);
                if (where_cond && !matchesWhereCond(record, where_cond)) {
                    continue;
                }
                push records, record;
            }
        }

        i = new ListHashIterator(records);
    }

    #! Returns @ref True if the iterator is currently valid
    bool valid() {
        return i.valid();
    }

    #! Returns the current row value
    hash<auto> getValue() {
        return search_options.columns ? i.getValue(){search_options.columns} : i.getValue();
    }

    #! Returns the record type
    *hash<string, DataProvider::AbstractDataField> getRecordType() {
        return record_info.record_type;
    }

    #! Advances to the next element
    private bool nextImpl() {
        return i.next();
    }

    #! Returns up to \a limit rows that match the where conditions, reading the sheet page by page
    /** Pages of @ref FilteredPageSize rows are read until \a limit rows match or the sheet has no more rows, so
        no page after the one with the last needed match is read

        @throw THREAD-CANCELLED the thread was cancelled while reading pages
    */
    private list<hash<auto>> getFilteredRows(int limit) {
        list<hash<auto>> records = ();
        for (int page = 1; ; ++page) {
            if (thread_cancelled()) {
                throw "THREAD-CANCELLED", sprintf("reading Smartsheet sheet %y was cancelled at page %d", sheet_id,
                    page);
            }
            hash<auto> sheet = rest.restGet("sheets/" + sheet_id + "?" + buildQS({
                "pageSize": FilteredPageSize,
                "page": page,
            })).body;
            int rows = sheet.rows.lsize();
            foreach hash<auto> row in (sheet.rows) {
                hash<auto> record = mapRow(row);
                if (!matchesWhereCond(record, where_cond)) {
                    continue;
                }
                push records, record;
                if (records.size() == limit) {
                    return records;
                }
            }
            # the last page has fewer rows than the page size, or ends at the total row count
            if (rows < FilteredPageSize
                    || (exists sheet.totalRowCount && page * FilteredPageSize >= sheet.totalRowCount)) {
                return records;
            }
        }
    }

    #! Maps a raw API row to a record with column names
    private hash<auto> mapRow(hash<auto> row) {
        # an untyped hash: a literal whose values all have one type would be typed for them, and the cell values
        # added below can have any type
        hash<auto> rv = {};
        map rv{$1} = row{$1}, ("id", "rowNumber", "parentId", "createdAt", "modifiedAt");
        if (row.cells) {
            foreach hash<auto> cell in (row.cells) {
                *string col_name = record_info.column_id_to_name{string(cell.columnId)};
                if (col_name) {
                    rv{col_name} = cell.value;
                }
            }
        }
        return rv;
    }

    #! Checks if a record matches the where conditions
    private bool matchesWhereCond(hash<auto> record, hash<auto> cond) {
        return all record{$1.key} == $1.value, cond.pairIterator();
    }

    #! Builds a query string from params
    private string buildQS(hash<auto> params) {
        return SmartSheetDataProviderBase::buildQueryString(params);
    }
}

} # namespace SmartSheetDataProvider
