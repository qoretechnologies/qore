# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore FreshBooksDataProvider module definition - Watch Providers

/*  FreshBooksWatchDataProviders.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace FreshBooksDataProvider {

#! Watch for new invoices in FreshBooks
public class FreshBooksNewInvoiceWatchDataProvider inherits FreshBooksWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_INVOICE_CREATED,
            "desc": "Watch for new invoices in FreshBooks",
            "type": "FreshBooksNewInvoiceWatchDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    #! Creates the object with a REST client
    constructor(FreshBooksRestClient::FreshBooksRestClient rest, *hash<auto> options)
            : FreshBooksWatchDataProviderBase(rest, options) {}

    #! Returns the provider name
    string getName() { return EVENT_INVOICE_CREATED; }

    #! Returns the API path for invoices
    private string getResourcePath() { return accountPath("/invoices/invoices"); }

    #! Returns the response key for invoices
    private string getResourceKey() { return "invoices"; }

    #! Returns the record ID for an invoice
    private string getRecordId(hash<auto> record) { return string(record.invoiceid ?? record.id); }

    #! Returns the event name
    private string getEventName() { return EVENT_INVOICE_CREATED; }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Returns the data type for invoice event records
    private AbstractDataProviderType getRecordDataType() { return new FreshBooksInvoiceDataType(); }

    #! Returns query parameters with invoice-specific filters
    private hash<auto> getQueryParams() {
        hash<auto> params = FreshBooksWatchDataProviderBase::getQueryParams();
        params."search[vis_state]" = "0";
        return params;
    }

    #! Returns fake example data for invoices
    private hash<auto> getFakeExampleData() {
        return FreshBooksWatchDataProviderBase::getFakeExampleData() + {
            "invoiceid": 123456,
            "invoice_number": "INV-0001",
            "customerid": 654321,
            "current_organization": "Acme Corporation",
            "status": 2,
            "create_date": "2026-01-30",
            "currency_code": "USD",
            "amount": {"amount": "1500.00", "code": "USD"},
            "outstanding": {"amount": "1500.00", "code": "USD"},
        };
    }
}

#! Watch for new payments in FreshBooks
public class FreshBooksNewPaymentWatchDataProvider inherits FreshBooksWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_PAYMENT_CREATED,
            "desc": "Watch for new payments in FreshBooks",
            "type": "FreshBooksNewPaymentWatchDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    #! Creates the object with a REST client
    constructor(FreshBooksRestClient::FreshBooksRestClient rest, *hash<auto> options)
            : FreshBooksWatchDataProviderBase(rest, options) {}

    #! Returns the provider name
    string getName() { return EVENT_PAYMENT_CREATED; }

    #! Returns the API path for payments
    private string getResourcePath() { return accountPath("/payments/payments"); }

    #! Returns the response key for payments
    private string getResourceKey() { return "payments"; }

    #! Returns the record ID for a payment
    private string getRecordId(hash<auto> record) { return string(record.paymentid ?? record.id); }

    #! Returns the event name
    private string getEventName() { return EVENT_PAYMENT_CREATED; }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Returns the data type for payment event records
    private AbstractDataProviderType getRecordDataType() { return new FreshBooksPaymentDataType(); }

    #! Returns query parameters with payment-specific filters
    private hash<auto> getQueryParams() {
        hash<auto> params = FreshBooksWatchDataProviderBase::getQueryParams();
        params."search[updated_since]" = format_date("YYYY-MM-DD HH:mm:SS", last_check);
        return params;
    }

    #! Returns fake example data for payments
    private hash<auto> getFakeExampleData() {
        return FreshBooksWatchDataProviderBase::getFakeExampleData() + {
            "paymentid": 789012,
            "invoiceid": 123456,
            "amount": {"amount": "500.00", "code": "USD"},
            "date": "2026-01-30",
            "type": "Credit Card",
            "note": "Payment received",
        };
    }
}

#! Watch for new clients in FreshBooks
public class FreshBooksNewClientWatchDataProvider inherits FreshBooksWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_CLIENT_CREATED,
            "desc": "Watch for new clients in FreshBooks",
            "type": "FreshBooksNewClientWatchDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    #! Creates the object with a REST client
    constructor(FreshBooksRestClient::FreshBooksRestClient rest, *hash<auto> options)
            : FreshBooksWatchDataProviderBase(rest, options) {}

    #! Returns the provider name
    string getName() { return EVENT_CLIENT_CREATED; }

    #! Returns the API path for clients
    private string getResourcePath() { return accountPath("/users/clients"); }

    #! Returns the response key for clients
    private string getResourceKey() { return "clients"; }

    #! Returns the record ID for a client
    private string getRecordId(hash<auto> record) { return string(record.id); }

    #! Returns the event name
    private string getEventName() { return EVENT_CLIENT_CREATED; }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Returns the data type for client event records
    private AbstractDataProviderType getRecordDataType() { return new FreshBooksClientDataType(); }

    #! Returns query parameters with client-specific filters
    private hash<auto> getQueryParams() {
        hash<auto> params = FreshBooksWatchDataProviderBase::getQueryParams();
        params."search[updated_since]" = format_date("YYYY-MM-DD HH:mm:SS", last_check);
        params."search[vis_state]" = "0";
        return params;
    }

    #! Returns fake example data for clients
    private hash<auto> getFakeExampleData() {
        return FreshBooksWatchDataProviderBase::getFakeExampleData() + {
            "fname": "John",
            "lname": "Smith",
            "email": "john.smith@acme.com",
            "organization": "Acme Corporation",
            "phone": "+1-555-123-4567",
            "vis_state": 0,
        };
    }
}

#! Watch for new expenses in FreshBooks
public class FreshBooksNewExpenseWatchDataProvider inherits FreshBooksWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_EXPENSE_CREATED,
            "desc": "Watch for new expenses in FreshBooks",
            "type": "FreshBooksNewExpenseWatchDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    #! Creates the object with a REST client
    constructor(FreshBooksRestClient::FreshBooksRestClient rest, *hash<auto> options)
            : FreshBooksWatchDataProviderBase(rest, options) {}

    #! Returns the provider name
    string getName() { return EVENT_EXPENSE_CREATED; }

    #! Returns the API path for expenses
    private string getResourcePath() { return accountPath("/expenses/expenses"); }

    #! Returns the response key for expenses
    private string getResourceKey() { return "expenses"; }

    #! Returns the record ID for an expense
    private string getRecordId(hash<auto> record) { return string(record.expenseid ?? record.id); }

    #! Returns the event name
    private string getEventName() { return EVENT_EXPENSE_CREATED; }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Returns the data type for expense event records
    private AbstractDataProviderType getRecordDataType() { return new FreshBooksExpenseDataType(); }

    #! Returns query parameters with expense-specific filters
    private hash<auto> getQueryParams() {
        hash<auto> params = FreshBooksWatchDataProviderBase::getQueryParams();
        params."search[updated_since]" = format_date("YYYY-MM-DD HH:mm:SS", last_check);
        return params;
    }

    #! Returns fake example data for expenses
    private hash<auto> getFakeExampleData() {
        return FreshBooksWatchDataProviderBase::getFakeExampleData() + {
            "expenseid": 345678,
            "staffid": 1,
            "categoryid": 12345,
            "amount": {"amount": "125.50", "code": "USD"},
            "date": "2026-01-30",
            "notes": "Office supplies purchase",
            "vendor": "Office Depot",
            "status": 0,
        };
    }
}

#! Watch for new estimates in FreshBooks
public class FreshBooksNewEstimateWatchDataProvider inherits FreshBooksWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_ESTIMATE_CREATED,
            "desc": "Watch for new estimates in FreshBooks",
            "type": "FreshBooksNewEstimateWatchDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    #! Creates the object with a REST client
    constructor(FreshBooksRestClient::FreshBooksRestClient rest, *hash<auto> options)
            : FreshBooksWatchDataProviderBase(rest, options) {}

    #! Returns the provider name
    string getName() { return EVENT_ESTIMATE_CREATED; }

    #! Returns the API path for estimates
    private string getResourcePath() { return accountPath("/estimates/estimates"); }

    #! Returns the response key for estimates
    private string getResourceKey() { return "estimates"; }

    #! Returns the record ID for an estimate
    private string getRecordId(hash<auto> record) { return string(record.estimateid ?? record.id); }

    #! Returns the event name
    private string getEventName() { return EVENT_ESTIMATE_CREATED; }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Returns the data type for estimate event records
    private AbstractDataProviderType getRecordDataType() { return new FreshBooksEstimateDataType(); }

    #! Returns query parameters with estimate-specific filters
    private hash<auto> getQueryParams() {
        hash<auto> params = FreshBooksWatchDataProviderBase::getQueryParams();
        params."search[updated_since]" = format_date("YYYY-MM-DD HH:mm:SS", last_check);
        return params;
    }

    #! Returns fake example data for estimates
    private hash<auto> getFakeExampleData() {
        return FreshBooksWatchDataProviderBase::getFakeExampleData() + {
            "estimateid": 567890,
            "estimate_number": "EST-0001",
            "customerid": 654321,
            "current_organization": "Acme Corporation",
            "status": 2,
            "create_date": "2026-01-30",
            "currency_code": "USD",
            "amount": {"amount": "1800.00", "code": "USD"},
        };
    }
}

} # namespace FreshBooksDataProvider
