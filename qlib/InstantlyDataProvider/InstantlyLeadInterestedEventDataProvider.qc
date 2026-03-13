# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore InstantlyDataProvider module definition

/*  InstantlyLeadInterestedEventDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Instantly lead interested event data provider - watches for interest events
public class InstantlyLeadInterestedEventDataProvider inherits InstantlyBaseEventDataProvider {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "type": "InstantlyLeadInterestedEventDataProvider",
            "display_name": "Lead Interested",
            "short_desc": "Watch for lead interest events",
            "desc": "Triggers when a lead is marked as interested",
            "constructor_options": ConstructorOptions,
            "supports_observable": True,
        };
        const ConstructorOptions = InstantlyDataProviderBase::BaseConstructorOptions + LocalOptions;
    }

    private {
        static InstantlyLeadInterestedEventDataType eventDataType();
    }

    constructor(InstantlyRestClient::InstantlyRestClient rest, *hash<auto> options)
            : InstantlyBaseEventDataProvider(rest, options) {
    }

    string getName() {
        return EVENT_LEAD_INTERESTED;
    }

    *string getDesc() {
        return "Instantly lead interested event - triggers when a lead is marked as interested";
    }

    string getEventType() {
        return EVENT_LEAD_INTERESTED;
    }

    string getWebhookEventType() {
        return WEBHOOK_LEAD_INTERESTED;
    }

    AbstractDataProviderType getEventDataType() {
        return eventDataType;
    }

    private hash<auto> getFakeExampleData() {
        return {
            "event_type": WEBHOOK_LEAD_INTERESTED,
            "timestamp": now_us().format("YYYY-MM-DDTHH:mm:ss.xxxZ"),
            "campaign_id": "00000000-0000-0000-0000-000000000000",
            "campaign_name": "Example Campaign",
            "lead_email": "interested@example.com",
            "lead_first_name": "John",
            "lead_last_name": "Doe",
        };
    }

    private hash<auto> getEventSpecificFields(hash<auto> payload) {
        return {
            "lead_email": payload.lead_email,
            "lead_first_name": payload.lead_first_name ?? payload.first_name,
            "lead_last_name": payload.lead_last_name ?? payload.last_name,
        };
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace InstantlyDataProvider
