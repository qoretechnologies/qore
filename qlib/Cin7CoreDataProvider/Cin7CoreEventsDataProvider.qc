# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore Cin7CoreDataProvider module definition

/*  Cin7CoreEventsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Container for Cin7Core event/trigger data providers
public class Cin7CoreEventsDataProvider inherits Cin7CoreDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "events",
            "desc": "Cin7Core events container - polling-based triggers for Cin7Core changes",
            "type": "Cin7CoreEventsDataProvider",
            "supports_children": True,
            "children_can_support_observers": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Child event map
        const ChildMap = {
            EVENT_NEW_PAYMENT: Class::forName("Cin7CoreDataProvider::Cin7CoreNewPaymentWatchDataProvider"),
            EVENT_NEW_SALE_CREDIT_NOTE_AUTHORIZED:
                Class::forName("Cin7CoreDataProvider::Cin7CoreNewSaleCreditNoteWatchDataProvider"),
            EVENT_NEW_SALE_INVOICE_AUTHORIZED:
                Class::forName("Cin7CoreDataProvider::Cin7CoreNewSaleInvoiceWatchDataProvider"),
            EVENT_SALE_TRACKING_CHANGED:
                Class::forName("Cin7CoreDataProvider::Cin7CoreSaleTrackingWatchDataProvider"),
            EVENT_SALE_VOIDED: Class::forName("Cin7CoreDataProvider::Cin7CoreSaleVoidedWatchDataProvider"),
            EVENT_STOCK_LEVEL_UPDATE:
                Class::forName("Cin7CoreDataProvider::Cin7CoreStockLevelUpdateWatchDataProvider"),
            EVENT_CUSTOMER_UPDATED:
                Class::forName("Cin7CoreDataProvider::Cin7CoreCustomerUpdatedWatchDataProvider"),
            EVENT_SUPPLIER_UPDATED:
                Class::forName("Cin7CoreDataProvider::Cin7CoreSupplierUpdatedWatchDataProvider"),
        };
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
        return "events";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Cin7Core events container - polling-based triggers for Cin7Core changes";
    }

    #! Returns a list of child data provider names
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns a child data provider
    private *DataProvider::AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (!cls) {
            return;
        }
        return cls.newObject(rest);
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace Cin7CoreDataProvider
