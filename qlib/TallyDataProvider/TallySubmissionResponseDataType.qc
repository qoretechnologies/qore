# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore TallyDataProvider module definition

/** TallySubmissionResponseDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Submission response item data type (individual answer within a submission)
public class TallySubmissionResponseDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Response ID",
                "short_desc": "Unique response identifier",
                "desc": "The unique identifier for this response",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "formId": {
                "display_name": "Form ID",
                "short_desc": "The form this response belongs to",
                "desc": "The ID of the form this response was submitted to",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "questionId": {
                "display_name": "Question ID",
                "short_desc": "The question this response answers",
                "desc": "The ID of the question this response answers",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "respondentId": {
                "display_name": "Respondent ID",
                "short_desc": "The respondent identifier",
                "desc": "The identifier for the person who submitted this response",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "submissionId": {
                "display_name": "Submission ID",
                "short_desc": "The parent submission",
                "desc": "The ID of the submission containing this response",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "sessionUuid": {
                "display_name": "Session UUID",
                "short_desc": "Session identifier",
                "desc": "The UUID of the session during which this response was submitted",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "answer": {
                "display_name": "Answer",
                "short_desc": "The answer value",
                "desc": "The answer value; can be a string, number, boolean, array, or object depending "
                    "on the question type",
                "type": AbstractDataProviderType::get(AbstractDataProviderType::anyType),
            },
            "formattedAnswer": {
                "display_name": "Formatted Answer",
                "short_desc": "Human-readable formatted answer",
                "desc": "The human-readable formatted representation of the answer",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "createdAt": {
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "desc": "ISO 8601 timestamp when this response was created",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "updatedAt": {
                "display_name": "Updated At",
                "short_desc": "Last update timestamp",
                "desc": "ISO 8601 timestamp when this response was last updated",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Singleton instance
public const TallySubmissionResponseDataType = new TallySubmissionResponseDataType();
}
