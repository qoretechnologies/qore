# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WooCommerceDataProvider module definition

/*  WooCommerceCategoryDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! WooCommerce product category data type
public class WooCommerceCategoryDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "id": {
                "display_name": "ID",
                "short_desc": "Unique identifier for the category",
                "type": IntOrNothingType,
                "desc": "The unique identifier for the product category",
            },
            "name": {
                "display_name": "Name",
                "short_desc": "Category name",
                "type": StringOrNothingType,
                "desc": "The category name",
            },
            "slug": {
                "display_name": "Slug",
                "short_desc": "Category slug",
                "type": StringOrNothingType,
                "desc": "The category URL slug",
            },
            "parent": {
                "display_name": "Parent",
                "short_desc": "Parent category ID",
                "type": IntOrNothingType,
                "desc": "The ID of the parent category; `0` for top-level categories",
            },
            "description": {
                "display_name": "Description",
                "short_desc": "Category description",
                "type": StringOrNothingType,
                "desc": "The category description (may contain HTML)",
            },
            "display": {
                "display_name": "Display",
                "short_desc": "Category display type",
                "type": StringOrNothingType,
                "desc": "Category display type: `default`, `products`, `subcategories`, `both`",
            },
            "image": {
                "display_name": "Image",
                "short_desc": "Category image",
                "type": new WooCommerceImageDataType(AutoHashOrNothingType),
                "desc": "The image data for the category",
            },
            "menu_order": {
                "display_name": "Menu Order",
                "short_desc": "Category sort order",
                "type": IntOrNothingType,
                "desc": "The sort order for the category in menus",
            },
            "count": {
                "display_name": "Count",
                "short_desc": "Number of products in the category",
                "type": IntOrNothingType,
                "desc": "The number of products assigned to this category",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace WooCommerceDataProvider
