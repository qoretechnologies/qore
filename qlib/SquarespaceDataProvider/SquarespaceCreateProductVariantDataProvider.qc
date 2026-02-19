# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SquarespaceCreateProductVariantDataProvider.qc Squarespace create product variant action data provider

/*  SquarespaceCreateProductVariantDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request type for creating a product variant
public class SquarespaceCreateProductVariantRequestDataType inherits HashDataType {
    private {
        const Fields = {
            "product_id": {
                "type": StringType,
                "display_name": "Product ID",
                "short_desc": "The product ID to add the variant to",
                "desc": "The product ID to add the variant to",
                "required": True,
            },
            "sku": {
                "type": StringType,
                "display_name": "SKU",
                "short_desc": "The variant SKU",
                "desc": "The stock keeping unit identifier for the variant",
                "required": True,
            },
            "pricing": {
                "type": AutoHashType,
                "display_name": "Pricing",
                "short_desc": "The variant pricing (basePrice, salePrice, onSale)",
                "desc": "The variant pricing (basePrice, salePrice, onSale)",
                "required": True,
            },
            "stock": {
                "type": AutoHashOrNothingType,
                "display_name": "Stock",
                "short_desc": "The stock information (quantity, unlimited)",
                "desc": "The stock information (quantity, unlimited)",
            },
            "attributes": {
                "type": AutoHashOrNothingType,
                "display_name": "Attributes",
                "short_desc": "The variant attributes (key-value pairs)",
                "desc": "The variant attributes (key-value pairs)",
            },
            "shippingMeasurements": {
                "type": AutoHashOrNothingType,
                "display_name": "Shipping Measurements",
                "short_desc": "The shipping measurements (weight, dimensions)",
                "desc": "The shipping measurements (weight, dimensions)",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Squarespace create product variant action data provider
public class SquarespaceCreateProductVariantDataProvider inherits SquarespaceDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new product variant in Squarespace",
            "type": "SquarespaceCreateProductVariantDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(SquarespaceRestClient::SquarespaceRestClient rest) : SquarespaceDataProviderBase(rest) {
    }

    string getName() {
        return "create";
    }

    *string getDesc() {
        return "Create a new product variant in Squarespace";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new SquarespaceCreateProductVariantRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new SquarespaceProductVariantDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string product_id = remove req.product_id;
        return doPost(sprintf("v2/commerce/products/%s/variants", product_id), req);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SquarespaceDataProvider
