# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ChargebeeDataProvider module definition

/*  ChargebeeRecordInvoicePaymentDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for recording a payment on an invoice
public class ChargebeeRecordInvoicePaymentRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "Invoice ID",
                "short_desc": "The invoice ID to record payment for",
                "desc": "The unique identifier of the invoice to record payment for",
                "required": True,
            },
            "amount": {
                "type": IntOrNothingType,
                "display_name": "Amount",
                "short_desc": "Payment amount in cents",
                "desc": "The amount of the payment in the smallest currency unit (e.g., cents). "
                    "If not specified, the full balance is recorded",
            },
            "payment_method": {
                "type": StringOrNothingType,
                "display_name": "Payment Method",
                "short_desc": "Method of payment",
                "desc": "The method used for the payment",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "cash", "desc": "Cash payment"},
                    <AllowedValueInfo>{"value": "check", "desc": "Check payment"},
                    <AllowedValueInfo>{"value": "bank_transfer", "desc": "Bank transfer"},
                    <AllowedValueInfo>{"value": "other", "desc": "Other payment method"},
                ),
            },
            "date": {
                "type": IntOrNothingType,
                "display_name": "Payment Date",
                "short_desc": "Date of payment",
                "desc": "The date of the payment (Unix timestamp in seconds)",
            },
            "reference_number": {
                "type": StringOrNothingType,
                "display_name": "Reference Number",
                "short_desc": "Payment reference number",
                "desc": "A reference number for the payment (e.g., check number or transaction ID)",
            },
            "comment": {
                "type": StringOrNothingType,
                "display_name": "Comment",
                "short_desc": "Payment comment",
                "desc": "A comment about the payment",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for recording a payment on an invoice
public class ChargebeeRecordInvoicePaymentDataProvider inherits ChargebeeDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "record-invoice-payment",
            "desc": "Record a payment on an invoice in Chargebee",
            "type": "ChargebeeRecordInvoicePaymentDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static ChargebeeRecordInvoicePaymentRequestDataType RequestType();

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
        return "record-invoice-payment";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Record a payment on an invoice in Chargebee";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string id = req.id;
        hash<auto> body = req - "id";
        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: recording payment for invoice %s", id));

        hash<auto> response = doPost("/invoices/" + id + "/record_payment", body);
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
