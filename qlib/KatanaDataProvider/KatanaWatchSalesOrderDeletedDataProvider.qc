# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KatanaWatchSalesOrderDeletedDataProvider.qc Polling watch for soft-deleted sales orders

/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

#! Watch provider: emits an event for each soft-deleted sales order.
/** Polls \c /sales_orders with \c include_deleted=true; filters to records whose
    \c deleted_at is set and newer than \c last_check. Note: delete detection is
    best-effort — hard deletes that don't set \c deleted_at cannot be detected by
    polling and are not reported here.
*/
public class KatanaWatchSalesOrderDeletedDataProvider inherits KatanaWebhookEventDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_SO_DELETED,
            "desc": "Fires when a sales order is soft-deleted in Katana. Hard deletes that "
                "do not set `deleted_at` cannot be detected via polling.",
            "type": "KatanaWatchSalesOrderDeletedDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(*RestClientIo::RestClientIo rest, *hash<auto> options)
            : KatanaWebhookEventDataProviderBase(rest, options) {
    }

    string getName() { return ProviderInfo.name; }
    string getEndpoint() { return "sales_orders"; }
    string getEventName() { return EVENT_SO_DELETED; }
    *string getTargetStatus() { return NOTHING; }
    *string getTimestampField() { return "deleted_at"; }

    AbstractDataProviderType getRecordDataType() {
        return new KatanaSalesOrderDataType();
    }

    #! Override fetch to filter by deleted_at_min and include_deleted=true
    list<hash<auto>> fetchRecords(date since) {
        hash<auto> params = {
            "updated_at_min": KatanaDataProviderBase::formatUtcIso8601(since),
            "include_deleted": "true",
            "limit": MaxPageSize,
        };
        try {
            list<hash<auto>> all_records = doGetList(getEndpoint(), params);
            # filter to records that have deleted_at populated
            list<hash<auto>> deleted = ();
            foreach hash<auto> r in (all_records) {
                if (r.deleted_at) {
                    push deleted, r;
                }
            }
            return deleted;
        } catch (hash<ExceptionInfo> ex) {
            error("Error fetching deleted sales orders: %s: %s", ex.err, ex.desc);
            return ();
        }
    }

    hash<auto> getFakeExampleData() {
        return {
            "id": 1001,
            "order_no": "SO-0001",
            "customer_id": 42,
            "status": "CANCELLED",
            "created_at": "2026-05-01T10:15:00Z",
            "updated_at": "2026-05-03T14:00:00Z",
            "deleted_at": "2026-05-03T14:00:00Z",
        };
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace KatanaDataProvider
