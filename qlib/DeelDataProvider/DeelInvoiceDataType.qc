# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DeelInvoiceDataType.qc Deel invoice record data type

/*  DeelInvoiceDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the DeelDataProvider module
public namespace DeelDataProvider {

#! Deel invoice record type
public class DeelInvoiceDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": SoftStringType,
                "display_name": "ID",
                "short_desc": "The invoice ID",
                "desc": "The unique identifier of the invoice",
            },
            "contract_id": {
                "type": StringOrNothingType,
                "display_name": "Contract ID",
                "short_desc": "The associated contract ID",
                "desc": "The ID of the contract this invoice belongs to",
            },
            "amount": {
                "type": NumberOrNothingType,
                "display_name": "Amount",
                "short_desc": "The invoice amount",
                "desc": "The total monetary amount of the invoice",
                "example_value": 5000.00,
            },
            "currency": {
                "type": StringOrNothingType,
                "display_name": "Currency",
                "short_desc": "The invoice currency",
                "desc": "The ISO 4217 currency code for the invoice",
                "example_value": "USD",
            },
            "status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "The invoice status",
                "desc": "The current status of the invoice "
                    "(e.g., `pending`, `paid`, `cancelled`)",
                "example_value": "pending",
            },
            "due_date": {
                "type": StringOrNothingType,
                "display_name": "Due Date",
                "short_desc": "The payment due date",
                "desc": "The date by which the invoice should be paid (YYYY-MM-DD format)",
            },
            "invoice_number": {
                "type": StringOrNothingType,
                "display_name": "Invoice Number",
                "short_desc": "The invoice number",
                "desc": "A human-readable invoice reference number",
                "example_value": "INV-2026-001",
            },
            "period_start": {
                "type": StringOrNothingType,
                "display_name": "Period Start",
                "short_desc": "The billing period start",
                "desc": "The start date of the billing period for this invoice (YYYY-MM-DD format)",
            },
            "period_end": {
                "type": StringOrNothingType,
                "display_name": "Period End",
                "short_desc": "The billing period end",
                "desc": "The end date of the billing period for this invoice (YYYY-MM-DD format)",
            },
            "created_at": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the invoice was created",
                "desc": "The timestamp when the invoice was created (ISO 8601 format)",
            },
            "updated_at": {
                "type": StringOrNothingType,
                "display_name": "Updated At",
                "short_desc": "When the invoice was last updated",
                "desc": "The timestamp when the invoice was last updated (ISO 8601 format)",
            },
            "worker": {
                "type": AutoHashOrNothingType,
                "display_name": "Worker",
                "short_desc": "The worker details",
                "desc": "Information about the worker associated with this invoice",
            },
            "line_items": {
                "type": AutoListOrNothingType,
                "display_name": "Line Items",
                "short_desc": "The invoice line items",
                "desc": "Individual line items that make up the invoice total",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace DeelDataProvider
