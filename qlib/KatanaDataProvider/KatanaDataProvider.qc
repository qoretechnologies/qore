# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KatanaDataProvider.qc Root Katana data provider

/*  KatanaDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the KatanaDataProvider module
public namespace KatanaDataProvider {

#! The main Katana data provider class (root provider)
public class KatanaDataProvider inherits KatanaDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "katana",
            "desc": "Katana MRP data provider for managing products, materials, variants, "
                "manufacturing orders, sales orders, purchase orders, inventory, stock "
                "adjustments, customers, suppliers, tax rates, and webhooks. Locations are "
                "read-only.",
            "type": "KatanaDataProvider",
            "supports_children": True,
            "children_can_support_apis": True,
            "children_can_support_records": True,
            "children_can_support_observers": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Child provider map (collection providers + events container)
        const ChildMap = {
            "products": Class::forName("KatanaDataProvider::KatanaProductsDataProvider"),
            "materials": Class::forName("KatanaDataProvider::KatanaMaterialsDataProvider"),
            "variants": Class::forName("KatanaDataProvider::KatanaVariantsDataProvider"),
            "manufacturing-orders":
                Class::forName("KatanaDataProvider::KatanaManufacturingOrdersDataProvider"),
            "sales-orders":
                Class::forName("KatanaDataProvider::KatanaSalesOrdersDataProvider"),
            "sales-order-rows":
                Class::forName("KatanaDataProvider::KatanaSalesOrderRowsDataProvider"),
            "sales-order-addresses":
                Class::forName("KatanaDataProvider::KatanaSalesOrderAddressesDataProvider"),
            "purchase-orders":
                Class::forName("KatanaDataProvider::KatanaPurchaseOrdersDataProvider"),
            "purchase-order-rows":
                Class::forName("KatanaDataProvider::KatanaPurchaseOrderRowsDataProvider"),
            "customers":
                Class::forName("KatanaDataProvider::KatanaCustomersDataProvider"),
            "customer-addresses":
                Class::forName("KatanaDataProvider::KatanaCustomerAddressesDataProvider"),
            "suppliers":
                Class::forName("KatanaDataProvider::KatanaSuppliersDataProvider"),
            "locations":
                Class::forName("KatanaDataProvider::KatanaLocationsDataProvider"),
            "inventory":
                Class::forName("KatanaDataProvider::KatanaInventoryListDataProvider"),
            "stock-adjustments":
                Class::forName("KatanaDataProvider::KatanaStockAdjustmentsDataProvider"),
            "batch-stocks":
                Class::forName("KatanaDataProvider::KatanaBatchStocksDataProvider"),
            "tax-rates":
                Class::forName("KatanaDataProvider::KatanaTaxRatesDataProvider"),
            "webhooks":
                Class::forName("KatanaDataProvider::KatanaWebhooksDataProvider"),
            "events":
                Class::forName("KatanaDataProvider::KatanaEventsDataProvider"),
        };
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : KatanaDataProviderBase(options) {
    }

    #! Creates the object with a REST client (async)
    constructor(RestClientIo::RestClientIo rest) : KatanaDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(KatanaRestClient::KatanaRestConnection conn) : KatanaDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "katana";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Katana MRP data provider for managing products, materials, variants, "
            "manufacturing orders, sales orders, purchase orders, inventory, stock "
            "adjustments, customers, suppliers, tax rates, and webhooks. Locations are "
            "read-only.";
    }

    #! Returns child provider names (sorted for stable listing)
    private *list<string> getChildProviderNamesImpl() {
        return sort(keys ChildMap);
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

} # namespace KatanaDataProvider
