# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore JotformDataProvider module definition

/*  JotformFormDataType.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the JotformDataProvider module
public namespace JotformDataProvider {

#! JotForm form record type
public class JotformFormDataType inherits HashDataType {
    private {
        #! Field definitions for JotForm forms
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "Form ID",
                "short_desc": "The unique identifier of the form",
                "desc": "The unique identifier of the form",
                "example_value": "123456789012345678",
            },
            "username": {
                "type": StringOrNothingType,
                "display_name": "Username",
                "short_desc": "The form owner's username",
                "desc": "The username of the JotForm account that owns this form",
                "example_value": "myusername",
            },
            "title": {
                "type": StringOrNothingType,
                "display_name": "Title",
                "short_desc": "The form title",
                "desc": "The title of the form",
                "example_value": "Contact Form",
            },
            "height": {
                "type": StringOrNothingType,
                "display_name": "Height",
                "short_desc": "The form height in pixels",
                "desc": "The height of the form embed in pixels",
                "example_value": "539",
            },
            "status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "The form status",
                "desc": "The status of the form (ENABLED, DISABLED, DELETED)",
                "example_value": "ENABLED",
            },
            "new": {
                "type": StringOrNothingType,
                "display_name": "New Submissions",
                "short_desc": "Number of new/unread submissions",
                "desc": "The number of new (unread) submissions for this form",
                "example_value": "5",
            },
            "count": {
                "type": StringOrNothingType,
                "display_name": "Submission Count",
                "short_desc": "Total number of submissions",
                "desc": "The total number of submissions for this form",
                "example_value": "150",
            },
            "url": {
                "type": StringOrNothingType,
                "display_name": "Form URL",
                "short_desc": "The public URL of the form",
                "desc": "The public URL where the form can be accessed",
                "example_value": "https://form.jotform.com/123456789012345678",
            },
            "type": {
                "type": StringOrNothingType,
                "display_name": "Form Type",
                "short_desc": "The type of form",
                "desc": "The type of form (e.g., LEGACY, CARD)",
                "example_value": "LEGACY",
            },
            "favorite": {
                "type": StringOrNothingType,
                "display_name": "Favorite",
                "short_desc": "Whether the form is marked as favorite",
                "desc": "Indicates if the form is marked as a favorite (0 = no, 1 = yes)",
                "example_value": "0",
            },
            "archived": {
                "type": StringOrNothingType,
                "display_name": "Archived",
                "short_desc": "Whether the form is archived",
                "desc": "Indicates if the form is archived (0 = no, 1 = yes)",
                "example_value": "0",
            },
            "created_at": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the form was created",
                "desc": "The date and time when the form was created",
                "example_value": "2025-01-01 09:00:00",
            },
            "updated_at": {
                "type": StringOrNothingType,
                "display_name": "Updated At",
                "short_desc": "When the form was last updated",
                "desc": "The date and time when the form was last updated",
                "example_value": "2025-01-15 14:30:00",
            },
            "last_submission": {
                "type": StringOrNothingType,
                "display_name": "Last Submission",
                "short_desc": "When the last submission was received",
                "desc": "The date and time of the most recent submission",
                "example_value": "2025-01-15 16:45:00",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! JotForm report record type
public class JotformReportDataType inherits HashDataType {
    private {
        #! Field definitions for JotForm reports
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "Report ID",
                "short_desc": "The unique identifier of the report",
                "desc": "The unique identifier of the report",
                "example_value": "987654321098765432",
            },
            "form_id": {
                "type": StringOrNothingType,
                "display_name": "Form ID",
                "short_desc": "The form ID this report belongs to",
                "desc": "The ID of the form this report is associated with",
                "example_value": "123456789012345678",
            },
            "title": {
                "type": StringOrNothingType,
                "display_name": "Title",
                "short_desc": "The report title",
                "desc": "The title of the report",
                "example_value": "Monthly Submissions Report",
            },
            "list_type": {
                "type": StringOrNothingType,
                "display_name": "List Type",
                "short_desc": "The type of report",
                "desc": "The type of report (e.g., csv, excel, pdf)",
                "example_value": "csv",
            },
            "fields": {
                "type": StringOrNothingType,
                "display_name": "Fields",
                "short_desc": "Fields included in the report",
                "desc": "Comma-separated list of field IDs included in the report",
                "example_value": "1,2,3,4,5",
            },
            "status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "The report status",
                "desc": "The status of the report (ENABLED, DISABLED)",
                "example_value": "ENABLED",
            },
            "created_at": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the report was created",
                "desc": "The date and time when the report was created",
                "example_value": "2025-01-10 12:00:00",
            },
            "updated_at": {
                "type": StringOrNothingType,
                "display_name": "Updated At",
                "short_desc": "When the report was last updated",
                "desc": "The date and time when the report was last updated",
                "example_value": "2025-01-15 10:00:00",
            },
            "url": {
                "type": StringOrNothingType,
                "display_name": "Report URL",
                "short_desc": "The URL to access the report",
                "desc": "The URL where the report can be accessed or downloaded",
                "example_value": "https://www.jotform.com/report/987654321098765432",
            },
            "isProtected": {
                "type": StringOrNothingType,
                "display_name": "Is Protected",
                "short_desc": "Whether the report is password protected",
                "desc": "Indicates if the report requires a password to access",
                "example_value": "0",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace JotformDataProvider
