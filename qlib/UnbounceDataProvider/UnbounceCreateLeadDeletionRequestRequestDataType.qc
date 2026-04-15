# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file UnbounceCreateLeadDeletionRequestRequestDataType.qc Unbounce create lead deletion request data type

/*  UnbounceCreateLeadDeletionRequestRequestDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Unbounce create lead deletion request request data type
public class UnbounceCreateLeadDeletionRequestRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "page_id": {
                "display_name": "Page ID",
                "short_desc": "Target page ID",
                "desc": "The ID of the page to create the lead deletion request for",
                "type": SoftStringType,
                "required": True,
                "ref_data": "pages",
                "example_value": "abc123def456",
            },
            "lead_ids": {
                "display_name": "Lead IDs",
                "short_desc": "Specific lead IDs to delete",
                "desc": "A list of specific lead IDs to delete. If not provided, use `all_leads` "
                    "or date range filters instead",
                "type": new ListDataType("LeadIds", SoftStringType, True),
                "example_value": ("lead_id_1", "lead_id_2"),
            },
            "all_leads": {
                "display_name": "All Leads",
                "short_desc": "Delete all leads",
                "desc": "If `True`, delete all leads for the page. Cannot be combined with "
                    "`lead_ids`",
                "type": SoftBoolOrNothingType,
                "example_value": True,
            },
            "from": {
                "display_name": "From",
                "short_desc": "Start date filter",
                "desc": "Delete leads created on or after this date",
                "type": SoftDateOrNothingType,
                "example_value": "2026-01-01T00:00:00Z",
            },
            "to": {
                "display_name": "To",
                "short_desc": "End date filter",
                "desc": "Delete leads created on or before this date",
                "type": SoftDateOrNothingType,
                "example_value": "2026-12-31T23:59:59Z",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace UnbounceDataProvider
