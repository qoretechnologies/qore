# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerDataProvider.qc root BaseLinker data provider

/*  BaseLinkerDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! The root BaseLinker data provider
public class BaseLinkerDataProvider inherits BaseLinkerDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "baselinker",
            "desc": "BaseLinker data provider for managing orders, order returns, inventory, "
                "shipping labels, courier metadata, and polling-based journal events",
            "type": "BaseLinkerDataProvider",
            "supports_children": True,
            "children_can_support_apis": True,
            "children_can_support_records": True,
            "children_can_support_observers": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Child provider map
        const ChildMap = {
            "orders": Class::forName("BaseLinkerDataProvider::BaseLinkerOrdersDataProvider"),
            "order-returns":
                Class::forName("BaseLinkerDataProvider::BaseLinkerOrderReturnsDataProvider"),
            "inventories":
                Class::forName("BaseLinkerDataProvider::BaseLinkerInventoriesDataProvider"),
            "inventory-products":
                Class::forName("BaseLinkerDataProvider::BaseLinkerInventoryProductsDataProvider"),
            "inventory-stock":
                Class::forName("BaseLinkerDataProvider::BaseLinkerInventoryStockDataProvider"),
            "inventory-prices":
                Class::forName("BaseLinkerDataProvider::BaseLinkerInventoryPricesDataProvider"),
            "order-packages":
                Class::forName("BaseLinkerDataProvider::BaseLinkerCourierPackagesDataProvider"),
            "couriers": Class::forName("BaseLinkerDataProvider::BaseLinkerCouriersDataProvider"),
            "courier-services":
                Class::forName("BaseLinkerDataProvider::BaseLinkerCourierServicesDataProvider"),
            "courier-accounts":
                Class::forName("BaseLinkerDataProvider::BaseLinkerCourierAccountsDataProvider"),
            "courier-fields":
                Class::forName("BaseLinkerDataProvider::BaseLinkerCourierFieldsDataProvider"),
            "order-statuses":
                Class::forName("BaseLinkerDataProvider::BaseLinkerOrderStatusesDataProvider"),
            "order-sources":
                Class::forName("BaseLinkerDataProvider::BaseLinkerOrderSourcesDataProvider"),
            "events": Class::forName("BaseLinkerDataProvider::BaseLinkerEventsDataProvider"),
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
        return "baselinker";
    }

    #! Returns the provider description
    *string getDesc() {
        return ProviderInfo.desc;
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

} # namespace BaseLinkerDataProvider
