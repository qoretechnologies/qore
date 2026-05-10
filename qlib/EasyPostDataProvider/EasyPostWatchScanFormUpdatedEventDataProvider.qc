# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Watches for `scan_form.updated` webhook events
public class EasyPostWatchScanFormUpdatedEventDataProvider inherits EasyPostBaseEventDataProvider {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "watch-scan-form-updated",
            "desc": "Watches for EasyPost `scan_form.updated` webhook events",
            "type": "EasyPostWatchScanFormUpdatedEventDataProvider",
            "supports_observable": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = EasyPostDataProviderBase::BaseConstructorOptions;
    }

    private {
        static EasyPostScanFormEventType eventDataType();
    }

    constructor(*RestClientIo::RestClientIo rest, *hash<auto> options)
            : EasyPostBaseEventDataProvider(rest, options) {
    }

    string getName() {
        return "watch-scan-form-updated";
    }

    *string getDesc() {
        return "Watches for EasyPost `scan_form.updated` webhook events";
    }

    string getEventType() {
        return EventScanFormUpdated;
    }

    string getWebhookEventType() {
        return WebhookEventScanFormUpdated;
    }

    AbstractDataProviderType getEventDataType() {
        return eventDataType;
    }

    private hash<auto> getFakeEventData() {
        hash<auto> base = EasyPostBaseEventDataProvider::getFakeEventData();
        base.result = {
            "id": "sf_example",
            "object": "ScanForm",
            "mode": "test",
            "status": "created",
            "tracking_codes": ("EZ1000000001",),
            "form_url": "https://easypost-files.s3.amazonaws.com/files/scan_form/example.pdf",
            "form_file_type": "pdf",
            "batch_id": "batch_example",
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
