# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BitlyCreateCampaignDataProvider.qc Bitly create campaign action data provider

/*  BitlyCreateCampaignDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for creating a campaign
public class BitlyCreateCampaignRequestDataType inherits HashDataType {
    public {
        #! Field definitions for creating a campaign
        const Fields = {
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "Campaign name",
                "desc": "The name of the campaign to create",
                "required": True,
                "example_value": "Q1 Marketing Campaign",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "Campaign description",
                "desc": "A description of the campaign",
                "preselected": True,
            },
            "group_guid": {
                "type": StringType,
                "display_name": "Group GUID",
                "short_desc": "Group to create the campaign in",
                "desc": "The GUID of the group to create the campaign in",
                "required": True,
                "ref_data": "groups",
            },
            "channel_guids": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Channel GUIDs",
                "short_desc": "Channels to associate with the campaign",
                "desc": "A list of channel GUIDs to associate with the campaign",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Bitly create campaign action data provider
public class BitlyCreateCampaignDataProvider inherits BitlyDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new campaign",
            "type": "BitlyCreateCampaignDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(RestClientIo::RestClientIo rest) : BitlyDataProviderBase(rest) {
    }

    string getName() {
        return "create";
    }

    *string getDesc() {
        return "Create a new campaign";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new BitlyCreateCampaignRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new BitlyCampaignDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("campaigns", req);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BitlyDataProvider
