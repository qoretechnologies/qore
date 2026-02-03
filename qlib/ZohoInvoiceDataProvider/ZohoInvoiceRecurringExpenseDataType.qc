# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ZohoInvoiceDataProvider module definition

/*  ZohoInvoiceRecurringExpenseDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Zoho Invoice recurring expense data type
public class ZohoInvoiceRecurringExpenseDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "recurring_expense_id": {
                "display_name": "Recurring Expense ID",
                "short_desc": "Unique identifier",
                "desc": "The unique identifier for this recurring expense",
                "type": SoftStringType,
            },
            "recurrence_name": {
                "display_name": "Recurrence Name",
                "short_desc": "Name of the recurring expense",
                "desc": "The name of this recurring expense profile",
                "type": SoftStringOrNothingType,
            },
            "account_id": {
                "display_name": "Account ID",
                "short_desc": "Expense account",
                "desc": "The expense account ID",
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
                "desc": "The customer ID if billable",
                "type": SoftStringOrNothingType,
            },
            "customer_name": {
                "display_name": "Customer Name",
                "short_desc": "Customer name",
                "desc": "The name of the customer",
                "type": SoftStringOrNothingType,
            },
            "status": {
                "display_name": "Status",
                "short_desc": "Recurring expense status",
                "desc": "Status (active, stopped, expired)",
                "type": SoftStringOrNothingType,
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
                "desc": "The interval between recurrences",
                "type": SoftIntOrNothingType,
            },
            "start_date": {
                "display_name": "Start Date",
                "short_desc": "Start date",
                "desc": "The start date",
                "type": SoftStringOrNothingType,
            },
            "end_date": {
                "display_name": "End Date",
                "short_desc": "End date",
                "desc": "The end date",
                "type": SoftStringOrNothingType,
            },
            "last_created_date": {
                "display_name": "Last Created Date",
                "short_desc": "Date last created",
                "desc": "The date the last expense was created",
                "type": SoftStringOrNothingType,
            },
            "next_expense_date": {
                "display_name": "Next Expense Date",
                "short_desc": "Next expense date",
                "desc": "The date the next expense will be created",
                "type": SoftStringOrNothingType,
            },
            "amount": {
                "display_name": "Amount",
                "short_desc": "Expense amount",
                "desc": "The amount of the expense",
                "type": SoftFloatOrNothingType,
            },
            "currency_code": {
                "display_name": "Currency Code",
                "short_desc": "Currency code",
                "desc": "The currency code",
                "type": SoftStringOrNothingType,
            },
            "is_billable": {
                "display_name": "Is Billable",
                "short_desc": "Billable expense",
                "desc": "Whether this expense is billable",
                "type": SoftBoolOrNothingType,
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
                "desc": "The project ID",
                "type": SoftStringOrNothingType,
            },
            "tax_id": {
                "display_name": "Tax ID",
                "short_desc": "Tax rate ID",
                "desc": "The tax rate ID",
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
        map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type, $1.value.default_value)),
            Fields.pairIterator();
    }
}

#! Zoho Invoice create recurring expense request data type
public class ZohoInvoiceCreateRecurringExpenseRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "account_id": {
                "display_name": "Account ID",
                "short_desc": "Expense account",
                "desc": "The expense account ID",
                "type": SoftStringType,
                "required": True,
            },
            "recurrence_name": {
                "display_name": "Recurrence Name",
                "short_desc": "Name of the recurring expense",
                "desc": "The name of this recurring expense profile",
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
                "desc": "The start date (YYYY-MM-DD)",
                "type": SoftStringType,
                "required": True,
            },
            "end_date": {
                "display_name": "End Date",
                "short_desc": "End date",
                "desc": "The end date (YYYY-MM-DD)",
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
                "desc": "The customer ID if billable",
                "type": SoftStringOrNothingType,
            },
            "is_billable": {
                "display_name": "Is Billable",
                "short_desc": "Billable expense",
                "desc": "Whether this expense is billable",
                "type": SoftBoolOrNothingType,
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
                "desc": "The project ID",
                "type": SoftStringOrNothingType,
            },
            "tax_id": {
                "display_name": "Tax ID",
                "short_desc": "Tax rate ID",
                "desc": "The tax rate ID",
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
        map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type, $1.value.default_value)),
            Fields.pairIterator();
    }
}

} # namespace ZohoInvoiceDataProvider
