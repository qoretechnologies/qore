# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore KeapDataProvider module definition

/*  KeapOrderItemDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the KeapDataProvider module
public namespace KeapDataProvider {

#! Keap order item data type (for creating orders)
public class KeapOrderItemDataType inherits HashDataType {
    public {
        #! Field definitions for order items
        const Fields = {
            "product_id": {
                "type": StringType,
                "display_name": "Product ID",
                "short_desc": "The product ID",
                "desc": "The ID of the product. Must be a valid product ID",
                "required": True,
            },
            "quantity": {
                "type": IntType,
                "display_name": "Quantity",
                "short_desc": "The item quantity",
                "desc": "The quantity of the order item. Must be >= `1`",
                "required": True,
                "example_value": 1,
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "The item name",
                "desc": "The name of the order item. Defaults to the product name if not provided",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "The item description",
                "desc": "The description of the order item",
            },
            "notes": {
                "type": StringOrNothingType,
                "display_name": "Notes",
                "short_desc": "Item notes",
                "desc": "Notes about the order item",
            },
            "price_per_unit": {
                "type": FloatOrNothingType,
                "display_name": "Price Per Unit",
                "short_desc": "The unit price",
                "desc": "The price per unit. Defaults to the product price. Must be >= `0`",
                "example_value": 29.99,
            },
            "cost_per_unit": {
                "type": FloatOrNothingType,
                "display_name": "Cost Per Unit",
                "short_desc": "The unit cost",
                "desc": "The cost per unit. Defaults to the product cost. Must be >= `0`",
            },
            "item_type": {
                "type": StringOrNothingType,
                "display_name": "Item Type",
                "short_desc": "The item type",
                "desc": "The type of the order item. Defaults to `PRODUCT`",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "PRODUCT", "desc": "Standard product"},
                    <AllowedValueInfo>{"value": "SERVICE", "desc": "Service item"},
                    <AllowedValueInfo>{"value": "SHIPPING", "desc": "Shipping charge"},
                    <AllowedValueInfo>{"value": "TAX", "desc": "Tax item"},
                    <AllowedValueInfo>{"value": "UPSELL", "desc": "Upsell item"},
                    <AllowedValueInfo>{"value": "SUBSCRIPTION", "desc": "Subscription"},
                    <AllowedValueInfo>{"value": "DISCOUNT", "desc": "Discount"},
                    <AllowedValueInfo>{"value": "TIP", "desc": "Tip"},
                    <AllowedValueInfo>{"value": "OTHER", "desc": "Other"},
                ),
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("KeapOrderItemDataType") {
        addQoreFields(Fields);
    }

    #! Creates the type with a base type (for optional fields)
    constructor(Type base_type) : HashDataType(base_type, "KeapOrderItemDataType") {
        addQoreFields(Fields);
    }
}

} # namespace KeapDataProvider
