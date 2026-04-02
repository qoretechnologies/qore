# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AftershipDataProvider module definition

/** AftershipTrackingDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Tracking response data type
public class AftershipTrackingDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Tracking ID",
                "short_desc": "System-generated tracking ID",
                "desc": "System-generated tracking ID (32 chars in 2026-01)",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "legacy_id": {
                "display_name": "Legacy ID",
                "short_desc": "Previous 24-char format ID",
                "desc": "Previous 24-character format ID for backwards compatibility",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "created_at": {
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "desc": "ISO 8601 timestamp when the tracking was created",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "updated_at": {
                "display_name": "Updated At",
                "short_desc": "Last update timestamp",
                "desc": "ISO 8601 timestamp when the tracking was last updated",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "tracking_number": {
                "display_name": "Tracking Number",
                "short_desc": "Carrier tracking number",
                "desc": "Carrier tracking number",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "slug": {
                "display_name": "Courier Code",
                "short_desc": "Unique courier code",
                "desc": "Unique courier code (e.g., `fedex`, `ups`, `dhl`)",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "active": {
                "display_name": "Active",
                "short_desc": "Whether tracking is active",
                "desc": "`True` if the tracking is still being monitored",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "tag": {
                "display_name": "Status Tag",
                "short_desc": "Current status tag",
                "desc": "Current delivery status: `Pending`, `InfoReceived`, `InTransit`, `OutForDelivery`, "
                    "`AttemptFail`, `Delivered`, `AvailableForPickup`, `Exception`, `Expired`",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "subtag": {
                "display_name": "Subtag",
                "short_desc": "Detailed status code",
                "desc": "Detailed status code providing more specific information about the tracking status",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "subtag_message": {
                "display_name": "Subtag Message",
                "short_desc": "Status description",
                "desc": "Human-readable description of the status subtag",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "title": {
                "display_name": "Title",
                "short_desc": "Display name",
                "desc": "Display name for the tracking, defaults to tracking number",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "customer_name": {
                "display_name": "Customer Name",
                "short_desc": "Recipient name",
                "desc": "Name of the package recipient",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "order_id": {
                "display_name": "Order ID",
                "short_desc": "Global order identifier",
                "desc": "Global order identifier",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "order_id_path": {
                "display_name": "Order ID Path",
                "short_desc": "URL to order in merchant system",
                "desc": "URL to the order in the merchant's system",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "order_number": {
                "display_name": "Order Number",
                "short_desc": "Human-readable order number",
                "desc": "Human-readable order number",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "order_date": {
                "display_name": "Order Date",
                "short_desc": "Order placement date",
                "desc": "ISO 8601 timestamp when the order was placed",
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
            "expected_delivery": {
                "display_name": "Expected Delivery",
                "short_desc": "Carrier estimated delivery date",
                "desc": "Carrier estimated delivery date",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "signed_by": {
                "display_name": "Signed By",
                "short_desc": "Delivery signature name",
                "desc": "Name of the person who signed for the delivery",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "source": {
                "display_name": "Source",
                "short_desc": "Data source",
                "desc": "Source of the tracking data (e.g., `api`)",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "transit_time": {
                "display_name": "Transit Time",
                "short_desc": "Days from pickup to delivery",
                "desc": "Number of days from pickup to delivery",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "shipment_package_count": {
                "display_name": "Package Count",
                "short_desc": "Number of packages",
                "desc": "Number of packages in the shipment",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "shipment_pickup_date": {
                "display_name": "Pickup Date",
                "short_desc": "Pickup timestamp",
                "desc": "Timestamp when the shipment was picked up",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "shipment_delivery_date": {
                "display_name": "Delivery Date",
                "short_desc": "Actual delivery timestamp",
                "desc": "Timestamp when the shipment was actually delivered",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "shipment_type": {
                "display_name": "Shipment Type",
                "short_desc": "Service type description",
                "desc": "Carrier service type description",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "courier_tracking_link": {
                "display_name": "Courier Tracking Link",
                "short_desc": "Direct carrier tracking URL",
                "desc": "Direct URL to the carrier's tracking page",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "courier_redirect_link": {
                "display_name": "Courier Redirect Link",
                "short_desc": "Redirect-based carrier URL",
                "desc": "Redirect-based URL to the carrier's tracking page",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "aftership_tracking_url": {
                "display_name": "Aftership Tracking URL",
                "short_desc": "Aftership public tracking URL",
                "desc": "AfterShip-hosted public tracking page URL",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "tracked_count": {
                "display_name": "Tracked Count",
                "short_desc": "Number of tracking updates",
                "desc": "Number of tracking updates received",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "return_to_sender": {
                "display_name": "Return to Sender",
                "short_desc": "Return shipment flag",
                "desc": "`True` if this is a return-to-sender shipment",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "language": {
                "display_name": "Language",
                "short_desc": "Language code (ISO 639-1)",
                "desc": "ISO 639-1 language code for notifications",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "unique_token": {
                "display_name": "Unique Token",
                "short_desc": "Unique authentication token",
                "desc": "Unique authentication token for the tracking",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "first_attempted_at": {
                "display_name": "First Attempted At",
                "short_desc": "First delivery attempt timestamp",
                "desc": "Timestamp of the first delivery attempt",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "delivery_type": {
                "display_name": "Delivery Type",
                "short_desc": "Delivery method",
                "desc": "Delivery method: `pickup_at_store`, `door_to_door`, or `pickup_at_courier`",
                "type": AbstractDataProviderTypeMap."*string",
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
            "on_time_status": {
                "display_name": "On Time Status",
                "short_desc": "Delivery timeliness indicator",
                "desc": "Indicates if the delivery is `on-time` or delayed",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "on_time_difference": {
                "display_name": "On Time Difference",
                "short_desc": "Days difference from promised date",
                "desc": "Number of days difference from the promised delivery date",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "location_id": {
                "display_name": "Location ID",
                "short_desc": "Fulfillment location reference",
                "desc": "Fulfillment location reference identifier",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "shipping_method": {
                "display_name": "Shipping Method",
                "short_desc": "Shipping service type",
                "desc": "Shipping service method chosen at checkout",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "failed_delivery_attempts": {
                "display_name": "Failed Delivery Attempts",
                "short_desc": "Count of failed delivery attempts",
                "desc": "Number of failed delivery attempts",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "checkpoints": {
                "display_name": "Checkpoints",
                "short_desc": "List of checkpoint events",
                "desc": "Array of checkpoint events for this tracking, each describing a specific "
                    "status update during shipment transit",
                "type": new ListDataType("AftershipCheckpointList", AftershipCheckpointDataType, False),
            },
            "custom_fields": {
                "display_name": "Custom Fields",
                "short_desc": "Custom key-value data",
                "desc": "Custom key-value string pairs attached to this tracking",
                "type": AbstractDataProviderTypeMap."*hash",
            },
            "order_promised_delivery_date": {
                "display_name": "Promised Delivery Date",
                "short_desc": "Expected delivery date",
                "desc": "The promised delivery date in format `YYYY-MM-DD` or ISO 8601",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "shipment_tags": {
                "display_name": "Shipment Tags",
                "short_desc": "Tags assigned to the shipment",
                "desc": "List of tags assigned to the shipment for categorization",
                "type": AbstractDataProviderType::get(SoftAutoListOrNothingType),
            },
            "courier_connection_id": {
                "display_name": "Courier Connection ID",
                "short_desc": "Courier connection reference",
                "desc": "Identifier for the courier connection used for this tracking",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "last_mile_tracking_supported": {
                "display_name": "Last Mile Tracking Supported",
                "short_desc": "Whether last-mile tracking is available",
                "desc": "`True` if last-mile tracking is supported for this shipment",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "delivery_location_type": {
                "display_name": "Delivery Location Type",
                "short_desc": "Type of delivery location",
                "desc": "Type of delivery location (e.g., front door, mailbox)",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "aftership_tracking_order_url": {
                "display_name": "Aftership Tracking Order URL",
                "short_desc": "Aftership order tracking page URL",
                "desc": "AfterShip-hosted tracking page URL for the order",
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
public const AftershipTrackingDataType = new AftershipTrackingDataType();
}
