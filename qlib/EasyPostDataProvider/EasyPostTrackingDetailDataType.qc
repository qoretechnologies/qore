# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! EasyPost tracking location (city/state/country/zip of a scan event)
public class EasyPostTrackingLocationType inherits HashDataType {
    public {
        const Fields = {
            "object": {"display_name": "Object Type", "type": SoftStringOrNothingType, "desc": "Always `TrackingLocation`"},
            "city": {"display_name": "City", "type": SoftStringOrNothingType, "desc": "City of the scan"},
            "state": {"display_name": "State", "type": SoftStringOrNothingType, "desc": "State or province code"},
            "country": {"display_name": "Country", "type": SoftStringOrNothingType, "desc": "ISO 2-letter country code"},
            "zip": {"display_name": "ZIP / Postal Code", "type": SoftStringOrNothingType, "desc": "ZIP or postal code"},
        };
    }
    constructor() {
        addQoreFields(Fields);
    }

    constructor(Type base_type) : HashDataType(base_type) {
        addQoreFields(Fields);
    }
}

#! EasyPost tracking detail (single scan event)
public class EasyPostTrackingDetailType inherits HashDataType {
    public {
        const Fields = {
            "object": {"display_name": "Object Type", "type": SoftStringOrNothingType, "desc": "Always `TrackingDetail`"},
            "message": {"display_name": "Message", "type": SoftStringOrNothingType, "desc": "Carrier scan message"},
            "description": {"display_name": "Description", "type": SoftStringOrNothingType, "desc": "Description of the tracking event"},
            "status": {
                "display_name": "Status",
                "type": SoftStringOrNothingType,
                "desc": "Status at this scan",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "unknown", "desc": "Unknown"},
                    <AllowedValueInfo>{"value": "pre_transit", "desc": "Pre-transit"},
                    <AllowedValueInfo>{"value": "in_transit", "desc": "In transit"},
                    <AllowedValueInfo>{"value": "out_for_delivery", "desc": "Out for delivery"},
                    <AllowedValueInfo>{"value": "delivered", "desc": "Delivered"},
                    <AllowedValueInfo>{"value": "available_for_pickup", "desc": "Available for pickup"},
                    <AllowedValueInfo>{"value": "return_to_sender", "desc": "Return to sender"},
                    <AllowedValueInfo>{"value": "failure", "desc": "Failure"},
                    <AllowedValueInfo>{"value": "cancelled", "desc": "Cancelled"},
                    <AllowedValueInfo>{"value": "error", "desc": "Error"},
                ),
            },
            "status_detail": {"display_name": "Status Detail", "type": SoftStringOrNothingType, "desc": "Detailed status code"},
            "datetime": {"display_name": "Datetime", "type": SoftDateOrNothingType, "desc": "Scan datetime"},
            "source": {"display_name": "Source", "type": SoftStringOrNothingType, "desc": "Source of the tracking data"},
            "carrier_code": {"display_name": "Carrier Code", "type": SoftStringOrNothingType, "desc": "Carrier code"},
            "tracking_location": {"display_name": "Tracking Location", "type": new EasyPostTrackingLocationType(AutoHashOrNothingType), "desc": "Hash with `city`, `state`, `country`, `zip`"},
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace EasyPostDataProvider
