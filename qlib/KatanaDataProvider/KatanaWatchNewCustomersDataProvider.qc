# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KatanaWatchNewCustomersDataProvider.qc Polling watch for newly created customers

/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

#! Watch provider: fires once per newly created customer.
public class KatanaWatchNewCustomersDataProvider inherits KatanaWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_NEW_CUSTOMERS,
            "desc": "Fires when a new customer is created in Katana.",
            "type": "KatanaWatchNewCustomersDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(*RestClientIo::RestClientIo rest, *hash<auto> options)
            : KatanaWatchDataProviderBase(rest, options) {
    }

    string getName() { return ProviderInfo.name; }
    string getEndpoint() { return "customers"; }
    string getEventName() { return EVENT_NEW_CUSTOMERS; }
    *string getTimestampField() { return "created_at"; }

    AbstractDataProviderType getRecordDataType() {
        return new KatanaCustomerDataType();
    }

    list<hash<auto>> fetchRecords(date since) {
        hash<auto> params = {
            "created_at_min": KatanaDataProviderBase::formatUtcIso8601(since),
            "limit": MaxPageSize,
        };
        try {
            return doGetList(getEndpoint(), params);
        } catch (hash<ExceptionInfo> ex) {
            LoggerWrapper::error(sprintf("Error fetching new customers: %s: %s", ex.err, ex.desc));
            return ();
        }
    }

    hash<auto> getFakeExampleData() {
        return {
            "id": 42,
            "name": "Acme Corp",
            "email": "billing@acme.example",
            "phone": "+1-555-0100",
            "currency": "USD",
            "created_at": "2026-05-01T09:00:00Z",
            "updated_at": "2026-05-01T09:00:00Z",
        };
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace KatanaDataProvider
