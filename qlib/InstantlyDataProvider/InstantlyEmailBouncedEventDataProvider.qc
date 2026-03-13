# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore InstantlyDataProvider module definition

/*  InstantlyEmailBouncedEventDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the InstantlyDataProvider module
public namespace InstantlyDataProvider {

#! Instantly email bounced event data provider - watches for bounce events
public class InstantlyEmailBouncedEventDataProvider inherits InstantlyBaseEventDataProvider {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "type": "InstantlyEmailBouncedEventDataProvider",
            "display_name": "Email Bounced",
            "short_desc": "Watch for email bounce events",
            "desc": "Triggers when a campaign email bounces (hard or soft)",
            "constructor_options": ConstructorOptions,
            "supports_observable": True,
        };
        const ConstructorOptions = InstantlyDataProviderBase::BaseConstructorOptions + LocalOptions;
    }

    private {
        static InstantlyEmailBouncedEventDataType eventDataType();
    }

    constructor(InstantlyRestClient::InstantlyRestClient rest, *hash<auto> options)
            : InstantlyBaseEventDataProvider(rest, options) {
    }

    string getName() {
        return EVENT_EMAIL_BOUNCED;
    }

    *string getDesc() {
        return "Instantly email bounced event - triggers when an email bounces";
    }

    string getEventType() {
        return EVENT_EMAIL_BOUNCED;
    }

    string getWebhookEventType() {
        return WEBHOOK_EMAIL_BOUNCED;
    }

    AbstractDataProviderType getEventDataType() {
        return eventDataType;
    }

    private hash<auto> getFakeExampleData() {
        return {
            "event_type": WEBHOOK_EMAIL_BOUNCED,
            "timestamp": now_us().format("YYYY-MM-DDTHH:mm:ss.xxxZ"),
            "campaign_id": "00000000-0000-0000-0000-000000000000",
            "campaign_name": "Example Campaign",
            "lead_email": "bounced@example.com",
            "email_account": "sender@example.com",
            "bounce_type": "hard",
        };
    }

    private hash<auto> getEventSpecificFields(hash<auto> payload) {
        return {
            "lead_email": payload.lead_email,
            "email_account": payload.email_account,
            "bounce_type": payload.bounce_type,
        };
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace InstantlyDataProvider
