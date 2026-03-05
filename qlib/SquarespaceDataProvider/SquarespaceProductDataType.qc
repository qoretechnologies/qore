# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SquarespaceDataProvider module definition

/*  SquarespaceProductDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Squarespace product record type
public class SquarespaceProductDataType inherits HashDataType {
    public {
        #! Field definitions for Squarespace products
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The product ID",
                "desc": "The unique identifier of the product",
            },
            "type": {
                "type": StringOrNothingType,
                "display_name": "Type",
                "short_desc": "The product type",
                "desc": "The type of the product (`PHYSICAL`, `SERVICE`, `GIFT_CARD`, or `DIGITAL`)",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "PHYSICAL", "desc": "Physical product"},
                    <AllowedValueInfo>{"value": "SERVICE", "desc": "Service product"},
                    <AllowedValueInfo>{"value": "GIFT_CARD", "desc": "Gift card"},
                    <AllowedValueInfo>{"value": "DIGITAL", "desc": "Digital product"},
                ),
            },
            "storePageId": {
                "type": StringOrNothingType,
                "display_name": "Store Page ID",
                "short_desc": "The store page ID",
                "desc": "The ID of the store page the product belongs to",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "The product name",
                "desc": "The name of the product",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "The product description",
                "desc": "The HTML description of the product",
            },
            "url": {
                "type": StringOrNothingType,
                "display_name": "URL",
                "short_desc": "The product URL",
                "desc": "The full URL of the product page",
            },
            "urlSlug": {
                "type": StringOrNothingType,
                "display_name": "URL Slug",
                "short_desc": "The URL slug",
                "desc": "The URL slug for the product",
            },
            "tags": {
                "type": SoftListOrNothingType,
                "display_name": "Tags",
                "short_desc": "The product tags",
                "desc": "The list of tags associated with the product",
            },
            "isVisible": {
                "type": SoftBoolOrNothingType,
                "display_name": "Is Visible",
                "short_desc": "Whether the product is visible",
                "desc": "Indicates whether the product is visible on the store",
            },
            "seoOptions": {
                "type": new SquarespaceSeoOptionsDataType(AutoHashOrNothingType),
                "display_name": "SEO Options",
                "short_desc": "SEO configuration",
                "desc": "The SEO options for the product (`title`, `description`)",
            },
            "variantAttributes": {
                "type": SoftListOrNothingType,
                "display_name": "Variant Attributes",
                "short_desc": "The variant attribute names",
                "desc": "The list of attribute names used for product variants",
            },
            "variants": {
                "type": SoftListOrNothingType,
                "display_name": "Variants",
                "short_desc": "The product variants",
                "desc": "The list of variants for the product",
            },
            "images": {
                "type": SoftListOrNothingType,
                "display_name": "Images",
                "short_desc": "The product images",
                "desc": "The list of images associated with the product",
            },
            "pricing": {
                "type": new SquarespaceFullPricingDataType(AutoHashOrNothingType),
                "display_name": "Pricing",
                "short_desc": "The product pricing",
                "desc": "The pricing information for the product (used for `DIGITAL` products)",
            },
            "digitalGood": {
                "type": AutoHashOrNothingType,
                "display_name": "Digital Good",
                "short_desc": "Digital good details",
                "desc": "The digital good details for digital products",
            },
            "createdOn": {
                "type": DateOrNothingType,
                "display_name": "Created On",
                "short_desc": "When the product was created",
                "desc": "The timestamp when the product was created",
            },
            "modifiedOn": {
                "type": DateOrNothingType,
                "display_name": "Modified On",
                "short_desc": "When the product was last modified",
                "desc": "The timestamp when the product was last modified",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace SquarespaceDataProvider
