# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShipStationDataProvider module definition

/*  ShipStationOrderDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! ShipStation order record type
/** Represents a complete order in ShipStation.
*/
public class ShipStationOrderDataType inherits HashDataType {
    public {
        #! Field definitions for ShipStation orders
        const Fields = {
            "orderId": {
                "type": IntOrNothingType,
                "display_name": "Order ID",
                "short_desc": "ShipStation order ID",
                "desc": "The unique identifier assigned by ShipStation (read-only on create)",
            },
            "orderNumber": {
                "type": StringType,
                "display_name": "Order Number",
                "short_desc": "User-defined order number",
                "desc": "A user-defined order identifier (max 50 characters)",
                "example_value": "ORD-12345",
            },
            "orderKey": {
                "type": StringOrNothingType,
                "display_name": "Order Key",
                "short_desc": "Unique order identifier",
                "desc": "A user-provided unique identifier. If not provided, ShipStation generates one.",
            },
            "orderDate": {
                "type": DateType,
                "display_name": "Order Date",
                "short_desc": "When order was placed",
                "desc": "The date the order was placed",
            },
            "createDate": {
                "type": DateOrNothingType,
                "display_name": "Create Date",
                "short_desc": "When order was created in ShipStation",
                "desc": "The date the order was created in ShipStation (read-only)",
            },
            "modifyDate": {
                "type": DateOrNothingType,
                "display_name": "Modify Date",
                "short_desc": "When order was last modified",
                "desc": "The date the order was last modified (read-only)",
            },
            "paymentDate": {
                "type": DateOrNothingType,
                "display_name": "Payment Date",
                "short_desc": "When payment was received",
                "desc": "The date payment was received for this order",
            },
            "shipByDate": {
                "type": DateOrNothingType,
                "display_name": "Ship By Date",
                "short_desc": "Suggested ship date",
                "desc": "The date by which this order should be shipped",
            },
            "orderStatus": {
                "type": StringType,
                "display_name": "Order Status",
                "short_desc": "Current order status",
                "desc": "The current status of the order",
                "example_value": "awaiting_shipment",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "awaiting_payment",
                        "display_name": "Awaiting Payment",
                        "desc": "Order is waiting for payment",
                    },
                    <AllowedValueInfo>{
                        "value": "awaiting_shipment",
                        "display_name": "Awaiting Shipment",
                        "desc": "Order is paid and ready to ship",
                    },
                    <AllowedValueInfo>{
                        "value": "shipped",
                        "display_name": "Shipped",
                        "desc": "Order has been shipped",
                    },
                    <AllowedValueInfo>{
                        "value": "on_hold",
                        "display_name": "On Hold",
                        "desc": "Order is on hold",
                    },
                    <AllowedValueInfo>{
                        "value": "cancelled",
                        "display_name": "Cancelled",
                        "desc": "Order has been cancelled",
                    },
                    <AllowedValueInfo>{
                        "value": "pending_fulfillment",
                        "display_name": "Pending Fulfillment",
                        "desc": "Order is pending fulfillment",
                    },
                ),
            },
            "customerId": {
                "type": IntOrNothingType,
                "display_name": "Customer ID",
                "short_desc": "ShipStation customer ID",
                "desc": "The ID of the customer associated with this order",
            },
            "customerUsername": {
                "type": StringOrNothingType,
                "display_name": "Customer Username",
                "short_desc": "Customer's username",
                "desc": "The customer's username (required to create a customer profile)",
            },
            "customerEmail": {
                "type": StringOrNothingType,
                "display_name": "Customer Email",
                "short_desc": "Customer's email address",
                "desc": "The customer's email address",
                "example_value": "customer@example.com",
            },
            "billTo": {
                "type": new ShipStationAddressDataType(),
                "display_name": "Bill To",
                "short_desc": "Billing address",
                "desc": "The billing address for this order",
            },
            "shipTo": {
                "type": new ShipStationAddressDataType(),
                "display_name": "Ship To",
                "short_desc": "Shipping address",
                "desc": "The shipping address for this order",
            },
            "orderTotal": {
                "type": FloatOrNothingType,
                "display_name": "Order Total",
                "short_desc": "Total order amount",
                "desc": "The total amount of the order",
            },
            "amountPaid": {
                "type": FloatOrNothingType,
                "display_name": "Amount Paid",
                "short_desc": "Amount paid",
                "desc": "The amount that has been paid",
            },
            "taxAmount": {
                "type": FloatOrNothingType,
                "display_name": "Tax Amount",
                "short_desc": "Total tax",
                "desc": "The total tax amount for the order",
            },
            "shippingAmount": {
                "type": FloatOrNothingType,
                "display_name": "Shipping Amount",
                "short_desc": "Shipping cost",
                "desc": "The shipping cost charged to the customer",
            },
            "customerNotes": {
                "type": StringOrNothingType,
                "display_name": "Customer Notes",
                "short_desc": "Notes from customer",
                "desc": "Notes provided by the customer",
            },
            "internalNotes": {
                "type": StringOrNothingType,
                "display_name": "Internal Notes",
                "short_desc": "Internal notes",
                "desc": "Internal notes (not visible to customer)",
            },
            "gift": {
                "type": BoolOrNothingType,
                "display_name": "Gift",
                "short_desc": "Is this a gift?",
                "desc": "Whether this order is a gift",
            },
            "giftMessage": {
                "type": StringOrNothingType,
                "display_name": "Gift Message",
                "short_desc": "Gift message",
                "desc": "The gift message to include with the order",
            },
            "paymentMethod": {
                "type": StringOrNothingType,
                "display_name": "Payment Method",
                "short_desc": "How the order was paid",
                "desc": "The payment method used for this order",
            },
            "requestedShippingService": {
                "type": StringOrNothingType,
                "display_name": "Requested Shipping Service",
                "short_desc": "Customer's shipping preference",
                "desc": "The shipping service requested by the customer",
            },
            "carrierCode": {
                "type": StringOrNothingType,
                "display_name": "Carrier Code",
                "short_desc": "Shipping carrier",
                "desc": "The carrier code for shipping (e.g., `stamps_com`, `fedex`, `ups`)",
                "ref_data": "carriers",
            },
            "serviceCode": {
                "type": StringOrNothingType,
                "display_name": "Service Code",
                "short_desc": "Shipping service",
                "desc": "The service code for shipping (e.g., 'usps_priority_mail')",
            },
            "packageCode": {
                "type": StringOrNothingType,
                "display_name": "Package Code",
                "short_desc": "Package type",
                "desc": "The package type code",
            },
            "confirmation": {
                "type": StringOrNothingType,
                "display_name": "Confirmation",
                "short_desc": "Delivery confirmation",
                "desc": "The delivery confirmation type",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "none", "display_name": "None"},
                    <AllowedValueInfo>{"value": "delivery", "display_name": "Delivery Confirmation"},
                    <AllowedValueInfo>{"value": "signature", "display_name": "Signature Required"},
                    <AllowedValueInfo>{"value": "adult_signature", "display_name": "Adult Signature"},
                    <AllowedValueInfo>{"value": "direct_signature", "display_name": "Direct Signature"},
                ),
            },
            "shipDate": {
                "type": DateOrNothingType,
                "display_name": "Ship Date",
                "short_desc": "When order was shipped",
                "desc": "The date the order was actually shipped",
            },
            "holdUntilDate": {
                "type": DateOrNothingType,
                "display_name": "Hold Until Date",
                "short_desc": "Hold until this date",
                "desc": "Date to hold the order until before shipping",
            },
            "weight": {
                "type": new ShipStationWeightDataType(),
                "display_name": "Weight",
                "short_desc": "Total order weight",
                "desc": "The total weight of the order",
            },
            "dimensions": {
                "type": new ShipStationDimensionsDataType(),
                "display_name": "Dimensions",
                "short_desc": "Package dimensions",
                "desc": "The package dimensions for this order",
            },
            "userId": {
                "type": StringOrNothingType,
                "display_name": "User ID",
                "short_desc": "Assigned user",
                "desc": "The ID of the user assigned to this order",
                "ref_data": "users",
            },
            "externallyFulfilled": {
                "type": BoolOrNothingType,
                "display_name": "Externally Fulfilled",
                "short_desc": "Fulfilled outside ShipStation",
                "desc": "Whether this order was fulfilled outside of ShipStation",
            },
            "externallyFulfilledBy": {
                "type": StringOrNothingType,
                "display_name": "Externally Fulfilled By",
                "short_desc": "External fulfillment service",
                "desc": "The name of the external fulfillment service",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);

        # Add list fields that require runtime initialization
        addField(new QoreDataField({
            "name": "items",
            "display_name": "Items",
            "short_desc": "Order line items",
            "desc": "The line items in this order",
            "type": new ListDataType("OrderItems", new ShipStationOrderItemDataType(), True),
        }));

        addField(new QoreDataField({
            "name": "tagIds",
            "display_name": "Tag IDs",
            "short_desc": "Order tags",
            "desc": "List of tag IDs applied to this order",
            "type": new ListDataType("TagIds", IntOrNothingType, True),
        }));

        # Add hash fields that require runtime initialization
        addField(new QoreDataField({
            "name": "insuranceOptions",
            "display_name": "Insurance Options",
            "short_desc": "Shipping insurance options",
            "desc": "Insurance options for the shipment",
            "type": AutoHashOrNothingType,
        }));

        addField(new QoreDataField({
            "name": "internationalOptions",
            "display_name": "International Options",
            "short_desc": "International shipping options",
            "desc": "Options for international shipments (customs info, etc.)",
            "type": AutoHashOrNothingType,
        }));

        addField(new QoreDataField({
            "name": "advancedOptions",
            "display_name": "Advanced Options",
            "short_desc": "Advanced order options",
            "desc": "Advanced options including store ID, warehouse ID, etc.",
            "type": AutoHashOrNothingType,
        }));
    }
}

} # namespace ShipStationDataProvider
