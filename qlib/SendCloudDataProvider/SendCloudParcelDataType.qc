# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SendCloudParcelDataType.qc SendCloud parcel response data type

/*  SendCloudParcelDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! SendCloud parcel response type (full parcel record)
public class SendCloudParcelDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": SoftIntOrNothingType,
                "display_name": "ID",
                "short_desc": "Parcel ID",
                "desc": "Unique numeric identifier of the parcel",
            },
            "name": {
                "type": SoftStringOrNothingType,
                "display_name": "Recipient Name",
                "short_desc": "Name of the recipient",
                "desc": "Full name of the parcel's recipient",
            },
            "company_name": {
                "type": SoftStringOrNothingType,
                "display_name": "Company",
                "short_desc": "Recipient company",
                "desc": "Recipient company name",
            },
            "address": {
                "type": SoftStringOrNothingType,
                "display_name": "Street Address",
                "short_desc": "Primary street address",
                "desc": "The street name of the recipient",
            },
            "address_2": {
                "type": SoftStringOrNothingType,
                "display_name": "Address Line 2",
                "short_desc": "Secondary address line",
                "desc": "Additional recipient address line",
            },
            "house_number": {
                "type": SoftStringOrNothingType,
                "display_name": "House Number",
                "short_desc": "Street number",
                "desc": "House or street number of the recipient",
            },
            "city": {
                "type": SoftStringOrNothingType,
                "display_name": "City",
                "short_desc": "City",
                "desc": "Recipient city",
            },
            "postal_code": {
                "type": SoftStringOrNothingType,
                "display_name": "Postal Code",
                "short_desc": "ZIP / postal code",
                "desc": "Recipient postal code",
            },
            "country": {
                "type": new SendCloudCountryObjectDataType(AutoHashOrNothingType),
                "display_name": "Country",
                "short_desc": "Country details",
                "desc": "Country object with `iso_2`, `iso_3`, and `name` as returned by SendCloud",
            },
            "country_state": {
                "type": SoftStringOrNothingType,
                "display_name": "State / Region",
                "short_desc": "State or region code",
                "desc": "State or region code of the recipient",
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
                "desc": "Email of the recipient",
            },
            "order_number": {
                "type": SoftStringOrNothingType,
                "display_name": "Order Number",
                "short_desc": "Customer-visible order number",
                "desc": "Merchant order number associated with this parcel",
            },
            "external_order_id": {
                "type": SoftStringOrNothingType,
                "display_name": "External Order ID",
                "short_desc": "External system order ID",
                "desc": "Reference order ID from the originating system",
            },
            "external_reference": {
                "type": SoftStringOrNothingType,
                "display_name": "External Reference",
                "short_desc": "Free-form external reference",
                "desc": "Extra reference attached to the parcel by the merchant",
            },
            "tracking_number": {
                "type": SoftStringOrNothingType,
                "display_name": "Tracking Number",
                "short_desc": "Carrier tracking number",
                "desc": "The tracking number for the parcel",
            },
            "tracking_url": {
                "type": SoftStringOrNothingType,
                "display_name": "Tracking URL",
                "short_desc": "Carrier tracking URL",
                "desc": "The tracking URL for the parcel",
            },
            "carrier": {
                "type": new SendCloudCarrierObjectDataType(AutoHashOrNothingType),
                "display_name": "Carrier",
                "short_desc": "Carrier info",
                "desc": "Carrier object with at least a `code` field (e.g., `{\"code\": \"postnl\"}`)",
            },
            "weight": {
                "type": SoftStringOrNothingType,
                "display_name": "Weight",
                "short_desc": "Parcel weight (kg)",
                "desc": "Weight of the parcel in kilograms (string-encoded decimal)",
            },
            "length": {
                "type": SoftStringOrNothingType,
                "display_name": "Length",
                "short_desc": "Parcel length (cm)",
                "desc": "Length of the parcel (string-encoded decimal)",
            },
            "width": {
                "type": SoftStringOrNothingType,
                "display_name": "Width",
                "short_desc": "Parcel width (cm)",
                "desc": "Width of the parcel (string-encoded decimal)",
            },
            "height": {
                "type": SoftStringOrNothingType,
                "display_name": "Height",
                "short_desc": "Parcel height (cm)",
                "desc": "Height of the parcel (string-encoded decimal)",
            },
            "length_unit": {
                "type": SoftStringOrNothingType,
                "display_name": "Length Unit",
                "short_desc": "Dimension unit",
                "desc": "Unit used for `length`/`width`/`height` (e.g., `cm`, `in`)",
            },
            "weight_unit": {
                "type": SoftStringOrNothingType,
                "display_name": "Weight Unit",
                "short_desc": "Weight unit",
                "desc": "Unit used for `weight` (e.g., `kg`, `g`, `lb`, `oz`)",
            },
            "total_insured_value": {
                "type": SoftFloatOrNothingType,
                "display_name": "Insured Value",
                "short_desc": "Insured value",
                "desc": "Total insured value of the parcel contents",
            },
            "total_order_value": {
                "type": SoftStringOrNothingType,
                "display_name": "Order Value",
                "short_desc": "Order value (string-encoded)",
                "desc": "Total order value (string-encoded decimal)",
            },
            "total_order_value_currency": {
                "type": SoftStringOrNothingType,
                "display_name": "Order Value Currency",
                "short_desc": "ISO 4217 currency code",
                "desc": "Currency code of the order value (e.g., EUR, USD)",
            },
            "shipment": {
                "type": new SendCloudShipmentRefDataType(),
                "display_name": "Shipping Method",
                "short_desc": "Chosen shipping method",
                "desc": "The shipping method used for the parcel (reference to shipping_methods)",
            },
            "requested_label_amount": {
                "type": SoftIntOrNothingType,
                "display_name": "Requested Label Amount",
                "short_desc": "Number of labels requested",
                "desc": "How many labels were requested for this parcel (for multi-collo)",
            },
            "status": {
                "type": new SendCloudParcelStatusDataType(),
                "display_name": "Status",
                "short_desc": "Parcel status",
                "desc": "Current status of the parcel",
            },
            "data": {
                "type": AutoHashOrNothingType,
                "display_name": "Data",
                "short_desc": "Extra parcel data",
                "desc": "Arbitrary merchant-supplied data attached to the parcel",
            },
            "label": {
                "type": new SendCloudLabelUrlsDataType(AutoHashOrNothingType),
                "display_name": "Label",
                "short_desc": "Label URLs hash",
                "desc": "Object with `normal_printer` (list of A4 PDF URLs) and `label_printer` (single A6 PDF URL)",
            },
            "documents": {
                "type": new ListDataType("ParcelDocumentList", new SendCloudParcelDocumentDataType(), True),
                "display_name": "Documents",
                "short_desc": "Available documents",
                "desc": "List of available documents (label, customs forms, etc.)",
            },
            "customs_invoice_nr": {
                "type": SoftStringOrNothingType,
                "display_name": "Customs Invoice Number",
                "short_desc": "Customs invoice number",
                "desc": "Customs invoice number for international shipments",
            },
            "customs_shipment_type": {
                "type": SoftIntOrNothingType,
                "display_name": "Customs Shipment Type",
                "short_desc": "Customs shipment type code",
                "desc": "Customs shipment type (0=Gift, 1=Documents, 2=Commercial Goods, 3=Commercial Sample, 4=Returned Goods)",
            },
            "parcel_items": {
                "type": new ListDataType("ParcelItemList", new SendCloudParcelItemDataType(), True),
                "display_name": "Parcel Items",
                "short_desc": "Customs line items",
                "desc": "List of items for customs declarations",
            },
            "sender_address": {
                "type": SoftIntOrNothingType,
                "display_name": "Sender Address ID",
                "short_desc": "Sender address ID",
                "desc": "ID of the sender address used for this parcel",
            },
            "brand": {
                "type": SoftIntOrNothingType,
                "display_name": "Brand ID",
                "short_desc": "Brand ID",
                "desc": "ID of the brand associated with this parcel",
            },
            "to_service_point": {
                "type": SoftIntOrNothingType,
                "display_name": "Service Point ID",
                "short_desc": "Destination service point",
                "desc": "ID of the destination service point (for service-point shipments)",
            },
            "to_post_number": {
                "type": SoftStringOrNothingType,
                "display_name": "Post Number",
                "short_desc": "Destination post number",
                "desc": "Post number for the destination service point (when applicable)",
            },
            "is_return": {
                "type": SoftBoolOrNothingType,
                "display_name": "Is Return",
                "short_desc": "Whether this parcel is a return",
                "desc": "`True` if this parcel is a return shipment",
            },
            "date_created": {
                "type": SoftDateOrNothingType,
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "desc": "Timestamp when the parcel was created",
            },
            "date_updated": {
                "type": SoftDateOrNothingType,
                "display_name": "Updated At",
                "short_desc": "Last update timestamp",
                "desc": "Timestamp when the parcel was last updated",
            },
            "date_announced": {
                "type": SoftDateOrNothingType,
                "display_name": "Announced At",
                "short_desc": "Announcement timestamp",
                "desc": "Timestamp when the parcel was announced to the carrier",
            },
            "collo_count": {
                "type": SoftIntOrNothingType,
                "display_name": "Collo Count",
                "short_desc": "Multi-collo parcel count",
                "desc": "Number of collo (pieces) in this shipment",
            },
            "collo_nr": {
                "type": SoftIntOrNothingType,
                "display_name": "Collo Number",
                "short_desc": "Collo index in the shipment",
                "desc": "Index of this parcel within a multi-collo shipment",
            },
        };
    }

    constructor() : HashDataType("SendCloudParcelDataType") {
        addQoreFields(Fields);
    }
}

} # namespace SendCloudDataProvider
