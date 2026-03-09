# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore UnleashedDataProvider module definition

/*  UnleashedCreateProductDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the UnleashedDataProvider module
public namespace UnleashedDataProvider {

#! Data provider for creating a product
public class UnleashedCreateProductDataProvider inherits UnleashedDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-product",
            "desc": "Creates a new product in Unleashed",
            "type": "UnleashedCreateProductDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static UnleashedCreateProductRequestDataType RequestType();

        #! Response type
        static UnleashedProductDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(UnleashedRestClient::UnleashedRestClient rest) : UnleashedDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(UnleashedRestClient::UnleashedRestConnection conn) : UnleashedDataProviderBase(conn) {
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : UnleashedDataProviderBase(options) {
    }

    #! Returns the provider name
    string getName() {
        return "create-product";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Creates a new product in Unleashed";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        @assert(req.ProductCode.val(), "ProductCode is required");
        @assert(req.ProductDescription.val(), "ProductDescription is required");

        hash<auto> body = {
            "ProductCode": req.ProductCode,
            "ProductDescription": req.ProductDescription,
        };

        # Optional fields
        if (exists req.Barcode) body.Barcode = req.Barcode;
        if (exists req.UnitOfMeasure) body.UnitOfMeasure = req.UnitOfMeasure;
        if (exists req.DefaultPurchasePrice) body.DefaultPurchasePrice = req.DefaultPurchasePrice;
        if (exists req.DefaultSellPrice) body.DefaultSellPrice = req.DefaultSellPrice;
        if (exists req.IsSellable) body.IsSellable = req.IsSellable;
        if (exists req.IsPurchasable) body.IsPurchasable = req.IsPurchasable;
        if (exists req.IsAssembledProduct) body.IsAssembledProduct = req.IsAssembledProduct;

        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: creating product %y", body.ProductCode));
        hash<auto> response = doPost("/Products", body);
        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: created product %y", response.Product.Guid));
        return response.Product;
    }

    #! Returns the request type
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the response type
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Request data type for creating a product
public class UnleashedCreateProductRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "ProductCode": {
                "type": SoftStringType,
                "display_name": "Product Code",
                "short_desc": "Unique product code",
                "desc": "The unique product code (required, cannot be modified after creation)",
                "required": True,
            },
            "ProductDescription": {
                "type": SoftStringType,
                "display_name": "Product Description",
                "short_desc": "Product description",
                "desc": "The product description (required)",
                "required": True,
            },
            "Barcode": {
                "type": SoftStringOrNothingType,
                "display_name": "Barcode",
                "short_desc": "Product barcode",
                "desc": "The product barcode",
            },
            "UnitOfMeasure": {
                "type": SoftStringOrNothingType,
                "display_name": "Unit of Measure",
                "short_desc": "Unit of measure",
                "desc": "The unit of measure (e.g., 'Each', 'Box', 'Kg')",
            },
            "DefaultPurchasePrice": {
                "type": SoftFloatOrNothingType,
                "display_name": "Default Purchase Price",
                "short_desc": "Default purchase price",
                "desc": "The default purchase price",
            },
            "DefaultSellPrice": {
                "type": SoftFloatOrNothingType,
                "display_name": "Default Sell Price",
                "short_desc": "Default sell price",
                "desc": "The default sell price",
            },
            "IsSellable": {
                "type": SoftBoolOrNothingType,
                "display_name": "Is Sellable",
                "short_desc": "Can product be sold",
                "desc": "Whether the product can be sold",
            },
            "IsPurchasable": {
                "type": SoftBoolOrNothingType,
                "display_name": "Is Purchasable",
                "short_desc": "Can product be purchased",
                "desc": "Whether the product can be purchased",
            },
            "IsAssembledProduct": {
                "type": SoftBoolOrNothingType,
                "display_name": "Is Assembled Product",
                "short_desc": "Is this an assembled product",
                "desc": "Whether this is an assembled product (made from components)",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("UnleashedCreateProductRequestDataType") {
        addQoreFields(Fields);
    }
}

} # namespace UnleashedDataProvider
