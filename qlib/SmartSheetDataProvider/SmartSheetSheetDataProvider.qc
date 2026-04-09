# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SmartSheetDataProvider module definition

/*  SmartSheetSheetDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Individual Smartsheet sheet data provider with dynamic column mapping
/** This provider models a single Smartsheet sheet as a table. It dynamically fetches column
    definitions and builds a typed record from them. Rows are records with column-name-based
    fields.
*/
public class SmartSheetSheetDataProvider inherits SmartSheetDataProviderBase {
    public {
        #! Search options for querying rows
        const SearchOptions = GenericRecordSearchOptions{"requires_result", "max_records"} + {
            "rowId": <DataProviderOptionInfo>{
                "display_name": "Row ID",
                "short_desc": "Filter by specific row ID",
                "type": AbstractDataProviderTypeMap."*int",
                "desc": "If provided, returns only the row with this specific ID",
            },
            "columns": <DataProviderOptionInfo>{
                "display_name": "Columns",
                "short_desc": "Columns to include in output",
                "type": AbstractDataProviderTypeMap."any",
                "desc": "A list of column names to include in the result. If not provided, "
                    "all columns are returned",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum number of rows to return",
                "type": AbstractDataProviderTypeMap."*int",
                "desc": "The maximum number of rows to return",
            },
        };

        #! Static info for the data provider
        const ProviderInfo = <DataProviderInfo>{
            "type": "SmartSheetSheetDataProvider",
            "has_record": True,
            "supports_read": True,
            "supports_create": True,
            "supports_update": True,
            "supports_delete": True,
            "supports_native_search": True,
            "search_options": SearchOptions,
        };
    }

    private {
        #! The sheet ID
        string sheet_id;

        #! The sheet name
        string sheet_name;

        #! The full sheet data from the API
        hash<auto> sheet_data;

        #! Record info built from column definitions
        hash<SmartSheetRecordInfo> record_info();
    }

    #! Creates the object from the REST client and sheet data
    /** @param rest the RestClientIo client
        @param sheet_data the full sheet data from GET /sheets/{id}
    */
    constructor(RestClientIo::RestClientIo rest, hash<auto> sheet_data)
            : SmartSheetDataProviderBase(rest) {
        self.sheet_data = sheet_data;
        self.sheet_id = string(sheet_data.id);
        self.sheet_name = sheet_data.name;
        buildRecordInfo();
    }

    #! Returns the data provider name
    string getName() {
        return sheet_id;
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("Smartsheet sheet '%s' (ID: %s) with %d columns",
            sheet_name, sheet_id, sheet_data.columns.size());
    }

