# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file UnbounceSubAccountDataType.qc Unbounce sub-account data type

/*  UnbounceSubAccountDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Unbounce sub-account data type
public class UnbounceSubAccountDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "ID",
                "short_desc": "Sub-account unique identifier",
                "desc": "The unique identifier of the sub-account",
                "type": SoftStringType,
            },
            "account_id": {
                "display_name": "Account ID",
                "short_desc": "Parent account ID",
                "desc": "The ID of the parent account",
                "type": SoftStringOrNothingType,
            },
            "name": {
                "display_name": "Name",
                "short_desc": "Sub-account name",
                "desc": "The name of the sub-account",
                "type": SoftStringType,
            },
            "created_at": {
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "desc": "When the sub-account was created",
                "type": SoftDateOrNothingType,
            },
            "domains_count": {
                "display_name": "Domains Count",
                "short_desc": "Number of domains",
                "desc": "The number of domains associated with this sub-account",
                "type": SoftIntOrNothingType,
            },
            "website": {
                "display_name": "Website",
                "short_desc": "Sub-account website",
                "desc": "The website URL of the sub-account",
                "type": SoftStringOrNothingType,
            },
        };
    }

    constructor() : HashDataType("UnbounceSubAccountDataType") {
        addQoreFields(Fields);
    }
}

} # namespace UnbounceDataProvider
