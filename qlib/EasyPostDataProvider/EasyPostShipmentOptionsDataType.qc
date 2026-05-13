# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Allowed values for `delivery_confirmation`
const DeliveryConfirmationAllowedValues = (
    <AllowedValueInfo>{"value": "ADULT_SIGNATURE", "desc": "Adult signature required (USPS, UPS, FedEx)"},
    <AllowedValueInfo>{"value": "ADULT_SIGNATURE_RESTRICTED", "desc": "Adult signature, restricted delivery"},
    <AllowedValueInfo>{"value": "SIGNATURE", "desc": "Signature required"},
    <AllowedValueInfo>{"value": "SIGNATURE_RESTRICTED", "desc": "Signature required, restricted delivery"},
    <AllowedValueInfo>{"value": "NO_SIGNATURE", "desc": "Signature waived"},
    <AllowedValueInfo>{"value": "INDIRECT_SIGNATURE", "desc": "Indirect signature"},
    <AllowedValueInfo>{"value": "DIRECT", "desc": "Direct signature"},
    <AllowedValueInfo>{"value": "STANDARD_SIGNATURE", "desc": "Standard signature"},
    <AllowedValueInfo>{"value": "SERVICE_DEFAULT", "desc": "Use the carrier service default"},
    <AllowedValueInfo>{"value": "DO_NOT_SAFE_DROP", "desc": "Do not safe-drop"},
);

#! Allowed values for `dropoff_type`
const DropoffTypeAllowedValues = (
    <AllowedValueInfo>{"value": "REGULAR_PICKUP", "desc": "Regular daily pickup"},
    <AllowedValueInfo>{"value": "SCHEDULED_PICKUP", "desc": "Scheduled pickup"},
    <AllowedValueInfo>{"value": "RETAIL_LOCATION", "desc": "Drop off at retail location"},
    <AllowedValueInfo>{"value": "STATION", "desc": "Drop off at station"},
    <AllowedValueInfo>{"value": "DROP_BOX", "desc": "Drop in drop-box"},
);

#! Allowed values for `label_format`
const LabelFormatAllowedValues = (
    <AllowedValueInfo>{"value": "PNG", "desc": "PNG image"},
    <AllowedValueInfo>{"value": "PDF", "desc": "PDF document"},
    <AllowedValueInfo>{"value": "ZPL", "desc": "Zebra ZPL"},
    <AllowedValueInfo>{"value": "EPL2", "desc": "Eltron EPL2"},
);

#! Allowed values for `label_size`
const LabelSizeAllowedValues = (
    <AllowedValueInfo>{"value": "4x6", "desc": "4 by 6 inches"},
    <AllowedValueInfo>{"value": "8.5x11", "desc": "8.5 by 11 inches"},
    <AllowedValueInfo>{"value": "4x5", "desc": "4 by 5 inches"},
);

#! Allowed values for `incoterm`
const IncotermAllowedValues = (
    <AllowedValueInfo>{"value": "CFR", "desc": "Cost and Freight"},
    <AllowedValueInfo>{"value": "CIF", "desc": "Cost, Insurance, and Freight"},
    <AllowedValueInfo>{"value": "CIP", "desc": "Carriage and Insurance Paid To"},
    <AllowedValueInfo>{"value": "CPT", "desc": "Carriage Paid To"},
    <AllowedValueInfo>{"value": "DAT", "desc": "Delivered at Terminal"},
    <AllowedValueInfo>{"value": "DAP", "desc": "Delivered at Place"},
    <AllowedValueInfo>{"value": "DDP", "desc": "Delivered Duty Paid"},
    <AllowedValueInfo>{"value": "EXW", "desc": "Ex Works"},
    <AllowedValueInfo>{"value": "FAS", "desc": "Free Alongside Ship"},
    <AllowedValueInfo>{"value": "FCA", "desc": "Free Carrier"},
    <AllowedValueInfo>{"value": "FOB", "desc": "Free on Board"},
);

#! Allowed values for `cod_method`
const CodMethodAllowedValues = (
    <AllowedValueInfo>{"value": "CASH", "desc": "Cash"},
    <AllowedValueInfo>{"value": "CHECK", "desc": "Check"},
    <AllowedValueInfo>{"value": "MONEY_ORDER", "desc": "Money order"},
);

