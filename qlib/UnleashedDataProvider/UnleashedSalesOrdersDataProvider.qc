# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore UnleashedDataProvider module definition

/*  UnleashedSalesOrdersDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the UnleashedDataProvider module
public namespace UnleashedDataProvider {

#! Sales orders search data provider for Unleashed
public class UnleashedSalesOrdersDataProvider inherits UnleashedDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "sales-orders",
            "desc": "Search and list sales orders from Unleashed",
            "type": "UnleashedSalesOrdersDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "has_record": True,
            "search_options": SearchOptions,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Search options for sales orders
        const SearchOptions = {
            "customerCode": <DataProviderOptionInfo>{
                "display_name": "Customer Code",
                "short_desc": "Filter by customer code",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter sales orders by customer code",
            },
            "customerId": <DataProviderOptionInfo>{
                "display_name": "Customer ID",
                "short_desc": "Filter by customer GUID",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter sales orders by customer GUID",
            },
            "orderNumber": <DataProviderOptionInfo>{
                "display_name": "Order Number",
                "short_desc": "Filter by order number",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter by specific order number",
            },
            "orderStatus": <DataProviderOptionInfo>{
                "display_name": "Order Status",
                "short_desc": "Filter by order status",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter by order status (Open, Parked, Backordered, Placed, Completed, Deleted)",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "Open", "desc": "Open orders"},
                    <AllowedValueInfo>{"value": "Parked", "desc": "Parked orders"},
                    <AllowedValueInfo>{"value": "Backordered", "desc": "Backordered orders"},
                    <AllowedValueInfo>{"value": "Placed", "desc": "Placed orders"},
                    <AllowedValueInfo>{"value": "Completed", "desc": "Completed orders"},
                    <AllowedValueInfo>{"value": "Deleted", "desc": "Deleted orders"},
                ),
            },
            "startDate": <DataProviderOptionInfo>{
                "display_name": "Start Date",
                "short_desc": "Filter by start date",
                "type": AbstractDataProviderType::get(DateOrNothingType),
                "desc": "Return orders from this date (YYYY-MM-DD)",
            },
            "endDate": <DataProviderOptionInfo>{
                "display_name": "End Date",
                "short_desc": "Filter by end date",
                "type": AbstractDataProviderType::get(DateOrNothingType),
                "desc": "Return orders until this date (YYYY-MM-DD)",
            },
            "modifiedSince": <DataProviderOptionInfo>{
                "display_name": "Modified Since",
                "short_desc": "Filter by modification date",
                "type": AbstractDataProviderType::get(DateOrNothingType),
                "desc": "Return orders modified after this date/time (UTC)",
            },
            "warehouseCode": <DataProviderOptionInfo>{
                "display_name": "Warehouse Code",
                "short_desc": "Filter by warehouse",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter by warehouse code",
            },
            "pageSize": <DataProviderOptionInfo>{
                "display_name": "Page Size",
                "short_desc": "Results per page",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Number of results per page (max 200, default 200)",
            },
        };
    }

    private {
        #! Record type
        static UnleashedSalesOrderDataType recordType();
    }

    #! Creates the object with a REST client
    constructor(UnleashedRestClient::UnleashedRestClient rest) : UnleashedDataProviderBase(rest) {
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : UnleashedDataProviderBase(options) {
    }

    #! Returns the provider name
    string getName() {
        return "sales-orders";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Search and list sales orders from Unleashed";
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns the record type
    private *AbstractDataProviderType getRecordTypeImpl(*hash<auto> search_options) {
        return recordType;
    }

    #! Performs a search/query
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        hash<auto> params;

        if (search_options.customerCode) {
            params.customerCode = search_options.customerCode;
        }
        if (search_options.customerId) {
            params.customerId = search_options.customerId;
        }
        if (search_options.orderNumber) {
            params.orderNumber = search_options.orderNumber;
        }
        if (search_options.orderStatus) {
            params.orderStatus = search_options.orderStatus;
        }
        if (search_options.startDate) {
            params.startDate = formatDate(search_options.startDate);
        }
        if (search_options.endDate) {
            params.endDate = formatDate(search_options.endDate);
        }
        if (search_options.modifiedSince) {
            params.modifiedSince = formatDate(search_options.modifiedSince);
        }
        if (search_options.warehouseCode) {
            params.warehouseCode = search_options.warehouseCode;
        }
        if (search_options.pageSize) {
            params.pageSize = search_options.pageSize;
        } else {
            params.pageSize = DefaultPageSize;
        }

        auto response = doGet("/SalesOrders", params);

        list<hash<auto>> records;
        if (response.typeCode() == NT_HASH && response.Items.typeCode() == NT_LIST) {
            records = response.Items;
        }

        return new DefaultRecordIterator(new ListHashIterator(records), where_cond,
            search_options, recordType.getFields());
    }
}

} # namespace UnleashedDataProvider
