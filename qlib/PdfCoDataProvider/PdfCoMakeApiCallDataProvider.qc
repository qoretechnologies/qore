# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file PdfCoMakeApiCallDataProvider.qc PDF.co make API call action data provider

/*  PdfCoMakeApiCallDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request type for making a custom API call to PDF.co
public class PdfCoMakeApiCallRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "method": {
                "type": StringType,
                "display_name": "HTTP Method",
                "short_desc": "HTTP method for the API call",
                "desc": "HTTP method for the API call",
                "required": True,
                "preselected": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "GET", "desc": "HTTP GET request"},
                    <AllowedValueInfo>{"value": "POST", "desc": "HTTP POST request"},
                ),
            },
            "path": {
                "type": StringType,
                "display_name": "API Path",
                "short_desc": "API endpoint path",
                "desc": "API endpoint path (e.g., `pdf/info`, `barcode/generate`)",
                "required": True,
                "preselected": True,
                "example_value": "pdf/info",
            },
            "body": {
                "type": StringOrNothingType,
                "display_name": "Request Body",
                "short_desc": "Request body as a JSON string",
                "desc": "Request body as a JSON string (for POST requests)",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! PDF.co make API call action data provider
public class PdfCoMakeApiCallDataProvider inherits PdfCoDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "make-api-call",
            "desc": "Performs a custom authorized API call to any PDF.co endpoint",
            "type": "PdfCoMakeApiCallDataProvider",
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
        return "make-api-call";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Performs a custom authorized API call to any PDF.co endpoint";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new PdfCoMakeApiCallRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new PdfCoUrlResponseDataType();
    }

    #! Performs the request
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string method = remove req.method;
        string path = remove req.path;
        *string body_str = remove req.body;
        if (method == "GET") {
            return doGet(path);
        }
        # Parse body JSON if provided
        *hash<auto> body;
        if (body_str.val()) {
            try {
                auto parsed = parse_json(body_str);
                if (exists parsed && parsed.typeCode() != NT_HASH) {
                    throw "PDFCO-ERROR", "the 'body' parameter must be a JSON object, not "
                        + parsed.type();
                }
                body = parsed;
            } catch (hash<ExceptionInfo> ex) {
                if (ex.err == "PDFCO-ERROR") {
                    rethrow;
                }
                throw "PDFCO-ERROR", sprintf("invalid JSON in 'body' parameter: %s", ex.desc);
            }
        }
        return doPost(path, body);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace PdfCoDataProvider
