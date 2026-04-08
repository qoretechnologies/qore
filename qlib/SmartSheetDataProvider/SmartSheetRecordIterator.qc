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
public class SmartSheetRecordIterator inherits DataProvider::AbstractDataProviderRecordIterator {
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
        } else {
            # Fetch the full sheet with rows
            hash<auto> params;
            if (search_options.limit) {
                params.pageSize = search_options.limit;
            }
            hash<auto> sheet = rest.restGet("sheets/" + sheet_id
                + (params ? "?" + buildQS(params) : "")).body;
            if (sheet.rows) {
                foreach hash<auto> row in (sheet.rows) {
                    hash<auto> record = mapRow(row);
                    # Apply client-side where conditions
                    if (where_cond && !matchesWhereCond(record, where_cond)) {
                        continue;
                    }
                    records += record;
                    if (search_options.limit && records.size() >= search_options.limit) {
                        break;
                    }
                }
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

    #! Maps a raw API row to a record with column names
    private hash<auto> mapRow(hash<auto> row) {
        hash<auto> rv = {
            "id": row.id,
            "rowNumber": row.rowNumber,
            "parentId": row.parentId,
            "createdAt": row.createdAt,
            "modifiedAt": row.modifiedAt,
        };
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
        foreach hash<auto> c in (cond.pairIterator()) {
            if (record{c.key} != c.value) {
                return False;
            }
        }
        return True;
    }

    #! Builds a query string from params
    private string buildQS(hash<auto> params) {
        return SmartSheetDataProviderBase::buildQueryString(params);
    }
}

} # namespace SmartSheetDataProvider
