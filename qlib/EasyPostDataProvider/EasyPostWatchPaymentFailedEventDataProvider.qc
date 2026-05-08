# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Watches for `payment.failed` webhook events
public class EasyPostWatchPaymentFailedEventDataProvider inherits EasyPostBaseEventDataProvider {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "watch-payment-failed",
            "desc": "Watches for EasyPost `payment.failed` webhook events",
            "type": "EasyPostWatchPaymentFailedEventDataProvider",
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
        return "watch-payment-failed";
    }

    *string getDesc() {
        return "Watches for EasyPost `payment.failed` webhook events";
    }

    string getEventType() {
        return EventPaymentFailed;
    }

    string getWebhookEventType() {
        return WebhookEventPaymentFailed;
    }

    AbstractDataProviderType getEventDataType() {
        return eventDataType;
    }

    private auto getExampleEventDataImpl(string event_id) {
        hash<auto> base = EasyPostBaseEventDataProvider::getExampleEventDataImpl(event_id);
        base.result = {
            "id": "pmt_example",
            "object": "Payment",
            "mode": "test",
            "amount": "10.00",
            "currency": "USD",
            "status": "failed",
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
