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
        /** Includes both resource/grouping providers AND every DPAT_API action provider as a
            direct child of root. Action paths registered in the action catalog are flat (e.g.
            \c /add-order, \c /set-order-fields) so they must resolve to a child of the root
            provider — without that resolution the framework leaves \c prov as null when
            building the action option schema, and \c ref_data lookups (e.g. order statuses
            for \c order_status_id) never run, leaving the UI dropdown empty. The same
            grouping providers (\c orders, \c inventory-products, etc.) keep their own
            ChildMaps so the action providers remain reachable both flat and nested.
        */
        const ChildMap = {
            # Resource/grouping providers
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
            "inventory-manufacturers":
                Class::forName("BaseLinkerDataProvider::BaseLinkerInventoryManufacturersDataProvider"),
            "inventory-categories":
                Class::forName("BaseLinkerDataProvider::BaseLinkerInventoryCategoriesDataProvider"),
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

            # Order action providers
            "add-order":
                Class::forName("BaseLinkerDataProvider::BaseLinkerAddOrderDataProvider"),
            "set-order-fields":
                Class::forName("BaseLinkerDataProvider::BaseLinkerSetOrderFieldsDataProvider"),
            "set-order-status":
                Class::forName("BaseLinkerDataProvider::BaseLinkerSetOrderStatusDataProvider"),
            "set-order-payment":
                Class::forName("BaseLinkerDataProvider::BaseLinkerSetOrderPaymentDataProvider"),
            "delete-order":
                Class::forName("BaseLinkerDataProvider::BaseLinkerDeleteOrdersDataProvider"),
            "add-order-product":
                Class::forName("BaseLinkerDataProvider::BaseLinkerAddOrderProductDataProvider"),
            "set-order-product-fields":
                Class::forName("BaseLinkerDataProvider::BaseLinkerSetOrderProductFieldsDataProvider"),
            "delete-order-product":
                Class::forName("BaseLinkerDataProvider::BaseLinkerDeleteOrderProductDataProvider"),

            # Order return action providers
            "add-order-return":
                Class::forName("BaseLinkerDataProvider::BaseLinkerAddOrderReturnDataProvider"),
            "set-order-return-fields":
                Class::forName("BaseLinkerDataProvider::BaseLinkerSetOrderReturnFieldsDataProvider"),
            "set-order-return-status":
                Class::forName("BaseLinkerDataProvider::BaseLinkerSetOrderReturnStatusDataProvider"),
            "set-order-return-refund":
                Class::forName("BaseLinkerDataProvider::BaseLinkerSetOrderReturnRefundDataProvider"),

            # Inventory action providers
            "add-inventory-product":
                Class::forName("BaseLinkerDataProvider::BaseLinkerAddInventoryProductDataProvider"),
            "delete-inventory-product":
                Class::forName("BaseLinkerDataProvider::BaseLinkerDeleteInventoryProductDataProvider"),
            "update-inventory-stock":
                Class::forName("BaseLinkerDataProvider::BaseLinkerUpdateInventoryProductsStockDataProvider"),
            "update-inventory-prices":
                Class::forName("BaseLinkerDataProvider::BaseLinkerUpdateInventoryProductsPricesDataProvider"),

            # Courier package action providers
            "add-package":
                Class::forName("BaseLinkerDataProvider::BaseLinkerCreatePackageDataProvider"),
            "get-label":
                Class::forName("BaseLinkerDataProvider::BaseLinkerGetLabelDataProvider"),
            "delete-courier-package":
                Class::forName("BaseLinkerDataProvider::BaseLinkerDeleteCourierPackageDataProvider"),
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
