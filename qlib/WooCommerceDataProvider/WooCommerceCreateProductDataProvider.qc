# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WooCommerceDataProvider module definition

/*  WooCommerceCreateProductDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for creating a product
public class WooCommerceCreateProductRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "name": {
                "display_name": "Name",
                "type": StringType,
                "desc": "The product name",
                "example_value": "Premium T-Shirt",
                "required": True,
            },
            "type": {
                "display_name": "Type",
                "type": StringOrNothingType,
                "desc": "Product type: `simple`, `grouped`, `external`, `variable`",
                "allowed_values": ProductTypeAllowedValues,
            },
            "status": {
                "display_name": "Status",
                "type": StringOrNothingType,
                "desc": "Product status: `draft`, `pending`, `private`, `publish`",
                "allowed_values": ProductStatusAllowedValues,
            },
            "regular_price": {
                "display_name": "Regular Price",
                "type": StringOrNothingType,
                "desc": "The regular price of the product",
                "example_value": "29.99",
            },
            "description": {
                "display_name": "Description",
                "type": StringOrNothingType,
                "desc": "The full product description (may contain HTML)",
                "example_value": "A premium quality cotton t-shirt available in multiple sizes.",
            },
            "short_description": {
                "display_name": "Short Description",
                "type": StringOrNothingType,
                "desc": "The short product description",
                "example_value": "Premium cotton t-shirt",
            },
            "sku": {
                "display_name": "SKU",
                "type": StringOrNothingType,
                "desc": "The unique stock keeping unit identifier",
                "example_value": "TSHIRT-PREM-001",
            },
            "categories": {
                "display_name": "Categories",
                "type": SoftListOrNothingType,
                "desc": "List of category objects (each with an `id` key)",
            },
            "tags": {
                "display_name": "Tags",
                "type": SoftListOrNothingType,
                "desc": "List of tag objects (each with an `id` key)",
            },
            "images": {
                "display_name": "Images",
                "type": SoftListOrNothingType,
                "desc": "List of image objects (each with a `src` key)",
            },
            "manage_stock": {
                "display_name": "Manage Stock",
                "type": SoftBoolOrNothingType,
                "desc": "If `True`, stock management is enabled at the product level",
                "example_value": True,
            },
            "stock_quantity": {
                "display_name": "Stock Quantity",
                "type": IntOrNothingType,
                "desc": "The number of items in stock",
                "example_value": 100,
            },
            "weight": {
                "display_name": "Weight",
                "type": StringOrNothingType,
                "desc": "The product weight in the shop's weight unit",
                "example_value": "0.5",
            },
            "dimensions": {
                "display_name": "Dimensions",
                "type": new WooCommerceDimensionsDataType(AutoHashOrNothingType),
                "desc": "Product dimensions: `length`, `width`, `height`",
            },
            "virtual": {
                "display_name": "Virtual",
                "type": SoftBoolOrNothingType,
                "desc": "If `True`, the product is virtual and does not require shipping",
                "example_value": False,
            },
            "downloadable": {
                "display_name": "Downloadable",
                "type": SoftBoolOrNothingType,
                "desc": "If `True`, the product is downloadable",
                "example_value": False,
            },
            "meta_data": {
                "display_name": "Meta Data",
                "type": SoftListOrNothingType,
                "desc": "List of metadata key-value pairs for the product",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for creating a new WooCommerce product
public class WooCommerceCreateProductDataProvider inherits WooCommerceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Creates a new WooCommerce product",
            "type": "WooCommerceCreateProductDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static WooCommerceCreateProductRequestDataType RequestType();

        #! Response type
        static WooCommerceProductDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(WooCommerceRestClient::WooCommerceRestClient rest)
            : WooCommerceDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(WooCommerceRestClient::WooCommerceRestConnection conn) : WooCommerceDataProviderBase(conn) {
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : WooCommerceDataProviderBase(options) {
    }

    #! Returns the provider name
    string getName() {
        return "create";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Creates a new WooCommerce product";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("products", {} + req);
    }

    #! Returns the request type
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the response type
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace WooCommerceDataProvider
