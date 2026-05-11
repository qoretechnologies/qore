# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KatanaWatchPurchaseOrderRowUpdatedDataProvider.qc Watch for updated purchase-order rows

/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

#! Watch provider: emits an event for each updated purchase-order row.
public class KatanaWatchPurchaseOrderRowUpdatedDataProvider
        inherits KatanaWebhookEventDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_POR_UPDATED,
            "desc": "Fires when a purchase-order row is updated in Katana.",
            "type": "KatanaWatchPurchaseOrderRowUpdatedDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(*RestClientIo::RestClientIo rest, *hash<auto> options)
            : KatanaWebhookEventDataProviderBase(rest, options) {
    }

    string getName() { return ProviderInfo.name; }
    string getEndpoint() { return "purchase_order_rows"; }
    string getEventName() { return EVENT_POR_UPDATED; }
    *string getTargetStatus() { return NOTHING; }

    AbstractDataProviderType getRecordDataType() {
        return new KatanaPurchaseOrderRowRecordDataType();
    }

    hash<auto> getFakeExampleData() {
        return {
            "id": 30001,
            "purchase_order_id": 2001,
            "quantity": 12,
            "variant_id": 555,
            "tax_rate_id": 1,
            "price_per_unit": 50.0,
            "total": 600.0,
            "currency": "USD",
            "created_at": "2026-05-01T11:00:00Z",
            "updated_at": "2026-05-02T08:00:00Z",
        };
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace KatanaDataProvider
