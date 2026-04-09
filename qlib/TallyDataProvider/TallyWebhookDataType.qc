# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore TallyDataProvider module definition

/** TallyWebhookDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Webhook response data type
public class TallyWebhookDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Webhook ID",
                "short_desc": "Unique webhook identifier",
                "desc": "The unique identifier for the webhook",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "formId": {
                "display_name": "Form ID",
                "short_desc": "The form this webhook is attached to",
                "desc": "The ID of the form that triggers this webhook",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "url": {
                "display_name": "URL",
                "short_desc": "The webhook endpoint URL",
                "desc": "The destination URL where webhook events are delivered",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "signingSecret": {
                "display_name": "Signing Secret",
                "short_desc": "Secret for payload signing",
                "desc": "Optional secret used to sign webhook payloads for verification",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "httpHeaders": {
                "display_name": "HTTP Headers",
                "short_desc": "Custom HTTP headers",
                "desc": "List of custom HTTP header objects (name/value pairs) sent with webhook "
                    "requests",
                "type": new ListDataType(new TallyHttpHeaderDataType(), True),
            },
            "eventTypes": {
                "display_name": "Event Types",
                "short_desc": "Subscribed event types",
                "desc": "List of event types this webhook is subscribed to; the only valid value "
                    "is `FORM_RESPONSE`",
                "type": AbstractDataProviderType::get(SoftListOrNothingType),
            },
            "externalSubscriber": {
                "display_name": "External Subscriber",
                "short_desc": "External subscriber identifier",
                "desc": "Optional identifier for the external subscriber",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "isEnabled": {
                "display_name": "Is Enabled",
                "short_desc": "Whether the webhook is active",
                "desc": "`True` if the webhook is enabled and delivering events",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "lastSyncedAt": {
                "display_name": "Last Synced At",
                "short_desc": "Last sync timestamp",
                "desc": "ISO 8601 timestamp of the last successful webhook delivery",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "createdAt": {
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "desc": "ISO 8601 timestamp when the webhook was created",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "updatedAt": {
                "display_name": "Updated At",
                "short_desc": "Last update timestamp",
                "desc": "ISO 8601 timestamp when the webhook was last updated",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Singleton instance
public const TallyWebhookDataType = new TallyWebhookDataType();
}
