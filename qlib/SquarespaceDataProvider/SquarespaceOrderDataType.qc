# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SquarespaceDataProvider module definition

/*  SquarespaceOrderDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SquarespaceDataProvider module
public namespace SquarespaceDataProvider {

#! Squarespace order record type
public class SquarespaceOrderDataType inherits HashDataType {
    public {
        #! Field definitions for Squarespace orders
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The order ID",
                "desc": "The unique identifier of the order",
            },
            "orderNumber": {
                "type": StringOrNothingType,
                "display_name": "Order Number",
                "short_desc": "The order number",
                "desc": "The customer-facing order number",
            },
            "createdOn": {
                "type": DateOrNothingType,
                "display_name": "Created On",
                "short_desc": "When the order was created",
                "desc": "The timestamp when the order was created",
            },
            "modifiedOn": {
                "type": DateOrNothingType,
                "display_name": "Modified On",
                "short_desc": "When the order was last modified",
                "desc": "The timestamp when the order was last modified",
            },
            "channel": {
                "type": StringOrNothingType,
                "display_name": "Channel",
                "short_desc": "The sales channel",
                "desc": "The sales channel through which the order was placed",
            },
            "testmode": {
                "type": SoftBoolOrNothingType,
                "display_name": "Test Mode",
                "short_desc": "Whether this is a test order",
                "desc": "Indicates whether the order was placed in test mode",
            },
            "customerEmail": {
                "type": StringOrNothingType,
                "display_name": "Customer Email",
                "short_desc": "The customer's email address",
                "desc": "The email address of the customer who placed the order",
            },
            "billingAddress": {
                "type": new SquarespaceAddressDataType(AutoHashOrNothingType),
                "display_name": "Billing Address",
                "short_desc": "The billing address",
                "desc": "The billing address for the order",
            },
            "shippingAddress": {
                "type": new SquarespaceAddressDataType(AutoHashOrNothingType),
                "display_name": "Shipping Address",
                "short_desc": "The shipping address",
                "desc": "The shipping address for the order",
            },
            "fulfillmentStatus": {
                "type": StringOrNothingType,
                "display_name": "Fulfillment Status",
                "short_desc": "The fulfillment status of the order",
                "desc": "The fulfillment status of the order (`PENDING`, `FULFILLED`, or `CANCELED`)",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "PENDING", "desc": "Pending fulfillment"},
                    <AllowedValueInfo>{"value": "FULFILLED", "desc": "Fulfilled"},
                    <AllowedValueInfo>{"value": "CANCELED", "desc": "Canceled"},
                ),
            },
            "lineItems": {
                "type": SoftListOrNothingType,
                "display_name": "Line Items",
                "short_desc": "The order line items",
                "desc": "The list of line items in the order",
            },
            "internalNotes": {
                "type": SoftListOrNothingType,
                "display_name": "Internal Notes",
                "short_desc": "Internal notes on the order",
                "desc": "The list of internal notes associated with the order",
            },
            "shippingLines": {
                "type": SoftListOrNothingType,
                "display_name": "Shipping Lines",
                "short_desc": "The shipping lines",
                "desc": "The list of shipping lines for the order",
            },
            "discountLines": {
                "type": SoftListOrNothingType,
                "display_name": "Discount Lines",
                "short_desc": "The discount lines",
                "desc": "The list of discount lines applied to the order",
            },
            "formSubmission": {
                "type": SoftListOrNothingType,
                "display_name": "Form Submission",
                "short_desc": "Custom form submissions",
                "desc": "The list of custom form field submissions on the order",
            },
            "fulfillments": {
                "type": SoftListOrNothingType,
                "display_name": "Fulfillments",
                "short_desc": "The order fulfillments",
                "desc": "The list of fulfillments for the order",
            },
            "subtotal": {
                "type": new SquarespaceMoneyDataType(AutoHashOrNothingType),
                "display_name": "Subtotal",
                "short_desc": "The order subtotal",
                "desc": "The subtotal of the order before shipping, taxes, and discounts",
            },
            "shippingTotal": {
                "type": new SquarespaceMoneyDataType(AutoHashOrNothingType),
                "display_name": "Shipping Total",
                "short_desc": "The total shipping cost",
                "desc": "The total shipping cost for the order",
            },
            "discountTotal": {
                "type": new SquarespaceMoneyDataType(AutoHashOrNothingType),
                "display_name": "Discount Total",
                "short_desc": "The total discount amount",
                "desc": "The total discount amount applied to the order",
            },
            "taxTotal": {
                "type": new SquarespaceMoneyDataType(AutoHashOrNothingType),
                "display_name": "Tax Total",
                "short_desc": "The total tax amount",
                "desc": "The total tax amount for the order",
            },
            "refundedTotal": {
                "type": new SquarespaceMoneyDataType(AutoHashOrNothingType),
                "display_name": "Refunded Total",
                "short_desc": "The total refunded amount",
                "desc": "The total amount refunded for the order",
            },
            "grandTotal": {
                "type": new SquarespaceMoneyDataType(AutoHashOrNothingType),
                "display_name": "Grand Total",
                "short_desc": "The order grand total",
                "desc": "The grand total of the order including shipping, taxes, and discounts",
            },
            "channelName": {
                "type": StringOrNothingType,
                "display_name": "Channel Name",
                "short_desc": "The sales channel name",
                "desc": "The name of the sales channel through which the order was placed",
            },
            "externalOrderReference": {
                "type": StringOrNothingType,
                "display_name": "External Order Reference",
                "short_desc": "External order reference",
                "desc": "The external reference identifier for the order",
            },
            "fulfilledOn": {
                "type": DateOrNothingType,
                "display_name": "Fulfilled On",
                "short_desc": "When the order was fulfilled",
                "desc": "The timestamp when the order was fulfilled",
            },
            "priceTaxInterpretation": {
                "type": StringOrNothingType,
                "display_name": "Price Tax Interpretation",
                "short_desc": "How prices are interpreted for tax",
                "desc": "Indicates how prices are interpreted for tax calculation purposes",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "EXCLUSIVE", "desc": "Prices exclude tax"},
                    <AllowedValueInfo>{"value": "INCLUSIVE", "desc": "Prices include tax"},
                ),
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace SquarespaceDataProvider
