# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AftershipDataProvider module definition

/** AftershipCreateTrackingRequestDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the AftershipDataProvider module
public namespace AftershipDataProvider {
#! Request type for creating a tracking
public class AftershipCreateTrackingRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "tracking_number": {
                "display_name": "Tracking Number",
                "short_desc": "Carrier tracking number",
                "desc": "Carrier tracking number (4-100 characters)",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "1Z999AA10123456784",
            },
            "slug": {
                "display_name": "Courier Code",
                "short_desc": "Unique courier code",
                "desc": "Unique courier code (e.g., `fedex`, `ups`). If not specified, Aftership will auto-detect",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "ups",
            },
            "title": {
                "display_name": "Title",
                "short_desc": "Display name for the tracking",
                "desc": "Display name for the tracking; defaults to the tracking number",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "order_id": {
                "display_name": "Order ID",
                "short_desc": "Global order identifier",
                "desc": "Global order identifier",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "order_number": {
                "display_name": "Order Number",
                "short_desc": "Human-readable order number",
                "desc": "Human-readable order number",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "order_id_path": {
                "display_name": "Order ID Path",
                "short_desc": "URL to order in merchant system",
                "desc": "URL to the order in the merchant's system",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "customer_name": {
                "display_name": "Customer Name",
                "short_desc": "Recipient name",
                "desc": "Name of the package recipient",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "language": {
                "display_name": "Language",
                "short_desc": "Notification language (ISO 639-1)",
                "desc": "ISO 639-1 language code for notifications",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "note": {
                "display_name": "Note",
                "short_desc": "Text notes",
                "desc": "User-provided text notes for this tracking",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "origin_country_region": {
                "display_name": "Origin Country",
                "short_desc": "Origin country code (ISO Alpha-3)",
                "desc": "ISO Alpha-3 origin country code",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "origin_state": {
                "display_name": "Origin State",
                "short_desc": "Origin state or province",
                "desc": "State or province of origin",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "origin_city": {
                "display_name": "Origin City",
                "short_desc": "Origin city",
                "desc": "City of origin",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "origin_postal_code": {
                "display_name": "Origin Postal Code",
                "short_desc": "Origin postal code",
                "desc": "Postal code of origin",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "destination_country_region": {
                "display_name": "Destination Country",
                "short_desc": "Destination country code (ISO Alpha-3)",
                "desc": "ISO Alpha-3 destination country code",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "destination_state": {
                "display_name": "Destination State",
                "short_desc": "Destination state or province",
                "desc": "State or province of destination",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "destination_city": {
                "display_name": "Destination City",
                "short_desc": "Destination city",
                "desc": "City of destination",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "destination_postal_code": {
                "display_name": "Destination Postal Code",
                "short_desc": "Destination postal code",
                "desc": "Postal code of destination",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "delivery_type": {
                "display_name": "Delivery Type",
                "short_desc": "Delivery method",
                "desc": "Delivery method",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Pickup at Store",
                        "value": "pickup_at_store",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Door to Door",
                        "value": "door_to_door",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Pickup at Courier",
                        "value": "pickup_at_courier",
                    },
                ),
            },
            "shipment_type": {
                "display_name": "Shipment Type",
                "short_desc": "Carrier service description",
                "desc": "Carrier service type description",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "tracking_account_number": {
                "display_name": "Tracking Account Number",
                "short_desc": "Carrier account number",
                "desc": "Carrier account number for tracking credentials",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "tracking_key": {
                "display_name": "Tracking Key",
                "short_desc": "Tracking credential",
                "desc": "Tracking credential for certain carriers",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "tracking_ship_date": {
                "display_name": "Ship Date",
                "short_desc": "Ship date",
                "desc": "Ship date for the tracking",
                "type": AbstractDataProviderType::get(DateOrNothingType),
            },
            "order_date": {
                "display_name": "Order Date",
                "short_desc": "Order placement date",
                "desc": "Date when the order was placed",
                "type": AbstractDataProviderType::get(DateOrNothingType),
            },
            "order_promised_delivery_date": {
                "display_name": "Promised Delivery Date",
                "short_desc": "Expected delivery date",
                "desc": "Expected delivery date promised to the customer",
                "type": AbstractDataProviderType::get(DateOrNothingType),
            },
            "slug_group": {
                "display_name": "Courier Group",
                "short_desc": "Courier group code",
                "desc": "Courier group code for multi-slug detection (e.g., `fedex-group`)",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "shipping_method": {
                "display_name": "Shipping Method",
                "short_desc": "Shipping service method",
                "desc": "Shipping service method chosen at checkout",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "location_id": {
                "display_name": "Location ID",
                "short_desc": "Store location ID",
                "desc": "Store location ID; auto-populates origin address fields",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "custom_fields": {
                "display_name": "Custom Fields",
                "short_desc": "Custom key-value data",
                "desc": "Custom key-value string pairs for additional tracking data",
                "type": AbstractDataProviderTypeMap."*hash",
            },
            "pickup_location": {
                "display_name": "Pickup Location",
                "short_desc": "Pickup site details",
                "desc": "Location details for receiver pickup",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "pickup_note": {
                "display_name": "Pickup Note",
                "short_desc": "Pickup instructions",
                "desc": "Instructions for package pickup",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "shipment_tags": {
                "display_name": "Shipment Tags",
                "short_desc": "Tags for the shipment",
                "desc": "List of tags to assign to the shipment for categorization",
                "type": AbstractDataProviderType::get(SoftAutoListOrNothingType),
            },
            "courier_connection_id": {
                "display_name": "Courier Connection ID",
                "short_desc": "Courier connection reference",
                "desc": "Identifier for the courier connection to use for this tracking",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Singleton instance
public const AftershipCreateTrackingRequestDataType = new AftershipCreateTrackingRequestDataType();
}