    #! Returns the record type for this sheet
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return record_info.record_type;
    }

    #! Returns the create record type (excludes non-creatable fields)
    private *hash<string, AbstractDataField> getCreateRecordTypeImpl(*hash<auto> create_options) {
        return record_info.record_type - keys record_info.no_create;
    }

    #! Returns the update record type (all fields optional, excludes non-updatable)
    private *hash<string, AbstractDataField> getUpdateRecordTypeImpl(*hash<auto> search_options) {
        return cast<*hash<string, AbstractDataField>>(map {
            $1.key: $1.value.getOrNothingType(),
        }, (record_info.record_type - keys record_info.no_update).pairIterator());
    }

    #! Creates a new row in the sheet
    /** @param rec a hash with column names as keys and cell values as values
        @param create_options create options (not used)
        @return the created row record
    */
    private *hash<auto> createRecordImpl(hash<auto> rec, *hash<auto> create_options) {
        list<hash<auto>> cells = ();
        foreach hash<auto> i in (rec.pairIterator()) {
            *string col_id = record_info.column_name_to_id{i.key};
            if (!col_id) {
                continue;
            }
            cells += {"columnId": col_id.toInt(), "value": i.value};
        }
        hash<auto> row = {"toTop": True, "cells": cells};
        hash<auto> resp = doPost("sheets/" + sheet_id + "/rows", (row,));
        if (resp.result) {
            return mapRowToRecord(resp.result[0]);
        }
        return resp;
    }

    #! Searches for rows matching the given criteria
    /** @param where_cond search criteria (column name -> value pairs)
        @param search_options search options (rowId, columns, limit)
        @return an iterator for matching rows
    */
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        return new SmartSheetRecordIterator(rest, sheet_id, record_info, where_cond,
            search_options);
    }

    #! Updates rows matching the given criteria
    /** @param set the field values to set
        @param where_cond search criteria to find rows to update
        @param search_options search options
        @return the number of rows updated
    */
    private int updateRecordsImpl(hash<auto> set, *hash<auto> where_cond,
            *hash<auto> search_options) {
        # First find the rows to update
        AbstractDataProviderRecordIterator i = searchRecordsImpl(where_cond, search_options);
        list<hash<auto>> rows_to_update = ();
        while (i.next()) {
            hash<auto> row = i.getValue();
            list<hash<auto>> cells = ();
            foreach hash<auto> s in (set.pairIterator()) {
                *string col_id = record_info.column_name_to_id{s.key};
                if (col_id) {
                    cells += {"columnId": col_id.toInt(), "value": s.value};
                }
            }
            if (cells) {
                rows_to_update += {"id": row.id, "cells": cells};
            }
        }
        if (!rows_to_update) {
            return 0;
        }
        # Update in batches of MaxBatchRows
        int updated = 0;
        while (rows_to_update) {
            list<hash<auto>> batch;
            if (rows_to_update.size() > MaxBatchRows) {
                batch = rows_to_update[0..MaxBatchRows-1];
                rows_to_update = rows_to_update[MaxBatchRows..];
            } else {
                batch = rows_to_update;
                rows_to_update = ();
            }
            doPut("sheets/" + sheet_id + "/rows", batch);
            updated += batch.size();
        }
        return updated;
    }

    #! Deletes rows matching the given criteria
    /** @param where_cond search criteria to find rows to delete
        @param search_options search options
        @return the number of rows deleted
    */
    private int deleteRecordsImpl(*hash<auto> where_cond, *hash<auto> search_options) {
        # First find the rows to delete
        AbstractDataProviderRecordIterator i = searchRecordsImpl(where_cond, search_options);
        list<string> row_ids = ();
        while (i.next()) {
            hash<auto> row = i.getValue();
            row_ids += string(row.id);
        }
        if (!row_ids) {
            return 0;
        }
        # Delete in batches (using comma-separated IDs in query parameter)
        int deleted = 0;
        while (row_ids) {
            list<string> batch;
            if (row_ids.size() > MaxBatchRows) {
                batch = row_ids[0..MaxBatchRows-1];
                row_ids = row_ids[MaxBatchRows..];
            } else {
                batch = row_ids;
                row_ids = ();
            }
            doDelete("sheets/" + sheet_id + "/rows", {"ids": batch.join(",")});
            deleted += batch.size();
        }
        return deleted;
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        hash<DataProviderInfo> rv = ProviderInfo;
        rv.name = sheet_id;
        rv.desc = getDesc();
        return rv;
    }

    #! Builds record info from the sheet's column definitions
    private buildRecordInfo() {
        # Add system fields first (always present on rows)
        record_info.record_type.id = new QoreDataField({
            "name": "id",
            "display_name": "Row ID",
            "type": SoftIntType,
            "short_desc": "Unique row identifier",
            "desc": "The unique identifier for the row",
        });
        record_info.no_create.id = True;
        record_info.no_update.id = True;

        record_info.record_type.rowNumber = new QoreDataField({
            "name": "rowNumber",
            "display_name": "Row Number",
            "type": SoftIntOrNothingType,
            "short_desc": "Row number in the sheet",
            "desc": "The row number within the sheet (1-based)",
        });
        record_info.no_create.rowNumber = True;
        record_info.no_update.rowNumber = True;

        record_info.record_type.parentId = new QoreDataField({
            "name": "parentId",
            "display_name": "Parent Row ID",
            "type": SoftIntOrNothingType,
            "short_desc": "Parent row ID for hierarchy",
            "desc": "The ID of the parent row (for hierarchical sheets)",
        });

        record_info.record_type.createdAt = new QoreDataField({
            "name": "createdAt",
            "display_name": "Created At",
            "type": SoftDateOrNothingType,
            "short_desc": "Row creation date",
            "desc": "The date and time the row was created",
        });
        record_info.no_create.createdAt = True;
        record_info.no_update.createdAt = True;

        record_info.record_type.modifiedAt = new QoreDataField({
            "name": "modifiedAt",
            "display_name": "Modified At",
            "type": SoftDateOrNothingType,
            "short_desc": "Row last modified date",
            "desc": "The date and time the row was last modified",
        });
        record_info.no_create.modifiedAt = True;
        record_info.no_update.modifiedAt = True;

        # Now process dynamic columns from the sheet
        if (sheet_data.columns) {
            foreach hash<auto> col in (sheet_data.columns) {
                string col_name = col.title;
                string col_id = string(col.id);
                string col_type = col.type ?? "TEXT_NUMBER";

                # Build column ID mappings
                record_info.column_id_to_name{col_id} = col_name;
                record_info.column_name_to_id{col_name} = col_id;
                record_info.column_types{col_name} = col_type;

                # Track date/time fields
                if (DateTimeColumnTypes{col_type}) {
                    record_info.date_time_fields{col_name} = True;
                } else if (DateColumnTypes{col_type}) {
                    record_info.date_fields{col_name} = True;
                }

                # Determine type and allowed values from column type
                Type field_type;
                *list<hash<AllowedValueInfo>> field_allowed_values;

                switch (col_type) {
                    case "TEXT_NUMBER":
                        field_type = SoftStringOrNothingType;
                        break;
                    case "CONTACT_LIST":
                        field_type = SoftStringOrNothingType;
                        if (col.contactOptions) {
                            field_allowed_values = map <AllowedValueInfo>{
                                "display_name": $1.name ?? $1.email,
                                "value": $1.email,
                            }, col.contactOptions;
                        }
                        break;
                    case "DATE":
                        field_type = SoftDateOrNothingType;
                        break;
                    case "DATETIME":
                        field_type = SoftDateOrNothingType;
                        break;
                    case "DURATION":
                        field_type = SoftStringOrNothingType;
                        break;
                    case "PICKLIST":
                        field_type = SoftStringOrNothingType;
                        if (col.options) {
                            field_allowed_values = map <AllowedValueInfo>{
                                "display_name": $1,
                                "value": $1,
                            }, col.options;
                        }
                        break;
                    case "CHECKBOX":
                        field_type = SoftBoolOrNothingType;
                        break;
                    case "ABSTRACT_DATETIME":
                        field_type = SoftDateOrNothingType;
                        break;
                    case "PREDECESSOR":
                        field_type = SoftStringOrNothingType;
                        break;
                    case "MULTI_CONTACT_LIST":
                        field_type = SoftAutoListOrNothingType;
                        if (col.contactOptions) {
                            field_allowed_values = map <AllowedValueInfo>{
                                "display_name": $1.name ?? $1.email,
                                "value": $1.email,
                            }, col.contactOptions;
                        }
                        break;
                    case "MULTI_PICKLIST":
                        field_type = SoftAutoListOrNothingType;
                        if (col.options) {
                            field_allowed_values = map <AllowedValueInfo>{
                                "display_name": $1,
                                "value": $1,
                            }, col.options;
                        }
                        break;
                    default:
                        field_type = SoftStringOrNothingType;
                        break;
                }

                # Build field info with mixed types in the initial literal
                hash<auto> field_info = {
                    "name": col_name,
                    "display_name": col_name,
                    "short_desc": col.description ?? col_name,
                    "desc": col.description
                        ? sprintf("%s — %s", col_name, col.description)
                        : sprintf("Column `%s`", col_name),
                    "type": field_type,
                };
                if (col.primary) {
                    field_info.preselected = True;
                }
                if (field_allowed_values) {
                    field_info.allowed_values = field_allowed_values;
                }

                record_info.record_type{col_name} = new QoreDataField(field_info);
            }
        }
    }

    #! Maps a raw API row to a record with column names
    /** @param row a row hash from the Smartsheet API
        @return a hash with column names as keys
    */
    hash<auto> mapRowToRecord(hash<auto> row) {
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
}

} # namespace SmartSheetDataProvider
