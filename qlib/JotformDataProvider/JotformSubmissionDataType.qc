# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore JotformDataProvider module definition

/*  JotformSubmissionDataType.qc Copyright 2025 - 2026 Qore Technologies, s.r.o.

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

#! JotForm submission record type
public class JotformSubmissionDataType inherits HashDataType {
    public {
        #! Field definitions for JotForm submissions
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "Submission ID",
                "short_desc": "The unique identifier of the submission",
                "desc": "The unique identifier of the submission",
                "example_value": "5432109876543210987",
            },
            "form_id": {
                "type": StringType,
                "display_name": "Form ID",
                "short_desc": "The ID of the form",
                "desc": "The ID of the form this submission belongs to",
                "example_value": "123456789012345678",
            },
            "ip": {
                "type": StringOrNothingType,
                "display_name": "IP Address",
                "short_desc": "The submitter's IP address",
                "desc": "The IP address of the user who submitted the form",
                "example_value": "192.168.1.100",
            },
            "status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "The submission status",
                "desc": "The status of the submission (`ACTIVE`, `DELETED`, etc.)",
                "example_value": "ACTIVE",
            },
            "new": {
                "type": StringOrNothingType,
                "display_name": "New",
                "short_desc": "Whether the submission is new/unread",
                "desc": "Indicates if the submission has been read (`0` = read, `1` = new/unread)",
                "example_value": "0",
            },
            "flag": {
                "type": StringOrNothingType,
                "display_name": "Flag",
                "short_desc": "Whether the submission is flagged",
                "desc": "Indicates if the submission has been flagged (`0` = not flagged, `1` = flagged)",
                "example_value": "0",
            },
            "notes": {
                "type": StringOrNothingType,
                "display_name": "Notes",
                "short_desc": "Notes attached to the submission",
                "desc": "Any notes or comments added to the submission",
                "example_value": "",
            },
            "answers": {
                "type": AutoHashOrNothingType,
                "display_name": "Answers",
                "short_desc": "Form field answers",
                "desc": "The form field answers as a hash keyed by question ID. Each answer contains "
                    "the question name, answer value, and other metadata specific to the question type.",
            },
            "created_at": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the submission was created",
                "desc": "The date and time when the submission was created (`YYYY-MM-DD HH:MM:SS` format)",
                "example_value": "2025-01-15 10:30:45",
            },
            "updated_at": {
                "type": StringOrNothingType,
                "display_name": "Updated At",
                "short_desc": "When the submission was last updated",
                "desc": "The date and time when the submission was last updated",
                "example_value": "2025-01-15 11:00:00",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! JotForm submission create input type
/** This is the base/fallback type used when no form is selected.
    When a form is selected, the dynamic type from getRequestTypeWithOptionsImpl()
    is used instead, which has form-specific fields at the top level.
*/
public class JotformSubmissionCreateDataType inherits HashDataType {
    public {
        #! Field definitions for creating submissions
        const Fields = {
            "formID": {
                "type": StringType,
                "display_name": "Form ID",
                "short_desc": "The form ID (required)",
                "desc": "The ID of the form to create the submission in",
                "example_value": "123456789012345678",
                "required": True,
            },
            # Note: submission is optional because dynamic fields may be at top level
            "submission": {
                "type": AutoHashOrNothingType,
                "display_name": "Submission Data",
                "short_desc": "The submission field data (legacy format)",
                "desc": "The form field data as a hash. This is the legacy format - when a form is selected, "
                    "individual form fields appear at the top level instead.\n"
                    "Keys should be question IDs or field names, values should be the answer values.",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! JotForm submission update input type
/** This is the base/fallback type used when no form is selected.
    When a form is selected, the dynamic type from getRequestTypeWithOptionsImpl()
    is used instead, which has form-specific fields at the top level.
*/
public class JotformSubmissionUpdateDataType inherits HashDataType {
    public {
        #! Field definitions for updating submissions
        const Fields = {
            "submissionID": {
                "type": StringType,
                "display_name": "Submission ID",
                "short_desc": "The submission ID (required)",
                "desc": "The ID of the submission to update",
                "example_value": "5432109876543210987",
                "required": True,
            },
            # Note: submission is optional because dynamic fields may be at top level
            "submission": {
                "type": AutoHashOrNothingType,
                "display_name": "Updated Data",
                "short_desc": "The updated field data (legacy format)",
                "desc": "The form field data to update. This is the legacy format - when a form is selected, "
                    "individual form fields appear at the top level instead.\n"
                    "Only include fields that need to be changed.",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace JotformDataProvider
