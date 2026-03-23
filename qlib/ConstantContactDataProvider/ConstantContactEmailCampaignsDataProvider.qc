# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ConstantContactEmailCampaignsDataProvider.qc Constant Contact email campaigns collection data provider

/*  ConstantContactEmailCampaignsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Constant Contact email campaigns collection data provider
public class ConstantContactEmailCampaignsDataProvider inherits ConstantContactDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "email-campaigns",
            "desc": "Constant Contact email campaigns data provider for listing and searching "
                "email campaigns",
            "type": "ConstantContactEmailCampaignsDataProvider",
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
            "campaign_id": <DataProviderOptionInfo>{
                "display_name": "Campaign ID",
                "short_desc": "Get a specific campaign by ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by campaign ID for single record lookup",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum results",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Maximum number of results to return",
            },
            "status": <DataProviderOptionInfo>{
                "display_name": "Status",
                "short_desc": "Filter by campaign status",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter campaigns by status",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "Draft",
                        "display_name": "Draft",
                        "desc": "Draft campaigns",
                    },
                    <AllowedValueInfo>{
                        "value": "Scheduled",
                        "display_name": "Scheduled",
                        "desc": "Scheduled campaigns",
                    },
                    <AllowedValueInfo>{
                        "value": "Executing",
                        "display_name": "Executing",
                        "desc": "Currently sending campaigns",
                    },
                    <AllowedValueInfo>{
                        "value": "Done",
                        "display_name": "Done",
                        "desc": "Completed campaigns",
                    },
                    <AllowedValueInfo>{
                        "value": "Cancelled",
                        "display_name": "Cancelled",
                        "desc": "Cancelled campaigns",
                    },
                    <AllowedValueInfo>{
                        "value": "Deleted",
                        "display_name": "Deleted",
                        "desc": "Deleted campaigns",
                    },
                ),
            },
        };

        #! Child provider map
        const ChildMap = {
            "create": Class::forName("ConstantContactCreateEmailCampaignDataProvider"),
            "update": Class::forName("ConstantContactUpdateEmailCampaignDataProvider"),
            "delete": Class::forName("ConstantContactDeleteEmailCampaignDataProvider"),
            "schedule": Class::forName("ConstantContactScheduleEmailCampaignDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(ConstantContactRestClient::ConstantContactRestClient rest)
            : ConstantContactDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "email-campaigns";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Constant Contact email campaigns data provider for listing and searching "
            "email campaigns";
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
    private *AbstractDataProviderType getRecordTypeImpl() {
        return new ConstantContactEmailCampaignDataType();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # If campaign_id provided, get single record
        if (search_options.campaign_id) {
            hash<auto> result = doGet("emails/" + search_options.campaign_id);
            return new DefaultRecordIterator(new ListHashIterator((result,)),
                where_cond, search_options);
        }

        # Build query params
        hash<auto> params;
        if (search_options.status) {
            params.status = search_options.status;
        }

        list<hash<auto>> all_records = fetchAllPages("emails", "campaigns", params,
            search_options.limit);
        return new DefaultRecordIterator(new ListHashIterator(all_records), where_cond,
            search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ConstantContactDataProvider
