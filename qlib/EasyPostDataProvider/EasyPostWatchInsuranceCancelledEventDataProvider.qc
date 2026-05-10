# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Watches for `insurance.cancelled` webhook events
/** EasyPost also fires this event when a standalone Insurance fails purchasing. */
public class EasyPostWatchInsuranceCancelledEventDataProvider inherits EasyPostBaseEventDataProvider {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "watch-insurance-cancelled",
            "desc": "Watches for EasyPost `insurance.cancelled` webhook events",
            "type": "EasyPostWatchInsuranceCancelledEventDataProvider",
            "supports_observable": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = EasyPostDataProviderBase::BaseConstructorOptions;
    }

    private {
        static EasyPostInsuranceEventType eventDataType();
    }

    constructor(*RestClientIo::RestClientIo rest, *hash<auto> options)
            : EasyPostBaseEventDataProvider(rest, options) {
    }

    string getName() {
        return "watch-insurance-cancelled";
    }

    *string getDesc() {
        return "Watches for EasyPost `insurance.cancelled` webhook events";
    }

    string getEventType() {
        return EventInsuranceCancelled;
    }

    string getWebhookEventType() {
        return WebhookEventInsuranceCancelled;
    }

    AbstractDataProviderType getEventDataType() {
        return eventDataType;
    }

    private hash<auto> getFakeEventData() {
        hash<auto> base = EasyPostBaseEventDataProvider::getFakeEventData();
        base.result = {
            "id": "ins_example",
            "object": "Insurance",
            "mode": "test",
            "amount": "100.00",
            "provider": "EasyPost",
            "status": "cancelled",
            "tracking_code": "EZ1000000001",
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
