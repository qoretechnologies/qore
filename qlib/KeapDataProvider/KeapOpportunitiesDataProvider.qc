# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KeapOpportunitiesDataProvider.qc Keap opportunities collection data provider

/*  KeapOpportunitiesDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the KeapDataProvider module
public namespace KeapDataProvider {

#! Keap opportunities collection data provider
public class KeapOpportunitiesDataProvider inherits KeapDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "opportunities",
            "desc": "Keap opportunities data provider for listing and searching opportunities",
            "type": "KeapOpportunitiesDataProvider",
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
            "contact_id": <DataProviderOptionInfo>{
                "display_name": "Contact ID",
                "short_desc": "Filter by contact ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter opportunities by associated contact ID",
            },
            "stage_id": <DataProviderOptionInfo>{
                "display_name": "Stage ID",
                "short_desc": "Filter by stage ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter opportunities by pipeline stage ID",
            },
            "order_by": <DataProviderOptionInfo>{
                "display_name": "Order By",
                "short_desc": "Field to order results by",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Field to order results by",
            },
            "order_direction": <DataProviderOptionInfo>{
                "display_name": "Order Direction",
                "short_desc": "Sort direction",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Sort direction for results",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "ASCENDING",
                        "desc": "Sort in ascending order",
                    },
                    <AllowedValueInfo>{
                        "value": "DESCENDING",
                        "desc": "Sort in descending order",
                    },
                ),
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
            "create": Class::forName("KeapCreateOpportunityDataProvider"),
            "update": Class::forName("KeapUpdateOpportunityDataProvider"),
            "delete": Class::forName("KeapDeleteOpportunityDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(KeapRestClient::KeapRestClient rest) : KeapDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "opportunities";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Keap opportunities data provider for listing and searching opportunities";
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
        return new KeapOpportunityDataType();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # If opportunity_id provided, get single record
        if (search_options.opportunity_id) {
            hash<auto> result = doGet("opportunities/" + search_options.opportunity_id);
            return new DefaultRecordIterator(new ListHashIterator((result,)),
                where_cond, search_options);
        }

        # Build query params
        hash<auto> params;
        if (search_options.contact_id) {
            params.contact_id = search_options.contact_id;
        }
        if (search_options.stage_id) {
            params.stage_id = search_options.stage_id;
        }
        if (search_options.order_by) {
            params.order_by = search_options.order_by;
        }
        if (search_options.order_direction) {
            params.order_direction = search_options.order_direction;
        }

        list<hash<auto>> all_records = fetchAllPages("opportunities", "opportunities", params,
            search_options.limit);
        return new DefaultRecordIterator(new ListHashIterator(all_records), where_cond,
            search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace KeapDataProvider
