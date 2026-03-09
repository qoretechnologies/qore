# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore Cin7CoreDataProvider module definition

/*  Cin7CoreSupplierDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Cin7Core supplier data type
public class Cin7CoreSupplierDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "ID": {
                "type": SoftStringOrNothingType,
                "display_name": "Supplier ID",
                "short_desc": "Unique supplier identifier",
                "desc": "The unique identifier (GUID) of the supplier in Cin7Core",
            },
            "Name": {
                "type": SoftStringOrNothingType,
                "display_name": "Name",
                "short_desc": "Supplier name",
                "desc": "The supplier's name or company name",
            },
            "Status": {
                "type": SoftStringOrNothingType,
                "display_name": "Status",
                "short_desc": "Supplier status",
                "desc": "Active or inactive status of the supplier",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "Active", "desc": "Active supplier"},
                    <AllowedValueInfo>{"value": "Deprecated", "desc": "Deprecated/inactive supplier"},
                ),
            },
            "Currency": {
                "type": SoftStringOrNothingType,
                "display_name": "Currency",
                "short_desc": "Default currency",
                "desc": "Three-letter ISO currency code for the supplier's default currency",
            },
            "PaymentTerm": {
                "type": SoftStringOrNothingType,
                "display_name": "Payment Term",
                "short_desc": "Payment terms",
                "desc": "The payment terms for this supplier",
            },
            "AccountPayable": {
                "type": SoftStringOrNothingType,
                "display_name": "Account Payable",
                "short_desc": "AP account",
                "desc": "The account payable account code",
            },
            "TaxRule": {
                "type": SoftStringOrNothingType,
                "display_name": "Tax Rule",
                "short_desc": "Tax rule",
                "desc": "The tax rule applied to this supplier",
            },
            "Discount": {
                "type": SoftFloatOrNothingType,
                "display_name": "Discount",
                "short_desc": "Default discount %",
                "desc": "Default discount percentage from this supplier",
            },
            "Comments": {
                "type": SoftStringOrNothingType,
                "display_name": "Comments",
                "short_desc": "Notes",
                "desc": "Additional notes about the supplier",
            },
            "TaxNumber": {
                "type": SoftStringOrNothingType,
                "display_name": "Tax Number",
                "short_desc": "Tax ID",
                "desc": "The supplier's tax identification number",
            },
            "Contacts": {
                "type": new ListDataType("Contacts", new Cin7CoreContactDataType(), True),
                "display_name": "Contacts",
                "short_desc": "Contact list",
                "desc": "List of contacts for this supplier",
            },
            "Addresses": {
                "type": new ListDataType("Addresses", new Cin7CoreAddressDataType(), True),
                "display_name": "Addresses",
                "short_desc": "Address list",
                "desc": "List of addresses for this supplier",
            },
            "AttributeSet": {
                "type": SoftStringOrNothingType,
                "display_name": "Attribute Set",
                "short_desc": "Attribute set name",
                "desc": "The attribute set applied to this supplier",
            },
            "Tags": {
                "type": SoftStringOrNothingType,
                "display_name": "Tags",
                "short_desc": "Supplier tags",
                "desc": "Comma-separated tags for the supplier",
            },
            "LastModifiedOn": {
                "type": SoftDateOrNothingType,
                "display_name": "Last Modified",
                "short_desc": "Last modification date",
                "desc": "Timestamp when the supplier was last modified",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("Cin7CoreSupplierDataType") {
        addQoreFields(Fields);
    }
}

#! Cin7Core create supplier request data type
public class Cin7CoreCreateSupplierRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "Name": {
                "type": SoftStringType,
                "display_name": "Name",
                "short_desc": "Supplier name",
                "desc": "The supplier's name or company name (required)",
                "required": True,
            },
            "Status": {
                "type": SoftStringOrNothingType,
                "display_name": "Status",
                "short_desc": "Supplier status",
                "desc": "Active or inactive status. Defaults to Active",
                "default_value": "Active",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "Active", "desc": "Active supplier"},
                    <AllowedValueInfo>{"value": "Deprecated", "desc": "Deprecated/inactive supplier"},
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
                "desc": "The payment terms for this supplier",
            },
            "AccountPayable": {
                "type": SoftStringOrNothingType,
                "display_name": "Account Payable",
                "short_desc": "AP account",
                "desc": "The account payable account code",
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
                "desc": "Additional notes about the supplier",
            },
            "TaxNumber": {
                "type": SoftStringOrNothingType,
                "display_name": "Tax Number",
                "short_desc": "Tax ID",
                "desc": "The supplier's tax identification number",
            },
            "Contacts": {
                "type": new ListDataType("Contacts", new Cin7CoreContactDataType(), True),
                "display_name": "Contacts",
                "short_desc": "Contact list",
                "desc": "List of contacts to create for this supplier",
            },
            "Addresses": {
                "type": new ListDataType("Addresses", new Cin7CoreAddressDataType(), True),
                "display_name": "Addresses",
                "short_desc": "Address list",
                "desc": "List of addresses to create for this supplier",
            },
            "Tags": {
                "type": SoftStringOrNothingType,
                "display_name": "Tags",
                "short_desc": "Supplier tags",
                "desc": "Comma-separated tags for the supplier",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("Cin7CoreCreateSupplierRequestDataType") {
        addQoreFields(Fields);
    }
}

} # namespace Cin7CoreDataProvider
