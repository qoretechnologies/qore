# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore Cin7CoreDataProvider module definition

/*  Cin7CoreSalePaymentDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the Cin7CoreDataProvider module
public namespace Cin7CoreDataProvider {

#! Cin7Core sale payment data type
public class Cin7CoreSalePaymentDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "ID": {
                "type": SoftStringOrNothingType,
                "display_name": "Payment ID",
                "short_desc": "Unique payment identifier",
                "desc": "The unique identifier (GUID) of the payment",
            },
            "SaleID": {
                "type": SoftStringOrNothingType,
                "display_name": "Sale ID",
                "short_desc": "Sale identifier",
                "desc": "The GUID of the associated sale",
            },
            "SaleOrderNumber": {
                "type": SoftStringOrNothingType,
                "display_name": "Sale Order Number",
                "short_desc": "Order number",
                "desc": "The sale order number",
            },
            "InvoiceNumber": {
                "type": SoftStringOrNothingType,
                "display_name": "Invoice Number",
                "short_desc": "Invoice number",
                "desc": "The invoice number this payment applies to",
            },
            "CreditNoteNumber": {
                "type": SoftStringOrNothingType,
                "display_name": "Credit Note Number",
                "short_desc": "Credit note number",
                "desc": "The credit note number if this is a refund",
            },
            "Reference": {
                "type": SoftStringOrNothingType,
                "display_name": "Reference",
                "short_desc": "Payment reference",
                "desc": "Payment reference or transaction ID",
            },
            "Amount": {
                "type": SoftFloatOrNothingType,
                "display_name": "Amount",
                "short_desc": "Payment amount",
                "desc": "The payment amount",
            },
            "DatePaid": {
                "type": SoftDateOrNothingType,
                "display_name": "Date Paid",
                "short_desc": "Payment date",
                "desc": "The date the payment was made",
            },
            "PaymentMethod": {
                "type": SoftStringOrNothingType,
                "display_name": "Payment Method",
                "short_desc": "Payment method",
                "desc": "The method of payment (Cash, Credit Card, etc.)",
            },
            "Account": {
                "type": SoftStringOrNothingType,
                "display_name": "Account",
                "short_desc": "Payment account",
                "desc": "The account the payment was made to",
            },
            "CurrencyRate": {
                "type": SoftFloatOrNothingType,
                "display_name": "Currency Rate",
                "short_desc": "Exchange rate",
                "desc": "Currency exchange rate at time of payment",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("Cin7CoreSalePaymentDataType") {
        addQoreFields(Fields);
    }
}

#! Cin7Core add sale payment request data type
public class Cin7CoreAddSalePaymentRequestDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "SaleID": {
                "type": SoftStringType,
                "display_name": "Sale ID",
                "short_desc": "Sale identifier",
                "desc": "The GUID of the sale to add the payment to (required)",
                "required": True,
                "ref_data": "sales",
            },
            "Reference": {
                "type": SoftStringOrNothingType,
                "display_name": "Reference",
                "short_desc": "Payment reference",
                "desc": "Payment reference or transaction ID",
            },
            "Amount": {
                "type": SoftFloatType,
                "display_name": "Amount",
                "short_desc": "Payment amount",
                "desc": "The payment amount (required)",
                "required": True,
            },
            "DatePaid": {
                "type": SoftDateOrNothingType,
                "display_name": "Date Paid",
                "short_desc": "Payment date",
                "desc": "The date the payment was made (defaults to today)",
            },
            "PaymentMethod": {
                "type": SoftStringOrNothingType,
                "display_name": "Payment Method",
                "short_desc": "Payment method",
                "desc": "The method of payment",
            },
            "Account": {
                "type": SoftStringOrNothingType,
                "display_name": "Account",
                "short_desc": "Payment account",
                "desc": "The account to record the payment to",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("Cin7CoreAddSalePaymentRequestDataType") {
        addQoreFields(Fields);
    }
}

} # namespace Cin7CoreDataProvider
