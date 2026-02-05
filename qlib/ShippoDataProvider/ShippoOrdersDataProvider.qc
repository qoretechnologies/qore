# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShippoDataProvider module definition

/*  ShippoOrdersDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ShippoDataProvider module
public namespace ShippoDataProvider {

#! Shippo orders collection data provider - list and search orders
public class ShippoOrdersDataProvider inherits ShippoDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "orders",
            "desc": "Shippo orders data provider - list and search orders",
            "type": "ShippoOrdersDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "has_record": True,
            "search_options": SearchOptions,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Search options
        const SearchOptions = {
            "object_created_gt": <DataProviderOptionInfo>{
                "display_name": "Created After",
                "short_desc": "Filter by creation date (after)",
                "type": AbstractDataProviderType::get(DateOrNothingType),
                "desc": "Return orders created after this date/time",
            },
            "object_created_lt": <DataProviderOptionInfo>{
                "display_name": "Created Before",
                "short_desc": "Filter by creation date (before)",
                "type": AbstractDataProviderType::get(DateOrNothingType),
                "desc": "Return orders created before this date/time",
            },
            "shop_app": <DataProviderOptionInfo>{
                "display_name": "Shop App",
                "short_desc": "Filter by shop app",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter orders by the shop app used to create them",
            },
            "order_status": <DataProviderOptionInfo>{
                "display_name": "Order Status",
                "short_desc": "Filter by order status",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter orders by status",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "UNKNOWN", "desc": "Unknown status"},
                    <AllowedValueInfo>{"value": "AWAITPAY", "desc": "Awaiting payment"},
                    <AllowedValueInfo>{"value": "PAID", "desc": "Paid"},
                    <AllowedValueInfo>{"value": "REFUNDED", "desc": "Refunded"},
                    <AllowedValueInfo>{"value": "CANCELLED", "desc": "Cancelled"},
                    <AllowedValueInfo>{"value": "PARTIALLY_FULFILLED", "desc": "Partially fulfilled"},
                    <AllowedValueInfo>{"value": "SHIPPED", "desc": "Shipped"},
                ),
            },
            "page": <DataProviderOptionInfo>{
                "display_name": "Page",
                "short_desc": "Page number",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Page number for pagination (starting at 1)",
            },
            "results": <DataProviderOptionInfo>{
                "display_name": "Page Size",
                "short_desc": "Results per page",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Number of results per page (default 25, max 100)",
            },
        };
    }

    private {
        #! Record type
        static ShippoOrderDataType recordType();
    }

    #! Creates the object with a REST client
    constructor(ShippoRestClient::ShippoRestClient rest) : ShippoDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(ShippoRestClient::ShippoRestConnection conn) : ShippoDataProviderBase(conn) {
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : ShippoDataProviderBase(options) {
    }

    #! Returns the provider name
    string getName() {
        return "orders";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Shippo orders data provider - list and search orders";
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    #! Searches for orders
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        hash<auto> params;

        # Build query parameters from search options
        if (search_options.object_created_gt) {
            params.object_created_gt = search_options.object_created_gt;
        }
        if (search_options.object_created_lt) {
            params.object_created_lt = search_options.object_created_lt;
        }
        if (search_options.shop_app) {
            params.shop_app = search_options.shop_app;
        }
        if (search_options.order_status) {
            params.order_status = search_options.order_status;
        }
        if (search_options.page) {
            params.page = search_options.page;
        }
        if (search_options.results) {
            params.results = search_options.results;
        } else {
            params.results = DefaultPageSize;
        }

        @debug(log(LoggerLevel::DEBUG, "searchRecordsImpl: params=%y", params));

        hash<auto> response = doGet("/orders", params);
        list<hash<auto>> records = response.results ?? ();

        return new DefaultRecordIterator(new ListHashIterator(records), where_cond, search_options,
            recordType.getFields());
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ShippoDataProvider
