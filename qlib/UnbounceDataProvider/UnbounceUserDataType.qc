# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file UnbounceUserDataType.qc Unbounce user data type

/*  UnbounceUserDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Unbounce user data type
public class UnbounceUserDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "ID",
                "short_desc": "User unique identifier",
                "desc": "The unique identifier of the user",
                "type": SoftStringType,
            },
            "first_name": {
                "display_name": "First Name",
                "short_desc": "User first name",
                "desc": "The first name of the user",
                "type": SoftStringOrNothingType,
            },
            "last_name": {
                "display_name": "Last Name",
                "short_desc": "User last name",
                "desc": "The last name of the user",
                "type": SoftStringOrNothingType,
            },
            "email": {
                "display_name": "Email",
                "short_desc": "User email address",
                "desc": "The email address of the user",
                "type": SoftStringOrNothingType,
            },
        };
    }

    constructor() : HashDataType("UnbounceUserDataType") {
        addQoreFields(Fields);
    }
}

} # namespace UnbounceDataProvider
