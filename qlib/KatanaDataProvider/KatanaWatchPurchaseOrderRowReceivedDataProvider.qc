# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KatanaWatchPurchaseOrderRowReceivedDataProvider.qc Watch for received purchase-order rows

/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

#! Watch provider: emits an event when a purchase-order row is received.
/** Purchase-order rows expose a \c received_date field once received; rows that have
    \c received_date populated and updated after \c last_check are considered received.
    Hard mapping to a \c status field is not available at row level, so this watcher
    filters by presence of \c received_date.
*/
public class KatanaWatchPurchaseOrderRowReceivedDataProvider
        inherits KatanaWebhookEventDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_POR_RECEIVED,
            "desc": "Fires when a purchase-order row is received in Katana (a non-null "
                "`received_date` appears or is updated).",
            "type": "KatanaWatchPurchaseOrderRowReceivedDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(*RestClientIo::RestClientIo rest, *hash<auto> options)
            : KatanaWebhookEventDataProviderBase(rest, options) {
    }

    string getName() { return ProviderInfo.name; }
    string getEndpoint() { return "purchase_order_rows"; }
    string getEventName() { return EVENT_POR_RECEIVED; }
    *string getTargetStatus() { return NOTHING; }

    AbstractDataProviderType getRecordDataType() {
        return new KatanaPurchaseOrderRowRecordDataType();
    }

    #! Overridden fetch: filter on rows that have \c received_date populated.
    list<hash<auto>> fetchRecords(date since) {
        hash<auto> params = {
            "updated_at_min": KatanaDataProviderBase::formatUtcIso8601(since),
            "limit": MaxPageSize,
        };
        try {
            list<hash<auto>> all_rows = doGetList(getEndpoint(), params);
            list<hash<auto>> received = ();
            foreach hash<auto> r in (all_rows) {
                if (r.received_date) {
                    push received, r;
                }
            }
            return received;
        } catch (hash<ExceptionInfo> ex) {
            error("Error fetching received purchase-order rows: %s: %s", ex.err, ex.desc);
            return ();
        }
    }

    hash<auto> getFakeExampleData() {
        return {
            "id": 30001,
            "purchase_order_id": 2001,
            "quantity": 10,
            "variant_id": 555,
            "received_date": "2026-05-05T14:30:00Z",
            "created_at": "2026-05-01T11:00:00Z",
            "updated_at": "2026-05-05T14:30:00Z",
        };
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace KatanaDataProvider
