# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore TallyDataProvider module definition

/** TallyFormDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the TallyDataProvider module
public namespace TallyDataProvider {
#! Form response data type
public class TallyFormDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Form ID",
                "short_desc": "Unique form identifier",
                "desc": "The unique identifier for the form",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "name": {
                "display_name": "Form Name",
                "short_desc": "The name of the form",
                "desc": "The display name of the form",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "workspaceId": {
                "display_name": "Workspace ID",
                "short_desc": "The workspace this form belongs to",
                "desc": "The ID of the workspace containing this form",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "status": {
                "display_name": "Status",
                "short_desc": "The form status",
                "desc": "The current status of the form",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "BLANK", "display_name": "Blank"},
                    <AllowedValueInfo>{"value": "DRAFT", "display_name": "Draft"},
                    <AllowedValueInfo>{"value": "PUBLISHED", "display_name": "Published"},
                    <AllowedValueInfo>{"value": "DELETED", "display_name": "Deleted"},
                ),
            },
            "numberOfSubmissions": {
                "display_name": "Number of Submissions",
                "short_desc": "Total submission count",
                "desc": "The total number of submissions for this form",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "isClosed": {
                "display_name": "Is Closed",
                "short_desc": "Whether the form is closed",
                "desc": "`True` if the form is closed and no longer accepting submissions",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "payments": {
                "display_name": "Payments",
                "short_desc": "Payment configurations",
                "desc": "List of payment configurations for the form, each with an amount and "
                    "currency code",
                "type": new ListDataType(new TallyPaymentDataType(), True),
            },
            "createdAt": {
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "desc": "ISO 8601 timestamp when the form was created",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "updatedAt": {
                "display_name": "Updated At",
                "short_desc": "Last update timestamp",
                "desc": "ISO 8601 timestamp when the form was last updated",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "settings": {
                "display_name": "Settings",
                "short_desc": "Form settings",
                "desc": "Form settings including closure, notifications, and display options; "
                    "only included when fetching a single form",
                "type": new TallyFormSettingsDataType(AutoHashOrNothingType),
            },
            "blocks": {
                "display_name": "Blocks",
                "short_desc": "Form blocks",
                "desc": "List of form content blocks (questions, text, images, etc.); "
                    "only included when fetching a single form",
                "type": new ListDataType(new TallyBlockDataType(), True),
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Singleton instance
public const TallyFormDataType = new TallyFormDataType();
}
