# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BigCommerceCustomerAddressesDataProvider.qc BigCommerce customer addresses collection data provider

/*  BigCommerceCustomerAddressesDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the BigCommerceDataProvider module
public namespace BigCommerceDataProvider {

#! BigCommerce customer addresses collection data provider
public class BigCommerceCustomerAddressesDataProvider inherits BigCommerceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "customer-addresses",
            "desc": "BigCommerce customer addresses data provider for listing and searching "
                "customer addresses",
            "type": "BigCommerceCustomerAddressesDataProvider",
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
                "short_desc": "Filter by customer ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by customer ID (comma-separated for multiple); maps to "
                    "BigCommerce `customer_id:in` query parameter",
            },
            "company": <DataProviderOptionInfo>{
                "display_name": "Company",
                "short_desc": "Filter by company name",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by company name (comma-separated for multiple); maps to "
                    "BigCommerce `company:in` query parameter",
            },
            "name": <DataProviderOptionInfo>{
                "display_name": "Name",
                "short_desc": "Filter by recipient name",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by recipient name (comma-separated for multiple); maps to "
                    "BigCommerce `name:in` query parameter",
            },
            "address_id": <DataProviderOptionInfo>{
                "display_name": "Address ID",
                "short_desc": "Filter by address ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by address ID (comma-separated for multiple); maps to "
                    "BigCommerce `id:in` query parameter",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum results to return",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Maximum number of results to return",
            },
            "page": <DataProviderOptionInfo>{
                "display_name": "Page",
                "short_desc": "Page number for pagination",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Page number for paginated results",
            },
        };

        #! Child provider map
        const ChildMap = {
            "create": Class::forName("BigCommerceCreateCustomerAddressDataProvider"),
            "delete": Class::forName("BigCommerceDeleteCustomerAddressDataProvider"),
            "update": Class::forName("BigCommerceUpdateCustomerAddressDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(BigCommerceRestClient::BigCommerceRestClient rest)
            : BigCommerceDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "customer-addresses";
    }

    #! Returns the provider description
    *string getDesc() {
        return "BigCommerce customer addresses data provider for listing and searching "
            "customer addresses";
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
        return new BigCommerceCustomerAddressDataType();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # Build query params mapping user-friendly names to BigCommerce API params
        hash<auto> params;

        if (search_options.customer_id) {
            params."customer_id:in" = search_options.customer_id;
        }
        if (search_options.company) {
            params."company:in" = search_options.company;
        }
        if (search_options.name) {
            params."name:in" = search_options.name;
        }
        if (search_options.address_id) {
            params."id:in" = search_options.address_id;
        }
        if (search_options.page) {
            params.page = search_options.page;
        }

        list<hash<auto>> all_records = fetchAllPagesV3("customers/addresses", params,
            search_options.limit);
        return new DefaultRecordIterator(new ListHashIterator(all_records), where_cond,
            search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BigCommerceDataProvider
