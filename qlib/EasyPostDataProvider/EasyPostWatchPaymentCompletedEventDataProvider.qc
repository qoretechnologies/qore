# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Watches for `payment.completed` webhook events
public class EasyPostWatchPaymentCompletedEventDataProvider inherits EasyPostBaseEventDataProvider {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "watch-payment-completed",
            "desc": "Watches for EasyPost `payment.completed` webhook events",
            "type": "EasyPostWatchPaymentCompletedEventDataProvider",
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
        return "watch-payment-completed";
    }

    *string getDesc() {
        return "Watches for EasyPost `payment.completed` webhook events";
    }

    string getEventType() {
        return EventPaymentCompleted;
    }

    string getWebhookEventType() {
        return WebhookEventPaymentCompleted;
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
            "status": "completed",
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
