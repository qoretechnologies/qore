# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShipStationDataProvider module definition

/*  ShipStationUpdateProductDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ShipStationDataProvider module
public namespace ShipStationDataProvider {

#! Request type for updating a product
public class ShipStationUpdateProductRequestDataType inherits HashDataType {
    private {
        const Fields = {
            "productId": {
                "type": IntType,
                "display_name": "Product ID",
                "short_desc": "The product ID to update",
                "desc": "The unique ShipStation product ID to update",
                "required": True,
            },
            "sku": {
                "type": SoftStringOrNothingType,
                "display_name": "SKU",
                "short_desc": "Stock keeping unit",
                "desc": "The product's stock keeping unit (SKU)",
            },
            "name": {
                "type": SoftStringOrNothingType,
                "display_name": "Name",
                "short_desc": "Product name",
                "desc": "The product name",
            },
            "price": {
                "type": SoftFloatOrNothingType,
                "display_name": "Price",
                "short_desc": "Product price",
                "desc": "The product price",
            },
            "defaultCost": {
                "type": SoftFloatOrNothingType,
                "display_name": "Default Cost",
                "short_desc": "Default cost",
                "desc": "The default cost of the product",
            },
            "length": {
                "type": SoftFloatOrNothingType,
                "display_name": "Length",
                "short_desc": "Product length",
                "desc": "The product length in inches",
            },
            "width": {
                "type": SoftFloatOrNothingType,
                "display_name": "Width",
                "short_desc": "Product width",
                "desc": "The product width in inches",
            },
            "height": {
                "type": SoftFloatOrNothingType,
                "display_name": "Height",
                "short_desc": "Product height",
                "desc": "The product height in inches",
            },
            "weightOz": {
                "type": SoftFloatOrNothingType,
                "display_name": "Weight (oz)",
                "short_desc": "Product weight in ounces",
                "desc": "The product weight in ounces",
            },
            "internalNotes": {
                "type": SoftStringOrNothingType,
                "display_name": "Internal Notes",
                "short_desc": "Internal notes",
                "desc": "Internal notes about the product",
            },
            "fulfillmentSku": {
                "type": SoftStringOrNothingType,
                "display_name": "Fulfillment SKU",
                "short_desc": "Fulfillment center SKU",
                "desc": "The SKU used by the fulfillment center",
            },
            "active": {
                "type": SoftBoolOrNothingType,
                "display_name": "Active",
                "short_desc": "Is product active",
                "desc": "Whether the product is active",
            },
            "warehouseLocation": {
                "type": SoftStringOrNothingType,
                "display_name": "Warehouse Location",
                "short_desc": "Location in warehouse",
                "desc": "The location of the product in the warehouse",
            },
            "defaultCarrierCode": {
                "type": SoftStringOrNothingType,
                "display_name": "Default Carrier Code",
                "short_desc": "Default carrier",
                "desc": "The default carrier code for shipping this product",
            },
            "defaultServiceCode": {
                "type": SoftStringOrNothingType,
                "display_name": "Default Service Code",
                "short_desc": "Default service",
                "desc": "The default service code for shipping this product",
            },
            "defaultPackageCode": {
                "type": SoftStringOrNothingType,
                "display_name": "Default Package Code",
                "short_desc": "Default package",
                "desc": "The default package code for shipping this product",
            },
            "defaultConfirmation": {
                "type": SoftStringOrNothingType,
                "display_name": "Default Confirmation",
                "short_desc": "Default confirmation type",
                "desc": "The default confirmation type for shipments",
            },
            "customsDescription": {
                "type": SoftStringOrNothingType,
                "display_name": "Customs Description",
                "short_desc": "Customs description",
                "desc": "The customs description for international shipments",
            },
            "customsValue": {
                "type": SoftFloatOrNothingType,
                "display_name": "Customs Value",
                "short_desc": "Customs value",
                "desc": "The customs value for international shipments",
            },
            "customsTariffNo": {
                "type": SoftStringOrNothingType,
                "display_name": "Customs Tariff Number",
                "short_desc": "HS tariff number",
                "desc": "The harmonized system tariff number for customs",
            },
            "customsCountryCode": {
                "type": SoftStringOrNothingType,
                "display_name": "Customs Country Code",
                "short_desc": "Country of origin",
                "desc": "The country of origin code for customs",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for updating a product
public class ShipStationUpdateProductDataProvider inherits ShipStationDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "update-product",
            "desc": "Update a ShipStation product",
            "type": "ShipStationUpdateProductDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static ShipStationUpdateProductRequestDataType RequestType();

        #! Response type
        static ShipStationProductDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(ShipStationRestClient::ShipStationRestClient rest)
            : ShipStationDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(ShipStationRestClient::ShipStationRestConnection conn) : ShipStationDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "update-product";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Update a ShipStation product";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        int productId = req.productId;
        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: updating product %d", productId));

        hash<auto> body = req - "productId";
        hash<auto> response = doPut(sprintf("/products/%d", productId), body);
        return response;
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

} # namespace ShipStationDataProvider
