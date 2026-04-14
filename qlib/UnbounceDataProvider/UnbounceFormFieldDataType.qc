# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file UnbounceFormFieldDataType.qc Unbounce form field data type

/*  UnbounceFormFieldDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Unbounce form field data type
public class UnbounceFormFieldDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "ID",
                "short_desc": "Form field unique identifier",
                "desc": "The unique identifier of the form field",
                "type": SoftStringType,
            },
            "name": {
                "display_name": "Name",
                "short_desc": "Field name",
                "desc": "The name of the form field",
                "type": SoftStringType,
            },
            "type": {
                "display_name": "Type",
                "short_desc": "Field type",
                "desc": "The type of form field",
                "type": SoftStringOrNothingType,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "text", "desc": "Text input field"},
                    <AllowedValueInfo>{"value": "textarea", "desc": "Multi-line text area"},
                    <AllowedValueInfo>{"value": "checkbox", "desc": "Checkbox field"},
                    <AllowedValueInfo>{"value": "radio", "desc": "Radio button field"},
                    <AllowedValueInfo>{"value": "hidden", "desc": "Hidden field"},
                ),
            },
            "options": {
                "display_name": "Options",
                "short_desc": "Field options",
                "desc": "List of available options for radio/checkbox fields",
                "type": SoftListOrNothingType,
            },
            "validations": {
                "display_name": "Validations",
                "short_desc": "Field validations",
                "desc": "Validation rules configured for this form field",
                "type": new UnbounceFormFieldValidationsDataType(),
            },
        };
    }

    constructor() : HashDataType("UnbounceFormFieldDataType") {
        addQoreFields(Fields);
    }
}

} # namespace UnbounceDataProvider
