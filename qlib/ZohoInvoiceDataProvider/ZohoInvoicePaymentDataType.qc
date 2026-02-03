# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ZohoInvoiceDataProvider module definition

/*  ZohoInvoicePaymentDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Zoho Invoice payment data type
public class ZohoInvoicePaymentDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "payment_id": {
                "display_name": "Payment ID",
                "short_desc": "Unique identifier for the payment",
                "desc": "The unique identifier for this payment",
                "type": SoftStringType,
            },
            "payment_number": {
                "display_name": "Payment Number",
                "short_desc": "Payment number",
                "desc": "The payment number",
                "type": SoftStringOrNothingType,
            },
            "customer_id": {
                "display_name": "Customer ID",
                "short_desc": "Customer identifier",
                "desc": "The ID of the customer who made the payment",
                "type": SoftStringType,
            },
            "customer_name": {
                "display_name": "Customer Name",
                "short_desc": "Customer name",
                "desc": "The name of the customer",
                "type": SoftStringOrNothingType,
            },
            "date": {
                "display_name": "Payment Date",
                "short_desc": "Date of the payment",
                "desc": "The date of the payment (YYYY-MM-DD format)",
                "type": SoftStringOrNothingType,
            },
            "amount": {
                "display_name": "Amount",
                "short_desc": "Payment amount",
                "desc": "The amount of the payment",
                "type": SoftFloatType,
            },
            "currency_id": {
                "display_name": "Currency ID",
                "short_desc": "Currency identifier",
                "desc": "The currency ID for the payment",
                "type": SoftStringOrNothingType,
            },
            "currency_code": {
                "display_name": "Currency Code",
                "short_desc": "Currency code",
                "desc": "The currency code (e.g., USD, EUR)",
                "type": SoftStringOrNothingType,
            },
            "payment_mode": {
                "display_name": "Payment Mode",
                "short_desc": "Method of payment",
                "desc": "The method of payment (e.g., cash, check, credit_card)",
                "type": SoftStringOrNothingType,
            },
            "reference_number": {
                "display_name": "Reference Number",
                "short_desc": "Reference number",
                "desc": "An optional reference number (e.g., check number)",
                "type": SoftStringOrNothingType,
            },
            "description": {
                "display_name": "Description",
                "short_desc": "Payment description",
                "desc": "Description of the payment",
                "type": SoftStringOrNothingType,
            },
            "invoices": {
                "display_name": "Invoices",
                "short_desc": "Applied invoices",
                "desc": "The invoices this payment is applied to",
                "type": SoftAutoListOrNothingType,
            },
            "unused_amount": {
                "display_name": "Unused Amount",
                "short_desc": "Unused payment amount",
                "desc": "The amount of payment not yet applied to invoices",
                "type": SoftFloatOrNothingType,
            },
            "account_id": {
                "display_name": "Account ID",
                "short_desc": "Deposit account",
                "desc": "The account ID where payment was deposited",
                "type": SoftStringOrNothingType,
            },
            "account_name": {
                "display_name": "Account Name",
                "short_desc": "Account name",
                "desc": "The name of the deposit account",
                "type": SoftStringOrNothingType,
            },
            "created_time": {
                "display_name": "Created Time",
                "short_desc": "When the payment was created",
                "desc": "The timestamp when the payment was created",
                "type": SoftStringOrNothingType,
            },
            "last_modified_time": {
                "display_name": "Last Modified Time",
                "short_desc": "When the payment was last modified",
                "desc": "The timestamp when the payment was last modified",
                "type": SoftStringOrNothingType,
            },
        };
    }

    #! Creates the object
    constructor() {
        map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type, $1.value.default_value)),
            Fields.pairIterator();
    }
}

#! Zoho Invoice create payment request data type
public class ZohoInvoiceCreatePaymentRequestDataType inherits HashDataType {
    public {
        #! Field definitions for creating a payment
        const Fields = {
            "customer_id": {
                "display_name": "Customer ID",
                "short_desc": "Customer identifier",
                "desc": "The ID of the customer who made the payment",
                "type": SoftStringType,
                "required": True,
            },
            "date": {
                "display_name": "Payment Date",
                "short_desc": "Date of the payment",
                "desc": "The date of the payment (YYYY-MM-DD format)",
                "type": SoftStringOrNothingType,
            },
            "amount": {
                "display_name": "Amount",
                "short_desc": "Payment amount",
                "desc": "The amount of the payment",
                "type": SoftFloatType,
                "required": True,
            },
            "payment_mode": {
                "display_name": "Payment Mode",
                "short_desc": "Method of payment",
                "desc": "The method of payment (e.g., cash, check, credit_card)",
                "type": SoftStringOrNothingType,
            },
            "reference_number": {
                "display_name": "Reference Number",
                "short_desc": "Reference number",
                "desc": "An optional reference number (e.g., check number)",
                "type": SoftStringOrNothingType,
            },
            "description": {
                "display_name": "Description",
                "short_desc": "Payment description",
                "desc": "Description of the payment",
                "type": SoftStringOrNothingType,
            },
            "invoices": {
                "display_name": "Invoices",
                "short_desc": "Invoices to apply payment to",
                "desc": "List of invoices with amounts to apply the payment",
                "type": SoftAutoListOrNothingType,
            },
            "account_id": {
                "display_name": "Account ID",
                "short_desc": "Deposit account",
                "desc": "The account ID to deposit payment into",
                "type": SoftStringOrNothingType,
            },
            "custom_fields": {
                "display_name": "Custom Fields",
                "short_desc": "Custom field values",
                "desc": "Custom field values for the payment",
                "type": AutoHashOrNothingType,
            },
        };
    }

    #! Creates the object
    constructor() {
        map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type, $1.value.default_value)),
            Fields.pairIterator();
    }
}

} # namespace ZohoInvoiceDataProvider
