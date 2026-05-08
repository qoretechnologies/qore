# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Watches for `refund.successful` webhook events
public class EasyPostWatchRefundSuccessfulEventDataProvider inherits EasyPostBaseEventDataProvider {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "watch-refund-successful",
            "desc": "Watches for EasyPost `refund.successful` webhook events",
            "type": "EasyPostWatchRefundSuccessfulEventDataProvider",
            "supports_observable": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = EasyPostDataProviderBase::BaseConstructorOptions;
    }

    private {
        static EasyPostRefundEventType eventDataType();
    }

    constructor(*RestClientIo::RestClientIo rest, *hash<auto> options)
            : EasyPostBaseEventDataProvider(rest, options) {
    }

    string getName() {
        return "watch-refund-successful";
    }

    *string getDesc() {
        return "Watches for EasyPost `refund.successful` webhook events";
    }

    string getEventType() {
        return EventRefundSuccessful;
    }

    string getWebhookEventType() {
        return WebhookEventRefundSuccessful;
    }

    AbstractDataProviderType getEventDataType() {
        return eventDataType;
    }

    private auto getExampleEventDataImpl(string event_id) {
        hash<auto> base = EasyPostBaseEventDataProvider::getExampleEventDataImpl(event_id);
        base.result = {
            "id": "rfnd_example",
            "object": "Refund",
            "mode": "test",
            "carrier": "USPS",
            "tracking_code": "EZ1000000001",
            "status": "submitted",
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
