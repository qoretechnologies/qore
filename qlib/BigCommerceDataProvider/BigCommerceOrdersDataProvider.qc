# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BigCommerceOrdersDataProvider.qc BigCommerce orders collection data provider

/*  BigCommerceOrdersDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! BigCommerce orders collection data provider (V2 API)
public class BigCommerceOrdersDataProvider inherits BigCommerceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "orders",
            "desc": "BigCommerce orders data provider for listing and searching orders",
            "type": "BigCommerceOrdersDataProvider",
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
            "order_id": <DataProviderOptionInfo>{
                "display_name": "Order ID",
                "short_desc": "Filter by order ID",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Filter by a specific order ID",
            },
            "customer_id": <DataProviderOptionInfo>{
                "display_name": "Customer ID",
                "short_desc": "Filter by customer ID",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Filter orders by customer ID",
            },
            "status_id": <DataProviderOptionInfo>{
                "display_name": "Status ID",
                "short_desc": "Filter by status ID",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Filter orders by status ID",
                "allowed_values": (
                    <AllowedValueInfo>{"value": 0, "desc": "Incomplete"},
                    <AllowedValueInfo>{"value": 1, "desc": "Pending"},
                    <AllowedValueInfo>{"value": 2, "desc": "Shipped"},
                    <AllowedValueInfo>{"value": 3, "desc": "Partially Shipped"},
                    <AllowedValueInfo>{"value": 4, "desc": "Refunded"},
                    <AllowedValueInfo>{"value": 5, "desc": "Cancelled"},
                    <AllowedValueInfo>{"value": 6, "desc": "Declined"},
                    <AllowedValueInfo>{"value": 7, "desc": "Awaiting Payment"},
                    <AllowedValueInfo>{"value": 8, "desc": "Awaiting Pickup"},
                    <AllowedValueInfo>{"value": 9, "desc": "Awaiting Shipment"},
                    <AllowedValueInfo>{"value": 10, "desc": "Completed"},
                    <AllowedValueInfo>{"value": 11, "desc": "Awaiting Fulfillment"},
                    <AllowedValueInfo>{"value": 12, "desc": "Manual Verification Required"},
                    <AllowedValueInfo>{"value": 13, "desc": "Disputed"},
                    <AllowedValueInfo>{"value": 14, "desc": "Partially Refunded"},
                ),
            },
            "min_date_created": <DataProviderOptionInfo>{
                "display_name": "Min Date Created",
                "short_desc": "Minimum creation date",
                "type": AbstractDataProviderType::get(SoftDateOrNothingType),
                "desc": "Filter orders created on or after this date (RFC 2822 format)",
            },
            "max_date_created": <DataProviderOptionInfo>{
                "display_name": "Max Date Created",
                "short_desc": "Maximum creation date",
                "type": AbstractDataProviderType::get(SoftDateOrNothingType),
                "desc": "Filter orders created on or before this date (RFC 2822 format)",
            },
            "min_date_modified": <DataProviderOptionInfo>{
                "display_name": "Min Date Modified",
                "short_desc": "Minimum modification date",
                "type": AbstractDataProviderType::get(SoftDateOrNothingType),
                "desc": "Filter orders modified on or after this date (RFC 2822 format)",
            },
            "min_total": <DataProviderOptionInfo>{
                "display_name": "Min Total",
                "short_desc": "Minimum order total",
                "type": AbstractDataProviderType::get(SoftFloatOrNothingType),
                "desc": "Filter orders with total greater than or equal to this value",
            },
            "max_total": <DataProviderOptionInfo>{
                "display_name": "Max Total",
                "short_desc": "Maximum order total",
                "type": AbstractDataProviderType::get(SoftFloatOrNothingType),
                "desc": "Filter orders with total less than or equal to this value",
            },
            "is_deleted": <DataProviderOptionInfo>{
                "display_name": "Is Deleted",
                "short_desc": "Filter by deletion status",
                "type": AbstractDataProviderType::get(SoftBoolOrNothingType),
                "desc": "Filter by whether orders are deleted",
            },
            "payment_method": <DataProviderOptionInfo>{
                "display_name": "Payment Method",
                "short_desc": "Filter by payment method",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter orders by payment method (e.g., `Credit Card`)",
            },
            "sort": <DataProviderOptionInfo>{
                "display_name": "Sort",
                "short_desc": "Sort field",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Sort order for results",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "id",
                        "desc": "Sort by order ID"},
                    <AllowedValueInfo>{"value": "date_created",
                        "desc": "Sort by creation date"},
                    <AllowedValueInfo>{"value": "date_modified",
                        "desc": "Sort by modification date"},
                    <AllowedValueInfo>{"value": "customer_id",
                        "desc": "Sort by customer ID"},
                ),
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
            "get": Class::forName("BigCommerceGetOrderDataProvider"),
            "update": Class::forName("BigCommerceUpdateOrderDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(BigCommerceRestClient::BigCommerceRestClient rest)
            : BigCommerceDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "orders";
    }

    #! Returns the provider description
    *string getDesc() {
        return "BigCommerce orders data provider for listing and searching orders";
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
        return new BigCommerceOrderDataType().getFields();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # Build query params for V2 Orders API
        hash<auto> params;

        if (search_options.order_id) {
            # V2 single order lookup - get by ID directly
            try {
                auto result = doGetV2("orders/" + search_options.order_id);
                list<hash<auto>> data = ();
                if (result.typeCode() == NT_HASH) {
                    data = (result,);
                }
                return new DefaultRecordIterator(new ListHashIterator(data), where_cond,
                    search_options);
            } catch (hash<ExceptionInfo> ex) {
                return new DefaultRecordIterator(new ListHashIterator(()),
                    where_cond, search_options);
            }
        }
        if (search_options.customer_id) {
            params.customer_id = search_options.customer_id;
        }
        if (search_options.status_id) {
            params.status_id = search_options.status_id;
        }
        if (search_options.min_date_created) {
            params.min_date_created = formatDateTimeV2(search_options.min_date_created);
        }
        if (search_options.max_date_created) {
            params.max_date_created = formatDateTimeV2(search_options.max_date_created);
        }
        if (search_options.min_date_modified) {
            params.min_date_modified = formatDateTimeV2(search_options.min_date_modified);
        }
        if (search_options.min_total) {
            params.min_total = search_options.min_total;
        }
        if (search_options.max_total) {
            params.max_total = search_options.max_total;
        }
        if (exists search_options.is_deleted) {
            params.is_deleted = search_options.is_deleted ? "true" : "false";
        }
        if (search_options.payment_method) {
            params.payment_method = search_options.payment_method;
        }
        if (search_options.sort) {
            params.sort = search_options.sort;
        }
        if (search_options.page) {
            params.page = search_options.page;
        }

        list<hash<auto>> all_records = fetchAllPagesV2("orders", params,
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
