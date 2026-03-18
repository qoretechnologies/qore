# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WooCommerceDataProvider module definition

/*  WooCommerceProductDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! WooCommerce product data type
public class WooCommerceProductDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "id": {
                "display_name": "ID",
                "short_desc": "Unique identifier for the product",
                "type": IntOrNothingType,
                "desc": "The unique identifier for the product",
            },
            "name": {
                "display_name": "Name",
                "short_desc": "Product name",
                "type": StringOrNothingType,
                "desc": "The product name",
            },
            "slug": {
                "display_name": "Slug",
                "short_desc": "Product slug",
                "type": StringOrNothingType,
                "desc": "The product URL slug",
            },
            "type": {
                "display_name": "Type",
                "short_desc": "Product type",
                "type": StringOrNothingType,
                "desc": "Product type: `simple`, `grouped`, `external`, `variable`",
            },
            "status": {
                "display_name": "Status",
                "short_desc": "Product status",
                "type": StringOrNothingType,
                "desc": "The product publication status (e.g., `draft`, `pending`, `publish`, `private`)",
            },
            "featured": {
                "display_name": "Featured",
                "short_desc": "Whether the product is featured",
                "type": SoftBoolOrNothingType,
                "desc": "If `True`, the product is marked as featured",
            },
            "catalog_visibility": {
                "display_name": "Catalog Visibility",
                "short_desc": "Catalog visibility setting",
                "type": StringOrNothingType,
                "desc": "Catalog visibility: `visible`, `catalog`, `search`, `hidden`",
            },
            "description": {
                "display_name": "Description",
                "short_desc": "Product description",
                "type": StringOrNothingType,
                "desc": "The full product description (may contain HTML)",
            },
            "short_description": {
                "display_name": "Short Description",
                "short_desc": "Product short description",
                "type": StringOrNothingType,
                "desc": "The short product description (may contain HTML)",
            },
            "sku": {
                "display_name": "SKU",
                "short_desc": "Stock keeping unit",
                "type": StringOrNothingType,
                "desc": "The unique stock keeping unit (SKU) identifier",
            },
            "regular_price": {
                "display_name": "Regular Price",
                "short_desc": "Product regular price",
                "type": StringOrNothingType,
                "desc": "The regular price of the product",
            },
            "sale_price": {
                "display_name": "Sale Price",
                "short_desc": "Product sale price",
                "type": StringOrNothingType,
                "desc": "The sale price of the product, if on sale",
            },
            "on_sale": {
                "display_name": "On Sale",
                "short_desc": "Whether the product is on sale",
                "type": SoftBoolOrNothingType,
                "desc": "If `True`, the product is currently on sale",
            },
            "purchasable": {
                "display_name": "Purchasable",
                "short_desc": "Whether the product can be purchased",
                "type": SoftBoolOrNothingType,
                "desc": "If `True`, the product can be purchased",
            },
            "virtual": {
                "display_name": "Virtual",
                "short_desc": "Whether the product is virtual",
                "type": SoftBoolOrNothingType,
                "desc": "If `True`, the product is virtual and does not require shipping",
            },
            "downloadable": {
                "display_name": "Downloadable",
                "short_desc": "Whether the product is downloadable",
                "type": SoftBoolOrNothingType,
                "desc": "If `True`, the product is downloadable",
            },
            "total_sales": {
                "display_name": "Total Sales",
                "short_desc": "Total number of sales",
                "type": IntOrNothingType,
                "desc": "The total number of times this product has been sold",
            },
            "manage_stock": {
                "display_name": "Manage Stock",
                "short_desc": "Whether stock management is enabled",
                "type": SoftBoolOrNothingType,
                "desc": "If `True`, stock management is enabled at the product level",
            },
            "stock_quantity": {
                "display_name": "Stock Quantity",
                "short_desc": "Number of items in stock",
                "type": IntOrNothingType,
                "desc": "The number of items currently in stock",
            },
            "stock_status": {
                "display_name": "Stock Status",
                "short_desc": "Product stock status",
                "type": StringOrNothingType,
                "desc": "Stock status: `instock`, `outofstock`, `onbackorder`",
            },
            "weight": {
                "display_name": "Weight",
                "short_desc": "Product weight",
                "type": StringOrNothingType,
                "desc": "The product weight in the shop's weight unit",
            },
            "dimensions": {
                "display_name": "Dimensions",
                "short_desc": "Product dimensions",
                "type": new WooCommerceDimensionsDataType(AutoHashOrNothingType),
                "desc": "The product dimensions including `length`, `width`, and `height`",
            },
            "categories": {
                "display_name": "Categories",
                "short_desc": "Product categories",
                "type": SoftListOrNothingType,
                "desc": "List of categories the product belongs to",
            },
            "tags": {
                "display_name": "Tags",
                "short_desc": "Product tags",
                "type": SoftListOrNothingType,
                "desc": "List of tags assigned to the product",
            },
            "images": {
                "display_name": "Images",
                "short_desc": "Product images",
                "type": SoftListOrNothingType,
                "desc": "List of product images",
            },
            "attributes": {
                "display_name": "Attributes",
                "short_desc": "Product attributes",
                "type": SoftListOrNothingType,
                "desc": "List of product attributes (e.g., color, size)",
            },
            "variations": {
                "display_name": "Variations",
                "short_desc": "Product variation IDs",
                "type": SoftListOrNothingType,
                "desc": "List of variation IDs for variable products",
            },
            "price": {
                "display_name": "Price",
                "short_desc": "Current product price",
                "type": StringOrNothingType,
                "desc": "The current active price (sale price if on sale, otherwise regular price)",
            },
            "date_created": {
                "display_name": "Date Created",
                "short_desc": "Date the product was created",
                "type": DateOrNothingType,
                "desc": "The date and time the product was created",
            },
            "date_modified": {
                "display_name": "Date Modified",
                "short_desc": "Date the product was last modified",
                "type": DateOrNothingType,
                "desc": "The date and time the product was last modified",
            },
            "meta_data": {
                "display_name": "Meta Data",
                "short_desc": "Product metadata",
                "type": SoftListOrNothingType,
                "desc": "List of metadata key-value pairs associated with the product",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace WooCommerceDataProvider
