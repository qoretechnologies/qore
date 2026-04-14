# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file UnbounceFormSubmissionDataType.qc Unbounce form submission nested data type

/*  UnbounceFormSubmissionDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Unbounce form submission nested data type for create lead requests
public class UnbounceFormSubmissionDataType inherits HashDataType {
    public {
        const Fields = {
            "variant_id": {
                "display_name": "Variant ID",
                "short_desc": "Page variant ID",
                "desc": "The ID of the page variant where the form was submitted",
                "type": SoftStringType,
                "required": True,
            },
            "submitter_ip": {
                "display_name": "Submitter IP",
                "short_desc": "IP address of submitter",
                "desc": "The IP address of the form submitter",
                "type": SoftStringType,
                "required": True,
                "example_value": "192.168.1.1",
            },
            "form_data": {
                "display_name": "Form Data",
                "short_desc": "Submitted form data",
                "desc": "The form data as key-value pairs. Keys correspond to form field names "
                    "defined on the page",
                "type": AutoHashType,
                "required": True,
            },
        };
    }

    constructor() : HashDataType("UnbounceFormSubmissionDataType") {
        addQoreFields(Fields);
    }
}

} # namespace UnbounceDataProvider
