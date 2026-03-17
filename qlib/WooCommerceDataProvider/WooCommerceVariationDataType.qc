# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WooCommerceDataProvider module definition

/*  WooCommerceVariationDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! WooCommerce product variation data type
public class WooCommerceVariationDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "id": {
                "display_name": "ID",
                "short_desc": "Unique identifier for the variation",
                "type": IntOrNothingType,
                "desc": "The unique identifier for the product variation",
            },
            "date_created": {
                "display_name": "Date Created",
                "short_desc": "Date the variation was created",
                "type": DateOrNothingType,
                "desc": "The date and time the variation was created",
            },
            "sku": {
                "display_name": "SKU",
                "short_desc": "Stock keeping unit",
                "type": StringOrNothingType,
                "desc": "The unique stock keeping unit (SKU) identifier for the variation",
            },
            "price": {
                "display_name": "Price",
                "short_desc": "Current variation price",
                "type": StringOrNothingType,
                "desc": "The current active price of the variation",
            },
            "regular_price": {
                "display_name": "Regular Price",
                "short_desc": "Variation regular price",
                "type": StringOrNothingType,
                "desc": "The regular price of the variation",
            },
            "sale_price": {
                "display_name": "Sale Price",
                "short_desc": "Variation sale price",
                "type": StringOrNothingType,
                "desc": "The sale price of the variation, if on sale",
            },
            "on_sale": {
                "display_name": "On Sale",
                "short_desc": "Whether the variation is on sale",
                "type": SoftBoolOrNothingType,
                "desc": "If `True`, the variation is currently on sale",
            },
            "status": {
                "display_name": "Status",
                "short_desc": "Variation status",
                "type": StringOrNothingType,
                "desc": "The variation publication status (e.g., `publish`, `private`)",
            },
            "purchasable": {
                "display_name": "Purchasable",
                "short_desc": "Whether the variation can be purchased",
                "type": SoftBoolOrNothingType,
                "desc": "If `True`, the variation can be purchased",
            },
            "manage_stock": {
                "display_name": "Manage Stock",
                "short_desc": "Whether stock management is enabled",
                "type": SoftBoolOrNothingType,
                "desc": "If `True`, stock management is enabled at the variation level",
            },
            "stock_quantity": {
                "display_name": "Stock Quantity",
                "short_desc": "Number of items in stock",
                "type": IntOrNothingType,
                "desc": "The number of variation items currently in stock",
            },
            "stock_status": {
                "display_name": "Stock Status",
                "short_desc": "Variation stock status",
                "type": StringOrNothingType,
                "desc": "Stock status: `instock`, `outofstock`, `onbackorder`",
            },
            "weight": {
                "display_name": "Weight",
                "short_desc": "Variation weight",
                "type": StringOrNothingType,
                "desc": "The variation weight in the shop's weight unit",
            },
            "dimensions": {
                "display_name": "Dimensions",
                "short_desc": "Variation dimensions",
                "type": AutoHashOrNothingType,
                "desc": "The variation dimensions including `length`, `width`, and `height`",
            },
            "attributes": {
                "display_name": "Attributes",
                "short_desc": "Variation attributes",
                "type": SoftListOrNothingType,
                "desc": "List of attributes that define this variation (e.g., `color`, `size`)",
            },
            "image": {
                "display_name": "Image",
                "short_desc": "Variation image",
                "type": AutoHashOrNothingType,
                "desc": "The image data for the variation",
            },
            "meta_data": {
                "display_name": "Meta Data",
                "short_desc": "Variation metadata",
                "type": SoftListOrNothingType,
                "desc": "List of metadata key-value pairs associated with the variation",
            },
        };
    }

    #! Creates the object
    constructor() {
        map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type, $1.value.default_value)),
            Fields.pairIterator();
    }
}

} # namespace WooCommerceDataProvider
