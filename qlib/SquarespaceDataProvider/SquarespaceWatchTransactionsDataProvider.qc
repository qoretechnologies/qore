# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SquarespaceWatchTransactionsDataProvider.qc Squarespace watch transactions event data provider

/*  SquarespaceWatchTransactionsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Squarespace watch transactions event data provider
public class SquarespaceWatchTransactionsDataProvider inherits SquarespaceWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_TRANSACTION_UPDATED,
            "desc": "Squarespace transaction updated event provider - polls for new or updated transactions",
            "type": "SquarespaceWatchTransactionsDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    #! Creates the object with a REST client
    constructor(SquarespaceRestClient::SquarespaceRestClient rest, *hash<auto> options)
            : SquarespaceWatchDataProviderBase(rest, options) {
    }

    #! Returns the provider name
    string getName() {
        return EVENT_TRANSACTION_UPDATED;
    }

    #! Returns the provider description
    *string getDesc() {
        return "Squarespace transaction updated event provider - polls for new or updated transactions";
    }

    #! Returns the event name for notifications
    private string getEventName() {
        return EVENT_TRANSACTION_UPDATED;
    }

    #! Returns the data type for event records
    private AbstractDataProviderType getRecordDataType() {
        return new SquarespaceTransactionDataType();
    }

    #! Fetches records modified since the given date
    /** @param since the date to fetch records modified after
    */
    private list<hash<auto>> fetchRecords(date since) {
        return fetchAllPages("1.0/commerce/transactions", "documents",
            {"modifiedAfter": formatDateTime(since)});
    }

    #! Returns fake example data for transactions
    private hash<auto> getFakeExampleData() {
        return SquarespaceWatchDataProviderBase::getFakeExampleData() + {
            "customerEmail": "customer@example.com",
            "salesOrderId": "EXAMPLE_ORDER_1",
            "voided": False,
            "totalSales": {"currency": "USD", "value": "29.99"},
            "totalNetSales": {"currency": "USD", "value": "29.99"},
            "totalNetShipping": {"currency": "USD", "value": "5.99"},
            "totalTaxes": {"currency": "USD", "value": "2.85"},
            "total": {"currency": "USD", "value": "38.83"},
            "totalNetPayment": {"currency": "USD", "value": "38.83"},
            "payments": ({
                "id": "EXAMPLE_PAYMENT_1",
                "provider": "STRIPE",
                "amount": {"currency": "USD", "value": "38.83"},
            },),
            "salesLineItems": ({
                "id": "EXAMPLE_SALES_LINE_1",
                "quantity": 1,
                "totalNetSales": {"currency": "USD", "value": "29.99"},
            },),
        };
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SquarespaceDataProvider
