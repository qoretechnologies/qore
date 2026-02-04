# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShipStationDataProvider module definition

/*  ShipStationDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ShipStationDataProvider module
public namespace ShipStationDataProvider {

#! The main ShipStation data provider class (root provider)
public class ShipStationDataProvider inherits ShipStationDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "shipstation",
            "desc": "ShipStation data provider for shipping and fulfillment management",
            "type": "ShipStationDataProvider",
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
            # Collection/List providers
            "orders": Class::forName("ShipStationOrdersDataProvider"),
            "carriers": Class::forName("ShipStationCarriersDataProvider"),
            "customers": Class::forName("ShipStationCustomersDataProvider"),
            "products": Class::forName("ShipStationProductsDataProvider"),
            "shipments": Class::forName("ShipStationShipmentsDataProvider"),
            "fulfillments": Class::forName("ShipStationFulfillmentsDataProvider"),
            "stores": Class::forName("ShipStationStoresDataProvider"),
            "warehouses": Class::forName("ShipStationWarehousesDataProvider"),
            "users": Class::forName("ShipStationUsersDataProvider"),
            "tags": Class::forName("ShipStationTagsDataProvider"),
            "marketplaces": Class::forName("ShipStationMarketplacesDataProvider"),

            # Action providers - Orders
            "get-order": Class::forName("ShipStationGetOrderDataProvider"),
            "create-order": Class::forName("ShipStationCreateOrderDataProvider"),
            "update-order": Class::forName("ShipStationUpdateOrderDataProvider"),
            "delete-order": Class::forName("ShipStationDeleteOrderDataProvider"),
            "mark-order-as-shipped": Class::forName("ShipStationMarkAsShippedDataProvider"),
            "add-tag-to-order": Class::forName("ShipStationAddTagToOrderDataProvider"),
            "remove-tag-from-order": Class::forName("ShipStationRemoveTagFromOrderDataProvider"),
            "assign-user-to-order": Class::forName("ShipStationAssignUserToOrderDataProvider"),
            "unassign-user-from-order": Class::forName("ShipStationUnassignUserFromOrderDataProvider"),

            # Action providers - Carriers
            "get-carrier": Class::forName("ShipStationGetCarrierDataProvider"),
            "list-services": Class::forName("ShipStationListServicesDataProvider"),
            "list-packages": Class::forName("ShipStationListPackagesDataProvider"),
            "add-funds": Class::forName("ShipStationAddFundsDataProvider"),

            # Action providers - Customers
            "get-customer": Class::forName("ShipStationGetCustomerDataProvider"),

            # Action providers - Products
            "get-product": Class::forName("ShipStationGetProductDataProvider"),
            "update-product": Class::forName("ShipStationUpdateProductDataProvider"),

            # Action providers - Shipments
            "void-label": Class::forName("ShipStationVoidLabelDataProvider"),

            # Action providers - Stores
            "get-store": Class::forName("ShipStationGetStoreDataProvider"),
            "update-store": Class::forName("ShipStationUpdateStoreDataProvider"),
            "deactivate-store": Class::forName("ShipStationDeactivateStoreDataProvider"),
            "reactivate-store": Class::forName("ShipStationReactivateStoreDataProvider"),
            "refresh-store": Class::forName("ShipStationRefreshStoreDataProvider"),

            # Action providers - Warehouses
            "get-warehouse": Class::forName("ShipStationGetWarehouseDataProvider"),
            "create-warehouse": Class::forName("ShipStationCreateWarehouseDataProvider"),
            "update-warehouse": Class::forName("ShipStationUpdateWarehouseDataProvider"),

            # Utility providers
            "make-api-call": Class::forName("ShipStationMakeApiCallDataProvider"),

            # Event providers
            "events": Class::forName("ShipStationEventsDataProvider"),
        };
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : ShipStationDataProviderBase(options) {
    }

    #! Creates the object with a REST client
    constructor(ShipStationRestClient::ShipStationRestClient rest)
            : ShipStationDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(ShipStationRestClient::ShipStationRestConnection conn) : ShipStationDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "shipstation";
    }

    #! Returns the provider description
    *string getDesc() {
        return "ShipStation data provider for shipping and fulfillment management";
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

} # namespace ShipStationDataProvider
