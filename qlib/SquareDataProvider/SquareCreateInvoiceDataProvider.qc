# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SquareCreateInvoiceDataProvider.qc Square create invoice action

/*  SquareCreateInvoiceDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for creating an invoice (user-friendly flattened fields)
public class SquareCreateInvoiceRequestDataType inherits HashDataType {
    private {
        const Fields = {
            "location_id": {
                "type": StringType,
                "display_name": "Location ID",
                "short_desc": "The location ID (required)",
                "desc": "The ID of the location where the invoice is created",
                "required": True,
            },
            "order_id": {
                "type": StringType,
                "display_name": "Order ID",
                "short_desc": "The order ID (required)",
                "desc": "The ID of the order associated with this invoice",
                "required": True,
            },
            "customer_id": {
                "type": StringOrNothingType,
                "display_name": "Customer ID",
                "short_desc": "The customer ID",
                "desc": "The ID of the customer to invoice (used in primary_recipient)",
            },
            "payment_request_type": {
                "type": StringOrNothingType,
                "display_name": "Payment Request Type",
                "short_desc": "Type of payment request",
                "desc": "The type of payment request (BALANCE, DEPOSIT, INSTALLMENT). "
                    "Default: BALANCE",
                "default_value": "BALANCE",
            },
            "due_date": {
                "type": StringOrNothingType,
                "display_name": "Due Date",
                "short_desc": "Invoice due date",
                "desc": "The due date for the invoice payment (YYYY-MM-DD format)",
            },
            "delivery_method": {
                "type": StringOrNothingType,
                "display_name": "Delivery Method",
                "short_desc": "How to deliver the invoice",
                "desc": "The delivery method for the invoice (EMAIL, SHARE_MANUALLY, SMS)",
            },
            "invoice_number": {
                "type": StringOrNothingType,
                "display_name": "Invoice Number",
                "short_desc": "Custom invoice number",
                "desc": "A user-defined invoice number",
            },
            "title": {
                "type": StringOrNothingType,
                "display_name": "Title",
                "short_desc": "Invoice title",
                "desc": "The title of the invoice",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "Invoice description",
                "desc": "A description of the invoice",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Square create invoice action data provider
public class SquareCreateInvoiceDataProvider inherits SquareDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new invoice in Square",
            "type": "SquareCreateInvoiceDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(SquareRestClient::SquareRestClient rest) : SquareDataProviderBase(rest) {
    }

    string getName() {
        return "create";
    }

    *string getDesc() {
        return "Create a new invoice in Square";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new SquareCreateInvoiceRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new SquareInvoiceDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        # Build the invoice structure from flattened user-friendly fields
        hash<auto> invoice = {
            "location_id": req.location_id,
            "order_id": req.order_id,
        };

        if (req.customer_id) {
            invoice.primary_recipient = {"customer_id": req.customer_id};
        }
        if (req.invoice_number) {
            invoice.invoice_number = req.invoice_number;
        }
        if (req.title) {
            invoice.title = req.title;
        }
        if (req.description) {
            invoice.description = req.description;
        }
        if (req.delivery_method) {
            invoice.delivery_method = req.delivery_method;
        }

        # Build payment request
        hash<auto> payment_request = {
            "request_type": req.payment_request_type ?? "BALANCE",
        };
        if (req.due_date) {
            payment_request.due_date = req.due_date;
        }
        invoice.payment_requests = (payment_request,);

        hash<auto> body = {
            "idempotency_key": generateIdempotencyKey(),
            "invoice": invoice,
        };
        return doPost("invoices", body).invoice;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SquareDataProvider
