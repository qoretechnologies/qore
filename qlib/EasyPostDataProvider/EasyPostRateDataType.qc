# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! EasyPost rate (response-only): a quoted price for a shipment + carrier service
public class EasyPostRateType inherits HashDataType {
    public {
        const Fields = {
            "id": {"display_name": "ID", "type": SoftStringOrNothingType, "desc": "Rate ID (`rate_xxx`)"},
            "object": {"display_name": "Object Type", "type": SoftStringOrNothingType, "desc": "Always `Rate`"},
            "mode": {"display_name": "Mode", "type": SoftStringOrNothingType, "desc": "`production` or `test`"},
            "service": {"display_name": "Service", "type": SoftStringOrNothingType, "desc": "Carrier service code (e.g. `Priority`, `Ground`)"},
            "carrier": {"display_name": "Carrier", "type": SoftStringOrNothingType, "desc": "Carrier name (e.g. `USPS`, `FedEx`)"},
            "carrier_account_id": {"display_name": "Carrier Account ID", "type": SoftStringOrNothingType, "desc": "Carrier account ID (`ca_xxx`)"},
            "shipment_id": {"display_name": "Shipment ID", "type": SoftStringOrNothingType, "desc": "Shipment ID (`shp_xxx`)"},
            "rate": {"display_name": "Rate", "type": SoftStringOrNothingType, "desc": "Cost as a string (e.g. `\"7.85\"`)"},
            "currency": {"display_name": "Currency", "type": SoftStringOrNothingType, "desc": "ISO currency code"},
            "retail_rate": {"display_name": "Retail Rate", "type": SoftStringOrNothingType, "desc": "Retail price"},
            "retail_currency": {"display_name": "Retail Currency", "type": SoftStringOrNothingType, "desc": "Retail rate currency"},
            "list_rate": {"display_name": "List Rate", "type": SoftStringOrNothingType, "desc": "List price"},
            "list_currency": {"display_name": "List Currency", "type": SoftStringOrNothingType, "desc": "List rate currency"},
            "billing_type": {"display_name": "Billing Type", "type": SoftStringOrNothingType, "desc": "Billing type"},
            "delivery_days": {"display_name": "Delivery Days", "type": SoftIntOrNothingType, "desc": "Estimated delivery days"},
            "delivery_date": {"display_name": "Delivery Date", "type": SoftDateOrNothingType, "desc": "Estimated delivery date"},
            "delivery_date_guaranteed": {"display_name": "Delivery Date Guaranteed", "type": SoftBoolOrNothingType, "desc": "Whether the delivery date is guaranteed"},
            "est_delivery_days": {"display_name": "Est. Delivery Days", "type": SoftIntOrNothingType, "desc": "Estimated delivery days"},
            "created_at": {"display_name": "Created At", "type": SoftDateOrNothingType, "desc": "Creation timestamp"},
            "updated_at": {"display_name": "Updated At", "type": SoftDateOrNothingType, "desc": "Last-update timestamp"},
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace EasyPostDataProvider
