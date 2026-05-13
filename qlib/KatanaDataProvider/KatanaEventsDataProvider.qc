# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KatanaEventsDataProvider.qc Container provider for all Katana event/watch providers

/*  KatanaEventsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License - see COPYING for details
*/

#! Contains all public definitions in the KatanaDataProvider module
public namespace KatanaDataProvider {

#! Container data provider grouping all Katana event/watch providers.
/** Children include both native-webhook-style events (polled and filtered by status)
    and generic polling watch providers (new/updated record streams).
*/
public class KatanaEventsDataProvider inherits KatanaDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "events",
            "desc": "Container for Katana event/watch providers. Native Katana webhook event "
                "types and generic new/updated record streams are exposed as children.",
            "type": "KatanaEventsDataProvider",
            "supports_children": True,
            "children_can_support_observers": True,
            "constructor_options": BaseConstructorOptions,
        };

        #! Child event-name → watch class map (30 entries)
        const ChildMap = {
            # Native webhook-style events (14)
            EVENT_SO_CREATED:
                Class::forName("KatanaDataProvider::KatanaWatchSalesOrderCreatedDataProvider"),
            EVENT_SO_UPDATED:
                Class::forName("KatanaDataProvider::KatanaWatchSalesOrderUpdatedDataProvider"),
            EVENT_SO_DELETED:
                Class::forName("KatanaDataProvider::KatanaWatchSalesOrderDeletedDataProvider"),
            EVENT_SO_PACKED:
                Class::forName("KatanaDataProvider::KatanaWatchSalesOrderPackedDataProvider"),
            EVENT_SO_DELIVERED:
                Class::forName("KatanaDataProvider::KatanaWatchSalesOrderDeliveredDataProvider"),
            EVENT_PO_CREATED:
                Class::forName("KatanaDataProvider::KatanaWatchPurchaseOrderCreatedDataProvider"),
            EVENT_PO_UPDATED:
                Class::forName("KatanaDataProvider::KatanaWatchPurchaseOrderUpdatedDataProvider"),
            EVENT_PO_DELETED:
                Class::forName("KatanaDataProvider::KatanaWatchPurchaseOrderDeletedDataProvider"),
            EVENT_PO_PARTIAL:
                Class::forName("KatanaDataProvider::KatanaWatchPurchaseOrderPartiallyReceivedDataProvider"),
            EVENT_PO_RECEIVED:
                Class::forName("KatanaDataProvider::KatanaWatchPurchaseOrderReceivedDataProvider"),
            EVENT_POR_CREATED:
                Class::forName("KatanaDataProvider::KatanaWatchPurchaseOrderRowCreatedDataProvider"),
            EVENT_POR_UPDATED:
                Class::forName("KatanaDataProvider::KatanaWatchPurchaseOrderRowUpdatedDataProvider"),
            EVENT_POR_DELETED:
                Class::forName("KatanaDataProvider::KatanaWatchPurchaseOrderRowDeletedDataProvider"),
            EVENT_POR_RECEIVED:
                Class::forName("KatanaDataProvider::KatanaWatchPurchaseOrderRowReceivedDataProvider"),

            # Generic polling watch providers (16)
            EVENT_NEW_PRODUCTS:
                Class::forName("KatanaDataProvider::KatanaWatchNewProductsDataProvider"),
            EVENT_UPD_PRODUCTS:
                Class::forName("KatanaDataProvider::KatanaWatchUpdatedProductsDataProvider"),
            EVENT_NEW_MATERIALS:
                Class::forName("KatanaDataProvider::KatanaWatchNewMaterialsDataProvider"),
            EVENT_UPD_MATERIALS:
                Class::forName("KatanaDataProvider::KatanaWatchUpdatedMaterialsDataProvider"),
            EVENT_NEW_MOS:
                Class::forName("KatanaDataProvider::KatanaWatchNewManufacturingOrdersDataProvider"),
            EVENT_UPD_MOS:
                Class::forName("KatanaDataProvider::KatanaWatchUpdatedManufacturingOrdersDataProvider"),
            EVENT_NEW_SOS:
                Class::forName("KatanaDataProvider::KatanaWatchNewSalesOrdersDataProvider"),
            EVENT_UPD_SOS:
                Class::forName("KatanaDataProvider::KatanaWatchUpdatedSalesOrdersDataProvider"),
            EVENT_NEW_POS:
                Class::forName("KatanaDataProvider::KatanaWatchNewPurchaseOrdersDataProvider"),
            EVENT_UPD_POS:
                Class::forName("KatanaDataProvider::KatanaWatchUpdatedPurchaseOrdersDataProvider"),
            EVENT_NEW_CUSTOMERS:
                Class::forName("KatanaDataProvider::KatanaWatchNewCustomersDataProvider"),
            EVENT_UPD_CUSTOMERS:
                Class::forName("KatanaDataProvider::KatanaWatchUpdatedCustomersDataProvider"),
            EVENT_NEW_SUPPLIERS:
                Class::forName("KatanaDataProvider::KatanaWatchNewSuppliersDataProvider"),
            EVENT_UPD_SUPPLIERS:
                Class::forName("KatanaDataProvider::KatanaWatchUpdatedSuppliersDataProvider"),
            EVENT_INV_UPDATED:
                Class::forName("KatanaDataProvider::KatanaWatchInventoryUpdatedDataProvider"),
            EVENT_NEW_STOCK_ADJ:
                Class::forName("KatanaDataProvider::KatanaWatchNewStockAdjustmentsDataProvider"),
        };
    }

    #! Creates the object with an async REST client
    constructor(*RestClientIo::RestClientIo rest) : KatanaDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(KatanaRestClient::KatanaRestConnection conn) : KatanaDataProviderBase(conn) {
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : KatanaDataProviderBase(options) {
    }

    #! Returns the provider name
    string getName() {
        return "events";
    }

    #! Returns the provider description
    *string getDesc() {
        return ProviderInfo.desc;
    }

    #! Returns the list of event types supported by this container
    private *hash<string, hash<DataProviderMessageInfo>> getEventTypesImpl() {
        hash<string, hash<DataProviderMessageInfo>> rv = {};
        foreach string ev in (keys ChildMap) {
            rv{ev} = <DataProviderMessageInfo>{
                "desc": sprintf("Fired when a %s event is detected", ev),
            };
        }
        return rv;
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

} # namespace KatanaDataProvider
