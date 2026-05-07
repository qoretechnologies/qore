# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file PdfCoAddPasswordDataProvider.qc PDF.co add password action data provider

/*  PdfCoAddPasswordDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request type for adding password protection to a PDF
public class PdfCoAddPasswordRequestDataType inherits HashDataType {
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
            "ownerPassword": {
                "type": StringType,
                "display_name": "Owner Password",
                "short_desc": "Owner password for full document control",
                "desc": "Owner password for full document control (editing, printing permissions)",
                "required": True,
                "sensitive": True,
                "example_value": "owner-secret",
            },
            "userPassword": {
                "type": StringOrNothingType,
                "display_name": "User Password",
                "short_desc": "User password required to open the document",
                "desc": "User password required to open the document",
                "preselected": True,
                "sensitive": True,
            },
            "encryptionAlgorithm": {
                "type": StringOrNothingType,
                "display_name": "Encryption Algorithm",
                "short_desc": "Encryption algorithm to use",
                "desc": "Encryption algorithm to use (default: `AES_128bit`)",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "RC4_40bit", "display_name": "RC4 40-bit",
                        "desc": "RC4 40-bit (legacy, weak)",
                    },
                    <AllowedValueInfo>{
                        "value": "RC4_128bit", "display_name": "RC4 128-bit",
                        "desc": "RC4 128-bit (legacy)",
                    },
                    <AllowedValueInfo>{
                        "value": "AES_128bit", "display_name": "AES 128-bit",
                        "desc": "AES 128-bit (recommended)",
                    },
                    <AllowedValueInfo>{
                        "value": "AES_256bit", "display_name": "AES 256-bit",
                        "desc": "AES 256-bit (strongest)",
                    },
                ),
            },
            "allowPrintDocument": {
                "type": SoftBoolOrNothingType,
                "display_name": "Allow Printing",
                "short_desc": "Allow printing the document",
                "desc": "Set to `True` to allow printing the document (requires `ownerPassword`)",
            },
            "allowFillForms": {
                "type": SoftBoolOrNothingType,
                "display_name": "Allow Filling Forms",
                "short_desc": "Allow filling interactive form fields",
                "desc": "Set to `True` to allow filling interactive form fields "
                    "(requires `ownerPassword`)",
            },
            "allowModifyDocument": {
                "type": SoftBoolOrNothingType,
                "display_name": "Allow Modifying Document",
                "short_desc": "Allow modifying the document",
                "desc": "Set to `True` to allow modifying the document (requires `ownerPassword`)",
            },
            "allowContentExtraction": {
                "type": SoftBoolOrNothingType,
                "display_name": "Allow Content Extraction",
                "short_desc": "Allow copying content from the document",
                "desc": "Set to `True` to allow copying text and graphics from the document "
                    "(requires `ownerPassword`)",
            },
            "allowModifyAnnotations": {
                "type": SoftBoolOrNothingType,
                "display_name": "Allow Modifying Annotations",
                "short_desc": "Allow modifying annotations and forms",
                "desc": "Set to `True` to allow interacting with annotations and form fields "
                    "(requires `ownerPassword`)",
            },
            "allowAccessibilitySupport": {
                "type": SoftBoolOrNothingType,
                "display_name": "Allow Accessibility Support",
                "short_desc": "Allow content extraction for accessibility",
                "desc": "Set to `True` to allow content extraction for accessibility tools "
                    "(requires `ownerPassword`)",
            },
            "allowAssemblyDocument": {
                "type": SoftBoolOrNothingType,
                "display_name": "Allow Assembling Document",
                "short_desc": "Allow assembling the document",
                "desc": "Set to `True` to allow inserting, rotating, or deleting pages and "
                    "creating bookmarks and thumbnails (requires `ownerPassword`)",
            },
            "printQuality": {
                "type": StringOrNothingType,
                "display_name": "Print Quality",
                "short_desc": "Maximum allowed print quality",
                "desc": "Maximum allowed print quality (requires `ownerPassword`)",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "HighResolution", "display_name": "High Resolution",
                        "desc": "High-resolution printing allowed",
                    },
                    <AllowedValueInfo>{
                        "value": "LowResolution", "display_name": "Low Resolution",
                        "desc": "Low-resolution printing only",
                    },
                ),
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
        } + PdfCoRemoteSourceFields;
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! PDF.co add password action data provider
public class PdfCoAddPasswordDataProvider inherits PdfCoDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "add-password",
            "desc": "Adds password protection and security settings to a PDF document",
            "type": "PdfCoAddPasswordDataProvider",
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
        return "add-password";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Adds password protection and security settings to a PDF document";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new PdfCoAddPasswordRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new PdfCoUrlResponseDataType();
    }

    #! Performs the request
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        req = uploadFileIfNeeded(req);
        return doPost("pdf/security/add", req);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace PdfCoDataProvider
