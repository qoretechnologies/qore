# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore TallyDataProvider module definition

/** TallyWebhookEventDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Webhook event delivery record data type
public class TallyWebhookEventDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Event ID",
                "short_desc": "Event identifier",
                "desc": "The unique identifier of the webhook delivery event",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "webhookId": {
                "display_name": "Webhook ID",
                "short_desc": "Owning webhook ID",
                "desc": "The ID of the webhook that produced this event",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "webhookUrl": {
                "display_name": "Webhook URL",
                "short_desc": "Destination URL",
                "desc": "The destination URL that the event was delivered to",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "eventType": {
                "display_name": "Event Type",
                "short_desc": "Webhook event type",
                "desc": "The type of webhook event; the only valid value is `FORM_RESPONSE`",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "FORM_RESPONSE", "display_name": "Form Response"},
                ),
            },
            "deliveryStatus": {
                "display_name": "Delivery Status",
                "short_desc": "Current delivery status",
                "desc": "The delivery status of the event",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "QUEUED", "display_name": "Queued"},
                    <AllowedValueInfo>{"value": "SUCCEEDED", "display_name": "Succeeded"},
                    <AllowedValueInfo>{"value": "FAILED", "display_name": "Failed"},
                    <AllowedValueInfo>{"value": "DROPPED", "display_name": "Dropped"},
                ),
            },
            "statusCode": {
                "display_name": "Status Code",
                "short_desc": "HTTP response status code",
                "desc": "The HTTP status code returned by the webhook destination, if any",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "response": {
                "display_name": "Response",
                "short_desc": "Webhook response body",
                "desc": "The response body returned by the webhook destination, if any",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "retry": {
                "display_name": "Retry Count",
                "short_desc": "Number of delivery attempts",
                "desc": "The number of times this event has been retried",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "payload": {
                "display_name": "Payload",
                "short_desc": "Webhook event payload",
                "desc": "The full payload that was (or would be) delivered to the webhook URL",
                "type": AbstractDataProviderTypeMap."auto",
            },
            "createdAt": {
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "desc": "ISO 8601 timestamp when the event was created",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "updatedAt": {
                "display_name": "Updated At",
                "short_desc": "Last update timestamp",
                "desc": "ISO 8601 timestamp when the event was last updated",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Singleton instance
public const TallyWebhookEventDataType = new TallyWebhookEventDataType();
}
