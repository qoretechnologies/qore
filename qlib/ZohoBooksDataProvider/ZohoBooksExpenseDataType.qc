# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ZohoBooksDataProvider module definition

/*  ZohoBooksExpenseDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Zoho Books expense data type
public class ZohoBooksExpenseDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "expense_id": {
                "display_name": "Expense ID",
                "short_desc": "Unique identifier for the expense",
                "desc": "The unique identifier for this expense in Zoho Books",
                "type": SoftStringType,
            },
            "account_id": {
                "display_name": "Account ID",
                "short_desc": "Account identifier",
                "desc": "The ID of the expense account",
                "type": SoftStringOrNothingType,
            },
            "account_name": {
                "display_name": "Account Name",
                "short_desc": "Account name",
                "desc": "The name of the expense account",
                "type": SoftStringOrNothingType,
            },
            "date": {
                "display_name": "Expense Date",
                "short_desc": "Date of the expense",
                "desc": "The date of the expense (YYYY-MM-DD format)",
                "type": SoftStringOrNothingType,
            },
            "amount": {
                "display_name": "Amount",
                "short_desc": "Expense amount",
                "desc": "The amount of the expense",
                "type": SoftFloatOrNothingType,
            },
            "tax_amount": {
                "display_name": "Tax Amount",
                "short_desc": "Tax amount",
                "desc": "The tax amount on the expense",
                "type": SoftFloatOrNothingType,
            },
            "total": {
                "display_name": "Total",
                "short_desc": "Total amount",
                "desc": "The total expense amount including tax",
                "type": SoftFloatOrNothingType,
            },
            "vendor_id": {
                "display_name": "Vendor ID",
                "short_desc": "Vendor identifier",
                "desc": "The ID of the vendor for this expense",
                "type": SoftStringOrNothingType,
            },
            "vendor_name": {
                "display_name": "Vendor Name",
                "short_desc": "Vendor name",
                "desc": "The name of the vendor",
                "type": SoftStringOrNothingType,
            },
            "description": {
                "display_name": "Description",
                "short_desc": "Expense description",
                "desc": "Description of the expense",
                "type": SoftStringOrNothingType,
            },
            "reference_number": {
                "display_name": "Reference Number",
                "short_desc": "Reference number",
                "desc": "An optional reference number",
                "type": SoftStringOrNothingType,
            },
            "is_billable": {
                "display_name": "Is Billable",
                "short_desc": "Billable flag",
                "desc": "Whether the expense is billable to a customer",
                "type": SoftBoolOrNothingType,
            },
            "customer_id": {
                "display_name": "Customer ID",
                "short_desc": "Customer identifier",
                "desc": "The ID of the customer if billable",
                "type": SoftStringOrNothingType,
            },
            "customer_name": {
                "display_name": "Customer Name",
                "short_desc": "Customer name",
                "desc": "The name of the customer if billable",
                "type": SoftStringOrNothingType,
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
            "status": {
                "display_name": "Status",
                "short_desc": "Expense status",
                "desc": "Status of the expense",
                "type": SoftStringOrNothingType,
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
        map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type, $1.value.default_value)),
            Fields.pairIterator();
    }
}

} # namespace ZohoBooksDataProvider
