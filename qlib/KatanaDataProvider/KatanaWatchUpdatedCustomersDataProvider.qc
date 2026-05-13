# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KatanaWatchUpdatedCustomersDataProvider.qc Polling watch for updated customers

/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

#! Watch provider: fires when a customer is updated.
public class KatanaWatchUpdatedCustomersDataProvider inherits KatanaWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_UPD_CUSTOMERS,
            "desc": "Fires when a customer is updated in Katana.",
            "type": "KatanaWatchUpdatedCustomersDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(*RestClientIo::RestClientIo rest, *hash<auto> options)
            : KatanaWatchDataProviderBase(rest, options) {
    }

    string getName() { return ProviderInfo.name; }
    string getEndpoint() { return "customers"; }
    string getEventName() { return EVENT_UPD_CUSTOMERS; }

    AbstractDataProviderType getRecordDataType() {
        return new KatanaCustomerDataType();
    }

    hash<auto> getFakeExampleData() {
        return {
            "id": 42,
            "name": "Acme Corp",
            "email": "billing@acme.example",
            "phone": "+1-555-0100",
            "currency": "USD",
            "created_at": "2026-05-01T09:00:00Z",
            "updated_at": "2026-05-04T15:00:00Z",
        };
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace KatanaDataProvider
