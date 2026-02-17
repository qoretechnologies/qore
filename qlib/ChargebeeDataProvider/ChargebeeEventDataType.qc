# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ChargebeeDataProvider module definition

/*  ChargebeeEventDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ChargebeeDataProvider module
public namespace ChargebeeDataProvider {

#! Data type for Chargebee event records
public class ChargebeeEventDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "id": {
                "display_name": "Event ID",
                "type": StringType,
                "desc": "Unique identifier of the event",
                "required": True,
            },
            "occurred_at": {
                "display_name": "Occurred At",
                "type": IntOrNothingType,
                "desc": "Timestamp (UTC, seconds) when the event occurred",
            },
            "source": {
                "display_name": "Source",
                "type": StringOrNothingType,
                "desc": "Source of the event",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "admin_console", "desc": "Chargebee dashboard"},
                    <AllowedValueInfo>{"value": "api", "desc": "Public API"},
                    <AllowedValueInfo>{"value": "scheduled_job", "desc": "Scheduled job"},
                    <AllowedValueInfo>{"value": "hosted_page", "desc": "Hosted page"},
                    <AllowedValueInfo>{"value": "portal", "desc": "Self-serve portal"},
                    <AllowedValueInfo>{"value": "system", "desc": "System-triggered"},
                    <AllowedValueInfo>{"value": "bulk_operation", "desc": "Bulk operation"},
                    <AllowedValueInfo>{"value": "external_service", "desc": "External service"},
                ),
            },
            "event_type": {
                "display_name": "Event Type",
                "type": StringOrNothingType,
                "desc": "The type of event (e.g., customer_created, subscription_cancelled)",
            },
            "api_version": {
                "display_name": "API Version",
                "type": StringOrNothingType,
                "desc": "API version of the event data",
            },
            "user": {
                "display_name": "User",
                "type": StringOrNothingType,
                "desc": "The user who triggered the event, if applicable",
            },
            "content": {
                "display_name": "Content",
                "type": AutoHashOrNothingType,
                "desc": "Event content containing the affected resource data",
            },
            "webhooks": {
                "display_name": "Webhooks",
                "type": SoftListOrNothingType,
                "desc": "Webhook call status information",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace ChargebeeDataProvider
