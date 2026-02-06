# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore UnleashedDataProvider module definition

/*  UnleashedUpdateProductDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Data provider for updating a product
public class UnleashedUpdateProductDataProvider inherits UnleashedDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "update-product",
            "desc": "Updates an existing product in Unleashed",
            "type": "UnleashedUpdateProductDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static UnleashedUpdateProductRequestDataType RequestType();

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
        return "update-product";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Updates an existing product in Unleashed (note: Unleashed uses POST for updates)";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string guid = req.guid;
        @assert(guid.val(), "guid is required");

        hash<auto> body = {};

        # Optional fields
        if (exists req.ProductDescription) body.ProductDescription = req.ProductDescription;
        if (exists req.Barcode) body.Barcode = req.Barcode;
        if (exists req.DefaultPurchasePrice) body.DefaultPurchasePrice = req.DefaultPurchasePrice;
        if (exists req.DefaultSellPrice) body.DefaultSellPrice = req.DefaultSellPrice;
        if (exists req.IsSellable) body.IsSellable = req.IsSellable;
        if (exists req.IsPurchasable) body.IsPurchasable = req.IsPurchasable;

        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: updating product %y", guid));
        # Unleashed uses POST for updates
        hash<auto> response = doPost("/Products/" + guid, body);
        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: updated product %y", response.Product.Guid));
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

#! Request data type for updating a product
public class UnleashedUpdateProductRequestDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "guid": {
                "type": StringType,
                "display_name": "Product GUID",
                "short_desc": "The product GUID",
                "desc": "The unique identifier (GUID) of the product to update",
                "required": True,
                "ref_data": "products",
            },
            "ProductDescription": {
                "type": SoftStringOrNothingType,
                "display_name": "Product Description",
                "short_desc": "Product description",
                "desc": "The product description",
            },
            "Barcode": {
                "type": SoftStringOrNothingType,
                "display_name": "Barcode",
                "short_desc": "Product barcode",
                "desc": "The product barcode",
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
        };
    }

    #! Creates the object
    constructor() : HashDataType("UnleashedUpdateProductRequestDataType") {
        addQoreFields(Fields);
    }
}

} # namespace UnleashedDataProvider
