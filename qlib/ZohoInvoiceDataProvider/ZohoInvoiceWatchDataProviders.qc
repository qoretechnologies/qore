# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ZohoInvoiceDataProvider module definition - Watch Providers

/*  ZohoInvoiceWatchDataProviders.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace ZohoInvoiceDataProvider {

#! Watch for new invoices
public class ZohoInvoiceNewInvoiceWatchDataProvider inherits ZohoInvoiceWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "new-invoice",
            "desc": "Watch for new invoices in Zoho Invoice",
            "type": "ZohoInvoiceNewInvoiceWatchDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(ZohoInvoiceRestClient::ZohoInvoiceRestClient rest, *hash<auto> options)
            : ZohoInvoiceWatchDataProviderBase(rest, options) {}

    string getName() { return "new-invoice"; }
    private string getResourcePath() { return "/invoices"; }
    private string getResourceKey() { return "invoices"; }
    private string getIdField() { return "invoice_id"; }
    private string getEventName() { return EVENT_NEW_INVOICE; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

#! Watch for new contacts
public class ZohoInvoiceNewContactWatchDataProvider inherits ZohoInvoiceWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "new-contact",
            "desc": "Watch for new contacts in Zoho Invoice",
            "type": "ZohoInvoiceNewContactWatchDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(ZohoInvoiceRestClient::ZohoInvoiceRestClient rest, *hash<auto> options)
            : ZohoInvoiceWatchDataProviderBase(rest, options) {}

    string getName() { return "new-contact"; }
    private string getResourcePath() { return "/contacts"; }
    private string getResourceKey() { return "contacts"; }
    private string getIdField() { return "contact_id"; }
    private string getEventName() { return EVENT_NEW_CONTACT; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

#! Watch for new estimates
public class ZohoInvoiceNewEstimateWatchDataProvider inherits ZohoInvoiceWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "new-estimate",
            "desc": "Watch for new estimates in Zoho Invoice",
            "type": "ZohoInvoiceNewEstimateWatchDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(ZohoInvoiceRestClient::ZohoInvoiceRestClient rest, *hash<auto> options)
            : ZohoInvoiceWatchDataProviderBase(rest, options) {}

    string getName() { return "new-estimate"; }
    private string getResourcePath() { return "/estimates"; }
    private string getResourceKey() { return "estimates"; }
    private string getIdField() { return "estimate_id"; }
    private string getEventName() { return EVENT_NEW_ESTIMATE; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

#! Watch for new expenses
public class ZohoInvoiceNewExpenseWatchDataProvider inherits ZohoInvoiceWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "new-expense",
            "desc": "Watch for new expenses in Zoho Invoice",
            "type": "ZohoInvoiceNewExpenseWatchDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(ZohoInvoiceRestClient::ZohoInvoiceRestClient rest, *hash<auto> options)
            : ZohoInvoiceWatchDataProviderBase(rest, options) {}

    string getName() { return "new-expense"; }
    private string getResourcePath() { return "/expenses"; }
    private string getResourceKey() { return "expenses"; }
    private string getIdField() { return "expense_id"; }
    private string getEventName() { return EVENT_NEW_EXPENSE; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

#! Watch for new items
public class ZohoInvoiceNewItemWatchDataProvider inherits ZohoInvoiceWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "new-item",
            "desc": "Watch for new items in Zoho Invoice",
            "type": "ZohoInvoiceNewItemWatchDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(ZohoInvoiceRestClient::ZohoInvoiceRestClient rest, *hash<auto> options)
            : ZohoInvoiceWatchDataProviderBase(rest, options) {}

    string getName() { return "new-item"; }
    private string getResourcePath() { return "/items"; }
    private string getResourceKey() { return "items"; }
    private string getIdField() { return "item_id"; }
    private string getEventName() { return EVENT_NEW_ITEM; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

#! Watch for new projects
public class ZohoInvoiceNewProjectWatchDataProvider inherits ZohoInvoiceWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "new-project",
            "desc": "Watch for new projects in Zoho Invoice",
            "type": "ZohoInvoiceNewProjectWatchDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(ZohoInvoiceRestClient::ZohoInvoiceRestClient rest, *hash<auto> options)
            : ZohoInvoiceWatchDataProviderBase(rest, options) {}

    string getName() { return "new-project"; }
    private string getResourcePath() { return "/projects"; }
    private string getResourceKey() { return "projects"; }
    private string getIdField() { return "project_id"; }
    private string getEventName() { return EVENT_NEW_PROJECT; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace ZohoInvoiceDataProvider
