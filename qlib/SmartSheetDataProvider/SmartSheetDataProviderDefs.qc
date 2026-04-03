# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SmartSheetDataProviderDefs.qc defines constants and definitions for the SmartSheetDataProvider module

/*  SmartSheetDataProviderDefs.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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
    #! Application name constant
    public const AppName = "Smartsheet";

    #! Default page size for list operations
    public const DefaultPageSize = 100;

    #! Maximum number of rows per batch operation
    public const MaxBatchRows = 500;

    #! Default polling interval in seconds
    public const DefaultPollInterval = 120;

    #! Minimum polling interval in seconds
    public const MinPollInterval = 60;

    #! Maximum number of retries for rate-limited or transient error requests
    public const MaxRetries = 3;

    #! Set of retriable socket errors
    public const RetrySet = {
        "SOCKET-CONNECT-ERROR": True,
        "SOCKET-SSL-ERROR": True,
        "SOCKET-CLOSED": True,
        "SOCKET-TIMEOUT": True,
    };

    # --- Event name constants ---

    #! Event name for sheet updated
    public const EVENT_SHEET_UPDATED = "sheet-updated";

    #! Event name for row created
    public const EVENT_ROW_CREATED = "row-created";

    #! Event name for row updated
    public const EVENT_ROW_UPDATED = "row-updated";

    #! Event name for row deleted
    public const EVENT_ROW_DELETED = "row-deleted";

    #! SmartSheet column type to Qore type mapping
    public const ColumnTypeMap = {
        "TEXT_NUMBER": "SoftStringOrNothingType",
        "CONTACT_LIST": "SoftStringOrNothingType",
        "DATE": "SoftDateOrNothingType",
        "DATETIME": "SoftDateOrNothingType",
        "DURATION": "SoftStringOrNothingType",
        "PICKLIST": "SoftStringOrNothingType",
        "CHECKBOX": "SoftBoolOrNothingType",
        "ABSTRACT_DATETIME": "SoftDateOrNothingType",
        "PREDECESSOR": "SoftStringOrNothingType",
        "MULTI_CONTACT_LIST": "SoftAutoListOrNothingType",
        "MULTI_PICKLIST": "SoftAutoListOrNothingType",
    };

    #! Date/time column types
    public const DateTimeColumnTypes = {
        "DATETIME": True,
        "ABSTRACT_DATETIME": True,
    };

    #! Date column types
    public const DateColumnTypes = {
        "DATE": True,
    };

    #! Smartsheet logo in SVG format (loaded from file)
    public const SmartSheetLogo = File::readTextFile(get_script_dir() + "/smartsheet-logo.svg");

    #! Hashdecl for SmartSheet record info (used by dynamic table providers)
    public hashdecl SmartSheetRecordInfo {
        #! Column name to data field mapping
        hash<string, AbstractDataField> record_type;
        #! Column ID to column name mapping
        hash<string, string> column_id_to_name;
        #! Column name to column ID mapping
        hash<string, string> column_name_to_id;
        #! Column name to SmartSheet type mapping
        hash<string, string> column_types;
        #! Date/time fields for conversion
        hash<string, bool> date_time_fields;
        #! Date fields for conversion
        hash<string, bool> date_fields;
        #! Fields that cannot be set on create
        hash<string, bool> no_create;
        #! Fields that cannot be set on update
        hash<string, bool> no_update;
    }
}
