# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SquarespaceInventoryDataProvider.qc Squarespace inventory collection data provider

/*  SquarespaceInventoryDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SquarespaceDataProvider module
public namespace SquarespaceDataProvider {

#! Squarespace inventory collection data provider
public class SquarespaceInventoryDataProvider inherits SquarespaceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "inventory",
            "desc": "Squarespace inventory data provider for listing and searching inventory",
            "type": "SquarespaceInventoryDataProvider",
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
            "variant_id": <DataProviderOptionInfo>{
                "display_name": "Variant ID",
                "short_desc": "Get inventory for specific variant ID(s)",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Get inventory for specific variant ID(s)",
            },
            "modified_after": <DataProviderOptionInfo>{
                "display_name": "Modified After",
                "short_desc": "Filter by modification date",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter inventory modified after this ISO 8601 datetime",
            },
            "modified_before": <DataProviderOptionInfo>{
                "display_name": "Modified Before",
                "short_desc": "Filter by modification date",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter inventory modified before this ISO 8601 datetime",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum number of results",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Maximum number of results to return",
            },
        };

        #! Child provider map
        const ChildMap = {
            "update-stock": Class::forName("SquarespaceUpdateStockDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(SquarespaceRestClient::SquarespaceRestClient rest) : SquarespaceDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "inventory";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Squarespace inventory data provider for listing and searching inventory";
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
        return new SquarespaceInventoryItemDataType();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # If variant_id provided, get inventory for specific variant
        if (search_options.variant_id) {
            hash<auto> result = doGet("1.0/commerce/inventory/" + search_options.variant_id);
            # Squarespace wraps even single-variant responses in {"inventory": [...]}
            list<hash<auto>> items = result.inventory ?? (result,);
            return new DefaultRecordIterator(new ListHashIterator(items),
                where_cond, search_options);
        }

        # Build query params
        hash<auto> params;
        if (search_options.modified_after) {
            params.modifiedAfter = search_options.modified_after;
        }
        if (search_options.modified_before) {
            params.modifiedBefore = search_options.modified_before;
        }

        list<hash<auto>> all_records = fetchAllPages("1.0/commerce/inventory", "inventory", params,
            search_options.limit);
        return new DefaultRecordIterator(new ListHashIterator(all_records), where_cond,
            search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SquarespaceDataProvider
