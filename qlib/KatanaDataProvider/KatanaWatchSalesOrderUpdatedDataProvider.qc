# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KatanaWatchSalesOrderUpdatedDataProvider.qc Polling watch for updated sales orders

/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

#! Watch provider: emits an event for each updated sales order.
/** Polls \c /sales_orders filtered by \c updated_at_min — any change fires the event.
    Approximates the native \c sales_order.updated webhook.
*/
public class KatanaWatchSalesOrderUpdatedDataProvider inherits KatanaWebhookEventDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_SO_UPDATED,
            "desc": "Fires when a sales order is updated in Katana.",
            "type": "KatanaWatchSalesOrderUpdatedDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(*RestClientIo::RestClientIo rest, *hash<auto> options)
            : KatanaWebhookEventDataProviderBase(rest, options) {
    }

    string getName() { return ProviderInfo.name; }
    string getEndpoint() { return "sales_orders"; }
    string getEventName() { return EVENT_SO_UPDATED; }
    *string getTargetStatus() { return NOTHING; }

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
            "created_at": "2026-05-01T10:15:00Z",
            "updated_at": "2026-05-02T08:30:00Z",
        };
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace KatanaDataProvider
