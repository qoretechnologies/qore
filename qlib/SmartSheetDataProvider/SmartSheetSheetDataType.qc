# -*- mode: qore; indent-tabs-mode: nil -*-

/*  SmartSheetSheetDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

public namespace SmartSheetDataProvider {

#! Data type for Smartsheet sheet metadata
public class SmartSheetSheetDataType inherits DataProvider::HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Sheet ID",
                "type": SoftIntType,
                "short_desc": "Unique sheet identifier",
                "desc": "The unique identifier for the sheet",
            },
            "name": {
                "display_name": "Name",
                "type": SoftStringType,
                "short_desc": "Sheet name",
                "desc": "The name of the sheet",
            },
            "owner": {
                "display_name": "Owner",
                "type": SoftStringOrNothingType,
                "short_desc": "Sheet owner email",
                "desc": "The email address of the sheet owner",
            },
            "ownerId": {
                "display_name": "Owner ID",
                "type": SoftIntOrNothingType,
                "short_desc": "Sheet owner user ID",
                "desc": "The unique identifier of the sheet owner",
            },
            "accessLevel": {
                "display_name": "Access Level",
                "type": SoftStringOrNothingType,
                "short_desc": "User's access level",
                "desc": "The access level the current user has on the sheet",
            },
            "permalink": {
                "display_name": "Permalink",
                "type": SoftStringOrNothingType,
                "short_desc": "Permanent URL",
                "desc": "URL that represents a direct link to the sheet in Smartsheet",
            },
            "createdAt": {
                "display_name": "Created At",
                "type": SoftDateOrNothingType,
                "short_desc": "Creation date",
                "desc": "The date and time the sheet was created",
            },
            "modifiedAt": {
                "display_name": "Modified At",
                "type": SoftDateOrNothingType,
                "short_desc": "Last modified date",
                "desc": "The date and time the sheet was last modified",
            },
            "version": {
                "display_name": "Version",
                "type": SoftIntOrNothingType,
                "short_desc": "Sheet version number",
                "desc": "The version number of the sheet, incremented on each change",
            },
            "totalRowCount": {
                "display_name": "Total Row Count",
                "type": SoftIntOrNothingType,
                "short_desc": "Number of rows",
                "desc": "The total number of rows in the sheet",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data type for a column definition when creating a sheet
public class SmartSheetColumnDefinitionDataType inherits DataProvider::HashDataType {
    public {
        const Fields = {
            "title": {
                "display_name": "Title",
                "type": SoftStringType,
                "short_desc": "Column name",
                "desc": "The display name/label for the column",
                "required": True,
                "example_value": "Task Name",
            },
            "type": {
                "display_name": "Type",
                "type": SoftStringType,
                "short_desc": "Column type",
                "desc": "The column type",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "TEXT_NUMBER", "display_name": "Text/Number"},
                    <AllowedValueInfo>{"value": "DATE", "display_name": "Date"},
                    <AllowedValueInfo>{"value": "DATETIME", "display_name": "Date & Time"},
                    <AllowedValueInfo>{"value": "CONTACT_LIST", "display_name": "Contact List"},
                    <AllowedValueInfo>{"value": "MULTI_CONTACT_LIST",
                        "display_name": "Multi Contact List"},
                    <AllowedValueInfo>{"value": "CHECKBOX", "display_name": "Checkbox"},
                    <AllowedValueInfo>{"value": "PICKLIST", "display_name": "Dropdown"},
                    <AllowedValueInfo>{"value": "MULTI_PICKLIST",
                        "display_name": "Multi Dropdown"},
                ),
                "example_value": "TEXT_NUMBER",
            },
            "primary": {
                "display_name": "Primary",
                "type": SoftBoolOrNothingType,
                "short_desc": "Whether this is the primary column",
                "desc": "Set to `True` for the sheet's primary column. Exactly one column "
                    "per sheet must be primary",
            },
            "options": {
                "display_name": "Options",
                "type": SoftAutoListOrNothingType,
                "short_desc": "Picklist options",
                "desc": "A list of dropdown values for `PICKLIST` or `MULTI_PICKLIST` columns",
            },
            "width": {
                "display_name": "Width",
                "type": SoftIntOrNothingType,
                "short_desc": "Display width in pixels",
                "desc": "The display width of the column in pixels",
            },
            "symbol": {
                "display_name": "Symbol",
                "type": SoftStringOrNothingType,
                "short_desc": "Symbol type for checkbox/picklist",
                "desc": "Visual symbol for `CHECKBOX` columns (`FLAG`, `STAR`) or "
                    "`PICKLIST` columns (symbol sets like `PRIORITY`, `RYG`, "
                    "`HARVEY_BALLS`, etc.)",
                "allowed_values": (
                    # Checkbox symbols
                    <AllowedValueInfo>{"value": "FLAG", "display_name": "Flag"},
                    <AllowedValueInfo>{"value": "STAR", "display_name": "Star"},
                    # Picklist symbol sets
                    <AllowedValueInfo>{"value": "ARROWS_3_WAY",
                        "display_name": "Arrows (3-way)"},
                    <AllowedValueInfo>{"value": "ARROWS_4_WAY",
                        "display_name": "Arrows (4-way)"},
                    <AllowedValueInfo>{"value": "ARROWS_5_WAY",
                        "display_name": "Arrows (5-way)"},
                    <AllowedValueInfo>{"value": "DECISION_SHAPES",
                        "display_name": "Decision Shapes"},
                    <AllowedValueInfo>{"value": "DECISION_SYMBOLS",
                        "display_name": "Decision Symbols"},
                    <AllowedValueInfo>{"value": "DIRECTIONS_3_WAY",
                        "display_name": "Directions (3-way)"},
                    <AllowedValueInfo>{"value": "DIRECTIONS_4_WAY",
                        "display_name": "Directions (4-way)"},
                    <AllowedValueInfo>{"value": "EFFORT", "display_name": "Effort"},
                    <AllowedValueInfo>{"value": "HARVEY_BALLS",
                        "display_name": "Harvey Balls"},
                    <AllowedValueInfo>{"value": "HEARTS", "display_name": "Hearts"},
                    <AllowedValueInfo>{"value": "MONEY", "display_name": "Money"},
                    <AllowedValueInfo>{"value": "PAIN", "display_name": "Pain"},
                    <AllowedValueInfo>{"value": "PRIORITY", "display_name": "Priority"},
                    <AllowedValueInfo>{"value": "PRIORITY_HML",
                        "display_name": "Priority (H/M/L)"},
                    <AllowedValueInfo>{"value": "PROGRESS", "display_name": "Progress"},
                    <AllowedValueInfo>{"value": "RYG", "display_name": "RYG"},
                    <AllowedValueInfo>{"value": "RYGB", "display_name": "RYGB"},
                    <AllowedValueInfo>{"value": "RYGG", "display_name": "RYGG"},
                    <AllowedValueInfo>{"value": "SIGNAL", "display_name": "Signal"},
                    <AllowedValueInfo>{"value": "SKI", "display_name": "Ski"},
                    <AllowedValueInfo>{"value": "STAR_RATING",
                        "display_name": "Star Rating"},
                    <AllowedValueInfo>{"value": "VCR", "display_name": "VCR"},
                    <AllowedValueInfo>{"value": "WEATHER", "display_name": "Weather"},
                ),
            },
            "description": {
                "display_name": "Description",
                "type": SoftStringOrNothingType,
                "short_desc": "Column description",
                "desc": "Explanatory text about the column",
            },
            "validation": {
                "display_name": "Validation",
                "type": SoftBoolOrNothingType,
                "short_desc": "Enable strict validation",
                "desc": "If `True`, restricts cell values to the column's `options` list "
                    "(strict mode for `PICKLIST`/`MULTI_PICKLIST`)",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }

    constructor(Type base_type) : HashDataType(base_type) {
        addQoreFields(Fields);
    }
}

#! Data type for Smartsheet create sheet request
public class SmartSheetCreateSheetRequestDataType inherits DataProvider::HashDataType {
    public {
        const Fields = {
            "name": {
                "display_name": "Sheet Name",
                "type": SoftStringType,
                "short_desc": "Name for the new sheet",
                "desc": "The name for the new sheet",
                "required": True,
            },
            "columns": {
                "display_name": "Columns",
                "type": new ListDataType("ColumnDefinitions",
                    new SmartSheetColumnDefinitionDataType(), False),
                "short_desc": "Column definitions",
                "desc": "A list of column definitions. Each column requires a `title` and "
                    "`type`. The first column must be the primary column with "
                    "`\"primary\": True`",
                "required": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data type for Smartsheet update sheet request
public class SmartSheetUpdateSheetRequestDataType inherits DataProvider::HashDataType {
    public {
        const Fields = {
            "sheetId": {
                "display_name": "Sheet ID",
                "type": SoftIntType,
                "short_desc": "ID of the sheet to update",
                "desc": "The unique identifier of the sheet to update",
                "required": True,
                "ref_data": "sheets",
            },
            "name": {
                "display_name": "Name",
                "type": SoftStringOrNothingType,
                "short_desc": "New sheet name",
                "desc": "The new name for the sheet",
                "preselected": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data type for Smartsheet delete sheet request
public class SmartSheetDeleteSheetRequestDataType inherits DataProvider::HashDataType {
    public {
        const Fields = {
            "sheetId": {
                "display_name": "Sheet ID",
                "type": SoftIntType,
                "short_desc": "ID of the sheet to delete",
                "desc": "The unique identifier of the sheet to delete",
                "required": True,
                "ref_data": "sheets",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace SmartSheetDataProvider
