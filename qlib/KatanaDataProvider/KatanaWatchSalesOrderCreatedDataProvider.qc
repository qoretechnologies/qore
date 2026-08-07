# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KatanaWatchSalesOrderCreatedDataProvider.qc Polling watch for newly created sales orders

/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

#! Watch provider: emits an event for each newly created sales order.
/** Polls \c /sales_orders filtered by \c created_at_min so only newly created orders
    fire the event. Approximates the native \c sales_order.created webhook.
*/
public class KatanaWatchSalesOrderCreatedDataProvider inherits KatanaWebhookEventDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_SO_CREATED,
            "desc": "Fires when a sales order is created in Katana (polled via "
                "`/sales_orders?created_at_min=...`).",
            "type": "KatanaWatchSalesOrderCreatedDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(*RestClientIo::RestClientIo rest, *hash<auto> options)
            : KatanaWebhookEventDataProviderBase(rest, options) {
    }

    string getName() { return ProviderInfo.name; }
    string getEndpoint() { return "sales_orders"; }
    string getEventName() { return EVENT_SO_CREATED; }
    *string getTargetStatus() { return NOTHING; }
    *string getTimestampField() { return "created_at"; }

    AbstractDataProviderType getRecordDataType() {
        return new KatanaSalesOrderDataType();
    }

    #! Override to filter by created_at_min instead of updated_at_min
    list<hash<auto>> fetchRecords(date since) {
        hash<auto> params = {
            "created_at_min": KatanaDataProviderBase::formatUtcIso8601(since),
            "limit": MaxPageSize,
        };
        try {
            return doGetList(getEndpoint(), params);
        } catch (hash<ExceptionInfo> ex) {
            LoggerWrapper::error(sprintf("Error fetching new sales orders: %s: %s", ex.err, ex.desc));
            return ();
        }
    }

    hash<auto> getFakeExampleData() {
        return {
            "id": 1001,
            "order_no": "SO-0001",
            "customer_id": 42,
            "status": "NOT_SHIPPED",
            "currency": "USD",
            "total": 199.95,
            "created_at": "2026-05-01T10:15:00Z",
            "updated_at": "2026-05-01T10:15:00Z",
        };
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace KatanaDataProvider
