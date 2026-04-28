# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SendCloudParcelCreateRequestDataType.qc request type for creating a SendCloud parcel

/*  SendCloudParcelCreateRequestDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SendCloudDataProvider module
public namespace SendCloudDataProvider {

#! Request type for creating a SendCloud parcel (POST /parcels wrapped in {parcel: ...})
public class SendCloudParcelCreateRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "name": {
                "type": SoftStringType,
                "display_name": "Recipient Name",
                "short_desc": "Full name of the recipient",
                "desc": "Name of the parcel's recipient",
                "required": True,
                "example_value": "John Doe",
            },
            "company_name": {
                "type": SoftStringOrNothingType,
                "display_name": "Company Name",
                "short_desc": "Recipient company name",
                "desc": "Optional company name of the recipient",
            },
            "address": {
                "type": SoftStringType,
                "display_name": "Street Address",
                "short_desc": "Primary recipient street",
                "desc": "Street address of the recipient",
                "required": True,
                "example_value": "Stadhuisplein",
            },
            "address_2": {
                "type": SoftStringOrNothingType,
                "display_name": "Address Line 2",
                "short_desc": "Secondary address line",
                "desc": "Apartment/suite or additional recipient address information",
            },
            "house_number": {
                "type": SoftStringOrNothingType,
                "display_name": "House Number",
                "short_desc": "Recipient house number",
                "desc": "House/street number of the recipient",
                "example_value": "10",
            },
            "city": {
                "type": SoftStringType,
                "display_name": "City",
                "short_desc": "Recipient city",
                "desc": "City of the recipient",
                "required": True,
                "example_value": "Eindhoven",
            },
            "postal_code": {
                "type": SoftStringType,
                "display_name": "Postal Code",
                "short_desc": "Recipient postal code",
                "desc": "Postal or ZIP code of the recipient",
                "required": True,
                "example_value": "5611 EM",
            },
            "country": {
                "type": SoftStringType,
                "display_name": "Country",
                "short_desc": "ISO 3166-1 alpha-2 country code",
                "desc": "Two-letter ISO country code of the recipient",
                "required": True,
                "ref_data": "countries",
                "example_value": "NL",
            },
            "country_state": {
                "type": SoftStringOrNothingType,
                "display_name": "State / Region",
                "short_desc": "Country state or region code",
                "desc": "State or region code (required for some destinations such as US and CA)",
            },
            "telephone": {
                "type": SoftStringOrNothingType,
                "display_name": "Telephone",
                "short_desc": "Recipient phone",
                "desc": "Phone number of the recipient",
            },
            "email": {
                "type": SoftStringOrNothingType,
                "display_name": "Email",
                "short_desc": "Recipient email",
                "desc": "Email address of the recipient",
            },
            "order_number": {
                "type": SoftStringOrNothingType,
                "display_name": "Order Number",
                "short_desc": "Merchant order number",
                "desc": "Order number to display on the label",
                "preselected": True,
            },
            "external_order_id": {
                "type": SoftStringOrNothingType,
                "display_name": "External Order ID",
                "short_desc": "External system order ID",
                "desc": "Order ID in the originating e-commerce system",
            },
            "external_reference": {
                "type": SoftStringOrNothingType,
                "display_name": "External Reference",
                "short_desc": "Free-form reference",
                "desc": "Additional reference attached to the parcel",
            },
            "weight": {
                "type": SoftStringType,
                "display_name": "Weight (kg)",
                "short_desc": "Parcel weight in kilograms",
                "desc": "Total weight of the parcel as a string-encoded decimal (e.g., `\"1.500\"`).  "
                    "The unit defaults to kilograms; override via `weight_unit` (`kg`, `g`, `lb`, `oz`).  "
                    "Required by the SendCloud v3 announce endpoint",
                "required": True,
                "example_value": "1.500",
            },
            "length": {
                "type": SoftStringOrNothingType,
                "display_name": "Length",
                "short_desc": "Parcel length",
                "desc": "Length of the parcel (string-encoded decimal, unit via `length_unit`, e.g., `\"30.0\"`)",
            },
            "width": {
                "type": SoftStringOrNothingType,
                "display_name": "Width",
                "short_desc": "Parcel width",
                "desc": "Width of the parcel (string-encoded decimal, unit via `length_unit`, e.g., `\"20.0\"`)",
            },
            "height": {
                "type": SoftStringOrNothingType,
                "display_name": "Height",
                "short_desc": "Parcel height",
                "desc": "Height of the parcel (string-encoded decimal, unit via `length_unit`, e.g., `\"10.0\"`)",
            },
            "length_unit": {
                "type": SoftStringOrNothingType,
                "display_name": "Length Unit",
                "short_desc": "Length/width/height unit",
                "desc": "Unit used for `length`, `width`, and `height`",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "cm", "display_name": "Centimeters", "desc": "Centimeters"},
                    <AllowedValueInfo>{"value": "m", "display_name": "Meters", "desc": "Meters"},
                    <AllowedValueInfo>{"value": "mm", "display_name": "Millimeters", "desc": "Millimeters"},
                    <AllowedValueInfo>{"value": "in", "display_name": "Inches", "desc": "Inches"},
                ),
            },
            "weight_unit": {
                "type": SoftStringOrNothingType,
                "display_name": "Weight Unit",
                "short_desc": "Weight unit",
                "desc": "Unit used for the `weight` field",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "kg", "display_name": "Kilograms", "desc": "Kilograms"},
                    <AllowedValueInfo>{"value": "g", "display_name": "Grams", "desc": "Grams"},
                    <AllowedValueInfo>{"value": "lb", "display_name": "Pounds", "desc": "Pounds"},
                    <AllowedValueInfo>{"value": "oz", "display_name": "Ounces", "desc": "Ounces"},
                ),
            },
            "total_order_value": {
                "type": SoftStringOrNothingType,
                "display_name": "Total Order Value",
                "short_desc": "Order value (string-encoded decimal)",
                "desc": "Total order value as a string-encoded decimal; pair with `total_order_value_currency`",
            },
            "total_order_value_currency": {
                "type": SoftStringOrNothingType,
                "display_name": "Order Value Currency",
                "short_desc": "ISO 4217 currency code",
                "desc": "Currency of the order value (e.g., EUR, USD)",
            },
            "shipping_method": {
                "type": SoftIntType,
                "display_name": "Shipping Method",
                "short_desc": "Shipping method ID",
                "desc": "Numeric ID of the SendCloud shipping method to use for this parcel.  "
                    "Required by the v3 announce endpoint (translated to a `shipping_option_code` "
                    "via the SendCloud compatibility endpoint).  Use `list-shipping-methods` to "
                    "discover available IDs for your account; `8` (*Unstamped letter*) is the "
                    "no-billing test method",
                "required": True,
                "ref_data": "shipping_methods",
                "example_value": 8,
            },
            "sender_address": {
                "type": SoftIntOrNothingType,
                "display_name": "Sender Address",
                "short_desc": "Sender address ID",
                "desc": "ID of the sender address. Omit to use the default sender address",
                "ref_data": "sender_addresses",
            },
            "brand": {
                "type": SoftIntOrNothingType,
                "display_name": "Brand",
                "short_desc": "Brand ID",
                "desc": "ID of the brand associated with this parcel (must be linked to the chosen sender address)",
                "ref_data": "brands",
            },
            "request_label": {
                "type": SoftBoolOrNothingType,
                "display_name": "Request Label",
                "short_desc": "Immediately announce & request a label",
                "desc": "If `True`, the parcel is announced to the carrier and a label is generated immediately",
                "preselected": True,
            },
            "apply_shipping_rules": {
                "type": SoftBoolOrNothingType,
                "display_name": "Apply Shipping Rules",
                "short_desc": "Apply SendCloud shipping rules",
                "desc": "If `True`, SendCloud applies configured shipping rules to choose a shipping method",
            },
            "to_service_point": {
                "type": SoftIntOrNothingType,
                "display_name": "Destination Service Point",
                "short_desc": "Service point ID the parcel is sent to",
                "desc": "ID of the service point the parcel will be delivered to (for service-point shipments)",
            },
            "to_post_number": {
                "type": SoftStringOrNothingType,
                "display_name": "Post Number",
                "short_desc": "Destination post number",
                "desc": "Post number used for the destination service point (when required)",
            },
            "is_return": {
                "type": SoftBoolOrNothingType,
                "display_name": "Is Return",
                "short_desc": "Whether this parcel is a return",
                "desc": "If `True`, mark this parcel as a return shipment",
            },
            "customs_invoice_nr": {
                "type": SoftStringOrNothingType,
                "display_name": "Customs Invoice Number",
                "short_desc": "Customs invoice number",
                "desc": "Invoice number to include on customs declarations (international shipments)",
            },
            "customs_shipment_type": {
                "type": SoftIntOrNothingType,
                "display_name": "Customs Shipment Type",
                "short_desc": "Customs shipment type code",
                "desc": "Customs shipment type; determines the CN22/CN23 declaration category",
                "allowed_values": (
                    <AllowedValueInfo>{"value": 0, "display_name": "Gift", "desc": "Gift"},
                    <AllowedValueInfo>{"value": 1, "display_name": "Documents", "desc": "Documents"},
                    <AllowedValueInfo>{"value": 2, "display_name": "Commercial Goods", "desc": "Commercial goods"},
                    <AllowedValueInfo>{"value": 3, "display_name": "Commercial Sample", "desc": "Commercial sample"},
                    <AllowedValueInfo>{"value": 4, "display_name": "Returned Goods", "desc": "Returned goods"},
                ),
            },
            "parcel_items": {
                "type": new ListDataType("ParcelItemsInput", new SendCloudParcelItemDataType(), True),
                "display_name": "Parcel Items",
                "short_desc": "Customs line items",
                "desc": "List of parcel items; required for international shipments that need a customs declaration",
            },
            "total_insured_value": {
                "type": SoftFloatOrNothingType,
                "display_name": "Total Insured Value",
                "short_desc": "Total insured value",
                "desc": "Insured value of the parcel contents in the order currency",
            },
            "data": {
                "type": AutoHashOrNothingType,
                "display_name": "Data",
                "short_desc": "Arbitrary extra parcel data",
                "desc": "Extra arbitrary merchant data attached to the parcel",
            },
            "from_name": {
                "type": SoftStringOrNothingType,
                "display_name": "Sender Name (Override)",
                "short_desc": "Per-parcel sender name override",
                "desc": "If provided, overrides the sender name for this parcel",
            },
            "from_company_name": {
                "type": SoftStringOrNothingType,
                "display_name": "Sender Company (Override)",
                "short_desc": "Per-parcel sender company override",
                "desc": "If provided, overrides the sender company for this parcel",
            },
            "from_address_1": {
                "type": SoftStringOrNothingType,
                "display_name": "Sender Street (Override)",
                "short_desc": "Per-parcel sender street override",
                "desc": "If provided, overrides the sender street line for this parcel",
            },
            "from_address_2": {
                "type": SoftStringOrNothingType,
                "display_name": "Sender Address 2 (Override)",
                "short_desc": "Per-parcel sender address line 2 override",
                "desc": "If provided, overrides the sender address-line-2 for this parcel",
            },
            "from_house_number": {
                "type": SoftStringOrNothingType,
                "display_name": "Sender House Number (Override)",
                "short_desc": "Per-parcel sender house number override",
                "desc": "If provided, overrides the sender house number for this parcel",
            },
            "from_city": {
                "type": SoftStringOrNothingType,
                "display_name": "Sender City (Override)",
                "short_desc": "Per-parcel sender city override",
                "desc": "If provided, overrides the sender city for this parcel",
            },
            "from_postal_code": {
                "type": SoftStringOrNothingType,
                "display_name": "Sender Postal Code (Override)",
                "short_desc": "Per-parcel sender postal-code override",
                "desc": "If provided, overrides the sender postal code for this parcel",
            },
            "from_country": {
                "type": SoftStringOrNothingType,
                "display_name": "Sender Country (Override)",
                "short_desc": "Per-parcel sender country override",
                "desc": "If provided, overrides the sender country (ISO 3166-1 alpha-2) for this parcel",
                "ref_data": "countries",
            },
            "from_country_state": {
                "type": SoftStringOrNothingType,
                "display_name": "Sender State (Override)",
                "short_desc": "Per-parcel sender state override",
                "desc": "If provided, overrides the sender state/region for this parcel",
            },
            "from_telephone": {
                "type": SoftStringOrNothingType,
                "display_name": "Sender Phone (Override)",
                "short_desc": "Per-parcel sender phone override",
                "desc": "If provided, overrides the sender phone number for this parcel",
            },
            "from_email": {
                "type": SoftStringOrNothingType,
                "display_name": "Sender Email (Override)",
                "short_desc": "Per-parcel sender email override",
                "desc": "If provided, overrides the sender email for this parcel",
            },
            "quantity": {
                "type": SoftIntOrNothingType,
                "display_name": "Label Quantity",
                "short_desc": "Number of labels to generate",
                "desc": "Number of labels to generate for this parcel (multi-collo shipments)",
            },
        };
    }

    constructor() : HashDataType("SendCloudParcelCreateRequestDataType") {
        addQoreFields(Fields);
    }
}

#! Request type for updating a SendCloud parcel (PUT /parcels with {parcel: {id, request_label, shipment}})
public class SendCloudParcelUpdateRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": SoftIntType,
                "display_name": "Parcel ID",
                "short_desc": "ID of the parcel to update",
                "desc": "Numeric ID of the existing parcel to update",
                "required": True,
                "ref_data": "parcels",
            },
            "request_label": {
                "type": SoftBoolType,
                "display_name": "Request Label",
                "short_desc": "Announce the parcel and generate a label",
                "desc": "If `True`, announces the parcel to the carrier and generates a shipping label",
                "required": True,
            },
            "shipment": {
                "type": new SendCloudShipmentRefDataType(AutoHashOrNothingType),
                "display_name": "Shipping Method",
                "short_desc": "Shipping method to assign (id + name)",
                "desc": "Shipping method to assign to the parcel before announcing — a hash with `id` "
                    "(required) and optional `name`. Required when `request_label` is `True`",
            },
        };
    }

    constructor() : HashDataType("SendCloudParcelUpdateRequestDataType") {
        addQoreFields(Fields);
    }
}

} # namespace SendCloudDataProvider
