# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file GoHighLevelRemoveContactFromCampaignDataProvider.qc GoHighLevel remove contact from campaign action data provider

/*  GoHighLevelRemoveContactFromCampaignDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

%modern

#! Contains all public definitions in the GoHighLevelDataProvider module
public namespace GoHighLevelDataProvider {

#! Request data type for removing a contact from a campaign
public class GoHighLevelRemoveContactFromCampaignRequestDataType inherits HashDataType {
    public {
        #! Field definitions for removing a contact from a campaign
        const Fields = {
            "contact_id": {
                "type": StringType,
                "display_name": "Contact ID",
                "short_desc": "The contact ID",
                "desc": "The contact ID",
                "required": True,
            },
            "campaign_id": {
                "type": StringType,
                "display_name": "Campaign ID",
                "short_desc": "The campaign ID to remove the contact from",
                "desc": "The campaign ID to remove the contact from",
                "required": True,
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! GoHighLevel remove contact from campaign action data provider
public class GoHighLevelRemoveContactFromCampaignDataProvider inherits GoHighLevelDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "remove-from-campaign",
            "desc": "Remove a contact from a campaign in GoHighLevel",
            "type": "GoHighLevelRemoveContactFromCampaignDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;
    }

    #! Creates the object with a REST client
    constructor(GoHighLevelRestClient::GoHighLevelRestClient rest) : GoHighLevelDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "remove-from-campaign";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Remove a contact from a campaign in GoHighLevel";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new GoHighLevelRemoveContactFromCampaignRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return AbstractDataProviderTypeMap."hash";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        doDelete("contacts/" + req.contact_id + "/campaigns/" + req.campaign_id);
        return {};
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace GoHighLevelDataProvider
