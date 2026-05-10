# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Watches for `shipment.invoice.created` webhook events
public class EasyPostWatchShipmentInvoiceCreatedEventDataProvider inherits EasyPostBaseEventDataProvider {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "watch-shipment-invoice-created",
            "desc": "Watches for EasyPost `shipment.invoice.created` webhook events",
            "type": "EasyPostWatchShipmentInvoiceCreatedEventDataProvider",
            "supports_observable": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = EasyPostDataProviderBase::BaseConstructorOptions;
    }

    private {
        static EasyPostShipmentInvoiceEventType eventDataType();
    }

    constructor(*RestClientIo::RestClientIo rest, *hash<auto> options)
            : EasyPostBaseEventDataProvider(rest, options) {
    }

    string getName() {
        return "watch-shipment-invoice-created";
    }

    *string getDesc() {
        return "Watches for EasyPost `shipment.invoice.created` webhook events";
    }

    string getEventType() {
        return EventShipmentInvoiceCreated;
    }

    string getWebhookEventType() {
        return WebhookEventShipmentInvoiceCreated;
    }

    AbstractDataProviderType getEventDataType() {
        return eventDataType;
    }

    private hash<auto> getFakeEventData() {
        hash<auto> base = EasyPostBaseEventDataProvider::getFakeEventData();
        base.result = {
            "id": "invoice_example",
            "object": "ShipmentInvoice",
            "mode": "test",
            "shipment_id": "shp_example",
            "amount": "5.50",
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
