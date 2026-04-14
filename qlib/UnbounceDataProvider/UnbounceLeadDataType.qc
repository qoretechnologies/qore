# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file UnbounceLeadDataType.qc Unbounce lead data type

/*  UnbounceLeadDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Unbounce lead data type
public class UnbounceLeadDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "ID",
                "short_desc": "Lead unique identifier",
                "desc": "The unique identifier of the lead",
                "type": SoftStringType,
            },
            "created_at": {
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "desc": "When the lead was created",
                "type": SoftDateOrNothingType,
            },
            "page_id": {
                "display_name": "Page ID",
                "short_desc": "Source page ID",
                "desc": "The ID of the page where the lead was captured",
                "type": SoftStringOrNothingType,
            },
            "variant_id": {
                "display_name": "Variant ID",
                "short_desc": "Page variant ID",
                "desc": "The ID of the page variant that captured the lead",
                "type": SoftStringOrNothingType,
            },
            "submitter_ip": {
                "display_name": "Submitter IP",
                "short_desc": "IP address of submitter",
                "desc": "The IP address of the lead submitter",
                "type": SoftStringOrNothingType,
            },
            "form_data": {
                "display_name": "Form Data",
                "short_desc": "Submitted form data",
                "desc": "The form data submitted by the lead as key-value pairs. Fields are "
                    "dynamic and depend on the page form configuration",
                "type": AutoHashOrNothingType,
            },
            "extra_data": {
                "display_name": "Extra Data",
                "short_desc": "Extra lead data",
                "desc": "Additional data captured with the lead submission. Fields are dynamic "
                    "and depend on the page configuration",
                "type": AutoHashOrNothingType,
            },
        };
    }

    constructor() : HashDataType("UnbounceLeadDataType") {
        addQoreFields(Fields);
    }
}

} # namespace UnbounceDataProvider
