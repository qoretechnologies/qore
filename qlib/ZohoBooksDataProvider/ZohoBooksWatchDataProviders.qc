# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ZohoBooksDataProvider module definition - Watch Providers

/*  ZohoBooksWatchDataProviders.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace ZohoBooksDataProvider {

#! Watch for new invoices
public class ZohoBooksNewInvoiceWatchDataProvider inherits ZohoBooksWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "new-invoice",
            "desc": "Watch for new invoices in Zoho Books",
            "type": "ZohoBooksNewInvoiceWatchDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(ZohoBooksRestClient::ZohoBooksRestClient rest, *hash<auto> options)
            : ZohoBooksWatchDataProviderBase(rest, options) {}

    string getName() { return "new-invoice"; }
    private string getResourcePath() { return "/invoices"; }
    private string getResourceKey() { return "invoices"; }
    private string getIdField() { return "invoice_id"; }
    private string getEventName() { return EVENT_NEW_INVOICE; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

#! Watch for new bills
public class ZohoBooksNewBillWatchDataProvider inherits ZohoBooksWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "new-bill",
            "desc": "Watch for new bills in Zoho Books",
            "type": "ZohoBooksNewBillWatchDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(ZohoBooksRestClient::ZohoBooksRestClient rest, *hash<auto> options)
            : ZohoBooksWatchDataProviderBase(rest, options) {}

    string getName() { return "new-bill"; }
    private string getResourcePath() { return "/bills"; }
    private string getResourceKey() { return "bills"; }
    private string getIdField() { return "bill_id"; }
    private string getEventName() { return EVENT_NEW_BILL; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

#! Watch for new payments
public class ZohoBooksNewPaymentWatchDataProvider inherits ZohoBooksWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "new-payment",
            "desc": "Watch for new payments in Zoho Books",
            "type": "ZohoBooksNewPaymentWatchDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(ZohoBooksRestClient::ZohoBooksRestClient rest, *hash<auto> options)
            : ZohoBooksWatchDataProviderBase(rest, options) {}

    string getName() { return "new-payment"; }
    private string getResourcePath() { return "/customerpayments"; }
    private string getResourceKey() { return "customerpayments"; }
    private string getIdField() { return "payment_id"; }
    private string getEventName() { return EVENT_NEW_PAYMENT; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

#! Watch for new customers
public class ZohoBooksNewCustomerWatchDataProvider inherits ZohoBooksWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "new-customer",
            "desc": "Watch for new customers in Zoho Books",
            "type": "ZohoBooksNewCustomerWatchDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(ZohoBooksRestClient::ZohoBooksRestClient rest, *hash<auto> options)
            : ZohoBooksWatchDataProviderBase(rest, options) {}

    string getName() { return "new-customer"; }
    private string getResourcePath() { return "/contacts"; }
    private string getResourceKey() { return "contacts"; }
    private string getIdField() { return "contact_id"; }
    private string getEventName() { return EVENT_NEW_CUSTOMER; }

    private hash<auto> getQueryParams() {
        return ZohoBooksWatchDataProviderBase::getQueryParams() + {"contact_type": "customer"};
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

#! Watch for new expenses
public class ZohoBooksNewExpenseWatchDataProvider inherits ZohoBooksWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "new-expense",
            "desc": "Watch for new expenses in Zoho Books",
            "type": "ZohoBooksNewExpenseWatchDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(ZohoBooksRestClient::ZohoBooksRestClient rest, *hash<auto> options)
            : ZohoBooksWatchDataProviderBase(rest, options) {}

    string getName() { return "new-expense"; }
    private string getResourcePath() { return "/expenses"; }
    private string getResourceKey() { return "expenses"; }
    private string getIdField() { return "expense_id"; }
    private string getEventName() { return EVENT_NEW_EXPENSE; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

#! Watch for new sales orders
public class ZohoBooksNewSalesOrderWatchDataProvider inherits ZohoBooksWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "new-salesorder",
            "desc": "Watch for new sales orders in Zoho Books",
            "type": "ZohoBooksNewSalesOrderWatchDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(ZohoBooksRestClient::ZohoBooksRestClient rest, *hash<auto> options)
            : ZohoBooksWatchDataProviderBase(rest, options) {}

    string getName() { return "new-salesorder"; }
    private string getResourcePath() { return "/salesorders"; }
    private string getResourceKey() { return "salesorders"; }
    private string getIdField() { return "salesorder_id"; }
    private string getEventName() { return EVENT_NEW_SALESORDER; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

#! Watch for new estimates
public class ZohoBooksNewEstimateWatchDataProvider inherits ZohoBooksWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "new-estimate",
            "desc": "Watch for new estimates in Zoho Books",
            "type": "ZohoBooksNewEstimateWatchDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(ZohoBooksRestClient::ZohoBooksRestClient rest, *hash<auto> options)
            : ZohoBooksWatchDataProviderBase(rest, options) {}

    string getName() { return "new-estimate"; }
    private string getResourcePath() { return "/estimates"; }
    private string getResourceKey() { return "estimates"; }
    private string getIdField() { return "estimate_id"; }
    private string getEventName() { return EVENT_NEW_ESTIMATE; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

#! Watch for new items
public class ZohoBooksNewItemWatchDataProvider inherits ZohoBooksWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "new-item",
            "desc": "Watch for new items in Zoho Books",
            "type": "ZohoBooksNewItemWatchDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(ZohoBooksRestClient::ZohoBooksRestClient rest, *hash<auto> options)
            : ZohoBooksWatchDataProviderBase(rest, options) {}

    string getName() { return "new-item"; }
    private string getResourcePath() { return "/items"; }
    private string getResourceKey() { return "items"; }
    private string getIdField() { return "item_id"; }
    private string getEventName() { return EVENT_NEW_ITEM; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

#! Watch for new purchase orders
public class ZohoBooksNewPurchaseOrderWatchDataProvider inherits ZohoBooksWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "new-purchaseorder",
            "desc": "Watch for new purchase orders in Zoho Books",
            "type": "ZohoBooksNewPurchaseOrderWatchDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(ZohoBooksRestClient::ZohoBooksRestClient rest, *hash<auto> options)
            : ZohoBooksWatchDataProviderBase(rest, options) {}

    string getName() { return "new-purchaseorder"; }
    private string getResourcePath() { return "/purchaseorders"; }
    private string getResourceKey() { return "purchaseorders"; }
    private string getIdField() { return "purchaseorder_id"; }
    private string getEventName() { return EVENT_NEW_PURCHASEORDER; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

#! Watch for new vendors
public class ZohoBooksNewVendorWatchDataProvider inherits ZohoBooksWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "new-vendor",
            "desc": "Watch for new vendors in Zoho Books",
            "type": "ZohoBooksNewVendorWatchDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(ZohoBooksRestClient::ZohoBooksRestClient rest, *hash<auto> options)
            : ZohoBooksWatchDataProviderBase(rest, options) {}

    string getName() { return "new-vendor"; }
    private string getResourcePath() { return "/contacts"; }
    private string getResourceKey() { return "contacts"; }
    private string getIdField() { return "contact_id"; }
    private string getEventName() { return EVENT_NEW_VENDOR; }

    private hash<auto> getQueryParams() {
        return ZohoBooksWatchDataProviderBase::getQueryParams() + {"contact_type": "vendor"};
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace ZohoBooksDataProvider
