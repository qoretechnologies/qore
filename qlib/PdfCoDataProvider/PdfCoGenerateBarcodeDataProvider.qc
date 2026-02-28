# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file PdfCoGenerateBarcodeDataProvider.qc PDF.co generate barcode action data provider

/*  PdfCoGenerateBarcodeDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the PdfCoDataProvider module
public namespace PdfCoDataProvider {

#! Request type for barcode generation
public class PdfCoGenerateBarcodeRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "type": {
                "type": StringType,
                "display_name": "Barcode Type",
                "short_desc": "Barcode type to generate",
                "desc": "Barcode type to generate",
                "required": True,
                "preselected": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "QRCode", "desc": "QR Code"},
                    <AllowedValueInfo>{"value": "Code128", "desc": "Code 128"},
                    <AllowedValueInfo>{"value": "Code39", "desc": "Code 39"},
                    <AllowedValueInfo>{"value": "Datamatrix", "desc": "Data Matrix"},
                    <AllowedValueInfo>{"value": "PDF417", "desc": "PDF417"},
                    <AllowedValueInfo>{"value": "EAN13", "desc": "EAN-13"},
                    <AllowedValueInfo>{"value": "EAN8", "desc": "EAN-8"},
                    <AllowedValueInfo>{"value": "UPCA", "desc": "UPC-A"},
                    <AllowedValueInfo>{"value": "UPCE", "desc": "UPC-E"},
                ),
            },
            "value": {
                "type": StringType,
                "display_name": "Value",
                "short_desc": "String value to encode",
                "desc": "String value to encode inside the barcode",
                "required": True,
                "preselected": True,
            },
            "decorationImage": {
                "type": StringOrNothingType,
                "display_name": "Decoration Image",
                "short_desc": "Logo image URL for QR code center",
                "desc": "URL to a logo image to embed in the center of a QR code",
            },
            "async": {
                "type": SoftBoolOrNothingType,
                "display_name": "Async",
                "short_desc": "Run as a background job",
                "desc": "Set to `True` to run as a background job",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Output Filename",
                "short_desc": "Output filename",
                "desc": "Output filename",
            },
            "expiration": {
                "type": SoftIntOrNothingType,
                "display_name": "Expiration",
                "short_desc": "Output link expiration time in minutes",
                "desc": "Output link expiration time in minutes (default: `60`)",
            },
            "inline": {
                "type": SoftBoolOrNothingType,
                "display_name": "Inline",
                "short_desc": "Return the barcode image inline",
                "desc": "Set to `True` to return the barcode image inline",
            },
            "profiles": {
                "type": StringOrNothingType,
                "display_name": "Profiles",
                "short_desc": "Advanced processing profiles",
                "desc": "Advanced processing profiles as a JSON string",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! PDF.co generate barcode action data provider
public class PdfCoGenerateBarcodeDataProvider inherits PdfCoDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "generate-barcode",
            "desc": "Generates a printable and scannable barcode image (QR Code, Code 39, Code 128, "
                "Datamatrix, PDF417, UPC, EAN, and more)",
            "type": "PdfCoGenerateBarcodeDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;
    }

    #! Creates the object with a REST client
    constructor(PdfCoRestClient::PdfCoRestClient rest) : PdfCoDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "generate-barcode";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Generates a printable and scannable barcode image (QR Code, Code 39, Code 128, "
            "Datamatrix, PDF417, UPC, EAN, and more)";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new PdfCoGenerateBarcodeRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new PdfCoUrlResponseDataType();
    }

    #! Performs the request
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("barcode/generate", req);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace PdfCoDataProvider
