# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WooCommerceDataProvider module definition

/*  WooCommerceCreateVariationDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for creating a variation
public class WooCommerceCreateVariationRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "product_id": {
                "display_name": "Product ID",
                "type": IntType,
                "desc": "The unique identifier for the parent product",
                "required": True,
            },
            "regular_price": {
                "display_name": "Regular Price",
                "type": StringOrNothingType,
                "desc": "The regular price of the variation",
                "example_value": "24.99",
            },
            "sale_price": {
                "display_name": "Sale Price",
                "type": StringOrNothingType,
                "desc": "The sale price of the variation",
                "example_value": "19.99",
            },
            "sku": {
                "display_name": "SKU",
                "type": StringOrNothingType,
                "desc": "The unique stock keeping unit identifier",
                "example_value": "TSHIRT-PREM-001-L",
            },
            "stock_quantity": {
                "display_name": "Stock Quantity",
                "type": IntOrNothingType,
                "desc": "The number of items in stock",
                "example_value": 50,
            },
            "manage_stock": {
                "display_name": "Manage Stock",
                "type": SoftBoolOrNothingType,
                "desc": "If `True`, stock management is enabled",
                "example_value": True,
            },
            "attributes": {
                "display_name": "Attributes",
                "type": SoftListOrNothingType,
                "desc": "List of variation attributes",
            },
            "weight": {
                "display_name": "Weight",
                "type": StringOrNothingType,
                "desc": "The variation weight",
                "example_value": "0.5",
            },
            "dimensions": {
                "display_name": "Dimensions",
                "type": new WooCommerceDimensionsDataType(AutoHashOrNothingType),
                "desc": "Variation dimensions: `length`, `width`, `height`",
            },
            "image": {
                "display_name": "Image",
                "type": new WooCommerceImageDataType(AutoHashOrNothingType),
                "desc": "Variation image object",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for creating a product variation
public class WooCommerceCreateVariationDataProvider inherits WooCommerceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-variation",
            "desc": "Creates a new variation for a WooCommerce product",
            "type": "WooCommerceCreateVariationDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static WooCommerceCreateVariationRequestDataType RequestType();

        #! Response type
        static WooCommerceVariationDataType ResponseType();
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
        return "create-variation";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Creates a new variation for a WooCommerce product";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        int product_id = remove req.product_id;
        return doPost(sprintf("products/%d/variations", product_id), {} + req);
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
