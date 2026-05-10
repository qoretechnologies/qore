# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Allowed values for tracker status (same as shipment status)
const TrackerStatusAllowedValues = ShipmentStatusAllowedValues;

#! EasyPost carrier_detail object (carrier-reported shipment metadata)
public class EasyPostCarrierDetailType inherits HashDataType {
    public {
        const Fields = {
            "object": {"display_name": "Object Type", "type": SoftStringOrNothingType, "desc": "Always `CarrierDetail`"},
            "service": {"display_name": "Service", "type": SoftStringOrNothingType, "desc": "Carrier service name"},
            "container_type": {"display_name": "Container Type", "type": SoftStringOrNothingType, "desc": "Carrier container type"},
            "est_delivery_date_local": {"display_name": "Est. Delivery Date (Local)", "type": SoftDateOrNothingType, "desc": "Carrier-estimated delivery date in the destination's local time zone"},
            "est_delivery_time_local": {"display_name": "Est. Delivery Time (Local)", "type": SoftStringOrNothingType, "desc": "Carrier-estimated delivery time in the destination's local time zone"},
            "origin_location": {"display_name": "Origin Location", "type": SoftStringOrNothingType, "desc": "Origin location string"},
            "origin_tracking_location": {"display_name": "Origin Tracking Location", "type": new EasyPostTrackingLocationType(AutoHashOrNothingType), "desc": "Structured origin tracking location"},
            "destination_location": {"display_name": "Destination Location", "type": SoftStringOrNothingType, "desc": "Destination location string"},
            "destination_tracking_location": {"display_name": "Destination Tracking Location", "type": new EasyPostTrackingLocationType(AutoHashOrNothingType), "desc": "Structured destination tracking location"},
            "guaranteed_delivery_date": {"display_name": "Guaranteed Delivery Date", "type": SoftDateOrNothingType, "desc": "Carrier-guaranteed delivery date"},
            "alternate_identifier": {"display_name": "Alternate Identifier", "type": SoftStringOrNothingType, "desc": "Alternate carrier identifier"},
            "initial_delivery_attempt": {"display_name": "Initial Delivery Attempt", "type": SoftDateOrNothingType, "desc": "Datetime of the initial delivery attempt"},
        };
    }
    constructor() {
        addQoreFields(Fields);
    }

    constructor(Type base_type) : HashDataType(base_type) {
        addQoreFields(Fields);
    }
}

#! EasyPost tracker record
public class EasyPostTrackerType inherits HashDataType {
    public {
        const Fields = {
            "id": {"display_name": "ID", "type": SoftStringOrNothingType, "desc": "Tracker ID (`trk_xxx`)"},
            "object": {"display_name": "Object Type", "type": SoftStringOrNothingType, "desc": "Always `Tracker`"},
            "mode": {"display_name": "Mode", "type": SoftStringOrNothingType, "desc": "`production` or `test`"},
            "tracking_code": {"display_name": "Tracking Code", "type": SoftStringOrNothingType, "desc": "Carrier tracking code"},
            "status": {"display_name": "Status", "type": SoftStringOrNothingType, "desc": "Current tracker status", "allowed_values": TrackerStatusAllowedValues},
            "status_detail": {"display_name": "Status Detail", "type": SoftStringOrNothingType, "desc": "Detailed status code"},
            "signed_by": {"display_name": "Signed By", "type": SoftStringOrNothingType, "desc": "Recipient who signed for delivery"},
            "weight": {"display_name": "Weight", "type": SoftFloatOrNothingType, "desc": "Weight in ounces"},
            "est_delivery_date": {"display_name": "Estimated Delivery Date", "type": SoftDateOrNothingType, "desc": "Carrier-estimated delivery date"},
            "shipment_id": {"display_name": "Shipment ID", "type": SoftStringOrNothingType, "desc": "Associated shipment ID"},
            "carrier": {"display_name": "Carrier", "type": SoftStringOrNothingType, "desc": "Carrier name"},
            "tracking_details": {"display_name": "Tracking Details", "type": new ListDataType("EasyPostTrackerScanList", new EasyPostTrackingDetailType(), True), "desc": "List of carrier scan events"},
            "fees": {"display_name": "Fees", "type": new ListDataType("EasyPostTrackerFeeList", new EasyPostFeeType(), True), "desc": "Fees associated with the tracker"},
            "carrier_detail": {"display_name": "Carrier Detail", "type": new EasyPostCarrierDetailType(AutoHashOrNothingType), "desc": "Carrier-specific details"},
            "public_url": {"display_name": "Public URL", "type": SoftStringOrNothingType, "desc": "Public tracking URL"},
            "finalized": {"display_name": "Finalized", "type": SoftBoolOrNothingType, "desc": "Whether the tracker is finalized"},
            "created_at": {"display_name": "Created At", "type": SoftDateOrNothingType, "desc": "Creation timestamp"},
            "updated_at": {"display_name": "Updated At", "type": SoftDateOrNothingType, "desc": "Last-update timestamp"},
        };
    }
    constructor() {
        addQoreFields(Fields);
    }

    constructor(Type base_type) : HashDataType(base_type) {
        addQoreFields(Fields);
    }
}

} # namespace EasyPostDataProvider
