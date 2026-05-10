# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WooCommerceDataProvider module definition

/*  WooCommerceLineItemDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the WooCommerceDataProvider module
public namespace WooCommerceDataProvider {

#! WooCommerce order line-item data type
public class WooCommerceLineItemDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "id": {
                "display_name": "ID",
                "short_desc": "Line item ID",
                "type": IntOrNothingType,
                "desc": "The line item ID (omit when creating)",
            },
            "product_id": {
                "display_name": "Product ID",
                "short_desc": "Product ID",
                "type": IntOrNothingType,
                "desc": "The product ID for this line item",
                "example_value": 456,
                "ref_data": "products",
            },
            "variation_id": {
                "display_name": "Variation ID",
                "short_desc": "Variation ID",
                "type": IntOrNothingType,
                "desc": "The variation ID, if the line item refers to a product variation",
            },
            "quantity": {
                "display_name": "Quantity",
                "short_desc": "Quantity ordered",
                "type": IntOrNothingType,
                "desc": "The quantity ordered",
                "example_value": 2,
            },
            "name": {
                "display_name": "Name",
                "short_desc": "Product name",
                "type": StringOrNothingType,
                "desc": "The product name (set automatically from `product_id` if omitted)",
            },
            "sku": {
                "display_name": "SKU",
                "short_desc": "Product SKU",
                "type": StringOrNothingType,
                "desc": "The product SKU",
            },
            "price": {
                "display_name": "Price",
                "short_desc": "Per-unit price",
                "type": StringOrNothingType,
                "desc": "The per-unit price",
                "example_value": "29.99",
            },
            "subtotal": {
                "display_name": "Subtotal",
                "short_desc": "Subtotal before discounts",
                "type": StringOrNothingType,
                "desc": "The line subtotal (before discounts)",
            },
            "total": {
                "display_name": "Total",
                "short_desc": "Total after discounts",
                "type": StringOrNothingType,
                "desc": "The line total (after discounts)",
            },
            "tax_class": {
                "display_name": "Tax Class",
                "short_desc": "Tax class slug",
                "type": StringOrNothingType,
                "desc": "Tax class slug applied to this line",
            },
            "meta_data": {
                "display_name": "Meta Data",
                "short_desc": "Line item meta data",
                "type": new ListDataType("LineItemMetaData", new WooCommerceMetaDataType(), True),
                "desc": "Per-line metadata key-value pairs",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }

    #! Creates the object with a specific base type
    constructor(Type base_type) : HashDataType(base_type, "WooCommerceLineItem") {
        addQoreFields(Fields);
    }
}

} # namespace WooCommerceDataProvider
