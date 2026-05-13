# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KatanaWatchSalesOrderPackedDataProvider.qc Polling watch for sales orders entering PACKED state

/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

#! Watch provider: emits an event when a sales order is packed (status=PACKED).
public class KatanaWatchSalesOrderPackedDataProvider inherits KatanaWebhookEventDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_SO_PACKED,
            "desc": "Fires when a sales order's status transitions to `PACKED` in Katana.",
            "type": "KatanaWatchSalesOrderPackedDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(*RestClientIo::RestClientIo rest, *hash<auto> options)
            : KatanaWebhookEventDataProviderBase(rest, options) {
    }

    string getName() { return ProviderInfo.name; }
    string getEndpoint() { return "sales_orders"; }
    string getEventName() { return EVENT_SO_PACKED; }
    *string getTargetStatus() { return "PACKED"; }

    AbstractDataProviderType getRecordDataType() {
        return new KatanaSalesOrderDataType();
    }

    hash<auto> getFakeExampleData() {
        return {
            "id": 1001,
            "order_no": "SO-0001",
            "customer_id": 42,
            "status": "PACKED",
            "currency": "USD",
            "total": 199.95,
            "picked_date": "2026-05-02T09:00:00Z",
            "created_at": "2026-05-01T10:15:00Z",
            "updated_at": "2026-05-02T09:30:00Z",
        };
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace KatanaDataProvider
