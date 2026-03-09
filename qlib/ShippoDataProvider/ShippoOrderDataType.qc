# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShippoDataProvider module definition

/*  ShippoOrderDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Shippo order data type
public class ShippoOrderDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "object_id": {
                "type": SoftStringOrNothingType,
                "display_name": "Object ID",
                "short_desc": "Unique order identifier",
                "desc": "The unique identifier of the order object in Shippo",
            },
            "object_owner": {
                "type": SoftStringOrNothingType,
                "display_name": "Owner",
                "short_desc": "Owner email",
                "desc": "Email of the user who owns this order",
            },
            "object_created": {
                "type": SoftStringOrNothingType,
                "display_name": "Created",
                "short_desc": "Creation timestamp",
                "desc": "ISO 8601 timestamp when the order was created in Shippo",
            },
            "object_updated": {
                "type": SoftStringOrNothingType,
                "display_name": "Updated",
                "short_desc": "Update timestamp",
                "desc": "ISO 8601 timestamp when the order was last updated",
            },
            "order_number": {
                "type": SoftStringOrNothingType,
                "display_name": "Order Number",
                "short_desc": "Customer order number",
                "desc": "Your internal order number or reference",
            },
            "order_status": {
                "type": SoftStringOrNothingType,
                "display_name": "Order Status",
                "short_desc": "Order status",
                "desc": "The current status of the order",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "UNKNOWN", "desc": "Unknown status"},
                    <AllowedValueInfo>{"value": "PAID", "desc": "Order has been paid"},
                    <AllowedValueInfo>{"value": "SHIPPED", "desc": "Order has been shipped"},
                    <AllowedValueInfo>{"value": "DELIVERED", "desc": "Order has been delivered"},
                    <AllowedValueInfo>{"value": "CANCELLED", "desc": "Order was cancelled"},
                    <AllowedValueInfo>{"value": "RETURNED", "desc": "Order was returned"},
                ),
            },
            "placed_at": {
                "type": SoftStringOrNothingType,
                "display_name": "Placed At",
                "short_desc": "Order placement date",
                "desc": "ISO 8601 timestamp when the order was placed",
            },
            "to_address": {
                "type": new ShippoAddressDataType(),
                "display_name": "Shipping Address",
                "short_desc": "Customer shipping address",
                "desc": "The destination address for the order",
            },
            "from_address": {
                "type": new ShippoAddressDataType(),
                "display_name": "From Address",
                "short_desc": "Sender address",
                "desc": "The origin address for the order (optional)",
            },
            "line_items": {
                "type": new ListDataType("line_items", new ShippoLineItemDataType(), True),
                "display_name": "Line Items",
                "short_desc": "Order items",
                "desc": "Array of line items in the order",
            },
            "shipping_cost": {
                "type": SoftStringOrNothingType,
                "display_name": "Shipping Cost",
                "short_desc": "Customer shipping cost",
                "desc": "The shipping cost charged to the customer",
            },
            "shipping_cost_currency": {
                "type": SoftStringOrNothingType,
                "display_name": "Shipping Cost Currency",
                "short_desc": "Currency code",
                "desc": "Three-letter ISO currency code for shipping cost",
            },
            "shipping_method": {
                "type": SoftStringOrNothingType,
                "display_name": "Shipping Method",
                "short_desc": "Selected shipping method",
                "desc": "The shipping method selected by the customer",
            },
            "shop_app": {
                "type": SoftStringOrNothingType,
                "display_name": "Shop App",
                "short_desc": "Shop platform",
                "desc": "The e-commerce platform the order came from (e.g., Shippo, Shopify)",
            },
            "subtotal_price": {
                "type": SoftStringOrNothingType,
                "display_name": "Subtotal",
                "short_desc": "Order subtotal",
                "desc": "The subtotal price of the order (before shipping and tax)",
            },
            "total_price": {
                "type": SoftStringOrNothingType,
                "display_name": "Total Price",
                "short_desc": "Order total",
                "desc": "The total price of the order",
            },
            "total_tax": {
                "type": SoftStringOrNothingType,
                "display_name": "Total Tax",
                "short_desc": "Tax amount",
                "desc": "The total tax amount for the order",
            },
            "currency": {
                "type": SoftStringOrNothingType,
                "display_name": "Currency",
                "short_desc": "Price currency",
                "desc": "Three-letter ISO currency code for prices",
            },
            "weight": {
                "type": SoftStringOrNothingType,
                "display_name": "Weight",
                "short_desc": "Total weight",
                "desc": "The total weight of the order",
            },
            "weight_unit": {
                "type": SoftStringOrNothingType,
                "display_name": "Weight Unit",
                "short_desc": "Weight unit",
                "desc": "The unit of measurement for weight",
            },
            "notes": {
                "type": SoftStringOrNothingType,
                "display_name": "Notes",
                "short_desc": "Order notes",
                "desc": "Any notes or special instructions for the order",
            },
            "transactions": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Transactions",
                "short_desc": "Shipping labels",
                "desc": "Array of transaction (label) object IDs for this order",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("ShippoOrderDataType") {
        addQoreFields(Fields);
    }
}

#! Shippo line item data type
public class ShippoLineItemDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "object_id": {
                "type": SoftStringOrNothingType,
                "display_name": "Object ID",
                "short_desc": "Unique line item identifier",
                "desc": "The unique identifier of the line item object",
            },
            "title": {
                "type": SoftStringOrNothingType,
                "display_name": "Title",
                "short_desc": "Item title",
                "desc": "The title/name of the product",
            },
            "variant_title": {
                "type": SoftStringOrNothingType,
                "display_name": "Variant Title",
                "short_desc": "Variant name",
                "desc": "The variant title (e.g., size, color)",
            },
            "sku": {
                "type": SoftStringOrNothingType,
                "display_name": "SKU",
                "short_desc": "Stock keeping unit",
                "desc": "The product SKU",
            },
            "quantity": {
                "type": SoftIntOrNothingType,
                "display_name": "Quantity",
                "short_desc": "Item quantity",
                "desc": "The quantity ordered",
            },
            "total_price": {
                "type": SoftStringOrNothingType,
                "display_name": "Total Price",
                "short_desc": "Line item total",
                "desc": "The total price for this line item",
            },
            "currency": {
                "type": SoftStringOrNothingType,
                "display_name": "Currency",
                "short_desc": "Price currency",
                "desc": "Three-letter ISO currency code",
            },
            "weight": {
                "type": SoftStringOrNothingType,
                "display_name": "Weight",
                "short_desc": "Item weight",
                "desc": "The weight of the item",
            },
            "weight_unit": {
                "type": SoftStringOrNothingType,
                "display_name": "Weight Unit",
                "short_desc": "Weight unit",
                "desc": "The unit of measurement for weight",
            },
            "manufacture_country": {
                "type": SoftStringOrNothingType,
                "display_name": "Country of Manufacture",
                "short_desc": "Origin country",
                "desc": "Two-letter ISO country code where the item was manufactured",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("ShippoLineItemDataType") {
        addQoreFields(Fields);
    }
}

} # namespace ShippoDataProvider
