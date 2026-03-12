# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file CloseOpportunitiesDataProvider.qc Close opportunities collection data provider

/*  CloseOpportunitiesDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the CloseDataProvider module
public namespace CloseDataProvider {

#! Close opportunities collection data provider
public class CloseOpportunitiesDataProvider inherits CloseDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "opportunities",
            "desc": "Close opportunities data provider for listing and searching opportunities",
            "type": "CloseOpportunitiesDataProvider",
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
            "opportunity_id": <DataProviderOptionInfo>{
                "display_name": "Opportunity ID",
                "short_desc": "Get a specific opportunity by ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by opportunity ID for single record lookup",
            },
            "lead_id": <DataProviderOptionInfo>{
                "display_name": "Lead ID",
                "short_desc": "Filter by lead ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter opportunities by the lead they belong to",
            },
            "status_type": <DataProviderOptionInfo>{
                "display_name": "Status Type",
                "short_desc": "Filter by status type",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter opportunities by status type: active, won, or lost",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Active",
                        "short_desc": "Active opportunities",
                        "desc": "Filter to show only active opportunities",
                        "value": "active",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Won",
                        "short_desc": "Won opportunities",
                        "desc": "Filter to show only won opportunities",
                        "value": "won",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Lost",
                        "short_desc": "Lost opportunities",
                        "desc": "Filter to show only lost opportunities",
                        "value": "lost",
                    },
                ),
            },
            "user_id": <DataProviderOptionInfo>{
                "display_name": "User ID",
                "short_desc": "Filter by assigned user",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter opportunities by the assigned user ID",
            },
            "status_id": <DataProviderOptionInfo>{
                "display_name": "Status ID",
                "short_desc": "Filter by status ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter opportunities by the specific status ID",
            },
            "query": <DataProviderOptionInfo>{
                "display_name": "Query",
                "short_desc": "Search query",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Text search query to filter opportunities",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum results",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Maximum number of results to return",
            },
        };

        #! Child provider map
        const ChildMap = {
            "create": Class::forName("CloseCreateOpportunityDataProvider"),
            "update": Class::forName("CloseUpdateOpportunityDataProvider"),
            "delete": Class::forName("CloseDeleteOpportunityDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(CloseRestClient::CloseRestClient rest) : CloseDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "opportunities";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Close opportunities data provider for listing and searching opportunities";
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
        return new CloseOpportunityDataType();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # If opportunity_id provided, get single record
        if (search_options.opportunity_id) {
            hash<auto> result = doGet("opportunity/" + search_options.opportunity_id);
            return new DefaultRecordIterator(new ListHashIterator((result,)),
                where_cond, search_options);
        }

        # Build query params
        hash<auto> params;
        if (search_options.lead_id) {
            params.lead_id = search_options.lead_id;
        }
        if (search_options.status_type) {
            params.status_type = search_options.status_type;
        }
        if (search_options.user_id) {
            params.user_id = search_options.user_id;
        }
        if (search_options.status_id) {
            params.status_id = search_options.status_id;
        }
        if (search_options.query) {
            params.query = search_options.query;
        }

        list<hash<auto>> all_records = fetchAllPages("opportunity", params,
            search_options.limit);
        return new DefaultRecordIterator(new ListHashIterator(all_records), where_cond,
            search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace CloseDataProvider
