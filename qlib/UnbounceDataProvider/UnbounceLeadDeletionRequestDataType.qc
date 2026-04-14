# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file UnbounceLeadDeletionRequestDataType.qc Unbounce lead deletion request data type

/*  UnbounceLeadDeletionRequestDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Unbounce lead deletion request response data type
public class UnbounceLeadDeletionRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "ID",
                "short_desc": "Deletion request ID",
                "desc": "The unique identifier of the lead deletion request",
                "type": SoftStringType,
            },
            "page_id": {
                "display_name": "Page ID",
                "short_desc": "Page ID",
                "desc": "The ID of the page associated with the deletion request",
                "type": SoftStringOrNothingType,
            },
            "created_by": {
                "display_name": "Created By",
                "short_desc": "Creator identifier",
                "desc": "The identifier of the user or process that created the deletion request",
                "type": SoftStringOrNothingType,
            },
            "created_at": {
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "desc": "When the deletion request was created",
                "type": SoftDateOrNothingType,
            },
            "completed_at": {
                "display_name": "Completed At",
                "short_desc": "Completion timestamp",
                "desc": "When the deletion request was completed",
                "type": SoftDateOrNothingType,
            },
            "total_leads_deleted": {
                "display_name": "Total Leads Deleted",
                "short_desc": "Number of leads deleted",
                "desc": "The total number of leads deleted by this request",
                "type": SoftIntOrNothingType,
            },
            "status": {
                "display_name": "Status",
                "short_desc": "Deletion request status",
                "desc": "The current status of the deletion request",
                "type": SoftStringOrNothingType,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "completed", "desc": "Deletion completed successfully"},
                    <AllowedValueInfo>{"value": "failed", "desc": "Deletion failed"},
                    <AllowedValueInfo>{"value": "pending", "desc": "Deletion is pending"},
                ),
            },
        };
    }

    constructor() : HashDataType("UnbounceLeadDeletionRequestDataType") {
        addQoreFields(Fields);
    }
}

} # namespace UnbounceDataProvider
