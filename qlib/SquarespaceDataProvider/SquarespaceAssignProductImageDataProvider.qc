# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SquarespaceAssignProductImageDataProvider.qc Squarespace assign product image action data provider

/*  SquarespaceAssignProductImageDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request type for assigning a product image to a variant
public class SquarespaceAssignProductImageRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "product_id": {
                "type": StringType,
                "display_name": "Product ID",
                "short_desc": "The product ID",
                "desc": "The ID of the product to assign the image to",
                "required": True,
                "ref_data": "products",
            },
            "image_id": {
                "type": StringType,
                "display_name": "Image ID",
                "short_desc": "The image ID",
                "desc": "The ID of the image to assign",
                "required": True,
            },
            "variant_id": {
                "type": StringType,
                "display_name": "Variant ID",
                "short_desc": "The variant ID",
                "desc": "The ID of the variant to assign the image to",
                "required": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Squarespace assign product image action data provider
public class SquarespaceAssignProductImageDataProvider inherits SquarespaceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "assign",
            "desc": "Assign a product image to a variant in Squarespace",
            "type": "SquarespaceAssignProductImageDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;
    }

    #! Creates the object with a REST client
    constructor(SquarespaceRestClient::SquarespaceRestClient rest) : SquarespaceDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "assign";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Assign a product image to a variant in Squarespace";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new SquarespaceAssignProductImageRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return AbstractDataProviderTypeMap."hash";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string path = sprintf("v2/commerce/products/%s/images/assign", req.product_id);
        hash<auto> body = {
            "imageId": req.image_id,
            "variantId": req.variant_id,
        };
        return doPost(path, body);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SquarespaceDataProvider
