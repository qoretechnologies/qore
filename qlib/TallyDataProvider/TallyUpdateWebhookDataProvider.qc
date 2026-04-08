# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore TallyDataProvider module definition

/** TallyUpdateWebhookDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Request type for updating a webhook
public class TallyUpdateWebhookRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Webhook ID",
                "short_desc": "The webhook ID to update",
                "desc": "The unique identifier of the webhook to update",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "formId": {
                "display_name": "Form ID",
                "short_desc": "The form this webhook is attached to",
                "desc": "The ID of the form that triggers this webhook",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "url": {
                "display_name": "URL",
                "short_desc": "Updated endpoint URL",
                "desc": "The destination URL where webhook events will be delivered",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "eventTypes": {
                "display_name": "Event Types",
                "short_desc": "Event types to subscribe to",
                "desc": "List of event types to subscribe to; the only valid value is "
                    "`FORM_RESPONSE`",
                "type": AbstractDataProviderType::get(SoftListType),
                "required": True,
            },
            "isEnabled": {
                "display_name": "Is Enabled",
                "short_desc": "Enable or disable the webhook",
                "desc": "Set to `True` to enable or `False` to disable the webhook",
                "type": AbstractDataProviderTypeMap."bool",
                "required": True,
            },
            "signingSecret": {
                "display_name": "Signing Secret",
                "short_desc": "Updated signing secret",
                "desc": "Updated secret for payload signing",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "httpHeaders": {
                "display_name": "HTTP Headers",
                "short_desc": "Updated HTTP headers",
                "desc": "Updated list of custom HTTP header objects",
                "type": new ListDataType(new TallyHttpHeaderDataType(), True),
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const TallyUpdateWebhookRequestDataType = new TallyUpdateWebhookRequestDataType();

#! Data provider for updating a webhook
public class TallyUpdateWebhookDataProvider inherits TallyDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "update-webhook",
            "type": "TallyUpdateWebhookDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = TallyUpdateWebhookRequestDataType;
        const ResponseType = TallyUpdateWebhookResultDataType;
    }

    constructor(RestClientIo::RestClientIo rest) : TallyDataProviderBase(rest) {}
    constructor(*hash<auto> options) : TallyDataProviderBase(options) {}
    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string id = req.id;
        hash<auto> body = req - "id";
        doRestCommand("PATCH", "webhooks/" + id, body);
        return {
            "status": "updated",
            "id": id,
        };
    }
}
}
