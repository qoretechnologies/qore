# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ZohoInvoiceDataProvider module definition

/*  ZohoInvoiceExpenseDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Zoho Invoice expense data type
public class ZohoInvoiceExpenseDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "expense_id": {
                "display_name": "Expense ID",
                "short_desc": "Unique identifier for the expense",
                "desc": "The unique identifier for this expense",
                "type": SoftStringType,
            },
            "date": {
                "display_name": "Expense Date",
                "short_desc": "Date of the expense",
                "desc": "The date of the expense (`YYYY-MM-DD` format)",
                "type": SoftStringOrNothingType,
            },
            "account_id": {
                "display_name": "Account ID",
                "short_desc": "Expense account",
                "desc": "The account ID for this expense",
                "type": SoftStringOrNothingType,
            },
            "account_name": {
                "display_name": "Account Name",
                "short_desc": "Account name",
                "desc": "The name of the expense account",
                "type": SoftStringOrNothingType,
            },
            "customer_id": {
                "display_name": "Customer ID",
                "short_desc": "Billable customer",
                "desc": "The customer ID if this is a billable expense",
                "type": SoftStringOrNothingType,
            },
            "customer_name": {
                "display_name": "Customer Name",
                "short_desc": "Customer name",
                "desc": "The name of the customer",
                "type": SoftStringOrNothingType,
            },
            "amount": {
                "display_name": "Amount",
                "short_desc": "Expense amount",
                "desc": "The amount of the expense",
                "type": SoftFloatType,
            },
            "currency_id": {
                "display_name": "Currency ID",
                "short_desc": "Currency identifier",
                "desc": "The currency ID for the expense",
                "type": SoftStringOrNothingType,
            },
            "currency_code": {
                "display_name": "Currency Code",
                "short_desc": "Currency code",
                "desc": "The currency code (e.g., USD, EUR)",
                "type": SoftStringOrNothingType,
            },
            "is_billable": {
                "display_name": "Is Billable",
                "short_desc": "Billable expense",
                "desc": "Whether this expense is billable to a customer",
                "type": SoftBoolOrNothingType,
            },
            "is_invoiced": {
                "display_name": "Is Invoiced",
                "short_desc": "Already invoiced",
                "desc": "Whether this expense has been invoiced",
                "type": SoftBoolOrNothingType,
            },
            "reference_number": {
                "display_name": "Reference Number",
                "short_desc": "Reference number",
                "desc": "An optional reference number",
                "type": SoftStringOrNothingType,
            },
            "description": {
                "display_name": "Description",
                "short_desc": "Expense description",
                "desc": "Description of the expense",
                "type": SoftStringOrNothingType,
            },
            "project_id": {
                "display_name": "Project ID",
                "short_desc": "Associated project",
                "desc": "The project ID this expense is associated with",
                "type": SoftStringOrNothingType,
            },
            "project_name": {
                "display_name": "Project Name",
                "short_desc": "Project name",
                "desc": "The name of the associated project",
                "type": SoftStringOrNothingType,
            },
            "tax_id": {
                "display_name": "Tax ID",
                "short_desc": "Tax rate ID",
                "desc": "The ID of the tax rate applied",
                "type": SoftStringOrNothingType,
            },
            "tax_name": {
                "display_name": "Tax Name",
                "short_desc": "Tax name",
                "desc": "The name of the tax applied",
                "type": SoftStringOrNothingType,
            },
            "tax_percentage": {
                "display_name": "Tax Percentage",
                "short_desc": "Tax rate percentage",
                "desc": "The tax percentage applied",
                "type": SoftFloatOrNothingType,
            },
            "tax_amount": {
                "display_name": "Tax Amount",
                "short_desc": "Tax amount",
                "desc": "The tax amount",
                "type": SoftFloatOrNothingType,
            },
            "total": {
                "display_name": "Total",
                "short_desc": "Total amount",
                "desc": "The total amount including tax",
                "type": SoftFloatOrNothingType,
            },
            "status": {
                "display_name": "Status",
                "short_desc": "Expense status",
                "desc": "Status of the expense",
                "type": SoftStringOrNothingType,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "unbilled", "desc": "Unbilled expense"},
                    <AllowedValueInfo>{"value": "invoiced", "desc": "Invoiced expense"},
                    <AllowedValueInfo>{"value": "reimbursed", "desc": "Reimbursed expense"},
                ),
            },
            "created_time": {
                "display_name": "Created Time",
                "short_desc": "When the expense was created",
                "desc": "The timestamp when the expense was created",
                "type": SoftStringOrNothingType,
            },
            "last_modified_time": {
                "display_name": "Last Modified Time",
                "short_desc": "When the expense was last modified",
                "desc": "The timestamp when the expense was last modified",
                "type": SoftStringOrNothingType,
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Zoho Invoice create expense request data type
public class ZohoInvoiceCreateExpenseRequestDataType inherits HashDataType {
    public {
        #! Field definitions for creating an expense
        const Fields = {
            "account_id": {
                "display_name": "Account ID",
                "short_desc": "Expense account",
                "desc": "The account ID for this expense",
                "type": SoftStringType,
                "required": True,
            },
            "date": {
                "display_name": "Expense Date",
                "short_desc": "Date of the expense",
                "desc": "The date of the expense (`YYYY-MM-DD` format)",
                "type": SoftStringOrNothingType,
            },
            "amount": {
                "display_name": "Amount",
                "short_desc": "Expense amount",
                "desc": "The amount of the expense",
                "type": SoftFloatType,
                "required": True,
            },
            "customer_id": {
                "display_name": "Customer ID",
                "short_desc": "Billable customer",
                "desc": "The customer ID if this is a billable expense",
                "type": SoftStringOrNothingType,
            },
            "is_billable": {
                "display_name": "Is Billable",
                "short_desc": "Billable expense",
                "desc": "Whether this expense is billable to a customer",
                "type": SoftBoolOrNothingType,
            },
            "reference_number": {
                "display_name": "Reference Number",
                "short_desc": "Reference number",
                "desc": "An optional reference number",
                "type": SoftStringOrNothingType,
            },
            "description": {
                "display_name": "Description",
                "short_desc": "Expense description",
                "desc": "Description of the expense",
                "type": SoftStringOrNothingType,
            },
            "project_id": {
                "display_name": "Project ID",
                "short_desc": "Associated project",
                "desc": "The project ID this expense is associated with",
                "type": SoftStringOrNothingType,
            },
            "tax_id": {
                "display_name": "Tax ID",
                "short_desc": "Tax rate ID",
                "desc": "The ID of the tax rate to apply",
                "type": SoftStringOrNothingType,
            },
            "currency_id": {
                "display_name": "Currency ID",
                "short_desc": "Currency identifier",
                "desc": "The currency ID for the expense",
                "type": SoftStringOrNothingType,
            },
            "custom_fields": {
                "display_name": "Custom Fields",
                "short_desc": "Custom field values",
                "desc": "Custom field values for the expense",
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
