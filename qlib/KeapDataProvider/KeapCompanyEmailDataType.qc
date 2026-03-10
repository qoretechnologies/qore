# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore KeapDataProvider module definition

/*  KeapCompanyEmailDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the KeapDataProvider module
public namespace KeapDataProvider {

#! Keap company email address type (single object for company create/update)
public class KeapCompanyEmailDataType inherits HashDataType {
    private {
        const Fields = {
            "email": {
                "type": StringType,
                "display_name": "Email",
                "short_desc": "The email address",
                "desc": "The email address",
                "required": True,
                "example_value": "info@acme.com",
            },
            "field": {
                "type": StringOrNothingType,
                "display_name": "Field",
                "short_desc": "The email field slot",
                "desc": "The email field slot to use",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "EMAIL1", "desc": "Primary email"},
                    <AllowedValueInfo>{"value": "EMAIL2", "desc": "Secondary email"},
                    <AllowedValueInfo>{"value": "EMAIL3", "desc": "Third email"},
                ),
            },
        };
    }

    constructor() : HashDataType("KeapCompanyEmail") {
        addQoreFields(Fields);
    }

    constructor(Type base_type) : HashDataType(base_type, "KeapCompanyEmail") {
        addQoreFields(Fields);
    }
}

} # namespace KeapDataProvider
