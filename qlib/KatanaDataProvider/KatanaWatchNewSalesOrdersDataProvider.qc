# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KatanaWatchNewSalesOrdersDataProvider.qc Polling watch for newly created sales orders

/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

#! Watch provider: fires once per newly created sales order (generic stream).
public class KatanaWatchNewSalesOrdersDataProvider inherits KatanaWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_NEW_SOS,
            "desc": "Fires when a new sales order is created in Katana (generic stream).",
            "type": "KatanaWatchNewSalesOrdersDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(*RestClientIo::RestClientIo rest, *hash<auto> options)
            : KatanaWatchDataProviderBase(rest, options) {
    }

    string getName() { return ProviderInfo.name; }
    string getEndpoint() { return "sales_orders"; }
    string getEventName() { return EVENT_NEW_SOS; }
    *string getTimestampField() { return "created_at"; }

    AbstractDataProviderType getRecordDataType() {
        return new KatanaSalesOrderDataType();
    }

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
