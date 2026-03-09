# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShippoDataProvider module definition

/*  ShippoRateDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Shippo rate data type
public class ShippoRateDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "object_id": {
                "type": SoftStringOrNothingType,
                "display_name": "Object ID",
                "short_desc": "Unique rate identifier",
                "desc": "The unique identifier of the rate object in Shippo",
            },
            "object_created": {
                "type": SoftStringOrNothingType,
                "display_name": "Created",
                "short_desc": "Creation timestamp",
                "desc": "ISO 8601 timestamp when the rate was created",
            },
            "object_owner": {
                "type": SoftStringOrNothingType,
                "display_name": "Owner",
                "short_desc": "Owner email",
                "desc": "Email of the user who owns this rate",
            },
            "shipment": {
                "type": SoftStringOrNothingType,
                "display_name": "Shipment ID",
                "short_desc": "Associated shipment",
                "desc": "The object ID of the associated shipment",
            },
            "amount": {
                "type": SoftStringOrNothingType,
                "display_name": "Amount",
                "short_desc": "Rate amount",
                "desc": "The cost of this shipping option",
            },
            "currency": {
                "type": SoftStringOrNothingType,
                "display_name": "Currency",
                "short_desc": "Currency code",
                "desc": "Three-letter ISO currency code",
            },
            "amount_local": {
                "type": SoftStringOrNothingType,
                "display_name": "Local Amount",
                "short_desc": "Amount in local currency",
                "desc": "The cost in the carrier's local currency",
            },
            "currency_local": {
                "type": SoftStringOrNothingType,
                "display_name": "Local Currency",
                "short_desc": "Local currency code",
                "desc": "Three-letter ISO code for the carrier's local currency",
            },
            "provider": {
                "type": SoftStringOrNothingType,
                "display_name": "Provider",
                "short_desc": "Carrier name",
                "desc": "The name of the shipping carrier (e.g., USPS, UPS, FedEx)",
            },
            "provider_image_75": {
                "type": SoftStringOrNothingType,
                "display_name": "Provider Logo (75px)",
                "short_desc": "Small carrier logo",
                "desc": "URL to the carrier's 75x75 pixel logo",
            },
            "provider_image_200": {
                "type": SoftStringOrNothingType,
                "display_name": "Provider Logo (200px)",
                "short_desc": "Large carrier logo",
                "desc": "URL to the carrier's 200x200 pixel logo",
            },
            "servicelevel": {
                "type": new ShippoServiceLevelDataType(AutoHashOrNothingType),
                "display_name": "Service Level",
                "short_desc": "Service details",
                "desc": "Service level details including `name`, `token`, `terms`, and "
                    "`extended_token`",
            },
            "estimated_days": {
                "type": SoftIntOrNothingType,
                "display_name": "Estimated Days",
                "short_desc": "Transit time",
                "desc": "Estimated number of days for delivery",
            },
            "arrives_by": {
                "type": SoftStringOrNothingType,
                "display_name": "Arrives By",
                "short_desc": "Delivery date",
                "desc": "Estimated delivery date/time",
            },
            "duration_terms": {
                "type": SoftStringOrNothingType,
                "display_name": "Duration Terms",
                "short_desc": "Delivery terms",
                "desc": "Description of the delivery time guarantee",
            },
            "carrier_account": {
                "type": SoftStringOrNothingType,
                "display_name": "Carrier Account",
                "short_desc": "Carrier account ID",
                "desc": "The object ID of the carrier account used for this rate",
            },
            "zone": {
                "type": SoftStringOrNothingType,
                "display_name": "Zone",
                "short_desc": "Shipping zone",
                "desc": "The postal zone for this shipment",
            },
            "messages": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Messages",
                "short_desc": "Rate messages",
                "desc": "Array of messages or warnings about the rate",
            },
            "attributes": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Attributes",
                "short_desc": "Rate attributes",
                "desc": "Array of attribute strings (e.g., CHEAPEST, FASTEST, BESTVALUE)",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("ShippoRateDataType") {
        addQoreFields(Fields);
    }
}

} # namespace ShippoDataProvider
