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
                "preselected": True,
                "sensitive": True,
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
                "desc": "Encryption algorithm to use",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "AES128", "desc": "AES 128-bit encryption"},
                    <AllowedValueInfo>{"value": "AES256", "desc": "AES 256-bit encryption"},
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
