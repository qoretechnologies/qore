# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ClickFunnelsFormSubmissionDataType.qc ClickFunnels form submission data type

/*  ClickFunnelsFormSubmissionDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

public namespace ClickFunnelsDataProvider {

#! ClickFunnels form submission record type
public class ClickFunnelsFormSubmissionDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {
                "type": IntType,
                "display_name": "ID",
                "short_desc": "The submission ID",
                "desc": "The unique identifier of the form submission",
            },
            "public_id": {
                "type": StringOrNothingType,
                "display_name": "Public ID",
                "short_desc": "The public identifier",
                "desc": "The public-facing unique identifier of the form submission",
            },
            "form_id": {
                "type": IntOrNothingType,
                "display_name": "Form ID",
                "short_desc": "The form ID",
                "desc": "The ID of the form this submission belongs to",
            },
            "contact_id": {
                "type": IntOrNothingType,
                "display_name": "Contact ID",
                "short_desc": "The contact ID",
                "desc": "The ID of the contact who submitted the form",
            },
            "page_id": {
                "type": IntOrNothingType,
                "display_name": "Page ID",
                "short_desc": "The page ID",
                "desc": "The ID of the page where the form was submitted",
            },
            "created_at": {
                "type": DateOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the submission was created",
                "desc": "The timestamp when the form was submitted",
            },
            "updated_at": {
                "type": DateOrNothingType,
                "display_name": "Updated At",
                "short_desc": "When the submission was last updated",
                "desc": "The timestamp when the submission was last updated",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace ClickFunnelsDataProvider
