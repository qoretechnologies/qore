# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KatanaWatchUpdatedManufacturingOrdersDataProvider.qc Polling watch for updated manufacturing orders

/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

#! Watch provider: fires when a manufacturing order is updated.
public class KatanaWatchUpdatedManufacturingOrdersDataProvider
        inherits KatanaWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_UPD_MOS,
            "desc": "Fires when a manufacturing order is updated in Katana.",
            "type": "KatanaWatchUpdatedManufacturingOrdersDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(*RestClientIo::RestClientIo rest, *hash<auto> options)
            : KatanaWatchDataProviderBase(rest, options) {
    }

    string getName() { return ProviderInfo.name; }
    string getEndpoint() { return "manufacturing_orders"; }
    string getEventName() { return EVENT_UPD_MOS; }

    AbstractDataProviderType getRecordDataType() {
        return new KatanaManufacturingOrderDataType();
    }

    hash<auto> getFakeExampleData() {
        return {
            "id": 7001,
            "order_no": "MO-0001",
            "variant_id": 555,
            "status": "IN_PROGRESS",
            "quantity": 100,
            "created_at": "2026-05-01T14:00:00Z",
            "updated_at": "2026-05-05T11:30:00Z",
        };
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace KatanaDataProvider
