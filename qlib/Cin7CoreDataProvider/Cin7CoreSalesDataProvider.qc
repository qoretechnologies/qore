# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore Cin7CoreDataProvider module definition

/*  Cin7CoreSalesDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Cin7Core sales collection data provider - list and search sales
public class Cin7CoreSalesDataProvider inherits Cin7CoreDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "sales",
            "desc": "Cin7Core sales data provider - list and search sales orders",
            "type": "Cin7CoreSalesDataProvider",
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
                "desc": "Return a specific sale by its unique identifier (GUID)",
                "preselected": True,
            },
            "SaleOrderNumber": <DataProviderOptionInfo>{
                "display_name": "Sale Order Number",
                "short_desc": "Filter by order number",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter by sale order number",
            },
            "Status": <DataProviderOptionInfo>{
                "display_name": "Status",
                "short_desc": "Filter by status",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter sales by status",
                "preselected": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "Draft", "desc": "Draft orders"},
                    <AllowedValueInfo>{"value": "Authorised", "desc": "Authorised orders"},
                    <AllowedValueInfo>{"value": "Voided", "desc": "Voided orders"},
                    <AllowedValueInfo>{"value": "Completed", "desc": "Completed orders"},
                ),
            },
            "Customer": <DataProviderOptionInfo>{
                "display_name": "Customer",
                "short_desc": "Filter by customer",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter by customer name",
            },
            "ModifiedSince": <DataProviderOptionInfo>{
                "display_name": "Modified Since",
                "short_desc": "Filter by modification date",
                "type": AbstractDataProviderType::get(DateOrNothingType),
                "desc": "Return sales modified after this date/time",
            },
            "CreatedSince": <DataProviderOptionInfo>{
                "display_name": "Created Since",
                "short_desc": "Filter by creation date",
                "type": AbstractDataProviderType::get(DateOrNothingType),
                "desc": "Return sales created after this date/time",
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
        static Cin7CoreSaleDataType recordType();
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
        return "sales";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Cin7Core sales data provider - list and search sales orders";
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    #! Searches for sales
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        hash<auto> params;

        # Build query parameters from search options
        if (search_options.SaleID) {
            params.SaleID = search_options.SaleID;
        }
        if (search_options.SaleOrderNumber) {
            params.SaleOrderNumber = search_options.SaleOrderNumber;
        }
        if (search_options.Status) {
            params.Status = search_options.Status;
        }
        if (search_options.Customer) {
            params.Customer = search_options.Customer;
        }
        if (search_options.ModifiedSince) {
            params.ModifiedSince = search_options.ModifiedSince;
        }
        if (search_options.CreatedSince) {
            params.CreatedSince = search_options.CreatedSince;
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

        hash<auto> response = doGet("/SaleList", params);
        list<hash<auto>> records = response.SaleList ?? ();

        return new DefaultRecordIterator(new ListHashIterator(records), where_cond, search_options,
            recordType.getFields());
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace Cin7CoreDataProvider
