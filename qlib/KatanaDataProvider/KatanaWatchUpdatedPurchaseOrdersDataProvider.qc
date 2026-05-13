# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KatanaWatchUpdatedPurchaseOrdersDataProvider.qc Polling watch for updated purchase orders

/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

#! Watch provider: fires when a purchase order is updated (generic stream).
public class KatanaWatchUpdatedPurchaseOrdersDataProvider inherits KatanaWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_UPD_POS,
            "desc": "Fires when a purchase order is updated in Katana (generic stream).",
            "type": "KatanaWatchUpdatedPurchaseOrdersDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(*RestClientIo::RestClientIo rest, *hash<auto> options)
            : KatanaWatchDataProviderBase(rest, options) {
    }

    string getName() { return ProviderInfo.name; }
    string getEndpoint() { return "purchase_orders"; }
    string getEventName() { return EVENT_UPD_POS; }

    AbstractDataProviderType getRecordDataType() {
        return new KatanaPurchaseOrderDataType();
    }

    hash<auto> getFakeExampleData() {
        return {
            "id": 2001,
            "order_no": "PO-0001",
            "supplier_id": 7,
            "status": "PARTIALLY_RECEIVED",
            "currency": "USD",
            "total": 5000.00,
            "created_at": "2026-05-01T11:00:00Z",
            "updated_at": "2026-05-03T09:15:00Z",
        };
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace KatanaDataProvider
