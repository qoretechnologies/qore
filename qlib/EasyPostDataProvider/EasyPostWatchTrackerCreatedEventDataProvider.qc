# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Watches for `tracker.created` webhook events
public class EasyPostWatchTrackerCreatedEventDataProvider inherits EasyPostBaseEventDataProvider {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "watch-tracker-created",
            "desc": "Watches for EasyPost `tracker.created` webhook events",
            "type": "EasyPostWatchTrackerCreatedEventDataProvider",
            "supports_observable": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = EasyPostDataProviderBase::BaseConstructorOptions;
    }

    private {
        static EasyPostTrackerEventType eventDataType();
    }

    constructor(*RestClientIo::RestClientIo rest, *hash<auto> options)
            : EasyPostBaseEventDataProvider(rest, options) {
    }

    string getName() {
        return "watch-tracker-created";
    }

    *string getDesc() {
        return "Watches for EasyPost `tracker.created` webhook events";
    }

    string getEventType() {
        return EventTrackerCreated;
    }

    string getWebhookEventType() {
        return WebhookEventTrackerCreated;
    }

    AbstractDataProviderType getEventDataType() {
        return eventDataType;
    }

    private hash<auto> getFakeEventData() {
        hash<auto> base = EasyPostBaseEventDataProvider::getFakeEventData();
        base.result = {
            "id": "trk_example",
            "object": "Tracker",
            "mode": "test",
            "tracking_code": "EZ1000000001",
            "status": "pre_transit",
            "carrier": "USPS",
            "shipment_id": "shp_example",
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
