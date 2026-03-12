# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore InstantlyDataProvider module definition

/*  InstantlyCampaignCompletedEventDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Instantly campaign completed event data provider - watches for campaign completion
public class InstantlyCampaignCompletedEventDataProvider inherits InstantlyBaseEventDataProvider {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "type": "InstantlyCampaignCompletedEventDataProvider",
            "display_name": "Campaign Completed",
            "short_desc": "Watch for campaign completion events",
            "desc": "Triggers when a campaign finishes sending all scheduled emails",
            "constructor_options": ConstructorOptions,
            "supports_observable": True,
        };
        const ConstructorOptions = InstantlyDataProviderBase::BaseConstructorOptions + LocalOptions;
    }

    private {
        static InstantlyCampaignCompletedEventDataType eventDataType();
    }

    constructor(InstantlyRestClient::InstantlyRestClient rest, *hash<auto> options)
            : InstantlyBaseEventDataProvider(rest, options) {
    }

    string getName() {
        return EVENT_CAMPAIGN_COMPLETED;
    }

    *string getDesc() {
        return "Instantly campaign completed event - triggers when a campaign finishes";
    }

    string getEventType() {
        return EVENT_CAMPAIGN_COMPLETED;
    }

    string getWebhookEventType() {
        return WEBHOOK_CAMPAIGN_COMPLETED;
    }

    AbstractDataProviderType getEventDataType() {
        return eventDataType;
    }

    private hash<auto> getFakeExampleData() {
        return {
            "event_type": WEBHOOK_CAMPAIGN_COMPLETED,
            "timestamp": now_us().format("YYYY-MM-DDTHH:mm:ss.xxxZ"),
            "campaign_id": "00000000-0000-0000-0000-000000000000",
            "campaign_name": "Example Campaign",
            "total_leads": 1000,
        };
    }

    private hash<auto> getEventSpecificFields(hash<auto> payload) {
        return {
            "total_leads": payload.total_leads,
        };
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace InstantlyDataProvider
