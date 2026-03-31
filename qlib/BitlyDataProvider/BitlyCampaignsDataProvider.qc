# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BitlyCampaignsDataProvider.qc Bitly campaigns collection data provider

/*  BitlyCampaignsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Bitly campaigns collection data provider
public class BitlyCampaignsDataProvider inherits BitlyDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "campaigns",
            "desc": "Bitly campaigns data provider for listing and managing campaigns",
            "type": "BitlyCampaignsDataProvider",
            "has_record": True,
            "supports_read": True,
            "supports_native_search": True,
            "supports_children": True,
            "children_can_support_apis": True,
            "constructor_options": ConstructorOptions,
            "search_options": SearchOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Search options
        const SearchOptions = {
            "campaign_guid": <DataProviderOptionInfo>{
                "display_name": "Campaign GUID",
                "short_desc": "Get a specific campaign by GUID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "The campaign GUID to retrieve. When provided, returns a single campaign",
                "ref_data": "campaigns",
            },
            "group_guid": <DataProviderOptionInfo>{
                "display_name": "Group GUID",
                "short_desc": "Filter by group",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter campaigns by group GUID",
                "ref_data": "groups",
            },
        };

        #! Child provider map
        const ChildMap = {
            "create": Class::forName("BitlyCreateCampaignDataProvider"),
            "update": Class::forName("BitlyUpdateCampaignDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(RestClientIo::RestClientIo rest) : BitlyDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "campaigns";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Bitly campaigns data provider for listing and managing campaigns";
    }

    #! Returns child provider names
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns a child provider by name
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) {
            return cls.newObject(rest);
        }
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new BitlyCampaignDataType().getFields();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # If campaign_guid provided, get single record
        if (search_options.campaign_guid) {
            hash<auto> result = doGet("campaigns/" + search_options.campaign_guid);
            return new DefaultRecordIterator(new ListHashIterator((result,)),
                where_cond, search_options);
        }

        hash<auto> params;
        if (search_options.group_guid) {
            params.group_guid = search_options.group_guid;
        }

        hash<auto> response = doGet("campaigns", params);
        list<hash<auto>> records = response.campaigns ?? ();
        return new DefaultRecordIterator(new ListHashIterator(records), where_cond,
            search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BitlyDataProvider
