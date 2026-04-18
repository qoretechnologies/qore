# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ClickFunnelsProductDataType.qc ClickFunnels product data type

/*  ClickFunnelsProductDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

public namespace ClickFunnelsDataProvider {

#! ClickFunnels product record type
public class ClickFunnelsProductDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {
                "type": IntType,
                "display_name": "ID",
                "short_desc": "The product ID",
                "desc": "The unique identifier of the product",
            },
            "public_id": {
                "type": StringOrNothingType,
                "display_name": "Public ID",
                "short_desc": "The public identifier",
                "desc": "The public-facing unique identifier of the product",
            },
            "workspace_id": {
                "type": IntOrNothingType,
                "display_name": "Workspace ID",
                "short_desc": "The workspace ID",
                "desc": "The ID of the workspace this product belongs to",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "The product name",
                "desc": "The name of the product",
            },
            "current_path": {
                "type": StringOrNothingType,
                "display_name": "Current Path",
                "short_desc": "The product URL path",
                "desc": "The current URL path of the product",
            },
            "visible_in_store": {
                "type": SoftBoolOrNothingType,
                "display_name": "Visible in Store",
                "short_desc": "Whether visible in the store",
                "desc": "Whether the product is visible in the store",
            },
            "visible_in_customer_center": {
                "type": SoftBoolOrNothingType,
                "display_name": "Visible in Customer Center",
                "short_desc": "Whether visible in the customer center",
                "desc": "Whether the product is visible in the customer center",
            },
            "seo_title": {
                "type": StringOrNothingType,
                "display_name": "SEO Title",
                "short_desc": "The SEO title",
                "desc": "The SEO title for the product page",
            },
            "seo_description": {
                "type": StringOrNothingType,
                "display_name": "SEO Description",
                "short_desc": "The SEO description",
                "desc": "The SEO meta description for the product page",
            },
            "seo_image_url": {
                "type": StringOrNothingType,
                "display_name": "SEO Image URL",
                "short_desc": "The SEO image URL",
                "desc": "The SEO image URL for the product page",
            },
            "commissionable": {
                "type": SoftBoolOrNothingType,
                "display_name": "Commissionable",
                "short_desc": "Whether the product is commissionable",
                "desc": "Whether the product is eligible for affiliate commissions",
            },
            "created_at": {
                "type": DateOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the product was created",
                "desc": "The timestamp when the product was created",
            },
            "updated_at": {
                "type": DateOrNothingType,
                "display_name": "Updated At",
                "short_desc": "When the product was last updated",
                "desc": "The timestamp when the product was last updated",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace ClickFunnelsDataProvider
