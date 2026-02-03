# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ZohoInvoiceDataProvider module definition

/*  ZohoInvoiceRefundPaymentDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ZohoInvoiceDataProvider module
public namespace ZohoInvoiceDataProvider {

#! Zoho Invoice refund payment data provider
public class ZohoInvoiceRefundPaymentDataProvider inherits ZohoInvoiceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "refund-payment",
            "desc": "Refunds a customer payment in Zoho Invoice",
            "type": "ZohoInvoiceRefundPaymentDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions + {
            "payment_id": <DataProviderOptionInfo>{
                "display_name": "Payment ID",
                "short_desc": "The payment to refund",
                "type": AbstractDataProviderType::get(SoftStringType),
                "desc": "The ID of the payment to refund",
                "required": True,
            },
        };

        #! Request type
        const RequestType = new HashDataType("RefundPaymentRequest", {
            "payment_id": {
                "display_name": "Payment ID",
                "type": SoftStringType,
                "desc": "The ID of the payment to refund",
            },
            "amount": {
                "display_name": "Refund Amount",
                "type": SoftFloatOrNothingType,
                "desc": "The amount to refund (defaults to full unused amount)",
            },
            "date": {
                "display_name": "Refund Date",
                "type": SoftStringOrNothingType,
                "desc": "The date of the refund (YYYY-MM-DD)",
            },
            "description": {
                "display_name": "Description",
                "type": SoftStringOrNothingType,
                "desc": "Description for the refund",
            },
        });

        #! Response type
        const ResponseType = new HashDataType("RefundPaymentResponse", {
            "code": {
                "display_name": "Code",
                "type": SoftIntType,
                "desc": "Response code (0 for success)",
            },
            "message": {
                "display_name": "Message",
                "type": SoftStringType,
                "desc": "Response message",
            },
        });
    }

    private {
        #! The payment ID
        string payment_id;
    }

    #! Creates the object with a REST client
    constructor(ZohoInvoiceRestClient::ZohoInvoiceRestClient rest) : ZohoInvoiceDataProviderBase(rest) {
    }

    #! Creates the object with options
    constructor(ZohoInvoiceRestClient::ZohoInvoiceRestClient rest, *hash<auto> options) : ZohoInvoiceDataProviderBase(rest) {
        if (options.payment_id) {
            payment_id = options.payment_id;
        }
    }

    #! Returns the provider name
    string getName() {
        return "refund-payment";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Refunds a customer payment in Zoho Invoice";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string id = req.payment_id ?? request_options.payment_id ?? payment_id;
        if (!id) {
            throw "ZOHOINVOICE-ERROR", "payment_id is required for refund operation";
        }

        # Build refund request body
        hash<auto> body;
        if (req.amount) {
            body.amount = req.amount;
        }
        if (req.date) {
            body.date = req.date;
        }
        if (req.description) {
            body.description = req.description;
        }

        return doPost(sprintf("/customerpayments/%s/refund", id), body);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ZohoInvoiceDataProvider
