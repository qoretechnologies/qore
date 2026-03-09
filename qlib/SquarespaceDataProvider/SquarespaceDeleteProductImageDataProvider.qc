# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SquarespaceDeleteProductImageDataProvider.qc Squarespace delete product image action data provider

/*  SquarespaceDeleteProductImageDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request type for deleting a product image
public class SquarespaceDeleteProductImageRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "product_id": {
                "type": StringType,
                "display_name": "Product ID",
                "short_desc": "The product ID",
                "desc": "The ID of the product that owns the image",
                "required": True,
                "ref_data": "products",
            },
            "image_id": {
                "type": StringType,
                "display_name": "Image ID",
                "short_desc": "The image ID",
                "desc": "The ID of the image to delete",
                "required": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Squarespace delete product image action data provider
public class SquarespaceDeleteProductImageDataProvider inherits SquarespaceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete",
            "desc": "Delete a product image from Squarespace",
            "type": "SquarespaceDeleteProductImageDataProvider",
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
        return "delete";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Delete a product image from Squarespace";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new SquarespaceDeleteProductImageRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return AbstractDataProviderTypeMap."hash";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string path = sprintf("v2/commerce/products/%s/images/%s", req.product_id, req.image_id);
        doDelete(path);
        return {};
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SquarespaceDataProvider
