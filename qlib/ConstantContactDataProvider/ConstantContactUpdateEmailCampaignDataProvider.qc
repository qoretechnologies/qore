# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ConstantContactUpdateEmailCampaignDataProvider.qc Constant Contact update email campaign action data provider

/*  ConstantContactUpdateEmailCampaignDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for updating an email campaign
public class ConstantContactUpdateEmailCampaignRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "campaign_id": {
                "type": StringType,
                "display_name": "Campaign ID",
                "short_desc": "The campaign ID to update",
                "desc": "The unique identifier of the email campaign to update",
                "required": True,
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "The campaign name",
                "desc": "The new name of the email campaign",
            },
            "email_campaign_activities": {
                "type": AutoListOrNothingType,
                "display_name": "Campaign Activities",
                "short_desc": "Campaign activity configurations",
                "desc": "A list of email campaign activity objects to update",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Constant Contact update email campaign action data provider
public class ConstantContactUpdateEmailCampaignDataProvider inherits ConstantContactDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "desc": "Update an existing email campaign in Constant Contact",
            "type": "ConstantContactUpdateEmailCampaignDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Static request type
        static ConstantContactUpdateEmailCampaignRequestDataType RequestType();

        #! Static response type
        static ConstantContactEmailCampaignDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(ConstantContactRestClient::ConstantContactRestClient rest)
            : ConstantContactDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "update";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Update an existing email campaign in Constant Contact";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new ConstantContactUpdateEmailCampaignRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new ConstantContactEmailCampaignDataType();
    }

    #! Makes the request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string campaign_id = remove req.campaign_id;
        return doPatch("emails/" + campaign_id, req);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ConstantContactDataProvider
