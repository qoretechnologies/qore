# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore Cin7CoreDataProvider module definition

/*  Cin7CoreFindSaleInvoiceDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Cin7Core sale invoices collection data provider
public class Cin7CoreFindSaleInvoiceDataProvider inherits Cin7CoreDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "find-sale-invoice",
            "desc": "Cin7Core sale invoice data provider - find invoices for sales",
            "type": "Cin7CoreFindSaleInvoiceDataProvider",
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
                "desc": "Return invoices for a specific sale",
            },
            "InvoiceNumber": <DataProviderOptionInfo>{
                "display_name": "Invoice Number",
                "short_desc": "Filter by invoice number",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Return a specific invoice by number",
            },
            "Status": <DataProviderOptionInfo>{
                "display_name": "Status",
                "short_desc": "Filter by status",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter invoices by status (Draft, Authorized, Void)",
            },
            "ModifiedSince": <DataProviderOptionInfo>{
                "display_name": "Modified Since",
                "short_desc": "Filter by modification date",
                "type": AbstractDataProviderType::get(DateOrNothingType),
                "desc": "Return invoices modified after this date/time",
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
        static Cin7CoreInvoiceDataType recordType();
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
        return "find-sale-invoice";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Cin7Core sale invoice data provider - find invoices for sales";
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    #! Searches for sale invoices
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        hash<auto> params;

        # Build query parameters from search options
        if (search_options.SaleID) {
            params.SaleID = search_options.SaleID;
        }
        if (search_options.InvoiceNumber) {
            params.InvoiceNumber = search_options.InvoiceNumber;
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

        hash<auto> response = doGet("/SaleInvoice", params);
        list<hash<auto>> records = response.InvoiceList ?? ();

        return new DefaultRecordIterator(new ListHashIterator(records), where_cond, search_options,
            recordType.getFields());
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Invoice data type
public class Cin7CoreInvoiceDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "ID": {
                "type": StringOrNothingType,
                "display_name": "ID",
                "short_desc": "Invoice ID",
                "desc": "The unique identifier for the invoice",
            },
            "SaleID": {
                "type": StringOrNothingType,
                "display_name": "Sale ID",
                "short_desc": "Associated sale ID",
                "desc": "The sale this invoice belongs to",
            },
            "InvoiceNumber": {
                "type": StringOrNothingType,
                "display_name": "Invoice Number",
                "short_desc": "Invoice number",
                "desc": "The invoice document number",
            },
            "InvoiceDate": {
                "type": DateOrNothingType,
                "display_name": "Invoice Date",
                "short_desc": "Invoice date",
                "desc": "The date of the invoice",
            },
            "DueDate": {
                "type": DateOrNothingType,
                "display_name": "Due Date",
                "short_desc": "Payment due date",
                "desc": "The payment due date",
            },
            "Status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "Invoice status",
                "desc": "The status of the invoice",
            },
            "Customer": {
                "type": StringOrNothingType,
                "display_name": "Customer",
                "short_desc": "Customer name",
                "desc": "The customer for this invoice",
            },
            "Total": {
                "type": FloatOrNothingType,
                "display_name": "Total",
                "short_desc": "Total amount",
                "desc": "The total amount of the invoice",
            },
            "TotalTax": {
                "type": FloatOrNothingType,
                "display_name": "Total Tax",
                "short_desc": "Total tax",
                "desc": "The total tax on the invoice",
            },
            "AmountPaid": {
                "type": FloatOrNothingType,
                "display_name": "Amount Paid",
                "short_desc": "Amount paid",
                "desc": "The amount that has been paid",
            },
            "Balance": {
                "type": FloatOrNothingType,
                "display_name": "Balance",
                "short_desc": "Balance due",
                "desc": "The remaining balance due",
            },
            "Lines": {
                "type": AutoListOrNothingType,
                "display_name": "Lines",
                "short_desc": "Invoice lines",
                "desc": "The line items on the invoice",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("Cin7CoreInvoiceDataType") {
        addQoreFields(Fields);
    }
}

} # namespace Cin7CoreDataProvider
