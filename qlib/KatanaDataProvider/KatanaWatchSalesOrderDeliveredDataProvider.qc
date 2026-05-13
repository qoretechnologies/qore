# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KatanaWatchSalesOrderDeliveredDataProvider.qc Polling watch for sales orders entering DELIVERED state

/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

#! Watch provider: emits an event when a sales order is delivered (status=DELIVERED).
public class KatanaWatchSalesOrderDeliveredDataProvider inherits KatanaWebhookEventDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_SO_DELIVERED,
            "desc": "Fires when a sales order's status transitions to `DELIVERED` in Katana.",
            "type": "KatanaWatchSalesOrderDeliveredDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(*RestClientIo::RestClientIo rest, *hash<auto> options)
            : KatanaWebhookEventDataProviderBase(rest, options) {
    }

    string getName() { return ProviderInfo.name; }
    string getEndpoint() { return "sales_orders"; }
    string getEventName() { return EVENT_SO_DELIVERED; }
    *string getTargetStatus() { return "DELIVERED"; }

    AbstractDataProviderType getRecordDataType() {
        return new KatanaSalesOrderDataType();
    }

    hash<auto> getFakeExampleData() {
        return {
            "id": 1001,
            "order_no": "SO-0001",
            "customer_id": 42,
            "status": "DELIVERED",
            "currency": "USD",
            "total": 199.95,
            "tracking_number": "1Z999AA10123456784",
            "created_at": "2026-05-01T10:15:00Z",
            "updated_at": "2026-05-04T16:45:00Z",
        };
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace KatanaDataProvider
