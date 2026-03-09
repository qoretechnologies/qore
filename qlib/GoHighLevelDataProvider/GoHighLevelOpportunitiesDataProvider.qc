# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file GoHighLevelOpportunitiesDataProvider.qc GoHighLevel opportunities collection data provider

/*  GoHighLevelOpportunitiesDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! GoHighLevel opportunities collection data provider
public class GoHighLevelOpportunitiesDataProvider inherits GoHighLevelDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "opportunities",
            "desc": "GoHighLevel opportunities data provider for listing and searching opportunities",
            "type": "GoHighLevelOpportunitiesDataProvider",
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
                "desc": "Get a specific opportunity by ID",
            },
            "q": <DataProviderOptionInfo>{
                "display_name": "Search Query",
                "short_desc": "Search query for opportunities",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Search query for opportunities",
            },
            "pipeline_id": <DataProviderOptionInfo>{
                "display_name": "Pipeline ID",
                "short_desc": "Filter by pipeline ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by pipeline ID",
            },
            "pipeline_stage_id": <DataProviderOptionInfo>{
                "display_name": "Pipeline Stage ID",
                "short_desc": "Filter by pipeline stage ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by pipeline stage ID",
            },
            "contact_id": <DataProviderOptionInfo>{
                "display_name": "Contact ID",
                "short_desc": "Filter by contact ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by contact ID",
            },
            "status": <DataProviderOptionInfo>{
                "display_name": "Status",
                "short_desc": "Filter by opportunity status",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter opportunities by status",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "open", "display_name": "Open"},
                    <AllowedValueInfo>{"value": "won", "display_name": "Won"},
                    <AllowedValueInfo>{"value": "lost", "display_name": "Lost"},
                    <AllowedValueInfo>{"value": "abandoned", "display_name": "Abandoned"},
                    <AllowedValueInfo>{"value": "all", "display_name": "All"},
                ),
            },
            "assigned_to": <DataProviderOptionInfo>{
                "display_name": "Assigned To",
                "short_desc": "Filter by assigned user ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by assigned user ID",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum number of results to return",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Maximum number of results to return",
            },
        };

        #! Child provider map
        const ChildMap = {
            "create": Class::forName("GoHighLevelCreateOpportunityDataProvider"),
            "update": Class::forName("GoHighLevelUpdateOpportunityDataProvider"),
            "delete": Class::forName("GoHighLevelDeleteOpportunityDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(GoHighLevelRestClient::GoHighLevelRestClient rest) : GoHighLevelDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "opportunities";
    }

    #! Returns the provider description
    *string getDesc() {
        return "GoHighLevel opportunities data provider for listing and searching opportunities";
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
        return new GoHighLevelOpportunityDataType();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # If opportunity_id provided, get single record
        if (search_options.opportunity_id) {
            hash<auto> result = doGet("opportunities/" + search_options.opportunity_id);
            return new DefaultRecordIterator(new ListHashIterator((result.opportunity,)),
                where_cond, search_options);
        }

        # Build query params for GET search
        hash<auto> params = {
            "page": 1,
        };
        if (search_options.q) {
            params.q = search_options.q;
        }
        if (search_options.pipeline_id) {
            params.pipeline_id = search_options.pipeline_id;
        }
        if (search_options.pipeline_stage_id) {
            params.pipeline_stage_id = search_options.pipeline_stage_id;
        }
        if (search_options.contact_id) {
            params.contact_id = search_options.contact_id;
        }
        if (search_options.status) {
            params.status = search_options.status;
        }
        if (search_options.assigned_to) {
            params.assigned_to = search_options.assigned_to;
        }
        if (search_options.limit) {
            params.limit = search_options.limit;
        }

        hash<auto> response = doGet("opportunities/search", params);
        return new DefaultRecordIterator(new ListHashIterator(response.opportunities ?? ()),
            where_cond, search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace GoHighLevelDataProvider
