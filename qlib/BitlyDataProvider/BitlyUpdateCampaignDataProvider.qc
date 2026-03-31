# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BitlyUpdateCampaignDataProvider.qc Bitly update campaign action data provider

/*  BitlyUpdateCampaignDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the BitlyDataProvider module
public namespace BitlyDataProvider {

#! Request type for updating a campaign
public class BitlyUpdateCampaignRequestDataType inherits HashDataType {
    public {
        #! Field definitions for updating a campaign
        const Fields = {
            "campaign_guid": {
                "type": StringType,
                "display_name": "Campaign GUID",
                "short_desc": "The campaign to update",
                "desc": "The GUID of the campaign to update",
                "required": True,
                "ref_data": "campaigns",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "New campaign name",
                "desc": "The new name for the campaign",
                "preselected": True,
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "New campaign description",
                "desc": "The new description for the campaign",
                "preselected": True,
            },
            "channel_guids": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Channel GUIDs",
                "short_desc": "Channels to associate with the campaign",
                "desc": "A new list of channel GUIDs to associate with the campaign",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Bitly update campaign action data provider
public class BitlyUpdateCampaignDataProvider inherits BitlyDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "desc": "Update an existing campaign",
            "type": "BitlyUpdateCampaignDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(RestClientIo::RestClientIo rest) : BitlyDataProviderBase(rest) {
    }

    string getName() {
        return "update";
    }

    *string getDesc() {
        return "Update an existing campaign";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new BitlyUpdateCampaignRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new BitlyCampaignDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string campaign_guid = remove req.campaign_guid;
        return doPatch("campaigns/" + campaign_guid, req);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BitlyDataProvider
