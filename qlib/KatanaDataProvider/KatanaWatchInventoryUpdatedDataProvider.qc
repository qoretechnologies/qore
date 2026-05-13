# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KatanaWatchInventoryUpdatedDataProvider.qc Polling watch for inventory updates

/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

#! Watch provider: fires when an inventory record is updated.
/** Inventory rows are identified by the composite (\c variant_id, \c location_id);
    this watcher uses that pair as the dedupe ID via \c getIdField() override.
*/
public class KatanaWatchInventoryUpdatedDataProvider inherits KatanaWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_INV_UPDATED,
            "desc": "Fires when an inventory record (variant/location stock state) is updated "
                "in Katana.",
            "type": "KatanaWatchInventoryUpdatedDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(*RestClientIo::RestClientIo rest, *hash<auto> options)
            : KatanaWatchDataProviderBase(rest, options) {
    }

    string getName() { return ProviderInfo.name; }
    string getEndpoint() { return "inventory"; }
    string getEventName() { return EVENT_INV_UPDATED; }

    AbstractDataProviderType getRecordDataType() {
        return new KatanaInventoryDataType();
    }

    #! Inventory records don't have a single ID — synthesize one from variant+location.
    private hash<auto> tagRecord(hash<auto> r) {
        if (!r.hasKey("id")) {
            r.id = sprintf("%s:%s", r.variant_id ?? "?", r.location_id ?? "?");
        }
        return r;
    }

    list<hash<auto>> fetchRecords(date since) {
        hash<auto> params = {
            "updated_at_min": KatanaDataProviderBase::formatUtcIso8601(since),
            "limit": MaxPageSize,
        };
        try {
            list<hash<auto>> rows = doGetList(getEndpoint(), params);
            list<hash<auto>> out = ();
            foreach hash<auto> r in (rows) {
                push out, tagRecord(r);
            }
            return out;
        } catch (hash<ExceptionInfo> ex) {
            error("Error fetching inventory updates: %s: %s", ex.err, ex.desc);
            return ();
        }
    }

    hash<auto> getFakeExampleData() {
        return {
            "id": "555:1",
            "variant_id": 555,
            "location_id": 1,
            "quantity_in_stock": 42.0,
            "average_cost": 12.50,
            "value_in_stock": 525.00,
            "updated_at": "2026-05-05T17:00:00Z",
        };
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace KatanaDataProvider
