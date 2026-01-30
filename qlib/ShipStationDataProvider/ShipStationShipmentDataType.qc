# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShipStationDataProvider module definition

/*  ShipStationShipmentDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! ShipStation shipment data type
public class ShipStationShipmentDataType inherits HashDataType {
    #! Creates the object
    constructor() : HashDataType("ShipStationShipment", {
        "shipmentId": {
            "display_name": "Shipment ID",
            "short_desc": "Unique identifier for the shipment",
            "type": IntType,
            "desc": "The unique numeric identifier for the shipment",
        },
        "orderId": {
            "display_name": "Order ID",
            "short_desc": "Associated order ID",
            "type": IntOrNothingType,
            "desc": "The order ID associated with this shipment",
        },
        "orderKey": {
            "display_name": "Order Key",
            "short_desc": "External order key",
            "type": SoftStringOrNothingType,
            "desc": "The external order key",
        },
        "userId": {
            "display_name": "User ID",
            "short_desc": "User who created the shipment",
            "type": SoftStringOrNothingType,
            "desc": "The user ID who created the shipment",
        },
        "customerEmail": {
            "display_name": "Customer Email",
            "short_desc": "Customer email address",
            "type": SoftStringOrNothingType,
            "desc": "The customer's email address",
        },
        "orderNumber": {
            "display_name": "Order Number",
            "short_desc": "Order number",
            "type": SoftStringOrNothingType,
            "desc": "The order number",
        },
        "createDate": {
            "display_name": "Create Date",
            "short_desc": "Date the shipment was created",
            "type": SoftStringOrNothingType,
            "desc": "The date and time the shipment was created (ISO 8601 format)",
        },
        "shipDate": {
            "display_name": "Ship Date",
            "short_desc": "Date the shipment was shipped",
            "type": SoftStringOrNothingType,
            "desc": "The date and time the shipment was shipped (ISO 8601 format)",
        },
        "shipmentCost": {
            "display_name": "Shipment Cost",
            "short_desc": "Cost of the shipment",
            "type": SoftFloatOrNothingType,
            "desc": "The total cost of the shipment",
        },
        "insuranceCost": {
            "display_name": "Insurance Cost",
            "short_desc": "Cost of insurance",
            "type": SoftFloatOrNothingType,
            "desc": "The cost of insurance for the shipment",
        },
        "trackingNumber": {
            "display_name": "Tracking Number",
            "short_desc": "Shipment tracking number",
            "type": SoftStringOrNothingType,
            "desc": "The tracking number for the shipment",
        },
        "isReturnLabel": {
            "display_name": "Is Return Label",
            "short_desc": "Whether this is a return label",
            "type": SoftBoolOrNothingType,
            "desc": "Whether this shipment is a return label",
        },
        "batchNumber": {
            "display_name": "Batch Number",
            "short_desc": "Batch number",
            "type": SoftStringOrNothingType,
            "desc": "The batch number if the shipment was part of a batch",
        },
        "carrierCode": {
            "display_name": "Carrier Code",
            "short_desc": "Carrier code",
            "type": SoftStringOrNothingType,
            "desc": "The carrier code (e.g., 'ups', 'fedex', 'stamps_com')",
        },
        "serviceCode": {
            "display_name": "Service Code",
            "short_desc": "Service code",
            "type": SoftStringOrNothingType,
            "desc": "The service code for the shipment",
        },
        "packageCode": {
            "display_name": "Package Code",
            "short_desc": "Package code",
            "type": SoftStringOrNothingType,
            "desc": "The package code for the shipment",
        },
        "confirmation": {
            "display_name": "Confirmation",
            "short_desc": "Confirmation type",
            "type": SoftStringOrNothingType,
            "desc": "The confirmation type (none, delivery, signature, adult_signature, direct_signature)",
        },
        "warehouseId": {
            "display_name": "Warehouse ID",
            "short_desc": "Origin warehouse ID",
            "type": IntOrNothingType,
            "desc": "The warehouse ID from which the shipment originated",
        },
        "voided": {
            "display_name": "Voided",
            "short_desc": "Whether the shipment has been voided",
            "type": SoftBoolOrNothingType,
            "desc": "Whether the shipment has been voided",
        },
        "voidDate": {
            "display_name": "Void Date",
            "short_desc": "Date the shipment was voided",
            "type": SoftStringOrNothingType,
            "desc": "The date and time the shipment was voided (if applicable)",
        },
        "marketplaceNotified": {
            "display_name": "Marketplace Notified",
            "short_desc": "Whether marketplace was notified",
            "type": SoftBoolOrNothingType,
            "desc": "Whether the marketplace was notified of the shipment",
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
            "desc": "The destination address for the shipment",
        },
        "weight": {
            "display_name": "Weight",
            "short_desc": "Package weight",
            "type": new ShipStationWeightDataType(),
            "desc": "The weight of the shipment",
        },
        "dimensions": {
            "display_name": "Dimensions",
            "short_desc": "Package dimensions",
            "type": new ShipStationDimensionsDataType(),
            "desc": "The dimensions of the shipment package",
        },
        "insuranceOptions": {
            "display_name": "Insurance Options",
            "short_desc": "Insurance configuration",
            "type": AutoHashOrNothingType,
            "desc": "The insurance options for the shipment",
        },
        "advancedOptions": {
            "display_name": "Advanced Options",
            "short_desc": "Advanced shipment options",
            "type": AutoHashOrNothingType,
            "desc": "Advanced options for the shipment",
        },
        "shipmentItems": {
            "display_name": "Shipment Items",
            "short_desc": "Items in the shipment",
            "type": SoftAutoListOrNothingType,
            "desc": "List of items included in the shipment",
        },
        "labelData": {
            "display_name": "Label Data",
            "short_desc": "Shipping label data",
            "type": SoftStringOrNothingType,
            "desc": "Base64-encoded label data (if requested)",
        },
        "formData": {
            "display_name": "Form Data",
            "short_desc": "Customs form data",
            "type": SoftStringOrNothingType,
            "desc": "Base64-encoded customs form data (if applicable)",
        },
    }) {
    }
}

} # namespace ShipStationDataProvider
