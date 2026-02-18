# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ChargebeeDataProvider module definition

/*  ChargebeeCollectInvoicePaymentDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ChargebeeDataProvider module
public namespace ChargebeeDataProvider {

#! Request type for collecting payment on an invoice
public class ChargebeeCollectInvoicePaymentRequestDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "Invoice ID",
                "short_desc": "The invoice ID to collect payment for",
                "desc": "The unique identifier of the invoice to collect payment for",
                "required": True,
            },
            "amount": {
                "type": IntOrNothingType,
                "display_name": "Amount",
                "short_desc": "Payment amount in cents",
                "desc": "The amount to collect in the smallest currency unit (e.g., cents). "
                    "If not specified, the full balance is collected",
            },
            "payment_source_id": {
                "type": StringOrNothingType,
                "display_name": "Payment Source ID",
                "short_desc": "Payment source to use",
                "desc": "The identifier of the payment source to use for collecting the payment",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for collecting payment on an invoice
public class ChargebeeCollectInvoicePaymentDataProvider inherits ChargebeeDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "collect-invoice-payment",
            "desc": "Collect payment on an invoice in Chargebee",
            "type": "ChargebeeCollectInvoicePaymentDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static ChargebeeCollectInvoicePaymentRequestDataType RequestType();

        #! Response type
        static ChargebeeInvoiceDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(ChargebeeRestClient::ChargebeeRestClient rest)
            : ChargebeeDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(ChargebeeRestClient::ChargebeeRestConnection conn) : ChargebeeDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "collect-invoice-payment";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Collect payment on an invoice in Chargebee";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string id = req.id;
        hash<auto> body = req - "id";
        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: collecting payment for invoice %s", id));

        hash<auto> response = doPost("/invoices/" + id + "/collect_payment", body);
        return response.invoice;
    }

    #! Returns the request type
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the response type
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ChargebeeDataProvider
