# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SquareCustomersDataProvider.qc Square customers collection data provider

/*  SquareCustomersDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SquareDataProvider module
public namespace SquareDataProvider {

#! Square customers collection data provider
public class SquareCustomersDataProvider inherits SquareDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "customers",
            "desc": "Square customers data provider for listing and searching customers",
            "type": "SquareCustomersDataProvider",
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
            "customer_id": <DataProviderOptionInfo>{
                "display_name": "Customer ID",
                "short_desc": "Get a specific customer by ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by customer ID for single record lookup",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum results",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Maximum number of results to return",
            },
            "sort_field": <DataProviderOptionInfo>{
                "display_name": "Sort Field",
                "short_desc": "Field to sort by",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Field to sort by (DEFAULT, CREATED_AT)",
            },
            "sort_order": <DataProviderOptionInfo>{
                "display_name": "Sort Order",
                "short_desc": "Sort direction",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Sort direction (ASC, DESC)",
            },
        };

        #! Child provider map
        const ChildMap = {
            "create": Class::forName("SquareCreateCustomerDataProvider"),
            "update": Class::forName("SquareUpdateCustomerDataProvider"),
            "add-to-group": Class::forName("SquareAddCustomerToGroupDataProvider"),
            "remove-from-group": Class::forName("SquareRemoveCustomerFromGroupDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(SquareRestClient::SquareRestClient rest) : SquareDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "customers";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Square customers data provider for listing and searching customers";
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
        return new SquareCustomerDataType();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # If customer_id provided, get single record
        if (search_options.customer_id) {
            hash<auto> result = doGet("customers/" + search_options.customer_id);
            return new DefaultRecordIterator(new ListHashIterator((result.customer,)),
                where_cond, search_options);
        }

        # Build query params
        hash<auto> params;
        if (search_options.limit) {
            params.limit = search_options.limit;
        }
        if (search_options.sort_field) {
            params.sort_field = search_options.sort_field;
        }
        if (search_options.sort_order) {
            params.sort_order = search_options.sort_order;
        }

        list<hash<auto>> all_records = fetchAllPages("customers", "customers", params,
            search_options.limit);
        return new DefaultRecordIterator(new ListHashIterator(all_records), where_cond,
            search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SquareDataProvider
