# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SquarespaceUploadProductImageDataProvider.qc Squarespace upload product image action data provider

/*  SquarespaceUploadProductImageDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request type for uploading a product image
public class SquarespaceUploadProductImageRequestDataType inherits HashDataType {
    private {
        const Fields = {
            "product_id": {
                "type": StringType,
                "display_name": "Product ID",
                "short_desc": "The product ID to upload the image for",
                "desc": "The product ID to upload the image for",
                "required": True,
            },
            "fileName": {
                "type": StringType,
                "display_name": "File Name",
                "short_desc": "The image file name or URL",
                "desc": "The image file name or URL",
                "required": True,
            },
            "altText": {
                "type": StringOrNothingType,
                "display_name": "Alt Text",
                "short_desc": "Alternative text for the image",
                "desc": "Alternative text for the image",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Squarespace upload product image action data provider
public class SquarespaceUploadProductImageDataProvider inherits SquarespaceDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "upload",
            "desc": "Upload a product image in Squarespace",
            "type": "SquarespaceUploadProductImageDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(SquarespaceRestClient::SquarespaceRestClient rest) : SquarespaceDataProviderBase(rest) {
    }

    string getName() {
        return "upload";
    }

    *string getDesc() {
        return "Upload a product image in Squarespace";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new SquarespaceUploadProductImageRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return AbstractDataProviderType::get(AutoHashType);
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body = {
            "productId": req.product_id,
            "fileName": req.fileName,
        };
        if (exists req.altText) {
            body.altText = req.altText;
        }
        return doPost("v2/commerce/products/images", body);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SquarespaceDataProvider
