# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore Cin7CoreDataProvider module definition

/*  Cin7CoreCustomerDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Cin7Core customer data type
public class Cin7CoreCustomerDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "ID": {
                "type": SoftStringOrNothingType,
                "display_name": "Customer ID",
                "short_desc": "Unique customer identifier",
                "desc": "The unique identifier (GUID) of the customer in Cin7Core",
            },
            "Name": {
                "type": SoftStringOrNothingType,
                "display_name": "Name",
                "short_desc": "Customer name",
                "desc": "The customer's name or company name",
            },
            "Status": {
                "type": SoftStringOrNothingType,
                "display_name": "Status",
                "short_desc": "Customer status",
                "desc": "Active or inactive status of the customer",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "Active", "desc": "Active customer"},
                    <AllowedValueInfo>{"value": "Deprecated", "desc": "Deprecated/inactive customer"},
                ),
            },
            "Currency": {
                "type": SoftStringOrNothingType,
                "display_name": "Currency",
                "short_desc": "Default currency",
                "desc": "Three-letter ISO currency code for the customer's default currency",
            },
            "PaymentTerm": {
                "type": SoftStringOrNothingType,
                "display_name": "Payment Term",
                "short_desc": "Payment terms",
                "desc": "The payment terms for this customer",
            },
            "AccountReceivable": {
                "type": SoftStringOrNothingType,
                "display_name": "Account Receivable",
                "short_desc": "AR account",
                "desc": "The account receivable account code",
            },
            "RevenueAccount": {
                "type": SoftStringOrNothingType,
                "display_name": "Revenue Account",
                "short_desc": "Revenue account",
                "desc": "The revenue account code",
            },
            "TaxRule": {
                "type": SoftStringOrNothingType,
                "display_name": "Tax Rule",
                "short_desc": "Tax rule",
                "desc": "The tax rule applied to this customer",
            },
            "Discount": {
                "type": SoftFloatOrNothingType,
                "display_name": "Discount",
                "short_desc": "Default discount %",
                "desc": "Default discount percentage for this customer",
            },
            "Comments": {
                "type": SoftStringOrNothingType,
                "display_name": "Comments",
                "short_desc": "Notes",
                "desc": "Additional notes about the customer",
            },
            "TaxNumber": {
                "type": SoftStringOrNothingType,
                "display_name": "Tax Number",
                "short_desc": "Tax ID",
                "desc": "The customer's tax identification number",
            },
            "CreditLimit": {
                "type": SoftFloatOrNothingType,
                "display_name": "Credit Limit",
                "short_desc": "Credit limit",
                "desc": "The customer's credit limit",
            },
            "IsOnCreditHold": {
                "type": SoftBoolOrNothingType,
                "display_name": "On Credit Hold",
                "short_desc": "Credit hold status",
                "desc": "Whether the customer is on credit hold",
            },
            "Contacts": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Contacts",
                "short_desc": "Contact list",
                "desc": "List of contacts for this customer",
            },
            "Addresses": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Addresses",
                "short_desc": "Address list",
                "desc": "List of addresses for this customer",
            },
            "AttributeSet": {
                "type": SoftStringOrNothingType,
                "display_name": "Attribute Set",
                "short_desc": "Attribute set name",
                "desc": "The attribute set applied to this customer",
            },
            "Tags": {
                "type": SoftStringOrNothingType,
                "display_name": "Tags",
                "short_desc": "Customer tags",
                "desc": "Comma-separated tags for the customer",
            },
            "LastModifiedOn": {
                "type": SoftStringOrNothingType,
                "display_name": "Last Modified",
                "short_desc": "Last modification date",
                "desc": "ISO 8601 timestamp when the customer was last modified",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("Cin7CoreCustomerDataType") {
        addQoreFields(Fields);
    }
}

#! Cin7Core create customer request data type
public class Cin7CoreCreateCustomerRequestDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "Name": {
                "type": SoftStringType,
                "display_name": "Name",
                "short_desc": "Customer name",
                "desc": "The customer's name or company name (required)",
                "required": True,
            },
            "Status": {
                "type": SoftStringOrNothingType,
                "display_name": "Status",
                "short_desc": "Customer status",
                "desc": "Active or inactive status. Defaults to Active",
                "default_value": "Active",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "Active", "desc": "Active customer"},
                    <AllowedValueInfo>{"value": "Deprecated", "desc": "Deprecated/inactive customer"},
                ),
            },
            "Currency": {
                "type": SoftStringOrNothingType,
                "display_name": "Currency",
                "short_desc": "Default currency",
                "desc": "Three-letter ISO currency code",
            },
            "PaymentTerm": {
                "type": SoftStringOrNothingType,
                "display_name": "Payment Term",
                "short_desc": "Payment terms",
                "desc": "The payment terms for this customer",
            },
            "AccountReceivable": {
                "type": SoftStringOrNothingType,
                "display_name": "Account Receivable",
                "short_desc": "AR account",
                "desc": "The account receivable account code",
            },
            "RevenueAccount": {
                "type": SoftStringOrNothingType,
                "display_name": "Revenue Account",
                "short_desc": "Revenue account",
                "desc": "The revenue account code",
            },
            "TaxRule": {
                "type": SoftStringOrNothingType,
                "display_name": "Tax Rule",
                "short_desc": "Tax rule",
                "desc": "The tax rule to apply",
            },
            "Discount": {
                "type": SoftFloatOrNothingType,
                "display_name": "Discount",
                "short_desc": "Default discount %",
                "desc": "Default discount percentage",
            },
            "Comments": {
                "type": SoftStringOrNothingType,
                "display_name": "Comments",
                "short_desc": "Notes",
                "desc": "Additional notes about the customer",
            },
            "TaxNumber": {
                "type": SoftStringOrNothingType,
                "display_name": "Tax Number",
                "short_desc": "Tax ID",
                "desc": "The customer's tax identification number",
            },
            "CreditLimit": {
                "type": SoftFloatOrNothingType,
                "display_name": "Credit Limit",
                "short_desc": "Credit limit",
                "desc": "The customer's credit limit",
            },
            "Contacts": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Contacts",
                "short_desc": "Contact list",
                "desc": "List of contacts to create for this customer",
            },
            "Addresses": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Addresses",
                "short_desc": "Address list",
                "desc": "List of addresses to create for this customer",
            },
            "Tags": {
                "type": SoftStringOrNothingType,
                "display_name": "Tags",
                "short_desc": "Customer tags",
                "desc": "Comma-separated tags for the customer",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("Cin7CoreCreateCustomerRequestDataType") {
        addQoreFields(Fields);
    }
}

} # namespace Cin7CoreDataProvider
