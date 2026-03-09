# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SquareSendInvoiceDataProvider.qc Square send (publish) invoice action

/*  SquareSendInvoiceDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SquareDataProvider module
public namespace SquareDataProvider {

#! Request type for sending (publishing) an invoice
public class SquareSendInvoiceRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "invoice_id": {
                "type": StringType,
                "display_name": "Invoice ID",
                "short_desc": "The invoice ID to send",
                "desc": "The unique identifier of the invoice to publish/send",
                "required": True,
            },
            "version": {
                "type": IntType,
                "display_name": "Version",
                "short_desc": "The invoice version (required)",
                "desc": "The current version of the invoice (for optimistic concurrency)",
                "required": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Square send invoice action data provider
public class SquareSendInvoiceDataProvider inherits SquareDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "send",
            "desc": "Publish (send) an invoice in Square",
            "type": "SquareSendInvoiceDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(SquareRestClient::SquareRestClient rest) : SquareDataProviderBase(rest) {
    }

    string getName() {
        return "send";
    }

    *string getDesc() {
        return "Publish (send) an invoice in Square";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new SquareSendInvoiceRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new SquareInvoiceDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body = {
            "idempotency_key": generateIdempotencyKey(),
            "version": req.version,
        };
        return doPost(sprintf("invoices/%s/publish", req.invoice_id), body).invoice;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SquareDataProvider
