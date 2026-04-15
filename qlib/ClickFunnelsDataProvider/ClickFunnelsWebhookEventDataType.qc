# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ClickFunnelsWebhookEventDataType.qc ClickFunnels webhook event data type

/*  ClickFunnelsWebhookEventDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

public namespace ClickFunnelsDataProvider {

#! ClickFunnels webhook event record type
public class ClickFunnelsWebhookEventDataType inherits HashDataType {
    private {
        const Fields = {
            "data": {
                "type": AutoHashOrNothingType,
                "display_name": "Data",
                "short_desc": "The event data payload",
                "desc": "The full data payload of the webhook event",
            },
            "event_id": {
                "type": StringOrNothingType,
                "display_name": "Event ID",
                "short_desc": "The event identifier",
                "desc": "The unique identifier of the webhook event",
            },
            "event_type": {
                "type": StringOrNothingType,
                "display_name": "Event Type",
                "short_desc": "The type of event",
                "desc": "The type of event that triggered the webhook",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "contact.created", "desc": "A new contact was created"},
                    <AllowedValueInfo>{"value": "contact.updated", "desc": "A contact was updated"},
                    <AllowedValueInfo>{"value": "order.created", "desc": "A new order was created"},
                    <AllowedValueInfo>{"value": "order.updated", "desc": "An order was updated"},
                    <AllowedValueInfo>{"value": "order.completed", "desc": "An order was completed"},
                    <AllowedValueInfo>{"value": "form_submission.created", "desc": "A form was submitted"},
                    <AllowedValueInfo>{"value": "courses/enrollment.created", "desc": "A course enrollment was created"},
                    <AllowedValueInfo>{"value": "fulfillment.created", "desc": "A fulfillment was created"},
                ),
            },
            "subject_id": {
                "type": IntOrNothingType,
                "display_name": "Subject ID",
                "short_desc": "The subject resource ID",
                "desc": "The ID of the resource that triggered the event",
            },
            "subject_type": {
                "type": StringOrNothingType,
                "display_name": "Subject Type",
                "short_desc": "The subject resource type",
                "desc": "The type of resource that triggered the event",
            },
            "created_at": {
                "type": DateOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the event occurred",
                "desc": "The timestamp when the event occurred",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace ClickFunnelsDataProvider
