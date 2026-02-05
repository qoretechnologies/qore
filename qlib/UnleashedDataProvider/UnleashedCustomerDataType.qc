# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore UnleashedDataProvider module definition

/*  UnleashedCustomerDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the UnleashedDataProvider module
public namespace UnleashedDataProvider {

#! Customer data type for Unleashed
public class UnleashedCustomerDataType inherits HashDataType {
    private {
        const Fields = {
            "Guid": {
                "type": SoftStringOrNothingType,
                "display_name": "Customer GUID",
                "short_desc": "Unique customer identifier",
                "desc": "The unique GUID identifier of the customer",
            },
            "CustomerCode": {
                "type": SoftStringOrNothingType,
                "display_name": "Customer Code",
                "short_desc": "Customer code",
                "desc": "The unique customer code (required for create, cannot be modified after creation)",
            },
            "CustomerName": {
                "type": SoftStringOrNothingType,
                "display_name": "Customer Name",
                "short_desc": "Customer name",
                "desc": "The customer's display name",
            },
            "Currency": {
                "type": SoftAutoHashOrNothingType,
                "display_name": "Currency",
                "short_desc": "Currency details",
                "desc": "Currency information including code and description",
            },
            "CreditLimit": {
                "type": SoftFloatOrNothingType,
                "display_name": "Credit Limit",
                "short_desc": "Customer credit limit",
                "desc": "The customer's credit limit",
            },
            "DiscountRate": {
                "type": SoftFloatOrNothingType,
                "display_name": "Discount Rate",
                "short_desc": "Default discount rate",
                "desc": "The customer's default discount rate",
            },
            "Taxable": {
                "type": SoftBoolOrNothingType,
                "display_name": "Taxable",
                "short_desc": "Is customer taxable",
                "desc": "Whether the customer is taxable",
            },
            "StopCredit": {
                "type": SoftBoolOrNothingType,
                "display_name": "Stop Credit",
                "short_desc": "Credit stop status",
                "desc": "Whether credit is stopped for this customer",
            },
            "Obsolete": {
                "type": SoftBoolOrNothingType,
                "display_name": "Obsolete",
                "short_desc": "Is customer obsolete",
                "desc": "Whether the customer is marked as obsolete",
            },
            "ContactFirstName": {
                "type": SoftStringOrNothingType,
                "display_name": "Contact First Name",
                "short_desc": "Primary contact first name",
                "desc": "First name of the primary contact",
            },
            "ContactLastName": {
                "type": SoftStringOrNothingType,
                "display_name": "Contact Last Name",
                "short_desc": "Primary contact last name",
                "desc": "Last name of the primary contact",
            },
            "Email": {
                "type": SoftStringOrNothingType,
                "display_name": "Email",
                "short_desc": "Primary email address",
                "desc": "Primary email address for the customer",
            },
            "PhoneNumber": {
                "type": SoftStringOrNothingType,
                "display_name": "Phone Number",
                "short_desc": "Phone number",
                "desc": "Customer's phone number",
            },
            "FaxNumber": {
                "type": SoftStringOrNothingType,
                "display_name": "Fax Number",
                "short_desc": "Fax number",
                "desc": "Customer's fax number",
            },
            "MobileNumber": {
                "type": SoftStringOrNothingType,
                "display_name": "Mobile Number",
                "short_desc": "Mobile phone number",
                "desc": "Customer's mobile phone number",
            },
            "Website": {
                "type": SoftStringOrNothingType,
                "display_name": "Website",
                "short_desc": "Customer website",
                "desc": "Customer's website URL",
            },
            "Addresses": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Addresses",
                "short_desc": "Customer addresses",
                "desc": "List of customer addresses (Shipping, Postal, Physical)",
            },
            "Contacts": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Contacts",
                "short_desc": "Customer contacts",
                "desc": "List of customer contacts",
            },
            "Salesperson": {
                "type": SoftAutoHashOrNothingType,
                "display_name": "Salesperson",
                "short_desc": "Assigned salesperson",
                "desc": "The salesperson assigned to this customer",
            },
            "DefaultWarehouse": {
                "type": SoftAutoHashOrNothingType,
                "display_name": "Default Warehouse",
                "short_desc": "Default warehouse",
                "desc": "The default warehouse for this customer",
            },
            "PaymentTerm": {
                "type": SoftAutoHashOrNothingType,
                "display_name": "Payment Term",
                "short_desc": "Payment terms",
                "desc": "The payment terms for this customer",
            },
            "CustomerType": {
                "type": SoftStringOrNothingType,
                "display_name": "Customer Type",
                "short_desc": "Customer type name",
                "desc": "The customer type name",
            },
            "CustomerTypeGuid": {
                "type": SoftStringOrNothingType,
                "display_name": "Customer Type GUID",
                "short_desc": "Customer type identifier",
                "desc": "The unique identifier for the customer type",
            },
            "DeliveryInstruction": {
                "type": SoftStringOrNothingType,
                "display_name": "Delivery Instruction",
                "short_desc": "Delivery instructions",
                "desc": "Special delivery instructions for this customer",
            },
            "CreatedOn": {
                "type": SoftDateOrNothingType,
                "display_name": "Created On",
                "short_desc": "Creation date",
                "desc": "When the customer was created",
            },
            "LastModifiedOn": {
                "type": SoftDateOrNothingType,
                "display_name": "Last Modified On",
                "short_desc": "Last modification date",
                "desc": "When the customer was last modified",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("UnleashedCustomerDataType") {
        addQoreFields(Fields);
    }
}

#! Customer creation request data type
public class UnleashedCreateCustomerRequestDataType inherits HashDataType {
    private {
        const Fields = {
            "CustomerCode": {
                "type": SoftStringType,
                "display_name": "Customer Code",
                "short_desc": "Unique customer code",
                "desc": "The unique customer code (required, cannot be modified after creation)",
                "required": True,
            },
            "CustomerName": {
                "type": SoftStringType,
                "display_name": "Customer Name",
                "short_desc": "Customer name",
                "desc": "The customer's display name (required)",
                "required": True,
            },
            "ContactFirstName": {
                "type": SoftStringOrNothingType,
                "display_name": "Contact First Name",
                "short_desc": "Primary contact first name",
                "desc": "First name of the primary contact",
            },
            "ContactLastName": {
                "type": SoftStringOrNothingType,
                "display_name": "Contact Last Name",
                "short_desc": "Primary contact last name",
                "desc": "Last name of the primary contact",
            },
            "Email": {
                "type": SoftStringOrNothingType,
                "display_name": "Email",
                "short_desc": "Primary email address",
                "desc": "Primary email address for the customer",
            },
            "PhoneNumber": {
                "type": SoftStringOrNothingType,
                "display_name": "Phone Number",
                "short_desc": "Phone number",
                "desc": "Customer's phone number",
            },
            "MobileNumber": {
                "type": SoftStringOrNothingType,
                "display_name": "Mobile Number",
                "short_desc": "Mobile phone number",
                "desc": "Customer's mobile phone number",
            },
            "CreditLimit": {
                "type": SoftFloatOrNothingType,
                "display_name": "Credit Limit",
                "short_desc": "Customer credit limit",
                "desc": "The customer's credit limit",
            },
            "DiscountRate": {
                "type": SoftFloatOrNothingType,
                "display_name": "Discount Rate",
                "short_desc": "Default discount rate",
                "desc": "The customer's default discount rate",
            },
            "Taxable": {
                "type": SoftBoolOrNothingType,
                "display_name": "Taxable",
                "short_desc": "Is customer taxable",
                "desc": "Whether the customer is taxable",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("UnleashedCreateCustomerRequestDataType") {
        addQoreFields(Fields);
    }
}

} # namespace UnleashedDataProvider
