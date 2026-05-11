# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KatanaWatchNewStockAdjustmentsDataProvider.qc Polling watch for newly created stock adjustments

/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

#! Watch provider: fires once per newly created stock adjustment.
public class KatanaWatchNewStockAdjustmentsDataProvider inherits KatanaWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_NEW_STOCK_ADJ,
            "desc": "Fires when a new stock adjustment is created in Katana.",
            "type": "KatanaWatchNewStockAdjustmentsDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(*RestClientIo::RestClientIo rest, *hash<auto> options)
            : KatanaWatchDataProviderBase(rest, options) {
    }

    string getName() { return ProviderInfo.name; }
    string getEndpoint() { return "stock_adjustments"; }
    string getEventName() { return EVENT_NEW_STOCK_ADJ; }
    *string getTimestampField() { return "created_at"; }

    AbstractDataProviderType getRecordDataType() {
        return new KatanaStockAdjustmentDataType();
    }

    list<hash<auto>> fetchRecords(date since) {
        hash<auto> params = {
            "created_at_min": KatanaDataProviderBase::formatUtcIso8601(since),
            "limit": MaxPageSize,
        };
        try {
            return doGetList(getEndpoint(), params);
        } catch (hash<ExceptionInfo> ex) {
            error("Error fetching new stock adjustments: %s: %s", ex.err, ex.desc);
            return ();
        }
    }

    hash<auto> getFakeExampleData() {
        return {
            "id": 8001,
            "stock_adjustment_number": "SA-0001",
            "location_id": 1,
            "reason": "Cycle count",
            "additional_info": "Monthly count",
            "created_at": "2026-05-05T18:00:00Z",
            "updated_at": "2026-05-05T18:00:00Z",
        };
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace KatanaDataProvider
