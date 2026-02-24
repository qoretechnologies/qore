# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BigCommerceCustomersDataProvider.qc BigCommerce customers collection data provider

/*  BigCommerceCustomersDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! BigCommerce customers collection data provider
public class BigCommerceCustomersDataProvider inherits BigCommerceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "customers",
            "desc": "BigCommerce customers data provider for listing and searching customers",
            "type": "BigCommerceCustomersDataProvider",
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
                "desc": "Filter by customer ID (single ID or comma-separated list); maps to "
                    "BigCommerce `id:in` query parameter",
            },
            "company": <DataProviderOptionInfo>{
                "display_name": "Company",
                "short_desc": "Filter by company name",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by company name (comma-separated for multiple); maps to "
                    "BigCommerce `company:in` query parameter",
            },
            "customer_group_id": <DataProviderOptionInfo>{
                "display_name": "Customer Group ID",
                "short_desc": "Filter by customer group",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by customer group ID (comma-separated for multiple); maps to "
                    "BigCommerce `customer_group_id:in` query parameter",
            },
            "email": <DataProviderOptionInfo>{
                "display_name": "Email",
                "short_desc": "Filter by email address",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by email address (comma-separated for multiple); maps to "
                    "BigCommerce `email:in` query parameter",
            },
            "name": <DataProviderOptionInfo>{
                "display_name": "Name",
                "short_desc": "Filter by exact name",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by exact customer name (comma-separated for multiple); maps to "
                    "BigCommerce `name:in` query parameter",
            },
            "name_like": <DataProviderOptionInfo>{
                "display_name": "Name (Like)",
                "short_desc": "Filter by partial name match",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by partial name match; maps to BigCommerce `name:like` query "
                    "parameter",
            },
            "date_created_min": <DataProviderOptionInfo>{
                "display_name": "Date Created (Min)",
                "short_desc": "Filter by minimum creation date",
                "type": AbstractDataProviderType::get(SoftDateOrNothingType),
                "desc": "Filter customers created on or after this date; maps to BigCommerce "
                    "`date_created:min` query parameter",
            },
            "date_modified_min": <DataProviderOptionInfo>{
                "display_name": "Date Modified (Min)",
                "short_desc": "Filter by minimum modification date",
                "type": AbstractDataProviderType::get(SoftDateOrNothingType),
                "desc": "Filter customers modified on or after this date; maps to BigCommerce "
                    "`date_modified:min` query parameter",
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
            "sort": <DataProviderOptionInfo>{
                "display_name": "Sort",
                "short_desc": "Sort order for results",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Sort order for results",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "date_created:asc",
                        "desc": "Sort by creation date ascending"},
                    <AllowedValueInfo>{"value": "date_created:desc",
                        "desc": "Sort by creation date descending"},
                    <AllowedValueInfo>{"value": "last_name:asc",
                        "desc": "Sort by last name ascending"},
                    <AllowedValueInfo>{"value": "last_name:desc",
                        "desc": "Sort by last name descending"},
                    <AllowedValueInfo>{"value": "date_modified:asc",
                        "desc": "Sort by modification date ascending"},
                    <AllowedValueInfo>{"value": "date_modified:desc",
                        "desc": "Sort by modification date descending"},
                ),
            },
        };

        #! Child provider map
        const ChildMap = {
            "create": Class::forName("BigCommerceCreateCustomerDataProvider"),
            "delete": Class::forName("BigCommerceDeleteCustomerDataProvider"),
            "get": Class::forName("BigCommerceGetCustomerDataProvider"),
            "update": Class::forName("BigCommerceUpdateCustomerDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(BigCommerceRestClient::BigCommerceRestClient rest)
            : BigCommerceDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "customers";
    }

    #! Returns the provider description
    *string getDesc() {
        return "BigCommerce customers data provider for listing and searching customers";
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
        return new BigCommerceCustomerDataType();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # Build query params mapping user-friendly names to BigCommerce API params
        hash<auto> params;

        if (search_options.customer_id) {
            params."id:in" = search_options.customer_id;
        }
        if (search_options.company) {
            params."company:in" = search_options.company;
        }
        if (search_options.customer_group_id) {
            params."customer_group_id:in" = search_options.customer_group_id;
        }
        if (search_options.email) {
            params."email:in" = search_options.email;
        }
        if (search_options.name) {
            params."name:in" = search_options.name;
        }
        if (search_options.name_like) {
            params."name:like" = search_options.name_like;
        }
        if (search_options.date_created_min) {
            params."date_created:min" = formatDateTimeV3(search_options.date_created_min);
        }
        if (search_options.date_modified_min) {
            params."date_modified:min" = formatDateTimeV3(search_options.date_modified_min);
        }
        if (search_options.sort) {
            params.sort = search_options.sort;
        }
        if (search_options.page) {
            params.page = search_options.page;
        }

        # If customer_id is a single ID (no comma), do a targeted lookup
        if (search_options.customer_id && search_options.customer_id !~ /,/) {
            hash<auto> response = doGetV3("customers", params);
            list<hash<auto>> data = ();
            if (response.data.typeCode() == NT_LIST && response.data.size() > 0) {
                data = response.data;
            }
            return new DefaultRecordIterator(new ListHashIterator(data), where_cond,
                search_options);
        }

        list<hash<auto>> all_records = fetchAllPagesV3("customers", params,
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
