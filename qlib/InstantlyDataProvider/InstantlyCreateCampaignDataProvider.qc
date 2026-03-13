# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore InstantlyDataProvider module definition

/*  InstantlyCreateCampaignDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for creating a campaign
public class InstantlyCreateCampaignRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "Campaign name",
                "desc": "The name of the campaign to create",
                "required": True,
                "preselected": True,
            },
            "email_list": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Email List",
                "short_desc": "Account emails for sending",
                "desc": "List of account email addresses to use for sending in this campaign",
            },
            "daily_limit": {
                "type": IntOrNothingType,
                "display_name": "Daily Limit",
                "short_desc": "Daily sending cap",
                "desc": "The maximum number of emails that can be sent per day for this campaign",
            },
            "timezone": {
                "type": StringOrNothingType,
                "display_name": "Timezone",
                "short_desc": "Campaign timezone for scheduling",
                "desc": "The timezone for campaign scheduling (e.g., `America/New_York`)",
                "example_value": "America/New_York",
                "preselected": True,
            },
            "stop_on_reply": {
                "type": SoftBoolOrNothingType,
                "display_name": "Stop on Reply",
                "short_desc": "Stop sending when lead replies",
                "desc": "If `True`, the campaign will stop sending follow-up emails to a lead "
                    "after they reply",
            },
            "link_tracking": {
                "type": SoftBoolOrNothingType,
                "display_name": "Link Tracking",
                "short_desc": "Enable link click tracking",
                "desc": "If `True`, links in campaign emails will be tracked for clicks",
            },
            "open_tracking": {
                "type": SoftBoolOrNothingType,
                "display_name": "Open Tracking",
                "short_desc": "Enable email open tracking",
                "desc": "If `True`, email opens will be tracked via an invisible pixel",
            },
            "sequences": {
                "type": new ListDataType("CampaignSequenceList",
                    new InstantlyCampaignSequenceDataType(AutoHashOrNothingType), False),
                "display_name": "Sequences",
                "short_desc": "Email sequences",
                "desc": "A list of email sequences for the campaign; only the first element is "
                    "used, so provide a single sequence with all steps inside it",
                "preselected": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for creating a new campaign
public class InstantlyCreateCampaignDataProvider inherits InstantlyDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-campaign",
            "desc": "Create a new campaign in Instantly",
            "type": "InstantlyCreateCampaignDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static InstantlyCreateCampaignRequestDataType RequestType();

        #! Response type
        static InstantlyCampaignDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(InstantlyRestClient::InstantlyRestClient rest)
            : InstantlyDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "create-campaign";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Create a new campaign in Instantly";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("/campaigns", req);
    }

    #! Returns the request type
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the response type
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace InstantlyDataProvider
