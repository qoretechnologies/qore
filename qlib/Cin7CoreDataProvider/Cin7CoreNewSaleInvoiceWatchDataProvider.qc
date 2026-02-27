# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore Cin7CoreDataProvider module definition

/*  Cin7CoreNewSaleInvoiceWatchDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Watch provider for new authorized sale invoices
public class Cin7CoreNewSaleInvoiceWatchDataProvider inherits Cin7CoreWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_NEW_SALE_INVOICE_AUTHORIZED,
            "desc": "Triggers when a new sale invoice is authorized",
            "type": "Cin7CoreNewSaleInvoiceWatchDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    #! Creates the object with a REST client
    constructor(Cin7CoreRestClient::Cin7CoreRestClient rest, *hash<auto> options)
            : Cin7CoreWatchDataProviderBase(rest, options) {
    }

    #! Creates the object with a connection
    constructor(Cin7CoreRestClient::Cin7CoreRestConnection conn, *hash<auto> options)
            : Cin7CoreWatchDataProviderBase(conn, options) {
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : Cin7CoreWatchDataProviderBase(options) {
    }

    #! Returns the provider name
    string getName() {
        return EVENT_NEW_SALE_INVOICE_AUTHORIZED;
    }

    #! Returns the provider description
    *string getDesc() {
        return "Triggers when a new sale invoice is authorized";
    }

    #! Poll for new authorized invoices
    list<hash<auto>> pollOnceImpl() {
        hash<auto> params = {
            "ModifiedSince": since,
            "Status": "Authorized",
            "Page": 1,
            "Limit": MaxPageSize,
        };

        @debug(log(LoggerLevel::DEBUG, "pollOnce: polling for authorized invoices since %y", since));
        hash<auto> response = doGet("/SaleInvoice", params);
        return response.InvoiceList ?? ();
    }

    #! Get the unique ID from an invoice record
    string getRecordId(hash<auto> record) {
        return record.ID ?? record.InvoiceNumber ?? "";
    }

    #! Get the event name
    string getEventName() {
        return EVENT_NEW_SALE_INVOICE_AUTHORIZED;
    }

    #! Get the event data type
    AbstractDataProviderType getEventDataType() {
        return new Cin7CoreInvoiceDataType();
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns resource-specific fake example data for invoices
    hash<auto> getFakeExampleData() {
        return {
            "ID": "12345678-1234-1234-1234-123456789012",
            "InvoiceNumber": "INV-00001",
            "SaleID": "abcdef12-3456-7890-abcd-ef1234567890",
            "SaleOrderNumber": "SO-00001",
            "Status": "AUTHORISED",
            "InvoiceDate": "2026-01-15",
            "DueDate": "2026-02-14",
            "Customer": "Acme Corporation",
            "CustomerID": "fedcba09-8765-4321-fedc-ba0987654321",
            "Currency": "USD",
            "Total": 1250.00,
            "AmountPaid": 0.00,
            "Balance": 1250.00,
            "TaxTotal": 100.00,
            "LastModifiedOn": "2026-01-15T11:30:00Z",
        };
    }
}

} # namespace Cin7CoreDataProvider
