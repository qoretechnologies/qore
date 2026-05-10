# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Watches for `payment.created` webhook events
/** Fired when a bank account or credit card is successfully charged. */
public class EasyPostWatchPaymentCreatedEventDataProvider inherits EasyPostBaseEventDataProvider {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "watch-payment-created",
            "desc": "Watches for EasyPost `payment.created` webhook events",
            "type": "EasyPostWatchPaymentCreatedEventDataProvider",
            "supports_observable": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = EasyPostDataProviderBase::BaseConstructorOptions;
    }

    private {
        static EasyPostPaymentEventType eventDataType();
    }

    constructor(*RestClientIo::RestClientIo rest, *hash<auto> options)
            : EasyPostBaseEventDataProvider(rest, options) {
    }

    string getName() {
        return "watch-payment-created";
    }

    *string getDesc() {
        return "Watches for EasyPost `payment.created` webhook events";
    }

    string getEventType() {
        return EventPaymentCreated;
    }

    string getWebhookEventType() {
        return WebhookEventPaymentCreated;
    }

    AbstractDataProviderType getEventDataType() {
        return eventDataType;
    }

    private hash<auto> getFakeEventData() {
        hash<auto> base = EasyPostBaseEventDataProvider::getFakeEventData();
        base.result = {
            "id": "pmt_example",
            "object": "Payment",
            "mode": "test",
            "amount": "10.00",
            "currency": "USD",
            "status": "created",
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
