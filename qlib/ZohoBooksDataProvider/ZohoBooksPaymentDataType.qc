# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ZohoBooksDataProvider module definition

/*  ZohoBooksPaymentDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Zoho Books payment data type
public class ZohoBooksPaymentDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "payment_id": {
                "display_name": "Payment ID",
                "short_desc": "Unique identifier for the payment",
                "desc": "The unique identifier for this payment in Zoho Books",
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
            "amount": {
                "display_name": "Amount",
                "short_desc": "Payment amount",
                "desc": "The payment amount",
                "type": SoftFloatType,
            },
            "unused_amount": {
                "display_name": "Unused Amount",
                "short_desc": "Unused payment amount",
                "desc": "The unused portion of the payment",
                "type": SoftFloatOrNothingType,
            },
            "date": {
                "display_name": "Payment Date",
                "short_desc": "Date of the payment",
                "desc": "The date of the payment (`YYYY-MM-DD` format)",
                "type": SoftStringOrNothingType,
            },
            "payment_mode": {
                "display_name": "Payment Mode",
                "short_desc": "Mode of payment",
                "desc": "The mode of payment used",
                "type": SoftStringOrNothingType,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "cash", "desc": "Cash payment"},
                    <AllowedValueInfo>{"value": "check", "desc": "Check payment"},
                    <AllowedValueInfo>{"value": "creditcard", "desc": "Credit card payment"},
                    <AllowedValueInfo>{"value": "banktransfer", "desc": "Bank transfer"},
                    <AllowedValueInfo>{"value": "bankremittance", "desc": "Bank remittance"},
                    <AllowedValueInfo>{"value": "autotransaction", "desc": "Auto transaction"},
                ),
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
            "exchange_rate": {
                "display_name": "Exchange Rate",
                "short_desc": "Currency exchange rate",
                "desc": "The exchange rate for currency conversion",
                "type": SoftFloatOrNothingType,
            },
            "bank_charges": {
                "display_name": "Bank Charges",
                "short_desc": "Bank charges",
                "desc": "Bank charges associated with the payment",
                "type": SoftFloatOrNothingType,
            },
            "account_id": {
                "display_name": "Account ID",
                "short_desc": "Account identifier",
                "desc": "The ID of the account receiving the payment",
                "type": SoftStringOrNothingType,
            },
            "account_name": {
                "display_name": "Account Name",
                "short_desc": "Account name",
                "desc": "The name of the account receiving the payment",
                "type": SoftStringOrNothingType,
            },
            "invoices": {
                "display_name": "Invoices",
                "short_desc": "Invoices linked to this payment",
                "desc": "The invoices this payment is applied to",
                "type": SoftAutoListOrNothingType,
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
        addQoreFields(Fields);
    }
}

} # namespace ZohoBooksDataProvider
