# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerEventsDataProvider.qc container for BaseLinker polling-based event providers

/*  BaseLinkerEventsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the BaseLinkerDataProvider module
public namespace BaseLinkerDataProvider {

#! Container data provider grouping the 22 BaseLinker polling watch providers
/** BaseLinker has no webhook API; all events are derived by polling
    `getJournalList`. This container exposes one child per `log_type` code
    (1..22) — each child is a dedicated watch provider.
*/
public class BaseLinkerEventsDataProvider inherits BaseLinkerDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "events",
            "desc": "BaseLinker events container — polling-based triggers derived from "
                "`getJournalList`. Each child provider watches a single `log_type` code.",
            "type": "BaseLinkerEventsDataProvider",
            "supports_children": True,
            "children_can_support_observers": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Child event-name → watch class map
        const ChildMap = {
            EVENT_ORDER_CREATED:
                Class::forName("BaseLinkerDataProvider::BaseLinkerWatchOrderCreatedDataProvider"),
            EVENT_ORDER_DOWNLOADED:
                Class::forName("BaseLinkerDataProvider::BaseLinkerWatchOrderDownloadedDataProvider"),
            EVENT_ORDER_PAID:
                Class::forName("BaseLinkerDataProvider::BaseLinkerWatchOrderPaidDataProvider"),
            EVENT_ORDER_REMOVED:
                Class::forName("BaseLinkerDataProvider::BaseLinkerWatchOrderRemovedDataProvider"),
            EVENT_ORDERS_MERGED:
                Class::forName("BaseLinkerDataProvider::BaseLinkerWatchOrdersMergedDataProvider"),
            EVENT_ORDER_SPLIT:
                Class::forName("BaseLinkerDataProvider::BaseLinkerWatchOrderSplitDataProvider"),
            EVENT_INVOICE_ISSUED:
                Class::forName("BaseLinkerDataProvider::BaseLinkerWatchInvoiceIssuedDataProvider"),
            EVENT_RECEIPT_ISSUED:
                Class::forName("BaseLinkerDataProvider::BaseLinkerWatchReceiptIssuedDataProvider"),
            EVENT_PACKAGE_CREATED:
                Class::forName("BaseLinkerDataProvider::BaseLinkerWatchPackageCreatedDataProvider"),
            EVENT_PACKAGE_DELETED:
                Class::forName("BaseLinkerDataProvider::BaseLinkerWatchPackageDeletedDataProvider"),
            EVENT_DELIVERY_DATA_EDITED:
                Class::forName("BaseLinkerDataProvider::BaseLinkerWatchDeliveryDataEditedDataProvider"),
            EVENT_ORDER_PRODUCT_ADDED:
                Class::forName("BaseLinkerDataProvider::BaseLinkerWatchOrderProductAddedDataProvider"),
            EVENT_ORDER_PRODUCT_EDITED:
                Class::forName("BaseLinkerDataProvider::BaseLinkerWatchOrderProductEditedDataProvider"),
            EVENT_ORDER_PRODUCT_REMOVED:
                Class::forName("BaseLinkerDataProvider::BaseLinkerWatchOrderProductRemovedDataProvider"),
            EVENT_BLACKLIST_ADDED:
                Class::forName("BaseLinkerDataProvider::BaseLinkerWatchBlacklistAddedDataProvider"),
            EVENT_ORDER_DATA_EDITED:
                Class::forName("BaseLinkerDataProvider::BaseLinkerWatchOrderDataEditedDataProvider"),
            EVENT_ORDER_COPIED:
                Class::forName("BaseLinkerDataProvider::BaseLinkerWatchOrderCopiedDataProvider"),
            EVENT_ORDER_STATUS_CHANGED:
                Class::forName("BaseLinkerDataProvider::BaseLinkerWatchOrderStatusChangedDataProvider"),
            EVENT_INVOICE_DELETED:
                Class::forName("BaseLinkerDataProvider::BaseLinkerWatchInvoiceDeletedDataProvider"),
            EVENT_RECEIPT_DELETED:
                Class::forName("BaseLinkerDataProvider::BaseLinkerWatchReceiptDeletedDataProvider"),
            EVENT_INVOICE_EDITED:
                Class::forName("BaseLinkerDataProvider::BaseLinkerWatchInvoiceEditedDataProvider"),
            EVENT_PACKAGE_STATUS_CHANGED:
                Class::forName("BaseLinkerDataProvider::BaseLinkerWatchPackageStatusChangedDataProvider"),
        };
    }

    #! Creates the object with an async REST client
    constructor(*RestClientIo::RestClientIo rest) : BaseLinkerDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(BaseLinkerRestClient::BaseLinkerRestConnection conn)
            : BaseLinkerDataProviderBase(conn) {
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : BaseLinkerDataProviderBase(options) {
    }

    #! Returns the provider name
    string getName() {
        return "events";
    }

    #! Returns the provider description
    *string getDesc() {
        return ProviderInfo.desc;
    }

    #! Returns the child provider names
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns a child data provider by name
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

} # namespace BaseLinkerDataProvider
