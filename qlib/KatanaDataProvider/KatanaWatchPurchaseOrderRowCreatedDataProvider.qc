# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KatanaWatchPurchaseOrderRowCreatedDataProvider.qc Watch for newly created purchase-order rows

/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

#! Watch provider: emits an event for each newly created purchase-order row.
public class KatanaWatchPurchaseOrderRowCreatedDataProvider
        inherits KatanaWebhookEventDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_POR_CREATED,
            "desc": "Fires when a purchase-order row is created in Katana.",
            "type": "KatanaWatchPurchaseOrderRowCreatedDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(*RestClientIo::RestClientIo rest, *hash<auto> options)
            : KatanaWebhookEventDataProviderBase(rest, options) {
    }

    string getName() { return ProviderInfo.name; }
    string getEndpoint() { return "purchase_order_rows"; }
    string getEventName() { return EVENT_POR_CREATED; }
    *string getTargetStatus() { return NOTHING; }
    *string getTimestampField() { return "created_at"; }

    AbstractDataProviderType getRecordDataType() {
        return new KatanaPurchaseOrderRowRecordDataType();
    }

    list<hash<auto>> fetchRecords(date since) {
        hash<auto> params = {
            "created_at_min": KatanaDataProviderBase::formatUtcIso8601(since),
            "limit": MaxPageSize,
        };
        try {
            return doGetList(getEndpoint(), params);
        } catch (hash<ExceptionInfo> ex) {
            error("Error fetching new purchase-order rows: %s: %s", ex.err, ex.desc);
            return ();
        }
    }

    hash<auto> getFakeExampleData() {
        return {
            "id": 30001,
            "purchase_order_id": 2001,
            "quantity": 10,
            "variant_id": 555,
            "tax_rate_id": 1,
            "price_per_unit": 50.0,
            "total": 500.0,
            "currency": "USD",
            "created_at": "2026-05-01T11:00:00Z",
            "updated_at": "2026-05-01T11:00:00Z",
        };
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace KatanaDataProvider
