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
    public {
        #! Field definitions
        const Fields = {
            "id": {
                "display_name": "Event ID",
                "short_desc": "Unique event identifier",
                "type": StringType,
                "desc": "Unique identifier of the event",
                "required": True,
            },
            "occurred_at": {
                "display_name": "Occurred At",
                "short_desc": "Event timestamp",
                "type": IntOrNothingType,
                "desc": "Timestamp (UTC, seconds) when the event occurred",
            },
            "source": {
                "display_name": "Source",
                "short_desc": "Event source",
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
                "short_desc": "Type of event",
                "type": StringOrNothingType,
                "desc": "The type of event (e.g., `customer_created`, `subscription_cancelled`)",
            },
            "api_version": {
                "display_name": "API Version",
                "short_desc": "API version of event data",
                "type": StringOrNothingType,
                "desc": "API version of the event data",
            },
            "user": {
                "display_name": "User",
                "short_desc": "User who triggered the event",
                "type": StringOrNothingType,
                "desc": "The user who triggered the event, if applicable",
            },
            "content": {
                "display_name": "Content",
                "short_desc": "Event resource data",
                "type": AutoHashOrNothingType,
                "desc": "Event content containing the affected resource data",
            },
            "webhooks": {
                "display_name": "Webhooks",
                "short_desc": "Webhook call status",
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
