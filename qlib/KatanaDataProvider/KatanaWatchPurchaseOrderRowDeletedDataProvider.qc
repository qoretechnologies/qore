# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KatanaWatchPurchaseOrderRowDeletedDataProvider.qc Watch for soft-deleted purchase-order rows

/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

#! Watch provider: emits an event for each soft-deleted purchase-order row.
/** Hard deletes that don't set \c deleted_at cannot be detected via polling.
*/
public class KatanaWatchPurchaseOrderRowDeletedDataProvider
        inherits KatanaWebhookEventDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_POR_DELETED,
            "desc": "Fires when a purchase-order row is soft-deleted in Katana. Hard deletes "
                "that do not set `deleted_at` cannot be detected via polling.",
            "type": "KatanaWatchPurchaseOrderRowDeletedDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(*RestClientIo::RestClientIo rest, *hash<auto> options)
            : KatanaWebhookEventDataProviderBase(rest, options) {
    }

    string getName() { return ProviderInfo.name; }
    string getEndpoint() { return "purchase_order_rows"; }
    string getEventName() { return EVENT_POR_DELETED; }
    *string getTargetStatus() { return NOTHING; }
    *string getTimestampField() { return "deleted_at"; }

    AbstractDataProviderType getRecordDataType() {
        return new KatanaPurchaseOrderRowRecordDataType();
    }

    list<hash<auto>> fetchRecords(date since) {
        hash<auto> params = {
            "updated_at_min": KatanaDataProviderBase::formatUtcIso8601(since),
            "include_deleted": "true",
            "limit": MaxPageSize,
        };
        try {
            list<hash<auto>> all_records = doGetList(getEndpoint(), params);
            list<hash<auto>> deleted = ();
            foreach hash<auto> r in (all_records) {
                if (r.deleted_at) {
                    push deleted, r;
                }
            }
            return deleted;
        } catch (hash<ExceptionInfo> ex) {
            LoggerWrapper::error(sprintf("Error fetching deleted purchase-order rows: %s: %s", ex.err, ex.desc));
            return ();
        }
    }

    hash<auto> getFakeExampleData() {
        return {
            "id": 30001,
            "purchase_order_id": 2001,
            "quantity": 10,
            "variant_id": 555,
            "created_at": "2026-05-01T11:00:00Z",
            "updated_at": "2026-05-03T11:00:00Z",
            "deleted_at": "2026-05-03T11:00:00Z",
        };
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace KatanaDataProvider
