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
                "type": SoftAutoListType,
                "short_desc": "Column definitions",
                "desc": "A list of column definition hashes. Each column requires a `title` "
                    "(string) and `type` (one of: `TEXT_NUMBER`, `DATE`, `DATETIME`, "
                    "`CONTACT_LIST`, `CHECKBOX`, `PICKLIST`, `DURATION`, `PREDECESSOR`, "
                    "`ABSTRACT_DATETIME`, `MULTI_CONTACT_LIST`, `MULTI_PICKLIST`). The first "
                    "column must be the primary column with `\"primary\": True`",
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
            },
            "name": {
                "display_name": "Name",
                "type": SoftStringOrNothingType,
                "short_desc": "New sheet name",
                "desc": "The new name for the sheet",
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
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace SmartSheetDataProvider
