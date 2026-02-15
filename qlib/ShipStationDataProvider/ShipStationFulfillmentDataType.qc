# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShipStationDataProvider module definition

/*  ShipStationFulfillmentDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ShipStationDataProvider module
public namespace ShipStationDataProvider {

#! ShipStation fulfillment data type
public class ShipStationFulfillmentDataType inherits HashDataType {
    #! Creates the object
    constructor() : HashDataType("ShipStationFulfillment", {
        "fulfillmentId": {
            "display_name": "Fulfillment ID",
            "short_desc": "Unique identifier for the fulfillment",
            "type": IntType,
            "desc": "The unique numeric identifier for the fulfillment",
        },
        "orderId": {
            "display_name": "Order ID",
            "short_desc": "Associated order ID",
            "type": IntOrNothingType,
            "desc": "The order ID associated with this fulfillment",
        },
        "orderNumber": {
            "display_name": "Order Number",
            "short_desc": "Order number",
            "type": SoftStringOrNothingType,
            "desc": "The order number",
        },
        "userId": {
            "display_name": "User ID",
            "short_desc": "User who created the fulfillment",
            "type": SoftStringOrNothingType,
            "desc": "The user ID who created the fulfillment",
        },
        "customerEmail": {
            "display_name": "Customer Email",
            "short_desc": "Customer email address",
            "type": SoftStringOrNothingType,
            "desc": "The customer's email address",
        },
        "trackingNumber": {
            "display_name": "Tracking Number",
            "short_desc": "Shipment tracking number",
            "type": SoftStringOrNothingType,
            "desc": "The tracking number for the fulfillment",
        },
        "createDate": {
            "display_name": "Create Date",
            "short_desc": "Date the fulfillment was created",
            "type": DateOrNothingType,
            "desc": "The date and time the fulfillment was created",
        },
        "shipDate": {
            "display_name": "Ship Date",
            "short_desc": "Date the fulfillment was shipped",
            "type": DateOrNothingType,
            "desc": "The date and time the fulfillment was shipped",
        },
        "voidDate": {
            "display_name": "Void Date",
            "short_desc": "Date the fulfillment was voided",
            "type": DateOrNothingType,
            "desc": "The date and time the fulfillment was voided (if applicable)",
        },
        "deliveryDate": {
            "display_name": "Delivery Date",
            "short_desc": "Actual delivery date",
            "type": DateOrNothingType,
            "desc": "The actual delivery date (if available)",
        },
        "carrierCode": {
            "display_name": "Carrier Code",
            "short_desc": "Carrier code",
            "type": SoftStringOrNothingType,
            "desc": "The carrier code (e.g., 'ups', 'fedex', 'stamps_com')",
        },
        "fulfillmentProviderCode": {
            "display_name": "Fulfillment Provider Code",
            "short_desc": "Fulfillment provider code",
            "type": SoftStringOrNothingType,
            "desc": "The fulfillment provider code (if using third-party fulfillment)",
        },
        "fulfillmentServiceCode": {
            "display_name": "Fulfillment Service Code",
            "short_desc": "Fulfillment service code",
            "type": SoftStringOrNothingType,
            "desc": "The fulfillment service code",
        },
        "fulfillmentFee": {
            "display_name": "Fulfillment Fee",
            "short_desc": "Fee for fulfillment",
            "type": SoftFloatOrNothingType,
            "desc": "The fee charged for fulfillment",
        },
        "voidRequested": {
            "display_name": "Void Requested",
            "short_desc": "Whether void has been requested",
            "type": SoftBoolOrNothingType,
            "desc": "Whether a void has been requested for this fulfillment",
        },
        "voided": {
            "display_name": "Voided",
            "short_desc": "Whether the fulfillment has been voided",
            "type": SoftBoolOrNothingType,
            "desc": "Whether the fulfillment has been voided",
        },
        "marketplaceNotified": {
            "display_name": "Marketplace Notified",
            "short_desc": "Whether marketplace was notified",
            "type": SoftBoolOrNothingType,
            "desc": "Whether the marketplace was notified of the fulfillment",
        },
        "notifyErrorMessage": {
            "display_name": "Notify Error Message",
            "short_desc": "Error message from marketplace notification",
            "type": SoftStringOrNothingType,
            "desc": "Error message if marketplace notification failed",
        },
        "shipTo": {
            "display_name": "Ship To",
            "short_desc": "Destination address",
            "type": new ShipStationAddressDataType(),
            "desc": "The destination address for the fulfillment",
        },
    }) {
    }
}

} # namespace ShipStationDataProvider
