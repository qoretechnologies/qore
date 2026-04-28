# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore TallyDataProvider module definition

/** TallyRetryWebhookEventDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Request type for retrying a webhook event
public class TallyRetryWebhookEventRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "webhook_id": {
                "display_name": "Webhook ID",
                "short_desc": "The webhook owning the event",
                "desc": "The ID of the webhook that originally produced the event",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "event_id": {
                "display_name": "Event ID",
                "short_desc": "The webhook event to retry",
                "desc": "The ID of the failed webhook event to retry",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const TallyRetryWebhookEventRequestDataType = new TallyRetryWebhookEventRequestDataType();

#! Result data type for webhook event retry (API returns 204)
public class TallyRetryWebhookEventResultDataType inherits HashDataType {
    public {
        const Fields = {
            "status": {
                "display_name": "Status",
                "short_desc": "Retry status",
                "desc": "The result status of the retry operation",
                "type": AbstractDataProviderTypeMap."string",
            },
            "webhook_id": {
                "display_name": "Webhook ID",
                "short_desc": "The webhook ID",
                "desc": "The ID of the webhook the event belongs to",
                "type": AbstractDataProviderTypeMap."string",
            },
            "event_id": {
                "display_name": "Event ID",
                "short_desc": "The retried event ID",
                "desc": "The ID of the webhook event that was retried",
                "type": AbstractDataProviderTypeMap."string",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public const TallyRetryWebhookEventResultDataType = new TallyRetryWebhookEventResultDataType();

#! Data provider for retrying a failed webhook event delivery
public class TallyRetryWebhookEventDataProvider inherits TallyDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "retry-webhook-event",
            "type": "TallyRetryWebhookEventDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = TallyRetryWebhookEventRequestDataType;
        const ResponseType = TallyRetryWebhookEventResultDataType;
    }

    constructor(RestClientIo::RestClientIo rest) : TallyDataProviderBase(rest) {}
    constructor(*hash<auto> options) : TallyDataProviderBase(options) {}
    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        doRestCommand("POST", "webhooks/" + req.webhook_id + "/events/" + req.event_id + "/retry");
        return {
            "status": "retried",
            "webhook_id": req.webhook_id,
            "event_id": req.event_id,
        };
    }
}
}
