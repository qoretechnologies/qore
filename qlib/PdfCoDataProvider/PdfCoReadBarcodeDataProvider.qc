# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file PdfCoReadBarcodeDataProvider.qc PDF.co read barcode action data provider

/*  PdfCoReadBarcodeDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request type for reading barcodes
public class PdfCoReadBarcodeRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "url": {
                "type": StringType,
                "display_name": "Source URL",
                "short_desc": "URL to the source file containing barcodes",
                "desc": "URL to the source file containing barcodes",
                "required": True,
            },
            "type": {
                "type": StringOrNothingType,
                "display_name": "Barcode Type",
                "short_desc": "Primary barcode type to detect",
                "desc": "Primary barcode type to detect (default: `QRCode`)",
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
            "types": {
                "type": StringOrNothingType,
                "display_name": "Barcode Types",
                "short_desc": "Comma-separated list of barcode types to detect",
                "desc": "Comma-separated list of barcode types to detect (overrides `type`)",
            },
            "pages": {
                "type": StringOrNothingType,
                "display_name": "Pages",
                "short_desc": "Page indices for barcode scanning",
                "desc": "Page indices as comma-separated values or ranges",
            },
            "async": {
                "type": SoftBoolOrNothingType,
                "display_name": "Async",
                "short_desc": "Run as a background job",
                "desc": "Set to `True` to run as a background job",
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

#! PDF.co read barcode action data provider
public class PdfCoReadBarcodeDataProvider inherits PdfCoDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "read-barcode",
            "desc": "Reads and decodes barcodes from images, TIFF, or PDF documents",
            "type": "PdfCoReadBarcodeDataProvider",
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
        return "read-barcode";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Reads and decodes barcodes from images, TIFF, or PDF documents";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new PdfCoReadBarcodeRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new PdfCoBarcodeReadResponseDataType();
    }

    #! Performs the request
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("barcode/read/from/url", req);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace PdfCoDataProvider