#! Allowed values for `endorsement`
const EndorsementAllowedValues = (
    <AllowedValueInfo>{"value": "ADDRESS_SERVICE_REQUESTED", "desc": "Address service requested"},
    <AllowedValueInfo>{"value": "FORWARDING_SERVICE_REQUESTED", "desc": "Forwarding service requested"},
    <AllowedValueInfo>{"value": "CHANGE_SERVICE_REQUESTED", "desc": "Change service requested"},
    <AllowedValueInfo>{"value": "RETURN_SERVICE_REQUESTED", "desc": "Return service requested"},
    <AllowedValueInfo>{"value": "LEAVE_IF_NO_RESPONSE", "desc": "Leave if no response"},
);

#! Allowed values for `hazmat`
const HazmatAllowedValues = (
    <AllowedValueInfo>{"value": "PRIMARY_CONTAINED", "desc": "Primary, contained"},
    <AllowedValueInfo>{"value": "PRIMARY_PACKED", "desc": "Primary, packed"},
    <AllowedValueInfo>{"value": "PRIMARY", "desc": "Primary"},
    <AllowedValueInfo>{"value": "SECONDARY_CONTAINED", "desc": "Secondary, contained"},
    <AllowedValueInfo>{"value": "SECONDARY_PACKED", "desc": "Secondary, packed"},
    <AllowedValueInfo>{"value": "SECONDARY", "desc": "Secondary"},
    <AllowedValueInfo>{"value": "ORMD", "desc": "ORMD - Other regulated material domestic"},
    <AllowedValueInfo>{"value": "LIMITED_QUANTITY", "desc": "Limited quantity"},
    <AllowedValueInfo>{"value": "LITHIUM", "desc": "Lithium"},
    <AllowedValueInfo>{"value": "AIR_ELIGIBLE_ETHANOL", "desc": "Air-eligible ethanol"},
    <AllowedValueInfo>{"value": "CLASS_1", "desc": "Class 1 (explosives)"},
    <AllowedValueInfo>{"value": "CLASS_2", "desc": "Class 2 (gases)"},
    <AllowedValueInfo>{"value": "CLASS_3", "desc": "Class 3 (flammable liquids)"},
    <AllowedValueInfo>{"value": "CLASS_4", "desc": "Class 4 (flammable solids)"},
    <AllowedValueInfo>{"value": "CLASS_5", "desc": "Class 5 (oxidizers)"},
    <AllowedValueInfo>{"value": "CLASS_6", "desc": "Class 6 (toxic / infectious)"},
    <AllowedValueInfo>{"value": "CLASS_7", "desc": "Class 7 (radioactive)"},
    <AllowedValueInfo>{"value": "CLASS_8", "desc": "Class 8 (corrosives)"},
    <AllowedValueInfo>{"value": "CLASS_9", "desc": "Class 9 (miscellaneous)"},
    <AllowedValueInfo>{"value": "DIVISION_4_1", "desc": "Division 4.1"},
    <AllowedValueInfo>{"value": "DIVISION_4_2", "desc": "Division 4.2"},
    <AllowedValueInfo>{"value": "DIVISION_4_3", "desc": "Division 4.3"},
    <AllowedValueInfo>{"value": "DIVISION_5_1", "desc": "Division 5.1"},
    <AllowedValueInfo>{"value": "DIVISION_5_2", "desc": "Division 5.2"},
    <AllowedValueInfo>{"value": "DIVISION_6_1", "desc": "Division 6.1"},
    <AllowedValueInfo>{"value": "DIVISION_6_2", "desc": "Division 6.2"},
    <AllowedValueInfo>{"value": "GROUND_ONLY", "desc": "Ground only"},
    <AllowedValueInfo>{"value": "LIGHTERS", "desc": "Lighters"},
);

#! Allowed values for `payment.type` and `duty_payment.type`
const PaymentTypeAllowedValues = (
    <AllowedValueInfo>{"value": "SENDER", "desc": "Sender pays"},
    <AllowedValueInfo>{"value": "THIRD_PARTY", "desc": "Third party pays (requires `account`, `country`, `postal_code`)"},
    <AllowedValueInfo>{"value": "RECEIVER", "desc": "Receiver pays (FedEx only)"},
    <AllowedValueInfo>{"value": "COLLECT", "desc": "Collect on delivery (FedEx only)"},
);

