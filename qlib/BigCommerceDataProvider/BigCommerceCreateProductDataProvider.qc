# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BigCommerceCreateProductDataProvider.qc BigCommerce create product action data provider

/*  BigCommerceCreateProductDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for creating a product
public class BigCommerceCreateProductRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "name": {
                "display_name": "Name",
                "short_desc": "Product name",
                "type": StringType,
                "desc": "Product name",
                "required": True,
                "example_value": "My Product",
            },
            "type": {
                "display_name": "Type",
                "short_desc": "Product type",
                "type": StringType,
                "desc": "Product type",
                "required": True,
                "example_value": "physical",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "physical", "desc": "Physical product"},
                    <AllowedValueInfo>{"value": "digital", "desc": "Digital product"},
                ),
            },
            "weight": {
                "display_name": "Weight",
                "short_desc": "Product weight",
                "type": FloatType,
                "desc": "Product weight",
                "required": True,
            },
            "price": {
                "display_name": "Price",
                "short_desc": "Product price",
                "type": FloatType,
                "desc": "Product price",
                "required": True,
            },
            "sku": {
                "display_name": "SKU",
                "short_desc": "SKU identifier",
                "type": StringOrNothingType,
                "desc": "Stock Keeping Unit identifier",
            },
            "description": {
                "display_name": "Description",
                "short_desc": "Product description",
                "type": StringOrNothingType,
                "desc": "Product description (HTML allowed)",
            },
            "categories": {
                "display_name": "Categories",
                "short_desc": "Category IDs",
                "type": AutoListOrNothingType,
                "desc": "Array of category IDs this product belongs to",
            },
            "brand_id": {
                "display_name": "Brand ID",
                "short_desc": "Brand ID",
                "type": IntOrNothingType,
                "desc": "ID of the product brand",
                "ref_data": "brands",
            },
            "is_visible": {
                "display_name": "Is Visible",
                "short_desc": "Storefront visibility",
                "type": SoftBoolOrNothingType,
                "desc": "Whether the product is visible on the storefront",
            },
            "inventory_level": {
                "display_name": "Inventory Level",
                "short_desc": "Inventory level",
                "type": IntOrNothingType,
                "desc": "Current inventory level",
            },
            "inventory_tracking": {
                "display_name": "Inventory Tracking",
                "short_desc": "Inventory tracking method",
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
            "condition": {
                "display_name": "Condition",
                "short_desc": "Product condition",
                "type": StringOrNothingType,
                "desc": "Product condition",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "New", "desc": "New product"},
                    <AllowedValueInfo>{"value": "Used", "desc": "Used product"},
                    <AllowedValueInfo>{"value": "Refurbished",
                        "desc": "Refurbished product"},
                ),
            },
            "cost_price": {
                "display_name": "Cost Price",
                "short_desc": "Cost price",
                "type": FloatOrNothingType,
                "desc": "Cost price of the product",
            },
            "retail_price": {
                "display_name": "Retail Price",
                "short_desc": "Retail price (MSRP)",
                "type": FloatOrNothingType,
                "desc": "Retail price (MSRP)",
            },
            "sale_price": {
                "display_name": "Sale Price",
                "short_desc": "Sale price",
                "type": FloatOrNothingType,
                "desc": "Sale price (overrides price when set)",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! BigCommerce create product action data provider
public class BigCommerceCreateProductDataProvider inherits BigCommerceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new product in BigCommerce",
            "type": "BigCommerceCreateProductDataProvider",
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
        return "create";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Create a new product in BigCommerce";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new BigCommerceCreateProductRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new BigCommerceProductDataType();
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> response = doPostV3("catalog/products", req);
        return response.data;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BigCommerceDataProvider
