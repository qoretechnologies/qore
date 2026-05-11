# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KatanaWatchNewMaterialsDataProvider.qc Polling watch for newly created materials

/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

#! Watch provider: fires once per newly created material.
public class KatanaWatchNewMaterialsDataProvider inherits KatanaWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_NEW_MATERIALS,
            "desc": "Fires when a new material is created in Katana.",
            "type": "KatanaWatchNewMaterialsDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(*RestClientIo::RestClientIo rest, *hash<auto> options)
            : KatanaWatchDataProviderBase(rest, options) {
    }

    string getName() { return ProviderInfo.name; }
    string getEndpoint() { return "materials"; }
    string getEventName() { return EVENT_NEW_MATERIALS; }
    *string getTimestampField() { return "created_at"; }

    AbstractDataProviderType getRecordDataType() {
        return new KatanaMaterialDataType();
    }

    list<hash<auto>> fetchRecords(date since) {
        hash<auto> params = {
            "created_at_min": KatanaDataProviderBase::formatUtcIso8601(since),
            "limit": MaxPageSize,
        };
        try {
            return doGetList(getEndpoint(), params);
        } catch (hash<ExceptionInfo> ex) {
            error("Error fetching new materials: %s: %s", ex.err, ex.desc);
            return ();
        }
    }

    hash<auto> getFakeExampleData() {
        return {
            "id": 6001,
            "name": "Steel Sheet 1mm",
            "uom": "m2",
            "category_name": "Metals",
            "created_at": "2026-05-01T13:00:00Z",
            "updated_at": "2026-05-01T13:00:00Z",
        };
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace KatanaDataProvider
