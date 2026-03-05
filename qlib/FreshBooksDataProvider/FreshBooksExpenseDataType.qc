# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore FreshBooksDataProvider module definition

/*  FreshBooksExpenseDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the FreshBooksDataProvider module
public namespace FreshBooksDataProvider {

#! FreshBooks expense record type
public class FreshBooksExpenseDataType inherits HashDataType {
    public {
        #! Field definitions for FreshBooks expenses
        const Fields = {
            "id": {
                "type": IntType,
                "display_name": "ID",
                "short_desc": "The expense ID",
                "desc": "The unique identifier of the expense",
            },
            "expenseid": {
                "type": IntType,
                "display_name": "Expense ID",
                "short_desc": "The internal expense ID",
                "desc": "The internal expense ID",
            },
            "categoryid": {
                "type": IntOrNothingType,
                "display_name": "Category ID",
                "short_desc": "The expense category ID",
                "desc": "The ID of the expense category",
            },
            "amount": {
                "type": AutoHashType,
                "display_name": "Amount",
                "short_desc": "The expense amount",
                "desc": "The amount of the expense (hash with `amount` and `code` keys)",
            },
            "vendor": {
                "type": StringOrNothingType,
                "display_name": "Vendor",
                "short_desc": "The vendor name",
                "desc": "The name of the vendor for the expense",
            },
            "date": {
                "type": StringOrNothingType,
                "display_name": "Date",
                "short_desc": "The expense date",
                "desc": "The date of the expense (YYYY-MM-DD format)",
            },
            "notes": {
                "type": StringOrNothingType,
                "display_name": "Notes",
                "short_desc": "The expense notes",
                "desc": "Notes about the expense",
            },
            "status": {
                "type": IntOrNothingType,
                "display_name": "Status",
                "short_desc": "The expense status",
                "desc": "The status of the expense",
                "allowed_values": (
                    <AllowedValueInfo>{"value": 0, "desc": "Internal"},
                    <AllowedValueInfo>{"value": 1, "desc": "Outstanding"},
                    <AllowedValueInfo>{"value": 2, "desc": "Invoiced"},
                    <AllowedValueInfo>{"value": 4, "desc": "Recouped"},
                ),
            },
            "vis_state": {
                "type": IntOrNothingType,
                "display_name": "Visibility State",
                "short_desc": "The visibility state",
                "allowed_values": (
                    <AllowedValueInfo>{"value": 0, "desc": "Active"},
                    <AllowedValueInfo>{"value": 1, "desc": "Deleted"},
                    <AllowedValueInfo>{"value": 2, "desc": "Archived"},
                ),
                "desc": "The visibility state of the expense",
            },
            "taxName1": {
                "type": StringOrNothingType,
                "display_name": "Tax Name",
                "short_desc": "The tax name",
                "desc": "The name of the tax applied to the expense",
            },
            "taxPercent1": {
                "type": StringOrNothingType,
                "display_name": "Tax Percent",
                "short_desc": "The tax percentage",
                "desc": "The percentage of the tax applied to the expense",
            },
            "staffid": {
                "type": IntOrNothingType,
                "display_name": "Staff ID",
                "short_desc": "The related staff member ID",
                "desc": "The ID of the staff member associated with the expense",
            },
            "taxAmount1": {
                "type": StringOrNothingType,
                "display_name": "Tax Amount",
                "short_desc": "The tax amount",
                "desc": "The amount of the tax applied to the expense",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! FreshBooks expense create request type
public class FreshBooksExpenseCreateDataType inherits HashDataType {
    public {
        #! Field definitions for creating expenses
        const Fields = FreshBooksDataProviderBase::AccountIdField + {
            "staffid": {
                "type": IntType,
                "display_name": "Staff ID",
                "short_desc": "The staff member for this expense (required)",
                "desc": "The ID of the staff member associated with the expense",
                "required": True,
            },
            "categoryid": {
                "type": IntOrNothingType,
                "display_name": "Category ID",
                "short_desc": "The expense category ID",
                "desc": "The ID of the expense category",
            },
            "amount": {
                "type": AutoHashType,
                "display_name": "Amount",
                "short_desc": "The expense amount (required)",
                "desc": "The amount of the expense (hash with `amount` and `code` keys)",
                "required": True,
            },
            "vendor": {
                "type": StringOrNothingType,
                "display_name": "Vendor",
                "short_desc": "The vendor name",
                "desc": "The name of the vendor for the expense",
            },
            "date": {
                "type": StringOrNothingType,
                "display_name": "Date",
                "short_desc": "The expense date",
                "desc": "The date of the expense (YYYY-MM-DD format)",
            },
            "notes": {
                "type": StringOrNothingType,
                "display_name": "Notes",
                "short_desc": "The expense notes",
                "desc": "Notes about the expense",
            },
            "taxName1": {
                "type": StringOrNothingType,
                "display_name": "Tax Name",
                "short_desc": "The tax name",
                "desc": "The name of the tax to apply to the expense",
            },
            "taxPercent1": {
                "type": StringOrNothingType,
                "display_name": "Tax Percent",
                "short_desc": "The tax percentage",
                "desc": "The percentage of the tax to apply to the expense",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! FreshBooks expense update request type
public class FreshBooksExpenseUpdateDataType inherits HashDataType {
    public {
        #! Field definitions for updating expenses
        const Fields = FreshBooksDataProviderBase::AccountIdField + {
            "id": {
                "type": IntType,
                "display_name": "ID",
                "short_desc": "The expense ID (required)",
                "desc": "The unique identifier of the expense to update",
                "required": True,
            },
            "categoryid": {
                "type": IntOrNothingType,
                "display_name": "Category ID",
                "short_desc": "The expense category ID",
                "desc": "The new expense category ID",
            },
            "amount": {
                "type": AutoHashOrNothingType,
                "display_name": "Amount",
                "short_desc": "The expense amount",
                "desc": "The new amount of the expense (hash with `amount` and `code` keys)",
            },
            "vendor": {
                "type": StringOrNothingType,
                "display_name": "Vendor",
                "short_desc": "The vendor name",
                "desc": "The new vendor name for the expense",
            },
            "date": {
                "type": StringOrNothingType,
                "display_name": "Date",
                "short_desc": "The expense date",
                "desc": "The new date of the expense (YYYY-MM-DD format)",
            },
            "notes": {
                "type": StringOrNothingType,
                "display_name": "Notes",
                "short_desc": "The expense notes",
                "desc": "The new notes about the expense",
            },
            "taxName1": {
                "type": StringOrNothingType,
                "display_name": "Tax Name",
                "short_desc": "The tax name",
                "desc": "The new tax name for the expense",
            },
            "taxPercent1": {
                "type": StringOrNothingType,
                "display_name": "Tax Percent",
                "short_desc": "The tax percentage",
                "desc": "The new tax percentage for the expense",
            },
            "staffid": {
                "type": IntOrNothingType,
                "display_name": "Staff ID",
                "short_desc": "The staff member for this expense",
                "desc": "The new staff member ID for the expense",
            },
            "vis_state": {
                "type": IntOrNothingType,
                "display_name": "Visibility State",
                "short_desc": "The visibility state",
                "allowed_values": (
                    <AllowedValueInfo>{"value": 0, "desc": "Active"},
                    <AllowedValueInfo>{"value": 1, "desc": "Deleted"},
                    <AllowedValueInfo>{"value": 2, "desc": "Archived"},
                ),
                "desc": "The new visibility state of the expense",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace FreshBooksDataProvider
