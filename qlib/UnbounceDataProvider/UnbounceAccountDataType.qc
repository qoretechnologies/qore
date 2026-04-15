# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file UnbounceAccountDataType.qc Unbounce account data type

/*  UnbounceAccountDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Unbounce account data type
public class UnbounceAccountDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "ID",
                "short_desc": "Account unique identifier",
                "desc": "The unique identifier of the account",
                "type": SoftStringType,
            },
            "name": {
                "display_name": "Name",
                "short_desc": "Account name",
                "desc": "The name of the account",
                "type": SoftStringType,
            },
            "state": {
                "display_name": "State",
                "short_desc": "Account state",
                "desc": "The current state of the account",
                "type": SoftStringOrNothingType,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "active", "display_name": "Active",
                        "desc": "Account is active"},
                    <AllowedValueInfo>{"value": "suspended", "display_name": "Suspended",
                        "desc": "Account is suspended"},
                ),
            },
            "created_at": {
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "desc": "When the account was created",
                "type": SoftDateOrNothingType,
            },
        };
    }

    constructor() : HashDataType("UnbounceAccountDataType") {
        addQoreFields(Fields);
    }
}

} # namespace UnbounceDataProvider
