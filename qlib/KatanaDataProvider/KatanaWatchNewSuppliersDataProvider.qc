# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KatanaWatchNewSuppliersDataProvider.qc Polling watch for newly created suppliers

/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

#! Watch provider: fires once per newly created supplier.
public class KatanaWatchNewSuppliersDataProvider inherits KatanaWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_NEW_SUPPLIERS,
            "desc": "Fires when a new supplier is created in Katana.",
            "type": "KatanaWatchNewSuppliersDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(*RestClientIo::RestClientIo rest, *hash<auto> options)
            : KatanaWatchDataProviderBase(rest, options) {
    }

    string getName() { return ProviderInfo.name; }
    string getEndpoint() { return "suppliers"; }
    string getEventName() { return EVENT_NEW_SUPPLIERS; }
    *string getTimestampField() { return "created_at"; }

    AbstractDataProviderType getRecordDataType() {
        return new KatanaSupplierDataType();
    }

    list<hash<auto>> fetchRecords(date since) {
        hash<auto> params = {
            "created_at_min": KatanaDataProviderBase::formatUtcIso8601(since),
            "limit": MaxPageSize,
        };
        try {
            return doGetList(getEndpoint(), params);
        } catch (hash<ExceptionInfo> ex) {
            LoggerWrapper::error(sprintf("Error fetching new suppliers: %s: %s", ex.err, ex.desc));
            return ();
        }
    }

    hash<auto> getFakeExampleData() {
        return {
            "id": 7,
            "name": "Best Materials Co",
            "email": "sales@bestmaterials.example",
            "phone": "+1-555-0200",
            "currency": "USD",
            "created_at": "2026-05-01T08:00:00Z",
            "updated_at": "2026-05-01T08:00:00Z",
        };
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace KatanaDataProvider
