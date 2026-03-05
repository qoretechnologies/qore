# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SquarespaceOrdersDataProvider.qc Squarespace orders collection data provider

/*  SquarespaceOrdersDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Squarespace orders collection data provider
public class SquarespaceOrdersDataProvider inherits SquarespaceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "orders",
            "desc": "Squarespace orders data provider for listing and searching orders",
            "type": "SquarespaceOrdersDataProvider",
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
                "short_desc": "Get a specific order by ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Get a specific order by ID",
            },
            "customer_id": <DataProviderOptionInfo>{
                "display_name": "Customer ID",
                "short_desc": "Filter orders by customer ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter orders by customer ID",
            },
            "fulfillment_status": <DataProviderOptionInfo>{
                "display_name": "Fulfillment Status",
                "short_desc": "Filter by status (PENDING, FULFILLED, CANCELED)",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by fulfillment status (`PENDING`, `FULFILLED`, `CANCELED`)",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "PENDING", "desc": "Pending fulfillment"},
                    <AllowedValueInfo>{"value": "FULFILLED", "desc": "Fulfilled"},
                    <AllowedValueInfo>{"value": "CANCELED", "desc": "Canceled"},
                ),
            },
            "modified_after": <DataProviderOptionInfo>{
                "display_name": "Modified After",
                "short_desc": "Orders modified after this datetime",
                "type": AbstractDataProviderType::get(SoftDateOrNothingType),
                "desc": "Orders modified after this datetime",
            },
            "modified_before": <DataProviderOptionInfo>{
                "display_name": "Modified Before",
                "short_desc": "Orders modified before this datetime",
                "type": AbstractDataProviderType::get(SoftDateOrNothingType),
                "desc": "Orders modified before this datetime",
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
            "create": Class::forName("SquarespaceCreateOrderDataProvider"),
            "fulfill": Class::forName("SquarespaceFulfillOrderDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(SquarespaceRestClient::SquarespaceRestClient rest) : SquarespaceDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "orders";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Squarespace orders data provider for listing and searching orders";
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
        return new SquarespaceOrderDataType().getFields();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # If order_id provided, get single record
        if (search_options.order_id) {
            hash<auto> result = doGet("1.0/commerce/orders/" + search_options.order_id);
            return new DefaultRecordIterator(new ListHashIterator((result,)),
                where_cond, search_options);
        }

        # Build query params
        hash<auto> params;
        if (search_options.customer_id) {
            params.customerId = search_options.customer_id;
        }
        if (search_options.fulfillment_status) {
            params.fulfillmentStatus = search_options.fulfillment_status;
        }
        if (search_options.modified_after) {
            params.modifiedAfter = search_options.modified_after;
        }
        if (search_options.modified_before) {
            params.modifiedBefore = search_options.modified_before;
        }

        list<hash<auto>> all_records = fetchAllPages("1.0/commerce/orders", "result", params,
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
