# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BigCommerceUpdateProductDataProvider.qc BigCommerce update product action data provider

/*  BigCommerceUpdateProductDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for updating a product
public class BigCommerceUpdateProductRequestDataType inherits HashDataType {
    private {
        const Fields = {
            "product_id": {
                "display_name": "Product ID",
                "type": IntType,
                "desc": "The ID of the product to update",
                "required": True,
            },
            "name": {
                "display_name": "Name",
                "type": StringOrNothingType,
                "desc": "Product name",
            },
            "type": {
                "display_name": "Type",
                "type": StringOrNothingType,
                "desc": "Product type",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "physical", "desc": "Physical product"},
                    <AllowedValueInfo>{"value": "digital", "desc": "Digital product"},
                ),
            },
            "weight": {
                "display_name": "Weight",
                "type": FloatOrNothingType,
                "desc": "Product weight",
            },
            "price": {
                "display_name": "Price",
                "type": FloatOrNothingType,
                "desc": "Product price",
            },
            "sku": {
                "display_name": "SKU",
                "type": StringOrNothingType,
                "desc": "Stock Keeping Unit identifier",
            },
            "description": {
                "display_name": "Description",
                "type": StringOrNothingType,
                "desc": "Product description (HTML allowed)",
            },
            "categories": {
                "display_name": "Categories",
                "type": AutoListOrNothingType,
                "desc": "Array of category IDs this product belongs to",
            },
            "brand_id": {
                "display_name": "Brand ID",
                "type": IntOrNothingType,
                "desc": "ID of the product brand",
            },
            "is_visible": {
                "display_name": "Is Visible",
                "type": SoftBoolOrNothingType,
                "desc": "Whether the product is visible on the storefront",
            },
            "inventory_level": {
                "display_name": "Inventory Level",
                "type": IntOrNothingType,
                "desc": "Current inventory level",
            },
            "condition": {
                "display_name": "Condition",
                "type": StringOrNothingType,
                "desc": "Product condition",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "New", "desc": "New product"},
                    <AllowedValueInfo>{"value": "Used", "desc": "Used product"},
                    <AllowedValueInfo>{"value": "Refurbished",
                        "desc": "Refurbished product"},
                ),
            },
            "inventory_tracking": {
                "display_name": "Inventory Tracking",
                "type": StringOrNothingType,
                "desc": "Inventory tracking method",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "none", "desc": "No inventory tracking"},
                    <AllowedValueInfo>{"value": "product",
                        "desc": "Track inventory at the product level"},
                    <AllowedValueInfo>{"value": "variant",
                        "desc": "Track inventory at the variant level"},
                ),
            },
            "availability": {
                "display_name": "Availability",
                "type": StringOrNothingType,
                "desc": "Product availability",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "available",
                        "desc": "Available for purchase"},
                    <AllowedValueInfo>{"value": "disabled", "desc": "Disabled"},
                    <AllowedValueInfo>{"value": "preorder",
                        "desc": "Available for pre-order"},
                ),
            },
            "is_featured": {
                "display_name": "Is Featured",
                "type": SoftBoolOrNothingType,
                "desc": "Whether the product is featured",
            },
            "cost_price": {
                "display_name": "Cost Price",
                "type": FloatOrNothingType,
                "desc": "Cost price of the product",
            },
            "retail_price": {
                "display_name": "Retail Price",
                "type": FloatOrNothingType,
                "desc": "Retail price (MSRP)",
            },
            "sale_price": {
                "display_name": "Sale Price",
                "type": FloatOrNothingType,
                "desc": "Sale price (overrides price when set)",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! BigCommerce update product action data provider
public class BigCommerceUpdateProductDataProvider inherits BigCommerceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "desc": "Update an existing product in BigCommerce",
            "type": "BigCommerceUpdateProductDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;
    }

    #! Creates the object with a REST client
    constructor(BigCommerceRestClient::BigCommerceRestClient rest) : BigCommerceDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "update";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Update an existing product in BigCommerce";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new BigCommerceUpdateProductRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new BigCommerceProductDataType();
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        int product_id = remove req.product_id;
        hash<auto> response = doPutV3("catalog/products/" + product_id, req);
        return response.data;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BigCommerceDataProvider
