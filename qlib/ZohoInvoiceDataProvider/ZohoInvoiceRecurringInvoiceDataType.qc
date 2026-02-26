# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ZohoInvoiceDataProvider module definition

/*  ZohoInvoiceRecurringInvoiceDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Zoho Invoice recurring invoice data type
public class ZohoInvoiceRecurringInvoiceDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "recurring_invoice_id": {
                "display_name": "Recurring Invoice ID",
                "short_desc": "Unique identifier",
                "desc": "The unique identifier for this recurring invoice",
                "type": SoftStringType,
            },
            "recurrence_name": {
                "display_name": "Recurrence Name",
                "short_desc": "Name of the recurring invoice",
                "desc": "The name of this recurring invoice profile",
                "type": SoftStringOrNothingType,
            },
            "customer_id": {
                "display_name": "Customer ID",
                "short_desc": "Customer identifier",
                "desc": "The ID of the customer",
                "type": SoftStringType,
            },
            "customer_name": {
                "display_name": "Customer Name",
                "short_desc": "Customer name",
                "desc": "The name of the customer",
                "type": SoftStringOrNothingType,
            },
            "status": {
                "display_name": "Status",
                "short_desc": "Recurring invoice status",
                "desc": "Status of the recurring invoice",
                "type": SoftStringOrNothingType,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "active", "desc": "Active recurring invoice"},
                    <AllowedValueInfo>{"value": "stopped", "desc": "Stopped recurring invoice"},
                    <AllowedValueInfo>{"value": "expired", "desc": "Expired recurring invoice"},
                ),
            },
            "recurrence_frequency": {
                "display_name": "Frequency",
                "short_desc": "Recurrence frequency",
                "desc": "The frequency of recurrence (days, weeks, months, years)",
                "type": SoftStringOrNothingType,
            },
            "repeat_every": {
                "display_name": "Repeat Every",
                "short_desc": "Repeat interval",
                "desc": "The interval between recurrences (e.g., 1 for every month)",
                "type": SoftIntOrNothingType,
            },
            "start_date": {
                "display_name": "Start Date",
                "short_desc": "Start date",
                "desc": "The start date for the recurring invoice",
                "type": SoftStringOrNothingType,
            },
            "end_date": {
                "display_name": "End Date",
                "short_desc": "End date",
                "desc": "The end date for the recurring invoice (if applicable)",
                "type": SoftStringOrNothingType,
            },
            "last_sent_date": {
                "display_name": "Last Sent Date",
                "short_desc": "Date last sent",
                "desc": "The date the last invoice was sent",
                "type": SoftStringOrNothingType,
            },
            "next_invoice_date": {
                "display_name": "Next Invoice Date",
                "short_desc": "Next invoice date",
                "desc": "The date the next invoice will be generated",
                "type": SoftStringOrNothingType,
            },
            "payment_terms": {
                "display_name": "Payment Terms",
                "short_desc": "Payment terms in days",
                "desc": "The payment terms in days",
                "type": SoftIntOrNothingType,
            },
            "currency_code": {
                "display_name": "Currency Code",
                "short_desc": "Currency code",
                "desc": "The currency code (e.g., USD, EUR)",
                "type": SoftStringOrNothingType,
            },
            "sub_total": {
                "display_name": "Subtotal",
                "short_desc": "Subtotal before tax",
                "desc": "The subtotal amount",
                "type": SoftFloatOrNothingType,
            },
            "tax_total": {
                "display_name": "Tax Total",
                "short_desc": "Total tax amount",
                "desc": "The total tax amount",
                "type": SoftFloatOrNothingType,
            },
            "total": {
                "display_name": "Total",
                "short_desc": "Invoice total",
                "desc": "The total amount",
                "type": SoftFloatOrNothingType,
            },
            "line_items": {
                "display_name": "Line Items",
                "short_desc": "Invoice line items",
                "desc": "The line items",
                "type": new SoftListDataType("LineItems", new ZohoInvoiceLineItemDataType(), True),
            },
            "notes": {
                "display_name": "Notes",
                "short_desc": "Invoice notes",
                "desc": "Notes to display on the invoice",
                "type": SoftStringOrNothingType,
            },
            "terms": {
                "display_name": "Terms & Conditions",
                "short_desc": "Terms and conditions",
                "desc": "Terms and conditions",
                "type": SoftStringOrNothingType,
            },
            "created_time": {
                "display_name": "Created Time",
                "short_desc": "When created",
                "desc": "The timestamp when created",
                "type": SoftStringOrNothingType,
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Zoho Invoice create recurring invoice request data type
public class ZohoInvoiceCreateRecurringInvoiceRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "customer_id": {
                "display_name": "Customer ID",
                "short_desc": "Customer identifier",
                "desc": "The ID of the customer",
                "type": SoftStringType,
                "required": True,
            },
            "recurrence_name": {
                "display_name": "Recurrence Name",
                "short_desc": "Name of the recurring invoice",
                "desc": "The name of this recurring invoice profile",
                "type": SoftStringOrNothingType,
            },
            "recurrence_frequency": {
                "display_name": "Frequency",
                "short_desc": "Recurrence frequency",
                "desc": "The frequency (days, weeks, months, years)",
                "type": SoftStringType,
                "required": True,
            },
            "repeat_every": {
                "display_name": "Repeat Every",
                "short_desc": "Repeat interval",
                "desc": "The interval between recurrences",
                "type": SoftIntOrNothingType,
            },
            "start_date": {
                "display_name": "Start Date",
                "short_desc": "Start date",
                "desc": "The start date (`YYYY-MM-DD`)",
                "type": SoftStringType,
                "required": True,
            },
            "end_date": {
                "display_name": "End Date",
                "short_desc": "End date",
                "desc": "The end date (`YYYY-MM-DD`)",
                "type": SoftStringOrNothingType,
            },
            "payment_terms": {
                "display_name": "Payment Terms",
                "short_desc": "Payment terms in days",
                "desc": "The payment terms in days",
                "type": SoftIntOrNothingType,
            },
            "line_items": {
                "display_name": "Line Items",
                "short_desc": "Invoice line items",
                "desc": "The line items",
                "type": new SoftListDataType("LineItems", new ZohoInvoiceLineItemDataType(), True),
                "required": True,
            },
            "notes": {
                "display_name": "Notes",
                "short_desc": "Invoice notes",
                "desc": "Notes to display",
                "type": SoftStringOrNothingType,
            },
            "terms": {
                "display_name": "Terms & Conditions",
                "short_desc": "Terms and conditions",
                "desc": "Terms and conditions",
                "type": SoftStringOrNothingType,
            },
            "custom_fields": {
                "display_name": "Custom Fields",
                "short_desc": "Custom field values",
                "desc": "Custom field values",
                "type": AutoHashOrNothingType,
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace ZohoInvoiceDataProvider
