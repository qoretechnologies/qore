# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file PdfCoFillPdfFormDataProvider.qc PDF.co fill PDF form action data provider

/*  PdfCoFillPdfFormDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request type for filling PDF form fields
public class PdfCoFillPdfFormRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "url": {
                "type": StringType,
                "display_name": "Source URL",
                "short_desc": "URL to the source PDF form",
                "desc": "URL to the source PDF form",
                "required": True,
            },
            "fields": {
                "type": StringType,
                "display_name": "Fields",
                "short_desc": "JSON array of field name/value pairs to fill",
                "desc": "JSON array of field name/value pairs to fill (e.g., "
                    "`[{\"fieldName\": \"value\"}]`)",
                "required": True,
                "preselected": True,
            },
            "fieldsString": {
                "type": StringOrNothingType,
                "display_name": "Fields String",
                "short_desc": "Pipe-delimited field name/value pairs",
                "desc": "Pipe-delimited field name/value pairs (alternative to `fields`)",
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

#! PDF.co fill PDF form action data provider
public class PdfCoFillPdfFormDataProvider inherits PdfCoDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "fill-pdf-form",
            "desc": "Fills in form fields of a PDF form",
            "type": "PdfCoFillPdfFormDataProvider",
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
        return "fill-pdf-form";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Fills in form fields of a PDF form";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new PdfCoFillPdfFormRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new PdfCoUrlResponseDataType();
    }

    #! Performs the request
    # NOTE: pdf/edit/add is the unified PDF.co endpoint for annotations, images, and form fields;
    # the request body (fields/fieldsString) determines behavior
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("pdf/edit/add", req);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace PdfCoDataProvider
