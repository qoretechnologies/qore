# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FreshsalesDealsDataProvider.qc Freshsales deals collection data provider

/*  FreshsalesDealsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the FreshsalesDataProvider module
public namespace FreshsalesDataProvider {

#! Freshsales deals collection data provider
public class FreshsalesDealsDataProvider inherits FreshsalesDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "deals",
            "desc": "Freshsales deals data provider for listing and searching deals",
            "type": "FreshsalesDealsDataProvider",
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
            "id": <DataProviderOptionInfo>{
                "display_name": "Deal ID",
                "short_desc": "Get a specific deal by ID",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "The deal ID to retrieve. When provided, returns a single record",
            },
            "view_id": <DataProviderOptionInfo>{
                "display_name": "View ID",
                "short_desc": "Filter by view",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "The ID of the view to list deals from",
                "preselected": True,
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
            "create": Class::forName("FreshsalesCreateDealDataProvider"),
            "update": Class::forName("FreshsalesUpdateDealDataProvider"),
            "delete": Class::forName("FreshsalesDeleteDealDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(RestClientIo::RestClientIo rest) : FreshsalesDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "deals";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Freshsales deals data provider for listing and searching deals";
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
        return new FreshsalesDealDataType().getFields();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        if (search_options.id) {
            hash<auto> result = doGet("deals/" + string(search_options.id));
            hash<auto> deal = result.deal ?? result;
            return new DefaultRecordIterator(new ListHashIterator((deal,)),
                where_cond, search_options);
        }

        if (search_options.view_id) {
            list<hash<auto>> all_records = fetchAllPages(
                "deals/view/" + string(search_options.view_id), "deals",
                NOTHING, search_options.limit);
            return new DefaultRecordIterator(new ListHashIterator(all_records),
                where_cond, search_options);
        }

        # Default: list using first available filter/view
        hash<auto> filters = doGet("deals/filters");
        *list<hash<auto>> filter_list = filters.filters;
        if (filter_list) {
            list<hash<auto>> all_records = fetchAllPages(
                "deals/view/" + string(filter_list[0].id), "deals",
                NOTHING, search_options.limit);
            return new DefaultRecordIterator(new ListHashIterator(all_records),
                where_cond, search_options);
        }

        return new DefaultRecordIterator(new ListHashIterator(()),
            where_cond, search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace FreshsalesDataProvider
