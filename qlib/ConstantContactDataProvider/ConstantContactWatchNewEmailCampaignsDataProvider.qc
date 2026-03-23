# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ConstantContactWatchNewEmailCampaignsDataProvider.qc Watch for new/updated email campaigns

/*  ConstantContactWatchNewEmailCampaignsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ConstantContactDataProvider module
public namespace ConstantContactDataProvider {

#! Watch for new or updated email campaigns in Constant Contact via polling
public class ConstantContactWatchNewEmailCampaignsDataProvider
        inherits ConstantContactWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_NEW_EMAIL_CAMPAIGN,
            "desc": "Watch for new or updated email campaigns in Constant Contact",
            "type": "ConstantContactWatchNewEmailCampaignsDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    #! Creates the object with a REST client
    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : ConstantContactWatchDataProviderBase(rest, options) {
    }

    #! Returns the provider name
    string getName() { return EVENT_NEW_EMAIL_CAMPAIGN; }

    #! Returns the provider description
    *string getDesc() {
        return "Watch for new or updated email campaigns in Constant Contact via polling";
    }

    #! Fetches email campaigns updated since the given date
    private list<hash<auto>> fetchRecords(date since) {
        return fetchAllPages("emails", "campaigns", {"limit": DefaultPageSize});
    }

    #! Returns the record ID
    private string getRecordId(hash<auto> record) {
        return string(record.campaign_id);
    }

    #! Returns the event name
    private string getEventName() { return EVENT_NEW_EMAIL_CAMPAIGN; }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Returns the data type for campaign records
    private AbstractDataProviderType getRecordDataType() {
        return new ConstantContactEmailCampaignDataType();
    }

    #! Returns fake example data for campaigns
    private hash<auto> getFakeExampleData() {
        return {
            "campaign_id": "91a60fe2-a6a3-4a36-b9db-e5a3c1a1234b",
            "name": "Monthly Newsletter - March 2026",
            "current_status": "Draft",
            "type": "CUSTOM_CODE_EMAIL",
            "created_at": "2026-01-30T10:30:00Z",
            "updated_at": "2026-01-30T10:30:00Z",
        };
    }
}

} # namespace ConstantContactDataProvider
