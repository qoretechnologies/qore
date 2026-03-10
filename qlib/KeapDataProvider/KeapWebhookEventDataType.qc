# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore KeapDataProvider module definition

/*  KeapWebhookEventDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the KeapDataProvider module
public namespace KeapDataProvider {

#! Keap webhook event record type
public class KeapWebhookEventDataType inherits HashDataType {
    private {
        #! Field definitions for Keap webhook events
        const Fields = {
            "event_key": {
                "type": StringType,
                "display_name": "Event Key",
                "short_desc": "The webhook event key",
                "desc": "The unique key identifying the webhook event",
            },
            "event_type": {
                "type": StringOrNothingType,
                "display_name": "Event Type",
                "short_desc": "The type of event",
                "desc": "The type of event that triggered the webhook",
                "example_value": "contact.add",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "contact.add"},
                    <AllowedValueInfo>{"value": "contact.edit"},
                    <AllowedValueInfo>{"value": "contact.delete"},
                    <AllowedValueInfo>{"value": "opportunity.add"},
                    <AllowedValueInfo>{"value": "order.add"},
                    <AllowedValueInfo>{"value": "task.add"},
                    <AllowedValueInfo>{"value": "note.add"},
                ),
            },
            "object_type": {
                "type": StringOrNothingType,
                "display_name": "Object Type",
                "short_desc": "The object type",
                "desc": "The type of object the event relates to",
                "example_value": "Contact",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "Contact"},
                    <AllowedValueInfo>{"value": "Opportunity"},
                    <AllowedValueInfo>{"value": "Order"},
                    <AllowedValueInfo>{"value": "Invoice"},
                    <AllowedValueInfo>{"value": "Task"},
                    <AllowedValueInfo>{"value": "Note"},
                ),
            },
            "object_keys": {
                "type": AutoListOrNothingType,
                "display_name": "Object Keys",
                "short_desc": "The affected object keys",
                "desc": "A list of keys identifying the objects affected by this event",
            },
            "timestamp": {
                "type": DateOrNothingType,
                "display_name": "Timestamp",
                "short_desc": "When the event occurred",
                "desc": "The timestamp when the event occurred",
            },
            "api_url": {
                "type": StringOrNothingType,
                "display_name": "API URL",
                "short_desc": "The API URL for the object",
                "desc": "The API URL that can be used to retrieve the affected object",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace KeapDataProvider
