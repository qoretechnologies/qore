# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore TallyDataProvider module definition

/** TallySubmissionDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the TallyDataProvider module
public namespace TallyDataProvider {
#! Submission response data type
public class TallySubmissionDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Submission ID",
                "short_desc": "Unique submission identifier",
                "desc": "The unique identifier for the submission",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "formId": {
                "display_name": "Form ID",
                "short_desc": "The form this submission belongs to",
                "desc": "The ID of the form this submission was made to",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "isCompleted": {
                "display_name": "Is Completed",
                "short_desc": "Whether the submission is completed",
                "desc": "`True` if the submission has been fully completed by the respondent",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "submittedAt": {
                "display_name": "Submitted At",
                "short_desc": "Submission timestamp",
                "desc": "ISO 8601 timestamp when the submission was submitted",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "createdAt": {
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "desc": "ISO 8601 timestamp when the submission was created",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "updatedAt": {
                "display_name": "Updated At",
                "short_desc": "Last update timestamp",
                "desc": "ISO 8601 timestamp when the submission was last updated",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "responses": {
                "display_name": "Responses",
                "short_desc": "List of question responses",
                "desc": "List of response objects containing the answers to form questions; "
                    "only questions that have been answered appear in this list",
                "type": new ListDataType("TallySubmissionResponseList",
                    TallySubmissionResponseDataType, True),
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Singleton instance
public const TallySubmissionDataType = new TallySubmissionDataType();
}
