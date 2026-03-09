# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ZohoBooksDataProvider module definition

/*  ZohoBooksBillDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Zoho Books bill data type
public class ZohoBooksBillDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "bill_id": {
                "display_name": "Bill ID",
                "short_desc": "Unique identifier for the bill",
                "desc": "The unique identifier for this bill in Zoho Books",
                "type": SoftStringType,
            },
            "bill_number": {
                "display_name": "Bill Number",
                "short_desc": "Bill number",
                "desc": "The bill number visible in the system",
                "type": SoftStringOrNothingType,
            },
            "vendor_id": {
                "display_name": "Vendor ID",
                "short_desc": "Vendor identifier",
                "desc": "The ID of the vendor for this bill",
                "type": SoftStringType,
            },
            "vendor_name": {
                "display_name": "Vendor Name",
                "short_desc": "Vendor name",
                "desc": "The name of the vendor",
                "type": SoftStringOrNothingType,
            },
            "status": {
                "display_name": "Status",
                "short_desc": "Bill status",
                "desc": "Status of the bill",
                "type": SoftStringOrNothingType,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "draft", "desc": "Draft bill"},
                    <AllowedValueInfo>{"value": "open", "desc": "Open bill"},
                    <AllowedValueInfo>{"value": "overdue", "desc": "Overdue bill"},
                    <AllowedValueInfo>{"value": "paid", "desc": "Fully paid"},
                    <AllowedValueInfo>{"value": "void", "desc": "Voided bill"},
                    <AllowedValueInfo>{"value": "partially_paid", "desc": "Partially paid"},
                ),
            },
            "date": {
                "display_name": "Bill Date",
                "short_desc": "Date of the bill",
                "desc": "The date of the bill (`YYYY-MM-DD` format)",
                "type": SoftStringOrNothingType,
            },
            "due_date": {
                "display_name": "Due Date",
                "short_desc": "Payment due date",
                "desc": "The date payment is due",
                "type": SoftStringOrNothingType,
            },
            "currency_id": {
                "display_name": "Currency ID",
                "short_desc": "Currency identifier",
                "desc": "The currency ID for the bill",
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
            "sub_total": {
                "display_name": "Subtotal",
                "short_desc": "Subtotal before tax",
                "desc": "The subtotal amount before taxes",
                "type": SoftFloatOrNothingType,
            },
            "tax_total": {
                "display_name": "Tax Total",
                "short_desc": "Total tax amount",
                "desc": "The total tax amount on the bill",
                "type": SoftFloatOrNothingType,
            },
            "total": {
                "display_name": "Total",
                "short_desc": "Bill total",
                "desc": "The total amount of the bill",
                "type": SoftFloatOrNothingType,
            },
            "balance": {
                "display_name": "Balance",
                "short_desc": "Outstanding balance",
                "desc": "The outstanding balance on the bill",
                "type": SoftFloatOrNothingType,
            },
            "reference_number": {
                "display_name": "Reference Number",
                "short_desc": "Reference number",
                "desc": "An optional reference number",
                "type": SoftStringOrNothingType,
            },
            "line_items": {
                "display_name": "Line Items",
                "short_desc": "Bill line items",
                "desc": "The line items on the bill",
                "type": new SoftListDataType("LineItems", new ZohoBooksLineItemDataType(), True),
            },
            "notes": {
                "display_name": "Notes",
                "short_desc": "Bill notes",
                "desc": "Notes for the bill",
                "type": SoftStringOrNothingType,
            },
            "terms": {
                "display_name": "Terms & Conditions",
                "short_desc": "Terms and conditions",
                "desc": "Terms and conditions for the bill",
                "type": SoftStringOrNothingType,
            },
            "created_time": {
                "display_name": "Created Time",
                "short_desc": "When the bill was created",
                "desc": "The timestamp when the bill was created",
                "type": SoftStringOrNothingType,
            },
            "last_modified_time": {
                "display_name": "Last Modified Time",
                "short_desc": "When the bill was last modified",
                "desc": "The timestamp when the bill was last modified",
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
