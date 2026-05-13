# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KatanaWatchUpdatedSuppliersDataProvider.qc Polling watch for updated suppliers

/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

#! Watch provider: fires when a supplier is updated.
public class KatanaWatchUpdatedSuppliersDataProvider inherits KatanaWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_UPD_SUPPLIERS,
            "desc": "Fires when a supplier is updated in Katana.",
            "type": "KatanaWatchUpdatedSuppliersDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(*RestClientIo::RestClientIo rest, *hash<auto> options)
            : KatanaWatchDataProviderBase(rest, options) {
    }

    string getName() { return ProviderInfo.name; }
    string getEndpoint() { return "suppliers"; }
    string getEventName() { return EVENT_UPD_SUPPLIERS; }

    AbstractDataProviderType getRecordDataType() {
        return new KatanaSupplierDataType();
    }

    hash<auto> getFakeExampleData() {
        return {
            "id": 7,
            "name": "Best Materials Co",
            "email": "sales@bestmaterials.example",
            "phone": "+1-555-0200",
            "currency": "USD",
            "created_at": "2026-05-01T08:00:00Z",
            "updated_at": "2026-05-04T10:00:00Z",
        };
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace KatanaDataProvider
