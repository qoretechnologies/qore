# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShippoDataProvider module definition

/*  ShippoShipmentExtraDataType.qc Copyright 2026 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#! Contains all public definitions in the ShippoDataProvider module
public namespace ShippoDataProvider {

#! Shippo label reference field data type (used for customer_reference, dept_number, invoice_number, etc.)
public class ShippoLabelReferenceFieldDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "prefix": {
                "type": SoftStringOrNothingType,
                "display_name": "Prefix",
                "short_desc": "Custom prefix text",
                "desc": "Custom prefix for the reference field (ZPL labels only). Up to 11 characters. "
                    "Empty string removes the default prefix. Omit to use the default prefix",
            },
            "value": {
                "type": SoftStringOrNothingType,
                "display_name": "Value",
                "short_desc": "Reference text",
                "desc": "Reference text to be printed on the shipping label. Up to 40 characters",
            },
            "ref_sort": {
                "type": SoftIntOrNothingType,
                "display_name": "Sort Order",
                "short_desc": "UPS reference sort order",
                "desc": "Order UPS reference fields are printed on ZPL labels. If set for one reference, "
                    "must be set for all other supported UPS references using unique integers",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("ShippoLabelReferenceFieldDataType") {
        addQoreFields(Fields);
    }

    #! Creates the object with a base type (for optional fields)
    constructor(Type base_type) : HashDataType(base_type, "ShippoLabelReferenceFieldDataType") {
        addQoreFields(Fields);
    }
}

#! Shippo insurance data type
public class ShippoInsuranceDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "amount": {
                "type": SoftStringOrNothingType,
                "display_name": "Amount",
                "short_desc": "Declared value to insure",
                "desc": "Declared value of the goods you want to insure",
                "example_value": "5.50",
            },
            "content": {
                "type": SoftStringOrNothingType,
                "display_name": "Content",
                "short_desc": "Description of package content",
                "desc": "Description of package content",
            },
            "currency": {
                "type": SoftStringOrNothingType,
                "display_name": "Currency",
                "short_desc": "Currency code",
                "desc": "Currency for the amount value (e.g., `USD`). Currently only USD is supported for "
                    "FedEx and UPS",
                "example_value": "USD",
            },
            "provider": {
                "type": SoftStringOrNothingType,
                "display_name": "Provider",
                "short_desc": "Insurance provider",
                "desc": "Set to `FEDEX`, `UPS`, or `ONTRAC` to use carrier-provided insurance instead of "
                    "Shippo's provider (XCover)",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "FEDEX", "desc": "FedEx carrier insurance"},
                    <AllowedValueInfo>{"value": "UPS", "desc": "UPS carrier insurance"},
                    <AllowedValueInfo>{"value": "ONTRAC", "desc": "OnTrac carrier insurance"},
                ),
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("ShippoInsuranceDataType") {
        addQoreFields(Fields);
    }

    #! Creates the object with a base type (for optional fields)
    constructor(Type base_type) : HashDataType(base_type, "ShippoInsuranceDataType") {
        addQoreFields(Fields);
    }
}

#! Shippo COD (collect on delivery) data type
public class ShippoCodDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "amount": {
                "type": SoftStringOrNothingType,
                "display_name": "Amount",
                "short_desc": "Amount to collect",
                "desc": "Amount to be collected on delivery",
                "example_value": "5.50",
            },
            "currency": {
                "type": SoftStringOrNothingType,
                "display_name": "Currency",
                "short_desc": "Currency code",
                "desc": "Currency for the amount to be collected. Currently only USD is supported for UPS",
                "example_value": "USD",
            },
            "payment_method": {
                "type": SoftStringOrNothingType,
                "display_name": "Payment Method",
                "short_desc": "Payment method for COD",
                "desc": "Payment method for collection on delivery. Defaults to `ANY` if not specified",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "SECURED_FUNDS", "desc": "Secured funds (money orders, "
                        "certified cheques)"},
                    <AllowedValueInfo>{"value": "CASH", "desc": "Cash on delivery"},
                    <AllowedValueInfo>{"value": "ANY", "desc": "Any payment method"},
                ),
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("ShippoCodDataType") {
        addQoreFields(Fields);
    }

    #! Creates the object with a base type (for optional fields)
    constructor(Type base_type) : HashDataType(base_type, "ShippoCodDataType") {
        addQoreFields(Fields);
    }
}

#! Shippo billing data type
public class ShippoBillingDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "account": {
                "type": SoftStringOrNothingType,
                "display_name": "Account",
                "short_desc": "Account number to bill",
                "desc": "Account number to be billed. For DHL Germany, leave this field blank",
            },
            "country": {
                "type": SoftStringOrNothingType,
                "display_name": "Country",
                "short_desc": "Account country code",
                "desc": "Two-letter ISO country code of the account to be billed (required for UPS third "
                    "party billing only)",
            },
            "participation_code": {
                "type": SoftStringOrNothingType,
                "display_name": "Participation Code",
                "short_desc": "DHL participation code",
                "desc": "2-digit code used to override your default participation code associated with your "
                    "DHL Germany account",
            },
            "type": {
                "type": SoftStringOrNothingType,
                "display_name": "Billing Type",
                "short_desc": "Party to be billed",
                "desc": "Party to be billed. Leave blank for DHL Germany",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "SENDER", "desc": "Bill the sender"},
                    <AllowedValueInfo>{"value": "RECIPIENT", "desc": "Bill the recipient"},
                    <AllowedValueInfo>{"value": "THIRD_PARTY", "desc": "Bill a third party"},
                    <AllowedValueInfo>{"value": "THIRD_PARTY_CONSIGNEE",
                        "desc": "Bill a third party consignee"},
                    <AllowedValueInfo>{"value": "COLLECT", "desc": "Collect on delivery"},
                ),
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("ShippoBillingDataType") {
        addQoreFields(Fields);
    }

    #! Creates the object with a base type (for optional fields)
    constructor(Type base_type) : HashDataType(base_type, "ShippoBillingDataType") {
        addQoreFields(Fields);
    }
}

#! Shippo dry ice data type
public class ShippoDryIceDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "contains_dry_ice": {
                "type": SoftBoolOrNothingType,
                "display_name": "Contains Dry Ice",
                "short_desc": "Package contains dry ice",
                "desc": "Specifies that the package contains dry ice (FedEx, Veho, and UPS only)",
            },
            "weight": {
                "type": SoftStringOrNothingType,
                "display_name": "Weight",
                "short_desc": "Dry ice weight in kg",
                "desc": "Weight of dry ice in kilograms. Cannot be greater than the package weight",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("ShippoDryIceDataType") {
        addQoreFields(Fields);
    }

    #! Creates the object with a base type (for optional fields)
    constructor(Type base_type) : HashDataType(base_type, "ShippoDryIceDataType") {
        addQoreFields(Fields);
    }
}

#! Shippo alcohol data type
public class ShippoAlcoholDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "contains_alcohol": {
                "type": SoftBoolOrNothingType,
                "display_name": "Contains Alcohol",
                "short_desc": "Package contains alcohol",
                "desc": "Specifies that the package contains alcohol (FedEx and UPS only)",
            },
            "recipient_type": {
                "type": SoftStringOrNothingType,
                "display_name": "Recipient Type",
                "short_desc": "Alcohol recipient type",
                "desc": "License type of the recipient (mandatory for FedEx only)",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "licensee", "desc": "Licensed recipient"},
                    <AllowedValueInfo>{"value": "consumer", "desc": "Consumer recipient"},
                ),
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("ShippoAlcoholDataType") {
        addQoreFields(Fields);
    }

    #! Creates the object with a base type (for optional fields)
    constructor(Type base_type) : HashDataType(base_type, "ShippoAlcoholDataType") {
        addQoreFields(Fields);
    }
}

#! Shippo shipment extra data type (additional shipment options)
public class ShippoShipmentExtraDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "insurance": {
                "type": new ShippoInsuranceDataType(AutoHashOrNothingType),
                "display_name": "Insurance",
                "short_desc": "Shipping insurance",
                "desc": "Insurance details. Specify `amount`, `content`, and `currency` for XCover "
                    "insurance, or additionally set `provider` for carrier-provided insurance",
            },
            "signature_confirmation": {
                "type": SoftStringOrNothingType,
                "display_name": "Signature Confirmation",
                "short_desc": "Signature required on delivery",
                "desc": "Request standard or adult signature confirmation on delivery",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "STANDARD", "desc": "Standard signature"},
                    <AllowedValueInfo>{"value": "ADULT", "desc": "Adult signature"},
                    <AllowedValueInfo>{"value": "CERTIFIED", "desc": "Certified mail (USPS only)"},
                    <AllowedValueInfo>{"value": "INDIRECT", "desc": "Indirect signature (FedEx only)"},
                    <AllowedValueInfo>{"value": "CARRIER_CONFIRMATION",
                        "desc": "Carrier confirmation (Deutsche Post only)"},
                ),
            },
            "authority_to_leave": {
                "type": SoftBoolOrNothingType,
                "display_name": "Authority to Leave",
                "short_desc": "Allow leaving parcel if absent",
                "desc": "Set to `True` to give the carrier permission to leave the parcel in a safe place "
                    "if no one answers the door. When `False`, the parcel will not be left if no one is "
                    "available (surcharges may apply)",
            },
            "saturday_delivery": {
                "type": SoftBoolOrNothingType,
                "display_name": "Saturday Delivery",
                "short_desc": "Saturday delivery requested",
                "desc": "Marks shipment as to be delivered on a Saturday",
            },
            "is_return": {
                "type": SoftBoolOrNothingType,
                "display_name": "Is Return",
                "short_desc": "Return shipment flag",
                "desc": "Specifies if this is a scan-based return shipment",
            },
            "COD": {
                "type": new ShippoCodDataType(AutoHashOrNothingType),
                "display_name": "COD",
                "short_desc": "Collect on delivery",
                "desc": "Collection on delivery details (UPS only)",
            },
            "billing": {
                "type": new ShippoBillingDataType(AutoHashOrNothingType),
                "display_name": "Billing",
                "short_desc": "Third-party billing",
                "desc": "Third-party billing details (UPS, FedEx, and DHL Germany only)",
            },
            "dry_ice": {
                "type": new ShippoDryIceDataType(AutoHashOrNothingType),
                "display_name": "Dry Ice",
                "short_desc": "Dry ice declaration",
                "desc": "Dry ice details (FedEx, Veho, and UPS only)",
            },
            "alcohol": {
                "type": new ShippoAlcoholDataType(AutoHashOrNothingType),
                "display_name": "Alcohol",
                "short_desc": "Alcohol shipment",
                "desc": "Alcohol shipment details (FedEx and UPS only)",
            },
            "dangerous_goods_code": {
                "type": SoftStringOrNothingType,
                "display_name": "Dangerous Goods Code",
                "short_desc": "DHL dangerous goods code",
                "desc": "Dangerous goods code (DHL eCommerce only)",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "01", "desc": "Category 01"},
                    <AllowedValueInfo>{"value": "02", "desc": "Category 02"},
                    <AllowedValueInfo>{"value": "03", "desc": "Category 03"},
                    <AllowedValueInfo>{"value": "04", "desc": "Category 04"},
                    <AllowedValueInfo>{"value": "05", "desc": "Category 05"},
                    <AllowedValueInfo>{"value": "06", "desc": "Category 06"},
                    <AllowedValueInfo>{"value": "07", "desc": "Category 07"},
                    <AllowedValueInfo>{"value": "08", "desc": "Category 08"},
                    <AllowedValueInfo>{"value": "09", "desc": "Category 09"},
                ),
            },
            "bypass_address_validation": {
                "type": SoftBoolOrNothingType,
                "display_name": "Bypass Address Validation",
                "short_desc": "Skip address validation",
                "desc": "Bypasses address validation (USPS, UPS, and LaserShip only)",
            },
            "carbon_neutral": {
                "type": SoftBoolOrNothingType,
                "display_name": "Carbon Neutral",
                "short_desc": "Request carbon offsets",
                "desc": "Request carbon offsets (UPS only)",
            },
            "request_retail_rates": {
                "type": SoftBoolOrNothingType,
                "display_name": "Request Retail Rates",
                "short_desc": "Return retail rates",
                "desc": "Returns retail rates instead of account-based rates (UPS and FedEx only)",
            },
            "container_type": {
                "type": SoftStringOrNothingType,
                "display_name": "Container Type",
                "short_desc": "Specify container type",
                "desc": "Specify the container type for the shipment",
            },
            "reference_1": {
                "type": SoftStringOrNothingType,
                "display_name": "Reference 1",
                "short_desc": "Label reference text 1",
                "desc": "Optional text (up to 50 characters) printed on the shipping label if supported by "
                    "the carrier",
            },
            "reference_2": {
                "type": SoftStringOrNothingType,
                "display_name": "Reference 2",
                "short_desc": "Label reference text 2",
                "desc": "Optional text (up to 50 characters) printed on the shipping label if supported by "
                    "the carrier. For DHL eCommerce, can be used for billing reference",
            },
            "customer_reference": {
                "type": new ShippoLabelReferenceFieldDataType(AutoHashOrNothingType),
                "display_name": "Customer Reference",
                "short_desc": "Customer reference field",
                "desc": "Customer reference field on the label (FedEx and UPS only). Contains "
                    "`prefix`, `value`, and `ref_sort` keys",
            },
            "dept_number": {
                "type": new ShippoLabelReferenceFieldDataType(AutoHashOrNothingType),
                "display_name": "Department Number",
                "short_desc": "Department number field",
                "desc": "Department number field on the label (FedEx and UPS only). Contains "
                    "`prefix`, `value`, and `ref_sort` keys",
            },
            "invoice_number": {
                "type": new ShippoLabelReferenceFieldDataType(AutoHashOrNothingType),
                "display_name": "Invoice Number",
                "short_desc": "Invoice number field",
                "desc": "Invoice number field on the label (FedEx and UPS only). Contains "
                    "`prefix`, `value`, and `ref_sort` keys",
            },
            "po_number": {
                "type": new ShippoLabelReferenceFieldDataType(AutoHashOrNothingType),
                "display_name": "PO Number",
                "short_desc": "Purchase order number",
                "desc": "Purchase order number field on the label (FedEx and UPS only). Contains "
                    "`prefix`, `value`, and `ref_sort` keys",
            },
            "rma_number": {
                "type": new ShippoLabelReferenceFieldDataType(AutoHashOrNothingType),
                "display_name": "RMA Number",
                "short_desc": "Return authorization number",
                "desc": "Return merchandise authorization number on the label (FedEx and UPS only). "
                    "Contains `prefix`, `value`, and `ref_sort` keys",
            },
            "delivery_instructions": {
                "type": SoftStringOrNothingType,
                "display_name": "Delivery Instructions",
                "short_desc": "Delivery instructions",
                "desc": "Delivery instructions (up to 500 characters, FedEx and OnTrac only)",
            },
            "qr_code_requested": {
                "type": SoftBoolOrNothingType,
                "display_name": "QR Code Requested",
                "short_desc": "Request QR code for label",
                "desc": "Request a QR code for the shipping label (USPS domestic and Evri UK only)",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("ShippoShipmentExtraDataType") {
        addQoreFields(Fields);
    }

    #! Creates the object with a base type (for optional fields)
    constructor(Type base_type) : HashDataType(base_type, "ShippoShipmentExtraDataType") {
        addQoreFields(Fields);
    }
}

} # namespace ShippoDataProvider
