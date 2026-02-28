# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file PdfCoAiInvoiceParserDataProvider.qc PDF.co AI invoice parser action data provider

/*  PdfCoAiInvoiceParserDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request type for AI invoice parsing
public class PdfCoAiInvoiceParserRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "url": {
                "type": StringType,
                "display_name": "Source URL",
                "short_desc": "URL to the source invoice file",
                "desc": "URL to the source invoice file (PDF, image, or scanned document)",
                "required": True,
            },
            "customField": {
                "type": StringOrNothingType,
                "display_name": "Custom Fields",
                "short_desc": "Custom field names to extract",
                "desc": "JSON string with custom field names to extract (camelCase format)",
            },
            "lineItemStructure": {
                "type": StringOrNothingType,
                "display_name": "Line Item Structure",
                "short_desc": "Custom line item schema",
                "desc": "Custom schema defining line item structure as a JSON string",
            },
            "async": {
                "type": SoftBoolOrNothingType,
                "display_name": "Async",
                "short_desc": "Run as a background job",
                "desc": "Set to `True` to run as a background job",
            },
            "callback": {
                "type": StringOrNothingType,
                "display_name": "Callback URL",
                "short_desc": "Webhook URL for results",
                "desc": "Webhook URL to receive results when processing is complete",
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

#! PDF.co AI invoice parser action data provider
public class PdfCoAiInvoiceParserDataProvider inherits PdfCoDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "ai-invoice-parser",
            "desc": "Extracts structured data from invoices using AI, including vendor, customer, line "
                "items, and payment details",
            "type": "PdfCoAiInvoiceParserDataProvider",
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
        return "ai-invoice-parser";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Extracts structured data from invoices using AI, including vendor, customer, line "
            "items, and payment details";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new PdfCoAiInvoiceParserRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new PdfCoInvoiceParserResponseDataType();
    }

    #! Performs the request
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("ai-invoice-parser", req);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace PdfCoDataProvider
