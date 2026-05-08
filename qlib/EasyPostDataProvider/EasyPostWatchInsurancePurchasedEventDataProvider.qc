# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Watches for `insurance.purchased` webhook events
public class EasyPostWatchInsurancePurchasedEventDataProvider inherits EasyPostBaseEventDataProvider {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "watch-insurance-purchased",
            "desc": "Watches for EasyPost `insurance.purchased` webhook events",
            "type": "EasyPostWatchInsurancePurchasedEventDataProvider",
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
        return "watch-insurance-purchased";
    }

    *string getDesc() {
        return "Watches for EasyPost `insurance.purchased` webhook events";
    }

    string getEventType() {
        return EventInsurancePurchased;
    }

    string getWebhookEventType() {
        return WebhookEventInsurancePurchased;
    }

    AbstractDataProviderType getEventDataType() {
        return eventDataType;
    }

    private auto getExampleEventDataImpl(string event_id) {
        hash<auto> base = EasyPostBaseEventDataProvider::getExampleEventDataImpl(event_id);
        base.result = {
            "id": "ins_example",
            "object": "Insurance",
            "mode": "test",
            "amount": "100.00",
            "provider": "EasyPost",
            "status": "purchased",
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
