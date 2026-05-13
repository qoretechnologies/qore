# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KatanaWatchPurchaseOrderCreatedDataProvider.qc Polling watch for newly created purchase orders

/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

#! Watch provider: emits an event for each newly created purchase order.
public class KatanaWatchPurchaseOrderCreatedDataProvider inherits KatanaWebhookEventDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_PO_CREATED,
            "desc": "Fires when a purchase order is created in Katana (polled via "
                "`/purchase_orders?created_at_min=...`).",
            "type": "KatanaWatchPurchaseOrderCreatedDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(*RestClientIo::RestClientIo rest, *hash<auto> options)
            : KatanaWebhookEventDataProviderBase(rest, options) {
    }

    string getName() { return ProviderInfo.name; }
    string getEndpoint() { return "purchase_orders"; }
    string getEventName() { return EVENT_PO_CREATED; }
    *string getTargetStatus() { return NOTHING; }
    *string getTimestampField() { return "created_at"; }

    AbstractDataProviderType getRecordDataType() {
        return new KatanaPurchaseOrderDataType();
    }

    list<hash<auto>> fetchRecords(date since) {
        hash<auto> params = {
            "created_at_min": KatanaDataProviderBase::formatUtcIso8601(since),
            "limit": MaxPageSize,
        };
        try {
            return doGetList(getEndpoint(), params);
        } catch (hash<ExceptionInfo> ex) {
            error("Error fetching new purchase orders: %s: %s", ex.err, ex.desc);
            return ();
        }
    }

    hash<auto> getFakeExampleData() {
        return {
            "id": 2001,
            "order_no": "PO-0001",
            "supplier_id": 7,
            "status": "NOT_RECEIVED",
            "currency": "USD",
            "total": 5000.00,
            "created_at": "2026-05-01T11:00:00Z",
            "updated_at": "2026-05-01T11:00:00Z",
        };
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace KatanaDataProvider
