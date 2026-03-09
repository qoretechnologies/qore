# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore Cin7CoreDataProvider module definition

/*  Cin7CoreFindSaleFulfilmentDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the Cin7CoreDataProvider module
public namespace Cin7CoreDataProvider {

#! Cin7Core sale fulfilments collection data provider
public class Cin7CoreFindSaleFulfilmentDataProvider inherits Cin7CoreDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "find-sale-fulfilment",
            "desc": "Cin7Core sale fulfilment data provider - find fulfilment/pick/pack tasks",
            "type": "Cin7CoreFindSaleFulfilmentDataProvider",
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
            "SaleID": <DataProviderOptionInfo>{
                "display_name": "Sale ID",
                "short_desc": "Filter by sale ID",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Return fulfilments for a specific sale",
                "preselected": True,
            },
            "TaskID": <DataProviderOptionInfo>{
                "display_name": "Task ID",
                "short_desc": "Filter by task ID",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Return a specific fulfilment task",
                "preselected": True,
            },
            "Status": <DataProviderOptionInfo>{
                "display_name": "Status",
                "short_desc": "Filter by status",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter fulfilments by status",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "Draft", "desc": "Draft fulfilment"},
                    <AllowedValueInfo>{"value": "Authorized", "desc": "Authorized fulfilment"},
                    <AllowedValueInfo>{"value": "Void", "desc": "Voided fulfilment"},
                ),
            },
            "ModifiedSince": <DataProviderOptionInfo>{
                "display_name": "Modified Since",
                "short_desc": "Filter by modification date",
                "type": AbstractDataProviderType::get(DateOrNothingType),
                "desc": "Return fulfilments modified after this date/time",
            },
            "Page": <DataProviderOptionInfo>{
                "display_name": "Page",
                "short_desc": "Page number",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Page number for pagination (starting at 1)",
            },
            "Limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Results per page",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Number of results per page (default 100, max 1000)",
            },
        };
    }

    private {
        #! Record type
        static Cin7CoreFulfilmentDataType recordType();
    }

    #! Creates the object with a REST client
    constructor(Cin7CoreRestClient::Cin7CoreRestClient rest) : Cin7CoreDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(Cin7CoreRestClient::Cin7CoreRestConnection conn) : Cin7CoreDataProviderBase(conn) {
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : Cin7CoreDataProviderBase(options) {
    }

    #! Returns the provider name
    string getName() {
        return "find-sale-fulfilment";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Cin7Core sale fulfilment data provider - find fulfilment/pick/pack tasks";
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    #! Searches for sale fulfilments
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        hash<auto> params;

        # Build query parameters from search options
        if (search_options.SaleID) {
            params.SaleID = search_options.SaleID;
        }
        if (search_options.TaskID) {
            params.TaskID = search_options.TaskID;
        }
        if (search_options.Status) {
            params.Status = search_options.Status;
        }
        if (search_options.ModifiedSince) {
            params.ModifiedSince = search_options.ModifiedSince;
        }
        if (search_options.Page) {
            params.Page = search_options.Page;
        } else {
            params.Page = 1;
        }
        if (search_options.Limit) {
            params.Limit = search_options.Limit;
        } else {
            params.Limit = DefaultPageSize;
        }

        @debug(log(LoggerLevel::DEBUG, "searchRecordsImpl: params=%y", params));

        hash<auto> response = doGet("/SaleFulfilment", params);
        list<hash<auto>> records = response.FulfilmentList ?? ();

        return new DefaultRecordIterator(new ListHashIterator(records), where_cond, search_options,
            recordType.getFields());
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Fulfilment data type
public class Cin7CoreFulfilmentDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "TaskID": {
                "type": StringOrNothingType,
                "display_name": "Task ID",
                "short_desc": "Fulfilment task ID",
                "desc": "The unique identifier for the fulfilment task",
            },
            "SaleID": {
                "type": StringOrNothingType,
                "display_name": "Sale ID",
                "short_desc": "Associated sale ID",
                "desc": "The sale this fulfilment belongs to",
            },
            "SaleOrderNumber": {
                "type": StringOrNothingType,
                "display_name": "Sale Order Number",
                "short_desc": "Sale order number",
                "desc": "The sale order number",
            },
            "Status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "Fulfilment status",
                "desc": "The status of the fulfilment",
            },
            "FulfilmentType": {
                "type": StringOrNothingType,
                "display_name": "Fulfilment Type",
                "short_desc": "Type of fulfilment",
                "desc": "The type of fulfilment (Pick, Pack, Ship)",
            },
            "Location": {
                "type": StringOrNothingType,
                "display_name": "Location",
                "short_desc": "Warehouse location",
                "desc": "The warehouse location for this fulfilment",
            },
            "Lines": {
                "type": new ListDataType("Lines", new Cin7CoreSaleLineItemDataType(), True),
                "display_name": "Lines",
                "short_desc": "Fulfilment lines",
                "desc": "The line items being fulfilled",
            },
            "Carrier": {
                "type": StringOrNothingType,
                "display_name": "Carrier",
                "short_desc": "Shipping carrier",
                "desc": "The shipping carrier",
            },
            "TrackingNumber": {
                "type": StringOrNothingType,
                "display_name": "Tracking Number",
                "short_desc": "Tracking number",
                "desc": "The shipment tracking number",
            },
            "ShippedDate": {
                "type": DateOrNothingType,
                "display_name": "Shipped Date",
                "short_desc": "Date shipped",
                "desc": "The date the items were shipped",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("Cin7CoreFulfilmentDataType") {
        addQoreFields(Fields);
    }
}

} # namespace Cin7CoreDataProvider
