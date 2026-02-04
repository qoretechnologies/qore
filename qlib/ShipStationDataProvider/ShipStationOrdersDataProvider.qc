# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShipStationDataProvider module definition

/*  ShipStationOrdersDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ShipStationDataProvider module
public namespace ShipStationDataProvider {

#! ShipStation orders collection data provider
public class ShipStationOrdersDataProvider inherits ShipStationDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "orders",
            "desc": "ShipStation orders data provider - list and search orders",
            "type": "ShipStationOrdersDataProvider",
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
            "orderStatus": <DataProviderOptionInfo>{
                "display_name": "Order Status",
                "short_desc": "Filter by order status",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter orders by status",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "awaiting_payment", "desc": "Awaiting payment"},
                    <AllowedValueInfo>{"value": "awaiting_shipment", "desc": "Awaiting shipment"},
                    <AllowedValueInfo>{"value": "shipped", "desc": "Shipped"},
                    <AllowedValueInfo>{"value": "on_hold", "desc": "On hold"},
                    <AllowedValueInfo>{"value": "cancelled", "desc": "Cancelled"},
                    <AllowedValueInfo>{"value": "pending_fulfillment", "desc": "Pending fulfillment"},
                ),
            },
            "customerName": <DataProviderOptionInfo>{
                "display_name": "Customer Name",
                "short_desc": "Filter by customer name",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter orders by customer name (partial match)",
            },
            "orderNumber": <DataProviderOptionInfo>{
                "display_name": "Order Number",
                "short_desc": "Filter by order number",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter orders by order number (starts with search)",
            },
            "storeId": <DataProviderOptionInfo>{
                "display_name": "Store",
                "short_desc": "Filter by store",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Filter orders by store ID",
                "ref_data": "stores",
            },
            "orderDateStart": <DataProviderOptionInfo>{
                "display_name": "Order Date Start",
                "short_desc": "Start of order date range",
                "type": AbstractDataProviderType::get(DateOrNothingType),
                "desc": "Filter orders placed on or after this date",
            },
            "orderDateEnd": <DataProviderOptionInfo>{
                "display_name": "Order Date End",
                "short_desc": "End of order date range",
                "type": AbstractDataProviderType::get(DateOrNothingType),
                "desc": "Filter orders placed on or before this date",
            },
            "modifyDateStart": <DataProviderOptionInfo>{
                "display_name": "Modify Date Start",
                "short_desc": "Start of modify date range",
                "type": AbstractDataProviderType::get(DateOrNothingType),
                "desc": "Filter orders modified on or after this date",
            },
            "modifyDateEnd": <DataProviderOptionInfo>{
                "display_name": "Modify Date End",
                "short_desc": "End of modify date range",
                "type": AbstractDataProviderType::get(DateOrNothingType),
                "desc": "Filter orders modified on or before this date",
            },
            "sortBy": <DataProviderOptionInfo>{
                "display_name": "Sort By",
                "short_desc": "Field to sort by",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Sort results by this field",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "OrderDate", "desc": "Sort by order date"},
                    <AllowedValueInfo>{"value": "ModifyDate", "desc": "Sort by modify date"},
                    <AllowedValueInfo>{"value": "CreateDate", "desc": "Sort by create date"},
                ),
            },
            "sortDir": <DataProviderOptionInfo>{
                "display_name": "Sort Direction",
                "short_desc": "Sort direction",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Sort direction (ASC or DESC)",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "ASC", "desc": "Ascending"},
                    <AllowedValueInfo>{"value": "DESC", "desc": "Descending"},
                ),
            },
            "page": <DataProviderOptionInfo>{
                "display_name": "Page",
                "short_desc": "Page number",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Page number for pagination (starting at 1)",
            },
            "pageSize": <DataProviderOptionInfo>{
                "display_name": "Page Size",
                "short_desc": "Results per page",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Number of results per page (max 500)",
            },
        };
    }

    private {
        #! Record type
        static ShipStationOrderDataType recordType();
    }

    #! Creates the object with a REST client
    constructor(ShipStationRestClient::ShipStationRestClient rest)
            : ShipStationDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(ShipStationRestClient::ShipStationRestConnection conn) : ShipStationDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "orders";
    }

    #! Returns the provider description
    *string getDesc() {
        return "ShipStation orders data provider - list and search orders";
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
        if (search_options.orderStatus) {
            params.orderStatus = search_options.orderStatus;
        }
        if (search_options.customerName) {
            params.customerName = search_options.customerName;
        }
        if (search_options.orderNumber) {
            params.orderNumber = search_options.orderNumber;
        }
        if (search_options.storeId) {
            params.storeId = search_options.storeId;
        }
        if (search_options.orderDateStart) {
            params.orderDateStart = search_options.orderDateStart;
        }
        if (search_options.orderDateEnd) {
            params.orderDateEnd = search_options.orderDateEnd;
        }
        if (search_options.modifyDateStart) {
            params.modifyDateStart = search_options.modifyDateStart;
        }
        if (search_options.modifyDateEnd) {
            params.modifyDateEnd = search_options.modifyDateEnd;
        }
        if (search_options.sortBy) {
            params.sortBy = search_options.sortBy;
        }
        if (search_options.sortDir) {
            params.sortDir = search_options.sortDir;
        }
        if (search_options.page) {
            params.page = search_options.page;
        }
        if (search_options.pageSize) {
            params.pageSize = search_options.pageSize;
        } else {
            params.pageSize = DefaultPageSize;
        }

        @debug(log(LoggerLevel::DEBUG, "searchRecordsImpl: params=%y", params));

        hash<auto> response = doGet("/orders", params);
        list<hash<auto>> records = response.orders ?? ();

        return new DefaultRecordIterator(new ListHashIterator(records), where_cond, search_options,
            recordType.getFields());
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ShipStationDataProvider
