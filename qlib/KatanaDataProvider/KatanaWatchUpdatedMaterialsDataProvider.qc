# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KatanaWatchUpdatedMaterialsDataProvider.qc Polling watch for updated materials

/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

#! Watch provider: fires when a material is updated.
public class KatanaWatchUpdatedMaterialsDataProvider inherits KatanaWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_UPD_MATERIALS,
            "desc": "Fires when a material is updated in Katana.",
            "type": "KatanaWatchUpdatedMaterialsDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(*RestClientIo::RestClientIo rest, *hash<auto> options)
            : KatanaWatchDataProviderBase(rest, options) {
    }

    string getName() { return ProviderInfo.name; }
    string getEndpoint() { return "materials"; }
    string getEventName() { return EVENT_UPD_MATERIALS; }

    AbstractDataProviderType getRecordDataType() {
        return new KatanaMaterialDataType();
    }

    hash<auto> getFakeExampleData() {
        return {
            "id": 6001,
            "name": "Steel Sheet 1mm",
            "uom": "m2",
            "category_name": "Metals",
            "created_at": "2026-05-01T13:00:00Z",
            "updated_at": "2026-05-04T10:00:00Z",
        };
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace KatanaDataProvider
