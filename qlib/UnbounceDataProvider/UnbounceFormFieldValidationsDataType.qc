# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file UnbounceFormFieldValidationsDataType.qc Unbounce form field validations nested data type

/*  UnbounceFormFieldValidationsDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Unbounce form field validations nested data type
public class UnbounceFormFieldValidationsDataType inherits HashDataType {
    public {
        const Fields = {
            "required": {
                "display_name": "Required",
                "short_desc": "Field is required",
                "desc": "Whether the form field is required",
                "type": SoftBoolOrNothingType,
            },
            "email": {
                "display_name": "Email",
                "short_desc": "Email validation",
                "desc": "Whether email validation is enabled for this field",
                "type": SoftBoolOrNothingType,
            },
            "phone": {
                "display_name": "Phone",
                "short_desc": "Phone validation",
                "desc": "Whether phone validation is enabled for this field",
                "type": SoftBoolOrNothingType,
            },
        };
    }

    constructor(Type base_type = AutoHashType) : HashDataType(base_type,
            "UnbounceFormFieldValidationsDataType") {
        addQoreFields(Fields);
    }
}

} # namespace UnbounceDataProvider
