# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file UnbounceLeadsDataProvider.qc Unbounce leads collection data provider

/*  UnbounceLeadsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the UnbounceDataProvider module
public namespace UnbounceDataProvider {

#! Unbounce leads collection data provider
public class UnbounceLeadsDataProvider inherits UnbounceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "leads",
            "desc": "Unbounce leads data provider for listing and retrieving leads",
            "type": "UnbounceLeadsDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "supports_children": True,
            "children_can_support_apis": True,
            "has_record": True,
            "constructor_options": ConstructorOptions,
            "search_options": SearchOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Search options
        const SearchOptions = {
            "page_id": <DataProviderOptionInfo>{
                "display_name": "Page ID",
                "short_desc": "Page to list leads for",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "The page ID to list leads for. Required for listing",
                "ref_data": "pages",
            },
            "lead_id": <DataProviderOptionInfo>{
                "display_name": "Lead ID",
                "short_desc": "Get a specific lead by ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by lead ID for single record lookup",
            },
            "sort_order": <DataProviderOptionInfo>{
                "display_name": "Sort Order",
                "short_desc": "Sort direction",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Sort direction for results",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "asc", "display_name": "Ascending",
                        "desc": "Sort in ascending order"},
                    <AllowedValueInfo>{"value": "desc", "display_name": "Descending",
                        "desc": "Sort in descending order"},
                ),
            },
            "from": <DataProviderOptionInfo>{
                "display_name": "From",
                "short_desc": "Start date filter",
                "type": AbstractDataProviderType::get(SoftDateOrNothingType),
                "desc": "Filter leads created on or after this date",
            },
            "to": <DataProviderOptionInfo>{
                "display_name": "To",
                "short_desc": "End date filter",
                "type": AbstractDataProviderType::get(SoftDateOrNothingType),
                "desc": "Filter leads created on or before this date",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum results",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Maximum number of results to return",
            },
            "offset": <DataProviderOptionInfo>{
                "display_name": "Offset",
                "short_desc": "Result offset",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Number of results to skip for pagination",
            },
        };

        #! Child provider map
        const ChildMap = {
            "create": Class::forName("UnbounceCreateLeadDataProvider"),
            "delete": Class::forName("UnbounceDeleteLeadDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(*RestClientIo::RestClientIo rest) : UnbounceDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "leads";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Unbounce leads data provider for listing and retrieving leads";
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
        return new UnbounceLeadDataType().getFields();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator<hash<auto>> searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # If lead_id provided, get single record
        if (search_options.lead_id) {
            hash<auto> result = doGet("leads/" + search_options.lead_id);
            return new DefaultRecordIterator<hash<auto>>(new ListHashIterator((result,)),
                where_cond, search_options);
        }

        if (!search_options.page_id) {
            throw "UNBOUNCE-ERROR", "page_id is required for listing leads";
        }

        # Build query params
        hash<auto> params;
        if (search_options.sort_order) {
            params.sort_order = search_options.sort_order;
        }
        if (search_options."from") {
            params."from" = formatDateTime(search_options."from");
        }
        if (search_options."to") {
            params."to" = formatDateTime(search_options."to");
        }

        list<hash<auto>> all_records = fetchAllPages(
            "pages/" + search_options.page_id + "/leads",
            "leads", params, search_options.limit);
        return new DefaultRecordIterator<hash<auto>>(new ListHashIterator(all_records), where_cond,
            search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace UnbounceDataProvider
