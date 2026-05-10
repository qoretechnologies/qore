# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Allowed values for shipment status
const ShipmentStatusAllowedValues = (
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
);

#! EasyPost shipment record
public class EasyPostShipmentType inherits HashDataType {
    public {
        const Fields = {
            "id": {"display_name": "ID", "type": SoftStringOrNothingType, "desc": "Shipment ID (`shp_xxx`)"},
            "object": {"display_name": "Object Type", "type": SoftStringOrNothingType, "desc": "Always `Shipment`"},
            "mode": {"display_name": "Mode", "type": SoftStringOrNothingType, "desc": "`production` or `test`"},
            "to_address": {"display_name": "To Address", "type": new EasyPostAddressType(), "desc": "Destination address"},
            "from_address": {"display_name": "From Address", "type": new EasyPostAddressType(), "desc": "Origin address"},
            "return_address": {"display_name": "Return Address", "type": new EasyPostAddressType(), "desc": "Return address"},
            "buyer_address": {"display_name": "Buyer Address", "type": new EasyPostAddressType(), "desc": "Buyer address (when different from `to_address`)"},
            "parcel": {"display_name": "Parcel", "type": new EasyPostParcelType(), "desc": "Parcel details"},
            "customs_info": {"display_name": "Customs Info", "type": new EasyPostCustomsInfoType(), "desc": "Customs information"},
            "rates": {"display_name": "Rates", "type": new ListDataType("EasyPostShipmentRateList", new EasyPostRateType(), True), "desc": "Available rates"},
            "selected_rate": {"display_name": "Selected Rate", "type": new EasyPostRateType(AutoHashOrNothingType), "desc": "Currently selected rate"},
            "postage_label": {"display_name": "Postage Label", "type": new EasyPostPostageLabelType(), "desc": "Generated postage label (if purchased)"},
            "messages": {"display_name": "Messages", "type": new ListDataType("EasyPostShipmentMessageList", new EasyPostMessageType(), True), "desc": "Carrier messages / errors"},
            "options": {"display_name": "Options", "type": new EasyPostShipmentOptionsType(), "desc": "Shipment options"},
            "is_return": {"display_name": "Is Return", "type": SoftBoolOrNothingType, "desc": "Whether this is a return shipment"},
            "tracking_code": {"display_name": "Tracking Code", "type": SoftStringOrNothingType, "desc": "Carrier tracking code"},
            "refund_status": {"display_name": "Refund Status", "type": SoftStringOrNothingType, "desc": "Refund status (if refunded)"},
            "scan_form": {"display_name": "Scan Form", "type": new EasyPostScanFormType(AutoHashOrNothingType), "desc": "Scan form metadata"},
            "forms": {"display_name": "Forms", "type": new ListDataType("EasyPostShipmentFormList", new EasyPostFormType(), True), "desc": "Customs forms"},
            "fees": {"display_name": "Fees", "type": new ListDataType("EasyPostShipmentFeeList", new EasyPostFeeType(), True), "desc": "Fees applied to the shipment"},
            "insurance": {"display_name": "Insurance Amount", "type": SoftStringOrNothingType, "desc": "Insurance amount as a string"},
            "tax_identifiers": {"display_name": "Tax Identifiers", "type": new ListDataType("EasyPostShipmentTaxIdList", new EasyPostTaxIdentifierType(), True), "desc": "Tax identifiers"},
            "status": {"display_name": "Status", "type": SoftStringOrNothingType, "desc": "Shipment status", "allowed_values": ShipmentStatusAllowedValues},
            "tracker": {"display_name": "Tracker", "type": new EasyPostTrackerType(AutoHashOrNothingType), "desc": "Embedded tracker (if any)"},
            "batch_id": {"display_name": "Batch ID", "type": SoftStringOrNothingType, "desc": "Containing batch ID (if any)"},
            "batch_status": {"display_name": "Batch Status", "type": SoftStringOrNothingType, "desc": "Batch status of this shipment"},
            "batch_message": {"display_name": "Batch Message", "type": SoftStringOrNothingType, "desc": "Batch processing message"},
            "reference": {"display_name": "Reference", "type": SoftStringOrNothingType, "desc": "Caller-supplied reference"},
            "service": {"display_name": "Service", "type": SoftStringOrNothingType, "desc": "Carrier service used"},
            "carrier": {"display_name": "Carrier", "type": SoftStringOrNothingType, "desc": "Carrier used"},
            "created_at": {"display_name": "Created At", "type": SoftDateOrNothingType, "desc": "Creation timestamp"},
            "updated_at": {"display_name": "Updated At", "type": SoftDateOrNothingType, "desc": "Last-update timestamp"},
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace EasyPostDataProvider
