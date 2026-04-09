# -*- mode: qore; indent-tabs-mode: nil -*-

/*  SmartSheetReportDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Data type for Smartsheet report
public class SmartSheetReportDataType inherits DataProvider::HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Report ID",
                "type": SoftIntType,
                "short_desc": "Unique report identifier",
                "desc": "The unique identifier for the report",
            },
            "name": {
                "display_name": "Name",
                "type": SoftStringType,
                "short_desc": "Report name",
                "desc": "The name of the report",
            },
            "accessLevel": {
                "display_name": "Access Level",
                "type": SoftStringOrNothingType,
                "short_desc": "User's access level",
                "desc": "The access level the current user has on the report",
            },
            "permalink": {
                "display_name": "Permalink",
                "type": SoftStringOrNothingType,
                "short_desc": "Permanent URL",
                "desc": "URL that represents a direct link to the report in Smartsheet",
            },
            "createdAt": {
                "display_name": "Created At",
                "type": SoftDateOrNothingType,
                "short_desc": "Creation date",
                "desc": "The date and time the report was created",
            },
            "modifiedAt": {
                "display_name": "Modified At",
                "type": SoftDateOrNothingType,
                "short_desc": "Last modified date",
                "desc": "The date and time the report was last modified",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace SmartSheetDataProvider
