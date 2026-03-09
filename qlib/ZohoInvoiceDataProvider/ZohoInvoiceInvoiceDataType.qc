# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ZohoInvoiceDataProvider module definition

/*  ZohoInvoiceInvoiceDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Zoho Invoice invoice data type
public class ZohoInvoiceInvoiceDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "invoice_id": {
                "display_name": "Invoice ID",
                "short_desc": "Unique identifier for the invoice",
                "desc": "The unique identifier for this invoice in Zoho Invoice",
                "type": SoftStringType,
            },
            "invoice_number": {
                "display_name": "Invoice Number",
                "short_desc": "Invoice number",
                "desc": "The invoice number visible to customers",
                "type": SoftStringOrNothingType,
            },
            "customer_id": {
                "display_name": "Customer ID",
                "short_desc": "Customer identifier",
                "desc": "The ID of the customer for this invoice",
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
                "short_desc": "Invoice status",
                "desc": "Status of the invoice",
                "type": SoftStringOrNothingType,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "draft", "desc": "Draft invoice"},
                    <AllowedValueInfo>{"value": "sent", "desc": "Sent invoice"},
                    <AllowedValueInfo>{"value": "overdue", "desc": "Overdue invoice"},
                    <AllowedValueInfo>{"value": "paid", "desc": "Paid invoice"},
                    <AllowedValueInfo>{"value": "void", "desc": "Voided invoice"},
                    <AllowedValueInfo>{"value": "unpaid", "desc": "Unpaid invoice"},
                    <AllowedValueInfo>{"value": "partially_paid", "desc": "Partially paid invoice"},
                ),
            },
            "date": {
                "display_name": "Invoice Date",
                "short_desc": "Date of the invoice",
                "desc": "The date of the invoice (`YYYY-MM-DD` format)",
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
                "desc": "The currency ID for the invoice",
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
                "desc": "The subtotal amount before taxes and discounts",
                "type": SoftFloatOrNothingType,
            },
            "tax_total": {
                "display_name": "Tax Total",
                "short_desc": "Total tax amount",
                "desc": "The total tax amount on the invoice",
                "type": SoftFloatOrNothingType,
            },
            "discount": {
                "display_name": "Discount",
                "short_desc": "Discount amount",
                "desc": "The discount amount or percentage",
                "type": SoftFloatOrNothingType,
            },
            "discount_type": {
                "display_name": "Discount Type",
                "short_desc": "Type of discount",
                "desc": "Whether discount is a percentage or fixed amount",
                "type": SoftStringOrNothingType,
            },
            "total": {
                "display_name": "Total",
                "short_desc": "Invoice total",
                "desc": "The total amount of the invoice",
                "type": SoftFloatOrNothingType,
            },
            "balance": {
                "display_name": "Balance",
                "short_desc": "Outstanding balance",
                "desc": "The outstanding balance on the invoice",
                "type": SoftFloatOrNothingType,
            },
            "payment_terms": {
                "display_name": "Payment Terms",
                "short_desc": "Payment terms",
                "desc": "The payment terms (e.g., Net 30)",
                "type": SoftIntOrNothingType,
            },
            "payment_terms_label": {
                "display_name": "Payment Terms Label",
                "short_desc": "Payment terms description",
                "desc": "The label for payment terms",
                "type": SoftStringOrNothingType,
            },
            "reference_number": {
                "display_name": "Reference Number",
                "short_desc": "Reference number",
                "desc": "An optional reference number (e.g., PO number)",
                "type": SoftStringOrNothingType,
            },
            "line_items": {
                "display_name": "Line Items",
                "short_desc": "Invoice line items",
                "desc": "The line items on the invoice",
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
                "desc": "Terms and conditions to display on the invoice",
                "type": SoftStringOrNothingType,
            },
            "shipping_charge": {
                "display_name": "Shipping Charge",
                "short_desc": "Shipping charges",
                "desc": "Shipping charges on the invoice",
                "type": SoftFloatOrNothingType,
            },
            "adjustment": {
                "display_name": "Adjustment",
                "short_desc": "Adjustment amount",
                "desc": "Any adjustment to the invoice total",
                "type": SoftFloatOrNothingType,
            },
            "adjustment_description": {
                "display_name": "Adjustment Description",
                "short_desc": "Description of adjustment",
                "desc": "Description for the adjustment",
                "type": SoftStringOrNothingType,
            },
            "salesperson_name": {
                "display_name": "Salesperson",
                "short_desc": "Salesperson name",
                "desc": "The name of the salesperson",
                "type": SoftStringOrNothingType,
            },
            "project_id": {
                "display_name": "Project ID",
                "short_desc": "Associated project",
                "desc": "The project ID this invoice is associated with",
                "type": SoftStringOrNothingType,
            },
            "created_time": {
                "display_name": "Created Time",
                "short_desc": "When the invoice was created",
                "desc": "The timestamp when the invoice was created",
                "type": SoftStringOrNothingType,
            },
            "last_modified_time": {
                "display_name": "Last Modified Time",
                "short_desc": "When the invoice was last modified",
                "desc": "The timestamp when the invoice was last modified",
                "type": SoftStringOrNothingType,
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Zoho Invoice create invoice request data type
public class ZohoInvoiceCreateInvoiceRequestDataType inherits HashDataType {
    public {
        #! Field definitions for creating an invoice
        const Fields = {
            "customer_id": {
                "display_name": "Customer ID",
                "short_desc": "Customer identifier",
                "desc": "The ID of the customer for this invoice",
                "type": SoftStringType,
                "required": True,
            },
            "date": {
                "display_name": "Invoice Date",
                "short_desc": "Date of the invoice",
                "desc": "The date of the invoice (`YYYY-MM-DD` format)",
                "type": SoftStringOrNothingType,
            },
            "due_date": {
                "display_name": "Due Date",
                "short_desc": "Payment due date",
                "desc": "The date payment is due (`YYYY-MM-DD` format)",
                "type": SoftStringOrNothingType,
            },
            "invoice_number": {
                "display_name": "Invoice Number",
                "short_desc": "Custom invoice number",
                "desc": "Custom invoice number (auto-generated if not provided)",
                "type": SoftStringOrNothingType,
            },
            "reference_number": {
                "display_name": "Reference Number",
                "short_desc": "Reference number",
                "desc": "An optional reference number (e.g., PO number)",
                "type": SoftStringOrNothingType,
            },
            "line_items": {
                "display_name": "Line Items",
                "short_desc": "Invoice line items",
                "desc": "The line items on the invoice",
                "type": new SoftListDataType("LineItems", new ZohoInvoiceLineItemDataType(), True),
                "required": True,
            },
            "discount": {
                "display_name": "Discount",
                "short_desc": "Discount amount",
                "desc": "The discount amount or percentage",
                "type": SoftFloatOrNothingType,
            },
            "discount_type": {
                "display_name": "Discount Type",
                "short_desc": "Type of discount",
                "desc": "Whether discount is 'entity_level' or 'item_level'",
                "type": SoftStringOrNothingType,
            },
            "is_discount_before_tax": {
                "display_name": "Discount Before Tax",
                "short_desc": "Apply discount before tax",
                "desc": "Whether to apply discount before calculating tax",
                "type": SoftBoolOrNothingType,
            },
            "payment_terms": {
                "display_name": "Payment Terms",
                "short_desc": "Payment terms in days",
                "desc": "The payment terms in days (e.g., 30 for Net 30)",
                "type": SoftIntOrNothingType,
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
                "desc": "Terms and conditions to display on the invoice",
                "type": SoftStringOrNothingType,
            },
            "shipping_charge": {
                "display_name": "Shipping Charge",
                "short_desc": "Shipping charges",
                "desc": "Shipping charges on the invoice",
                "type": SoftFloatOrNothingType,
            },
            "adjustment": {
                "display_name": "Adjustment",
                "short_desc": "Adjustment amount",
                "desc": "Any adjustment to the invoice total",
                "type": SoftFloatOrNothingType,
            },
            "adjustment_description": {
                "display_name": "Adjustment Description",
                "short_desc": "Description of adjustment",
                "desc": "Description for the adjustment",
                "type": SoftStringOrNothingType,
            },
            "salesperson_name": {
                "display_name": "Salesperson",
                "short_desc": "Salesperson name",
                "desc": "The name of the salesperson",
                "type": SoftStringOrNothingType,
            },
            "project_id": {
                "display_name": "Project ID",
                "short_desc": "Associated project",
                "desc": "The project ID this invoice is associated with",
                "type": SoftStringOrNothingType,
            },
            "custom_fields": {
                "display_name": "Custom Fields",
                "short_desc": "Custom field values",
                "desc": "Custom field values for the invoice",
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