#! Payment object used by `payment` and `duty_payment` shipment options
public class EasyPostPaymentType inherits HashDataType {
    public {
        const Fields = {
            "type": {
                "display_name": "Type",
                "short_desc": "Who pays",
                "desc": "Who pays. `RECEIVER` and `COLLECT` are only valid for FedEx; `THIRD_PARTY` requires "
                    "`account`, `country`, and `postal_code`.",
                "type": SoftStringOrNothingType,
                "allowed_values": PaymentTypeAllowedValues,
            },
            "account": {
                "display_name": "Account",
                "short_desc": "Account number",
                "desc": "Account number (required if `type` is `THIRD_PARTY`)",
                "type": SoftStringOrNothingType,
            },
            "country": {
                "display_name": "Country",
                "short_desc": "ISO 2-letter country code",
                "desc": "ISO 2-letter country code (required if `type` is `THIRD_PARTY`)",
                "type": SoftStringOrNothingType,
            },
            "postal_code": {
                "display_name": "Postal Code",
                "short_desc": "Postal code",
                "desc": "Postal code (required if `type` is `THIRD_PARTY`)",
                "type": SoftStringOrNothingType,
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }

    constructor(Type base_type) : HashDataType(base_type) {
        addQoreFields(Fields);
    }
}

#! EasyPost shipment options
public class EasyPostShipmentOptionsType inherits HashDataType {
    public {
        const Fields = {
            "additional_handling": {"display_name": "Additional Handling", "short_desc": "Apply additional handling fees", "type": SoftBoolOrNothingType, "desc": "Set to `True` for additional handling fees"},
            "address_validation_level": {"display_name": "Address Validation Level", "short_desc": "Carrier validation level", "type": SoftStringOrNothingType, "desc": "Carrier-specific address validation level"},
            "alcohol": {"display_name": "Alcohol", "short_desc": "Shipment contains alcohol", "type": SoftBoolOrNothingType, "desc": "Set to `True` if the shipment contains alcohol"},
            "billing_ref": {"display_name": "Billing Reference", "short_desc": "Billing reference", "type": SoftStringOrNothingType, "desc": "Billing reference for the shipment"},
            "by_drone": {"display_name": "By Drone", "short_desc": "Ship by drone", "type": SoftBoolOrNothingType, "desc": "Set to `True` if shipping by drone"},
            "carbon_neutral": {"display_name": "Carbon Neutral", "short_desc": "Carbon-neutral shipment", "type": SoftBoolOrNothingType, "desc": "Set to `True` for a carbon-neutral shipment"},
            "carrier_notification_email": {"display_name": "Carrier Notification Email", "short_desc": "Email for carrier notifications", "type": SoftStringOrNothingType, "desc": "Email address for carrier notifications"},
            "carrier_notification_sms": {"display_name": "Carrier Notification SMS", "short_desc": "SMS for carrier notifications", "type": SoftStringOrNothingType, "desc": "SMS number for carrier notifications"},
            "certified_mail": {"display_name": "Certified Mail", "short_desc": "Send as certified mail", "type": SoftBoolOrNothingType, "desc": "Set to `True` for certified mail"},
            "cod_address_id": {"display_name": "COD Address ID", "short_desc": "Address ID for COD return", "type": SoftStringOrNothingType, "desc": "Address ID for COD return"},
            "cod_amount": {"display_name": "COD Amount", "short_desc": "Cash-on-delivery amount", "type": SoftStringOrNothingType, "desc": "Cash-on-delivery amount (string, e.g. `\"100.00\"`)"},
            "cod_method": {"display_name": "COD Method", "short_desc": "Cash-on-delivery payment method", "type": SoftStringOrNothingType, "desc": "Cash-on-delivery payment method", "allowed_values": CodMethodAllowedValues},
            "content_description": {"display_name": "Content Description", "short_desc": "Description of contents", "type": SoftStringOrNothingType, "desc": "Description of the shipment contents"},
            "currency": {"display_name": "Currency", "short_desc": "ISO 4217 currency code", "type": SoftStringOrNothingType, "desc": "ISO 4217 currency code"},
            "delivery_confirmation": {"display_name": "Delivery Confirmation", "short_desc": "Type of delivery confirmation", "type": SoftStringOrNothingType, "desc": "Type of delivery confirmation requested", "allowed_values": DeliveryConfirmationAllowedValues},
            "delivery_max_datetime": {"display_name": "Delivery Max Datetime", "short_desc": "Latest acceptable delivery", "type": SoftDateOrNothingType, "desc": "Latest acceptable delivery datetime"},
            "delivery_min_datetime": {"display_name": "Delivery Min Datetime", "short_desc": "Earliest acceptable delivery", "type": SoftDateOrNothingType, "desc": "Earliest acceptable delivery datetime"},
            "dropoff_type": {"display_name": "Dropoff Type", "short_desc": "How shipment is dropped off", "type": SoftStringOrNothingType, "desc": "How the shipment will be dropped off", "allowed_values": DropoffTypeAllowedValues},
            "dry_ice": {"display_name": "Dry Ice", "short_desc": "Shipment contains dry ice", "type": SoftBoolOrNothingType, "desc": "Set to `True` if the shipment contains dry ice"},
            "dry_ice_medical": {"display_name": "Dry Ice Medical", "short_desc": "Dry ice is medical-grade", "type": SoftBoolOrNothingType, "desc": "Set to `True` if the dry ice is medical"},
            "dry_ice_weight": {"display_name": "Dry Ice Weight", "short_desc": "Dry ice weight (ounces)", "type": SoftStringOrNothingType, "desc": "Dry ice weight in ounces (string)"},
            "duty_payment": {"display_name": "Duty Payment", "short_desc": "Who pays customs duties", "type": new EasyPostPaymentType(AutoHashOrNothingType), "desc": "Duty payment object (`{type, account}` for the duty payer)"},
            "endorsement": {"display_name": "Endorsement", "short_desc": "USPS endorsement", "type": SoftStringOrNothingType, "desc": "USPS endorsement", "allowed_values": EndorsementAllowedValues},
            "end_shipper_id": {"display_name": "End Shipper ID", "short_desc": "End shipper ID", "type": SoftStringOrNothingType, "desc": "End shipper ID (`es_xxx`)"},
            "freight_charge": {"display_name": "Freight Charge", "short_desc": "Freight charge", "type": SoftFloatOrNothingType, "desc": "Freight charge"},
            "handling_instructions": {"display_name": "Handling Instructions", "short_desc": "Special handling instructions", "type": SoftStringOrNothingType, "desc": "Special handling instructions"},
            "hazmat": {"display_name": "Hazmat", "short_desc": "Hazardous materials class", "type": SoftStringOrNothingType, "desc": "Hazardous materials classification", "allowed_values": HazmatAllowedValues},
            "hold_for_pickup": {"display_name": "Hold For Pickup", "short_desc": "Hold at carrier facility", "type": SoftBoolOrNothingType, "desc": "Set to `True` to hold the shipment at a carrier facility"},
            "incoterm": {"display_name": "Incoterm", "short_desc": "International commercial term", "type": SoftStringOrNothingType, "desc": "International commercial term", "allowed_values": IncotermAllowedValues},
            "invoice_number": {"display_name": "Invoice Number", "short_desc": "Invoice number on customs forms", "type": SoftStringOrNothingType, "desc": "Invoice number printed on customs forms"},
            "label_date": {"display_name": "Label Date", "short_desc": "Date to print on label", "type": SoftDateOrNothingType, "desc": "Date to print on the label"},
            "label_format": {"display_name": "Label Format", "short_desc": "Label output format", "type": SoftStringOrNothingType, "desc": "Output format for shipping labels", "allowed_values": LabelFormatAllowedValues},
            "label_size": {"display_name": "Label Size", "short_desc": "Printed label size", "type": SoftStringOrNothingType, "desc": "Size of the printed label", "allowed_values": LabelSizeAllowedValues},
            "live_animal": {"display_name": "Live Animal", "short_desc": "Live animal type", "type": SoftStringOrNothingType, "desc": "Live animal type"},
            "machinable": {"display_name": "Machinable", "short_desc": "Shipment is USPS-machinable", "type": SoftBoolOrNothingType, "desc": "Set to `True` if the shipment is USPS-machinable"},
            "merchant_id": {"display_name": "Merchant ID", "short_desc": "Carrier merchant ID", "type": SoftStringOrNothingType, "desc": "Carrier merchant ID"},
            "payment": {"display_name": "Payment", "short_desc": "Who pays for shipping", "type": new EasyPostPaymentType(AutoHashOrNothingType), "desc": "Payment object (`{type, account, country, postal_code}`)"},
            "perishable": {"display_name": "Perishable", "short_desc": "Shipment is perishable", "type": SoftBoolOrNothingType, "desc": "Set to `True` for perishable goods"},
            "pickup_max_datetime": {"display_name": "Pickup Max Datetime", "short_desc": "Latest pickup datetime", "type": SoftDateOrNothingType, "desc": "Latest pickup datetime"},
            "pickup_min_datetime": {"display_name": "Pickup Min Datetime", "short_desc": "Earliest pickup datetime", "type": SoftDateOrNothingType, "desc": "Earliest pickup datetime"},
            "print_custom_1": {"display_name": "Print Custom 1", "short_desc": "Custom label field 1", "type": SoftStringOrNothingType, "desc": "Custom field 1 printed on label"},
            "print_custom_2": {"display_name": "Print Custom 2", "short_desc": "Custom label field 2", "type": SoftStringOrNothingType, "desc": "Custom field 2 printed on label"},
            "print_custom_3": {"display_name": "Print Custom 3", "short_desc": "Custom label field 3", "type": SoftStringOrNothingType, "desc": "Custom field 3 printed on label"},
            "print_custom_1_barcode": {"display_name": "Print Custom 1 Barcode", "short_desc": "Render custom field 1 as barcode", "type": SoftBoolOrNothingType, "desc": "Print custom field 1 as a barcode"},
            "print_custom_2_barcode": {"display_name": "Print Custom 2 Barcode", "short_desc": "Render custom field 2 as barcode", "type": SoftBoolOrNothingType, "desc": "Print custom field 2 as a barcode"},
            "print_custom_3_barcode": {"display_name": "Print Custom 3 Barcode", "short_desc": "Render custom field 3 as barcode", "type": SoftBoolOrNothingType, "desc": "Print custom field 3 as a barcode"},
            "print_custom_1_code": {"display_name": "Print Custom 1 Code", "short_desc": "Custom code 1", "type": SoftStringOrNothingType, "desc": "Custom code 1"},
            "print_custom_2_code": {"display_name": "Print Custom 2 Code", "short_desc": "Custom code 2", "type": SoftStringOrNothingType, "desc": "Custom code 2"},
            "print_custom_3_code": {"display_name": "Print Custom 3 Code", "short_desc": "Custom code 3", "type": SoftStringOrNothingType, "desc": "Custom code 3"},
            "registered_mail": {"display_name": "Registered Mail", "short_desc": "Send as registered mail", "type": SoftBoolOrNothingType, "desc": "Set to `True` for registered mail"},
            "registered_mail_amount": {"display_name": "Registered Mail Amount", "short_desc": "Registered mail declared value", "type": SoftFloatOrNothingType, "desc": "Declared value for registered mail"},
            "return_receipt": {"display_name": "Return Receipt", "short_desc": "Request return receipt", "type": SoftBoolOrNothingType, "desc": "Set to `True` to request a return receipt"},
            "saturday_delivery": {"display_name": "Saturday Delivery", "short_desc": "Request Saturday delivery", "type": SoftBoolOrNothingType, "desc": "Set to `True` for Saturday delivery"},
            "smartpost_hub": {"display_name": "Smartpost Hub", "short_desc": "FedEx SmartPost hub ID", "type": SoftStringOrNothingType, "desc": "FedEx SmartPost hub ID"},
            "smartpost_manifest": {"display_name": "Smartpost Manifest", "short_desc": "FedEx SmartPost manifest ID", "type": SoftStringOrNothingType, "desc": "FedEx SmartPost manifest ID"},
            "special_rates_eligibility": {"display_name": "Special Rates Eligibility", "short_desc": "Special rates eligibility", "type": SoftStringOrNothingType, "desc": "Special rates eligibility flag"},
            "suppress_etd": {"display_name": "Suppress ETD", "short_desc": "Suppress electronic trade docs", "type": SoftBoolOrNothingType, "desc": "Set to `True` to suppress electronic trade documents"},
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
