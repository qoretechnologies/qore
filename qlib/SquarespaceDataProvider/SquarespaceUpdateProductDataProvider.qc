# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SquarespaceUpdateProductDataProvider.qc Squarespace update product action data provider

/*  SquarespaceUpdateProductDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request type for updating a product
public class SquarespaceUpdateProductRequestDataType inherits HashDataType {
    private {
        const Fields = {
            "product_id": {
                "type": StringType,
                "display_name": "Product ID",
                "short_desc": "The product ID to update",
                "desc": "The product ID to update",
                "required": True,
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "The product name",
                "desc": "The product name",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "The product description (HTML)",
                "desc": "The product description (HTML)",
            },
            "urlSlug": {
                "type": StringOrNothingType,
                "display_name": "URL Slug",
                "short_desc": "The URL slug for the product",
                "desc": "The URL slug for the product",
            },
            "tags": {
                "type": SoftListOrNothingType,
                "display_name": "Tags",
                "short_desc": "List of tags for the product",
                "desc": "List of tags for the product",
            },
            "isVisible": {
                "type": SoftBoolOrNothingType,
                "display_name": "Is Visible",
                "short_desc": "Whether the product is visible on the store",
                "desc": "Whether the product is visible on the store",
            },
            "seoOptions": {
                "type": AutoHashOrNothingType,
                "display_name": "SEO Options",
                "short_desc": "SEO configuration (title, description)",
                "desc": "SEO configuration (title, description)",
            },
            "variantAttributes": {
                "type": SoftListOrNothingType,
                "display_name": "Variant Attributes",
                "short_desc": "List of variant attribute names",
                "desc": "List of variant attribute names (e.g., Size, Color)",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Squarespace update product action data provider
public class SquarespaceUpdateProductDataProvider inherits SquarespaceDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "desc": "Update an existing product in Squarespace",
            "type": "SquarespaceUpdateProductDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(SquarespaceRestClient::SquarespaceRestClient rest) : SquarespaceDataProviderBase(rest) {
    }

    string getName() {
        return "update";
    }

    *string getDesc() {
        return "Update an existing product in Squarespace";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new SquarespaceUpdateProductRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new SquarespaceProductDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string product_id = remove req.product_id;
        return doPost(sprintf("v2/commerce/products/%s", product_id), req);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SquarespaceDataProvider
