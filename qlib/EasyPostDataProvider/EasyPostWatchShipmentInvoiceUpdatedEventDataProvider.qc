# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Watches for `shipment.invoice.updated` webhook events
public class EasyPostWatchShipmentInvoiceUpdatedEventDataProvider inherits EasyPostBaseEventDataProvider {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "watch-shipment-invoice-updated",
            "desc": "Watches for EasyPost `shipment.invoice.updated` webhook events",
            "type": "EasyPostWatchShipmentInvoiceUpdatedEventDataProvider",
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
        return "watch-shipment-invoice-updated";
    }

    *string getDesc() {
        return "Watches for EasyPost `shipment.invoice.updated` webhook events";
    }

    string getEventType() {
        return EventShipmentInvoiceUpdated;
    }

    string getWebhookEventType() {
        return WebhookEventShipmentInvoiceUpdated;
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
            "status": "updated",
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
