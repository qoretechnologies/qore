# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BigCommerceProductDataType.qc defines the BigCommerceProductDataType class

/*  BigCommerceProductDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the BigCommerceDataProvider module
public namespace BigCommerceDataProvider {

#! Data type for BigCommerce product records
public class BigCommerceProductDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "id": {
                "display_name": "ID",
                "type": IntType,
                "desc": "Unique numeric ID of the product",
            },
            "name": {
                "display_name": "Name",
                "type": StringType,
                "desc": "Product name",
            },
            "type": {
                "display_name": "Type",
                "type": StringType,
                "desc": "Product type",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "physical", "desc": "Physical product"},
                    <AllowedValueInfo>{"value": "digital", "desc": "Digital product"},
                ),
            },
            "sku": {
                "display_name": "SKU",
                "type": SoftStringOrNothingType,
                "desc": "Stock Keeping Unit identifier",
            },
            "description": {
                "display_name": "Description",
                "type": SoftStringOrNothingType,
                "desc": "Product description (HTML allowed)",
            },
            "weight": {
                "display_name": "Weight",
                "type": SoftFloatOrNothingType,
                "desc": "Product weight",
            },
            "price": {
                "display_name": "Price",
                "type": SoftFloatOrNothingType,
                "desc": "Product price",
            },
            "cost_price": {
                "display_name": "Cost Price",
                "type": SoftFloatOrNothingType,
                "desc": "Cost price of the product",
            },
            "retail_price": {
                "display_name": "Retail Price",
                "type": SoftFloatOrNothingType,
                "desc": "Retail price (MSRP)",
            },
            "sale_price": {
                "display_name": "Sale Price",
                "type": SoftFloatOrNothingType,
                "desc": "Sale price (overrides price when set)",
            },
            "categories": {
                "display_name": "Categories",
                "type": AutoListOrNothingType,
                "desc": "Array of category IDs this product belongs to",
            },
            "brand_id": {
                "display_name": "Brand ID",
                "type": SoftIntOrNothingType,
                "desc": "ID of the product brand",
            },
            "inventory_level": {
                "display_name": "Inventory Level",
                "type": SoftIntOrNothingType,
                "desc": "Current inventory level",
            },
            "inventory_tracking": {
                "display_name": "Inventory Tracking",
                "type": SoftStringOrNothingType,
                "desc": "Inventory tracking method",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "none", "desc": "No inventory tracking"},
                    <AllowedValueInfo>{"value": "product",
                        "desc": "Track at product level"},
                    <AllowedValueInfo>{"value": "variant",
                        "desc": "Track at variant level"},
                ),
            },
            "is_visible": {
                "display_name": "Is Visible",
                "type": SoftBoolOrNothingType,
                "desc": "Whether the product is visible on the storefront",
            },
            "is_featured": {
                "display_name": "Is Featured",
                "type": SoftBoolOrNothingType,
                "desc": "Whether the product is featured",
            },
            "condition": {
                "display_name": "Condition",
                "type": SoftStringOrNothingType,
                "desc": "Product condition",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "New", "desc": "New product"},
                    <AllowedValueInfo>{"value": "Used", "desc": "Used product"},
                    <AllowedValueInfo>{"value": "Refurbished",
                        "desc": "Refurbished product"},
                ),
            },
            "availability": {
                "display_name": "Availability",
                "type": SoftStringOrNothingType,
                "desc": "Product availability",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "available",
                        "desc": "Available for purchase"},
                    <AllowedValueInfo>{"value": "disabled", "desc": "Disabled"},
                    <AllowedValueInfo>{"value": "preorder",
                        "desc": "Available for pre-order"},
                ),
            },
            "date_created": {
                "display_name": "Date Created",
                "type": SoftDateOrNothingType,
                "desc": "Date/time the product was created",
            },
            "date_modified": {
                "display_name": "Date Modified",
                "type": SoftDateOrNothingType,
                "desc": "Date/time the product was last modified",
            },
            "custom_url": {
                "display_name": "Custom URL",
                "type": AutoHashOrNothingType,
                "desc": "Custom URL for the product with 'url' and 'is_customized' keys",
            },
            "images": {
                "display_name": "Images",
                "type": AutoListOrNothingType,
                "desc": "Product images (when included)",
            },
            "variants": {
                "display_name": "Variants",
                "type": AutoListOrNothingType,
                "desc": "Product variants (when included)",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("BigCommerceProduct") {
        addQoreFields(Fields);
    }
}

} # namespace BigCommerceDataProvider
