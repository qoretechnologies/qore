# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file PdfCoCompressPdfDataProvider.qc PDF.co compress PDF action data provider

/*  PdfCoCompressPdfDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request type for compressing PDF files
public class PdfCoCompressPdfRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "file": {
                "type": FileOrNothingDataType,
                "display_name": "File",
                "short_desc": "File to process",
                "desc": "File to process. Provide either this or a source URL.",
                "required_groups": ("source",),
                "preselected": True,
            },
            "url": {
                "type": StringOrNothingType,
                "display_name": "Source URL",
                "short_desc": "URL to the source PDF file",
                "desc": "URL to the source PDF file",
                "required_groups": ("source",),
            },
            "config": {
                "type": StringOrNothingType,
                "display_name": "Config",
                "short_desc": "Compression configuration as a JSON string",
                "desc": "Compression configuration options serialized as a JSON string. Use this "
                    "to fine-tune the compression process.\n\n"
                    "**Example**: `{\"compressionLevel\": 4, \"imageQuality\": 75}`",
            },
            "pages": {
                "type": StringOrNothingType,
                "display_name": "Pages",
                "short_desc": "Page indices for compression",
                "desc": "Page indices as comma-separated values or ranges (e.g., `0, 1, 2-`)",
            },
            "password": {
                "type": StringOrNothingType,
                "display_name": "Password",
                "short_desc": "PDF file password",
                "desc": "PDF file password if the document is protected",
                "sensitive": True,
            },
            "async": {
                "type": SoftBoolOrNothingType,
                "display_name": "Async",
                "short_desc": "Run as a background job",
                "desc": "Set to `True` to run as a background job and return a `jobId` immediately. "
                    "Use the `job-check` action to poll for results.",
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
            "outputDataFormat": {
                "type": StringOrNothingType,
                "display_name": "Output Data Format",
                "short_desc": "Set to base64 to receive base64-encoded output content",
                "desc": "Set to `base64` to receive the compressed file as base64-encoded content "
                    "in the response instead of as a URL",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "base64",
                        "desc": "Return the compressed file as base64-encoded content",
                    },
                ),
            },
            "JPEGQuality": {
                "type": SoftIntOrNothingType,
                "display_name": "JPEG Quality",
                "short_desc": "JPEG quality for image compression",
                "desc": "JPEG quality for image compression (`1`-`100`, default: `25`)",
            },
        } + PdfCoRemoteSourceFields;
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! PDF.co compress PDF action data provider
public class PdfCoCompressPdfDataProvider inherits PdfCoDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "compress-pdf",
            "desc": "Reduces the file size of a PDF document",
            "type": "PdfCoCompressPdfDataProvider",
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
        return "compress-pdf";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Reduces the file size of a PDF document";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new PdfCoCompressPdfRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new PdfCoUrlResponseDataType();
    }

    #! Performs the request
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        req = uploadFileIfNeeded(req);
        return doPostV2("pdf/compress", req);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace PdfCoDataProvider
