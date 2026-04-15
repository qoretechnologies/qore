# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file UnbounceDeleteLeadRequestDataType.qc Unbounce delete lead request data type

/*  UnbounceDeleteLeadRequestDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Unbounce delete lead request data type
public class UnbounceDeleteLeadRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "page_id": {
                "display_name": "Page ID",
                "short_desc": "Page containing the lead",
                "desc": "The ID of the page containing the lead to delete",
                "type": SoftStringType,
                "required": True,
                "ref_data": "pages",
                "example_value": "abc123def456",
            },
            "lead_id": {
                "display_name": "Lead ID",
                "short_desc": "Lead to delete",
                "desc": "The unique identifier of the lead to delete",
                "type": SoftStringType,
                "required": True,
                "ref_data": "leads",
                "example_value": "abc123def456",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace UnbounceDataProvider
