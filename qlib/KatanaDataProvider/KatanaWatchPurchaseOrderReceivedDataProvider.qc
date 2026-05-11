# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KatanaWatchPurchaseOrderReceivedDataProvider.qc Watch for POs in RECEIVED state

/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

#! Watch provider: emits an event when a purchase order's status transitions to RECEIVED.
public class KatanaWatchPurchaseOrderReceivedDataProvider
        inherits KatanaWebhookEventDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_PO_RECEIVED,
            "desc": "Fires when a purchase order's status transitions to `RECEIVED` in Katana.",
            "type": "KatanaWatchPurchaseOrderReceivedDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(*RestClientIo::RestClientIo rest, *hash<auto> options)
            : KatanaWebhookEventDataProviderBase(rest, options) {
    }

    string getName() { return ProviderInfo.name; }
    string getEndpoint() { return "purchase_orders"; }
    string getEventName() { return EVENT_PO_RECEIVED; }
    *string getTargetStatus() { return "RECEIVED"; }

    AbstractDataProviderType getRecordDataType() {
        return new KatanaPurchaseOrderDataType();
    }

    hash<auto> getFakeExampleData() {
        return {
            "id": 2001,
            "order_no": "PO-0001",
            "supplier_id": 7,
            "status": "RECEIVED",
            "currency": "USD",
            "total": 5000.00,
            "created_at": "2026-05-01T11:00:00Z",
            "updated_at": "2026-05-05T14:30:00Z",
        };
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace KatanaDataProvider
