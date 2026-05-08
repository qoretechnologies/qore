# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! EasyPost refund record
public class EasyPostRefundType inherits HashDataType {
    public {
        const Fields = {
            "id": {"display_name": "ID", "type": SoftStringOrNothingType, "desc": "Refund ID (`rfnd_xxx`)"},
            "object": {"display_name": "Object Type", "type": SoftStringOrNothingType, "desc": "Always `Refund`"},
            "mode": {"display_name": "Mode", "type": SoftStringOrNothingType, "desc": "`production` or `test`"},
            "carrier": {"display_name": "Carrier", "type": SoftStringOrNothingType, "desc": "Carrier name"},
            "tracking_code": {"display_name": "Tracking Code", "type": SoftStringOrNothingType, "desc": "Tracking code being refunded"},
            "confirmation_number": {"display_name": "Confirmation Number", "type": SoftStringOrNothingType, "desc": "Carrier confirmation number"},
            "status": {
                "display_name": "Status",
                "type": SoftStringOrNothingType,
                "desc": "Refund status",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "submitted", "desc": "Submitted"},
                    <AllowedValueInfo>{"value": "refunded", "desc": "Refunded"},
                    <AllowedValueInfo>{"value": "rejected", "desc": "Rejected"},
                ),
            },
            "shipment_id": {"display_name": "Shipment ID", "type": SoftStringOrNothingType, "desc": "Associated shipment ID"},
            "created_at": {"display_name": "Created At", "type": SoftDateOrNothingType, "desc": "Creation timestamp"},
            "updated_at": {"display_name": "Updated At", "type": SoftDateOrNothingType, "desc": "Last-update timestamp"},
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace EasyPostDataProvider
