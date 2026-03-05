# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SquarespaceCreateOrderDataProvider.qc Squarespace create order action data provider

/*  SquarespaceCreateOrderDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request type for creating an order
public class SquarespaceCreateOrderRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "channelName": {
                "type": StringType,
                "display_name": "Channel Name",
                "short_desc": "Third-party channel name (max 30 chars)",
                "desc": "Third-party channel name (max 30 chars)",
                "required": True,
            },
            "externalOrderReference": {
                "type": StringType,
                "display_name": "External Order Reference",
                "short_desc": "Third-party order reference (max 200 chars)",
                "desc": "Third-party order reference (max 200 chars)",
                "required": True,
            },
            "createdOn": {
                "type": SoftDateType,
                "display_name": "Created On",
                "short_desc": "When the order was placed",
                "desc": "When the order was placed",
                "required": True,
            },
            "priceTaxInterpretation": {
                "type": StringType,
                "display_name": "Price Tax Interpretation",
                "short_desc": "EXCLUSIVE or INCLUSIVE",
                "desc": "`EXCLUSIVE` or `INCLUSIVE`",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "EXCLUSIVE", "desc": "Prices exclude tax"},
                    <AllowedValueInfo>{"value": "INCLUSIVE", "desc": "Prices include tax"},
                ),
            },
            "grandTotal": {
                "type": new SquarespaceMoneyDataType(),
                "display_name": "Grand Total",
                "short_desc": "Order grand total",
                "desc": "Order grand total with `currency` and `value`",
                "required": True,
            },
            "lineItems": {
                "type": new ListDataType("LineItemList", new SquarespaceLineItemDataType(), True),
                "display_name": "Line Items",
                "short_desc": "Order line items",
                "desc": "The list of line items for the order",
                "required": True,
            },
            "customerEmail": {
                "type": StringOrNothingType,
                "display_name": "Customer Email",
                "short_desc": "Customer email",
                "desc": "Customer email address",
                "preselected": True,
            },
            "billingAddress": {
                "type": new SquarespaceAddressDataType(AutoHashOrNothingType),
                "display_name": "Billing Address",
                "short_desc": "Billing address",
                "desc": "Billing address for the order",
            },
            "shippingAddress": {
                "type": new SquarespaceAddressDataType(AutoHashOrNothingType),
                "display_name": "Shipping Address",
                "short_desc": "Shipping address",
                "desc": "Shipping address for the order",
            },
            "fulfillmentStatus": {
                "type": StringOrNothingType,
                "display_name": "Fulfillment Status",
                "short_desc": "PENDING or FULFILLED",
                "desc": "`PENDING` or `FULFILLED`",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "PENDING", "desc": "Order is pending fulfillment"},
                    <AllowedValueInfo>{"value": "FULFILLED", "desc": "Order has been fulfilled"},
                ),
            },
            "inventoryBehavior": {
                "type": StringOrNothingType,
                "display_name": "Inventory Behavior",
                "short_desc": "DEDUCT or SKIP (default: SKIP)",
                "desc": "`DEDUCT` or `SKIP` (default: `SKIP`)",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "DEDUCT", "desc": "Deduct from inventory"},
                    <AllowedValueInfo>{"value": "SKIP", "desc": "Do not adjust inventory"},
                ),
            },
            "subtotal": {
                "type": new SquarespaceMoneyDataType(AutoHashOrNothingType),
                "display_name": "Subtotal",
                "short_desc": "Order subtotal",
                "desc": "Order subtotal with `currency` and `value`",
            },
            "shippingTotal": {
                "type": new SquarespaceMoneyDataType(AutoHashOrNothingType),
                "display_name": "Shipping Total",
                "short_desc": "Shipping total",
                "desc": "Shipping total with `currency` and `value`",
            },
            "discountTotal": {
                "type": new SquarespaceMoneyDataType(AutoHashOrNothingType),
                "display_name": "Discount Total",
                "short_desc": "Discount total",
                "desc": "Discount total with `currency` and `value`",
            },
            "taxTotal": {
                "type": new SquarespaceMoneyDataType(AutoHashOrNothingType),
                "display_name": "Tax Total",
                "short_desc": "Tax total",
                "desc": "Tax total with `currency` and `value`",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Squarespace create order action data provider
public class SquarespaceCreateOrderDataProvider inherits SquarespaceDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new order in Squarespace",
            "type": "SquarespaceCreateOrderDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(SquarespaceRestClient::SquarespaceRestClient rest) : SquarespaceDataProviderBase(rest) {
    }

    string getName() {
        return "create";
    }

    *string getDesc() {
        return "Create a new order in Squarespace";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new SquarespaceCreateOrderRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new SquarespaceOrderDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("1.0/commerce/orders", req);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SquarespaceDataProvider
