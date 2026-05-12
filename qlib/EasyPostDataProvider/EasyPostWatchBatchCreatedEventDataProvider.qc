# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Watches for `batch.created` webhook events
public class EasyPostWatchBatchCreatedEventDataProvider inherits EasyPostBaseEventDataProvider {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "watch-batch-created",
            "desc": "Watches for EasyPost `batch.created` webhook events",
            "type": "EasyPostWatchBatchCreatedEventDataProvider",
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
        return "watch-batch-created";
    }

    *string getDesc() {
        return "Watches for EasyPost `batch.created` webhook events";
    }

    string getEventType() {
        return EventBatchCreated;
    }

    string getWebhookEventType() {
        return WebhookEventBatchCreated;
    }

    AbstractDataProviderType getEventDataType() {
        return eventDataType;
    }

    private hash<auto> getFakeEventData() {
        hash<auto> base = EasyPostBaseEventDataProvider::getFakeEventData();
        base.result = {
            "id": "batch_example",
            "object": "Batch",
            "mode": "test",
            "state": "created",
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
