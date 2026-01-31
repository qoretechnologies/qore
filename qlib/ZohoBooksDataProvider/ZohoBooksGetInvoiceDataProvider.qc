# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ZohoBooksDataProvider module definition

/*  ZohoBooksGetInvoiceDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ZohoBooksDataProvider module
public namespace ZohoBooksDataProvider {

#! Zoho Books get invoice data provider
public class ZohoBooksGetInvoiceDataProvider inherits ZohoBooksDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-invoice",
            "desc": "Gets details of a specific invoice from Zoho Books",
            "type": "ZohoBooksGetInvoiceDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions + {
            "invoice_id": <DataProviderOptionInfo>{
                "display_name": "Invoice ID",
                "short_desc": "The invoice to retrieve",
                "type": AbstractDataProviderType::get(SoftStringType),
                "desc": "The ID of the invoice to retrieve",
                "required": True,
            },
        };

        #! Request type
        const RequestType = new HashDataType("GetInvoiceRequest", {
            "invoice_id": {
                "display_name": "Invoice ID",
                "type": SoftStringType,
                "desc": "The ID of the invoice to retrieve",
            },
        });
    }

    private {
        #! The invoice ID
        string invoice_id;
    }

    #! Creates the object with a REST client
    constructor(ZohoBooksRestClient::ZohoBooksRestClient rest) : ZohoBooksDataProviderBase(rest) {
    }

    #! Creates the object with options
    constructor(ZohoBooksRestClient::ZohoBooksRestClient rest, *hash<auto> options) : ZohoBooksDataProviderBase(rest) {
        if (options.invoice_id) {
            invoice_id = options.invoice_id;
        }
    }

    #! Returns the provider name
    string getName() {
        return "get-invoice";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Gets details of a specific invoice from Zoho Books";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new ZohoBooksInvoiceDataType();
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string id = req.invoice_id ?? request_options.invoice_id ?? invoice_id;
        if (!id) {
            throw "ZOHOBOOKS-ERROR", "invoice_id is required for get operation";
        }
        hash<auto> response = doGet(sprintf("/invoices/%s", id));
        return response.invoice;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ZohoBooksDataProvider
