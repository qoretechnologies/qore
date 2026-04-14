# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file UnbounceCreateLeadRequestDataType.qc Unbounce create lead request data type

/*  UnbounceCreateLeadRequestDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the UnbounceDataProvider module
public namespace UnbounceDataProvider {

#! Unbounce create lead request data type
public class UnbounceCreateLeadRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "page_id": {
                "display_name": "Page ID",
                "short_desc": "Target page ID",
                "desc": "The ID of the page to create the lead on",
                "type": SoftStringType,
                "required": True,
                "ref_data": "pages",
            },
            "form_submission": {
                "display_name": "Form Submission",
                "short_desc": "Form submission data",
                "desc": "The form submission data including variant ID, submitter IP, "
                    "and form field values",
                "type": new UnbounceFormSubmissionDataType(),
                "required": True,
            },
            "conversion": {
                "display_name": "Conversion",
                "short_desc": "Count as conversion",
                "desc": "Whether to count this lead as a conversion",
                "type": SoftBoolOrNothingType,
            },
            "visitor_id": {
                "display_name": "Visitor ID",
                "short_desc": "Visitor identifier",
                "desc": "The visitor ID to associate with this lead",
                "type": SoftStringOrNothingType,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace UnbounceDataProvider
