# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ZohoBooksDataProvider module definition

/*  ZohoBooksEventsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ZohoBooksDataProvider module
public namespace ZohoBooksDataProvider {

#! Zoho Books events data provider - parent for all polling triggers
public class ZohoBooksEventsDataProvider inherits ZohoBooksDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "events",
            "desc": "Zoho Books events/triggers data provider",
            "type": "ZohoBooksEventsDataProvider",
            "supports_children": True,
            "children_can_support_observers": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Child provider map for triggers
        const ChildMap = {
            "new-invoice": Class::forName("ZohoBooksNewInvoiceWatchDataProvider"),
            "new-bill": Class::forName("ZohoBooksNewBillWatchDataProvider"),
            "new-payment": Class::forName("ZohoBooksNewPaymentWatchDataProvider"),
            "new-customer": Class::forName("ZohoBooksNewCustomerWatchDataProvider"),
            "new-expense": Class::forName("ZohoBooksNewExpenseWatchDataProvider"),
            "new-salesorder": Class::forName("ZohoBooksNewSalesOrderWatchDataProvider"),
            "new-estimate": Class::forName("ZohoBooksNewEstimateWatchDataProvider"),
            "new-item": Class::forName("ZohoBooksNewItemWatchDataProvider"),
            "new-purchaseorder": Class::forName("ZohoBooksNewPurchaseOrderWatchDataProvider"),
            "new-vendor": Class::forName("ZohoBooksNewVendorWatchDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(ZohoBooksRestClient::ZohoBooksRestClient rest) : ZohoBooksDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "events";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Zoho Books events/triggers data provider for polling-based event detection";
    }

    #! Returns child provider names
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns a child provider by name
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) {
            return cls.newObject(rest);
        }
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ZohoBooksDataProvider
