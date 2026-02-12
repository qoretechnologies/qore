# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ZohoInventoryDataProvider module definition

/*  ZohoInventoryDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ZohoInventoryDataProvider module
public namespace ZohoInventoryDataProvider {

#! The main Zoho Inventory data provider class (root provider)
public class ZohoInventoryDataProvider inherits ZohoInventoryDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "zohoinventory",
            "desc": "Zoho Inventory data provider for inventory and order management",
            "type": "ZohoInventoryDataProvider",
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
            # Collection providers
            "contacts": Class::forName("ZohoInventoryContactsDataProvider"),
            "contactpersons": Class::forName("ZohoInventoryContactPersonsDataProvider"),
            "items": Class::forName("ZohoInventoryItemsDataProvider"),
            "itemgroups": Class::forName("ZohoInventoryItemGroupsDataProvider"),
            "inventoryadjustments": Class::forName("ZohoInventoryItemAdjustmentsDataProvider"),
            "packages": Class::forName("ZohoInventoryPackagesDataProvider"),
            "purchaseorders": Class::forName("ZohoInventoryPurchaseOrdersDataProvider"),
            "salesorders": Class::forName("ZohoInventorySalesOrdersDataProvider"),
            "invoices": Class::forName("ZohoInventoryInvoicesDataProvider"),
            "events": Class::forName("ZohoInventoryEventsDataProvider"),
            # CRUD providers
            "create-invoice": Class::forName("ZohoInventoryCreateInvoiceDataProvider"),
            "create-contact": Class::forName("ZohoInventoryCreateContactDataProvider"),
            "create-contact-person": Class::forName("ZohoInventoryCreateContactPersonDataProvider"),
            "create-item": Class::forName("ZohoInventoryCreateItemDataProvider"),
            "create-item-group": Class::forName("ZohoInventoryCreateItemGroupDataProvider"),
            "create-item-adjustment": Class::forName("ZohoInventoryCreateItemAdjustmentDataProvider"),
            "create-package": Class::forName("ZohoInventoryCreatePackageDataProvider"),
            "create-purchase-order": Class::forName("ZohoInventoryCreatePurchaseOrderDataProvider"),
            "create-sales-order": Class::forName("ZohoInventoryCreateSalesOrderDataProvider"),
            "create-shipment-order": Class::forName("ZohoInventoryCreateShipmentOrderDataProvider"),
            "get-invoice": Class::forName("ZohoInventoryGetInvoiceDataProvider"),
            "get-contact": Class::forName("ZohoInventoryGetContactDataProvider"),
            "get-contact-person": Class::forName("ZohoInventoryGetContactPersonDataProvider"),
            "get-item": Class::forName("ZohoInventoryGetItemDataProvider"),
            "get-item-group": Class::forName("ZohoInventoryGetItemGroupDataProvider"),
            "get-item-adjustment": Class::forName("ZohoInventoryGetItemAdjustmentDataProvider"),
            "get-package": Class::forName("ZohoInventoryGetPackageDataProvider"),
            "get-purchase-order": Class::forName("ZohoInventoryGetPurchaseOrderDataProvider"),
            "get-sales-order": Class::forName("ZohoInventoryGetSalesOrderDataProvider"),
            "get-shipment-order": Class::forName("ZohoInventoryGetShipmentOrderDataProvider"),
            "update-invoice": Class::forName("ZohoInventoryUpdateInvoiceDataProvider"),
            "update-contact": Class::forName("ZohoInventoryUpdateContactDataProvider"),
            "update-contact-person": Class::forName("ZohoInventoryUpdateContactPersonDataProvider"),
            "update-item": Class::forName("ZohoInventoryUpdateItemDataProvider"),
            "update-item-group": Class::forName("ZohoInventoryUpdateItemGroupDataProvider"),
            "update-package": Class::forName("ZohoInventoryUpdatePackageDataProvider"),
            "update-purchase-order": Class::forName("ZohoInventoryUpdatePurchaseOrderDataProvider"),
            "update-sales-order": Class::forName("ZohoInventoryUpdateSalesOrderDataProvider"),
            "delete-invoice": Class::forName("ZohoInventoryDeleteInvoiceDataProvider"),
            "delete-contact": Class::forName("ZohoInventoryDeleteContactDataProvider"),
            "delete-contact-person": Class::forName("ZohoInventoryDeleteContactPersonDataProvider"),
            "delete-item-group": Class::forName("ZohoInventoryDeleteItemGroupDataProvider"),
            "delete-item-adjustment": Class::forName("ZohoInventoryDeleteItemAdjustmentDataProvider"),
            "delete-package": Class::forName("ZohoInventoryDeletePackageDataProvider"),
            "delete-item": Class::forName("ZohoInventoryDeleteItemDataProvider"),
            "delete-purchase-order": Class::forName("ZohoInventoryDeletePurchaseOrderDataProvider"),
            "delete-sales-order": Class::forName("ZohoInventoryDeleteSalesOrderDataProvider"),
            "delete-shipment-order": Class::forName("ZohoInventoryDeleteShipmentOrderDataProvider"),
            "update-shipment-order": Class::forName("ZohoInventoryUpdateShipmentOrderDataProvider"),
            "shipmentorders": Class::forName("ZohoInventoryShipmentOrdersDataProvider"),
            # Special actions
            "mark-contact-active": Class::forName("ZohoInventoryMarkContactActiveDataProvider"),
            "mark-contact-person-primary": Class::forName("ZohoInventoryMarkContactPersonPrimaryDataProvider"),
            "mark-item-active": Class::forName("ZohoInventoryMarkItemActiveDataProvider"),
            "mark-shipment-delivered": Class::forName("ZohoInventoryMarkShipmentDeliveredDataProvider"),
            "api-call": Class::forName("ZohoInventoryApiCallDataProvider"),
        };
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : ZohoInventoryDataProviderBase(options) {
    }

    #! Creates the object with a REST client
    constructor(ZohoInventoryRestClient::ZohoInventoryRestClient rest) : ZohoInventoryDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(ZohoInventoryRestClient::ZohoInventoryRestConnection conn) : ZohoInventoryDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "zohoinventory";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Zoho Inventory data provider for inventory and order management";
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

} # namespace ZohoInventoryDataProvider
