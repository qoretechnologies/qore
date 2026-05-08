# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Watches for `batch.updated` webhook events
public class EasyPostWatchBatchUpdatedEventDataProvider inherits EasyPostBaseEventDataProvider {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "watch-batch-updated",
            "desc": "Watches for EasyPost `batch.updated` webhook events",
            "type": "EasyPostWatchBatchUpdatedEventDataProvider",
            "supports_observable": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = EasyPostDataProviderBase::BaseConstructorOptions;
    }

    private {
        static EasyPostBatchEventType eventDataType();
    }

    constructor(*RestClientIo::RestClientIo rest, *hash<auto> options)
            : EasyPostBaseEventDataProvider(rest, options) {
    }

    string getName() {
        return "watch-batch-updated";
    }

    *string getDesc() {
        return "Watches for EasyPost `batch.updated` webhook events";
    }

    string getEventType() {
        return EventBatchUpdated;
    }

    string getWebhookEventType() {
        return WebhookEventBatchUpdated;
    }

    AbstractDataProviderType getEventDataType() {
        return eventDataType;
    }

    private auto getExampleEventDataImpl(string event_id) {
        hash<auto> base = EasyPostBaseEventDataProvider::getExampleEventDataImpl(event_id);
        base.result = {
            "id": "batch_example",
            "object": "Batch",
            "mode": "test",
            "state": "purchased",
            "num_shipments": 3,
            "reference": "example-ref",
            "created_at": "2026-01-01T12:00:00Z",
            "updated_at": "2026-01-01T12:00:00Z",
        };
        return base;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace EasyPostDataProvider
