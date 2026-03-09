# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ZohoInvoiceDataProvider module definition

/*  ZohoInvoiceContactDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Zoho Invoice contact data type
public class ZohoInvoiceContactDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "contact_id": {
                "display_name": "Contact ID",
                "short_desc": "Unique identifier for the contact",
                "desc": "The unique identifier for this contact",
                "type": SoftStringType,
            },
            "contact_name": {
                "display_name": "Contact Name",
                "short_desc": "Name of the contact",
                "desc": "The name of the contact (company or person name)",
                "type": SoftStringType,
            },
            "company_name": {
                "display_name": "Company Name",
                "short_desc": "Company name",
                "desc": "The company name of the contact",
                "type": SoftStringOrNothingType,
            },
            "contact_type": {
                "display_name": "Contact Type",
                "short_desc": "Type of contact",
                "desc": "The type of contact (customer)",
                "type": SoftStringOrNothingType,
            },
            "status": {
                "display_name": "Status",
                "short_desc": "Contact status",
                "desc": "Status of the contact",
                "type": SoftStringOrNothingType,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "active", "desc": "Active contact"},
                    <AllowedValueInfo>{"value": "inactive", "desc": "Inactive contact"},
                ),
            },
            "email": {
                "display_name": "Email",
                "short_desc": "Primary email address",
                "desc": "The primary email address of the contact",
                "type": SoftStringOrNothingType,
            },
            "phone": {
                "display_name": "Phone",
                "short_desc": "Phone number",
                "desc": "The phone number of the contact",
                "type": SoftStringOrNothingType,
            },
            "mobile": {
                "display_name": "Mobile",
                "short_desc": "Mobile number",
                "desc": "The mobile number of the contact",
                "type": SoftStringOrNothingType,
            },
            "website": {
                "display_name": "Website",
                "short_desc": "Website URL",
                "desc": "The website URL of the contact",
                "type": SoftStringOrNothingType,
            },
            "currency_id": {
                "display_name": "Currency ID",
                "short_desc": "Currency identifier",
                "desc": "The default currency ID for the contact",
                "type": SoftStringOrNothingType,
            },
            "currency_code": {
                "display_name": "Currency Code",
                "short_desc": "Currency code",
                "desc": "The default currency code",
                "type": SoftStringOrNothingType,
            },
            "payment_terms": {
                "display_name": "Payment Terms",
                "short_desc": "Default payment terms",
                "desc": "The default payment terms for the contact",
                "type": SoftIntOrNothingType,
            },
            "payment_terms_label": {
                "display_name": "Payment Terms Label",
                "short_desc": "Payment terms description",
                "desc": "The label for payment terms",
                "type": SoftStringOrNothingType,
            },
            "notes": {
                "display_name": "Notes",
                "short_desc": "Contact notes",
                "desc": "Notes about the contact",
                "type": SoftStringOrNothingType,
            },
            "billing_address": {
                "display_name": "Billing Address",
                "short_desc": "Billing address",
                "desc": "The billing address of the contact",
                "type": AutoHashOrNothingType,
            },
            "shipping_address": {
                "display_name": "Shipping Address",
                "short_desc": "Shipping address",
                "desc": "The shipping address of the contact",
                "type": AutoHashOrNothingType,
            },
            "outstanding_receivable_amount": {
                "display_name": "Outstanding Receivable",
                "short_desc": "Outstanding receivable amount",
                "desc": "The outstanding receivable amount",
                "type": SoftFloatOrNothingType,
            },
            "unused_credits_receivable_amount": {
                "display_name": "Unused Credits",
                "short_desc": "Unused credits receivable",
                "desc": "The unused credits receivable amount",
                "type": SoftFloatOrNothingType,
            },
            "created_time": {
                "display_name": "Created Time",
                "short_desc": "When the contact was created",
                "desc": "The timestamp when the contact was created",
                "type": SoftStringOrNothingType,
            },
            "last_modified_time": {
                "display_name": "Last Modified Time",
                "short_desc": "When the contact was last modified",
                "desc": "The timestamp when the contact was last modified",
                "type": SoftStringOrNothingType,
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Zoho Invoice create contact request data type
public class ZohoInvoiceCreateContactRequestDataType inherits HashDataType {
    public {
        #! Field definitions for creating a contact
        const Fields = {
            "contact_name": {
                "display_name": "Contact Name",
                "short_desc": "Name of the contact",
                "desc": "The name of the contact (company or person name)",
                "type": SoftStringType,
                "required": True,
            },
            "company_name": {
                "display_name": "Company Name",
                "short_desc": "Company name",
                "desc": "The company name of the contact",
                "type": SoftStringOrNothingType,
            },
            "email": {
                "display_name": "Email",
                "short_desc": "Primary email address",
                "desc": "The primary email address of the contact",
                "type": SoftStringOrNothingType,
            },
            "phone": {
                "display_name": "Phone",
                "short_desc": "Phone number",
                "desc": "The phone number of the contact",
                "type": SoftStringOrNothingType,
            },
            "mobile": {
                "display_name": "Mobile",
                "short_desc": "Mobile number",
                "desc": "The mobile number of the contact",
                "type": SoftStringOrNothingType,
            },
            "website": {
                "display_name": "Website",
                "short_desc": "Website URL",
                "desc": "The website URL of the contact",
                "type": SoftStringOrNothingType,
            },
            "currency_id": {
                "display_name": "Currency ID",
                "short_desc": "Currency identifier",
                "desc": "The default currency ID for the contact",
                "type": SoftStringOrNothingType,
            },
            "payment_terms": {
                "display_name": "Payment Terms",
                "short_desc": "Default payment terms",
                "desc": "The default payment terms in days",
                "type": SoftIntOrNothingType,
            },
            "notes": {
                "display_name": "Notes",
                "short_desc": "Contact notes",
                "desc": "Notes about the contact",
                "type": SoftStringOrNothingType,
            },
            "billing_address": {
                "display_name": "Billing Address",
                "short_desc": "Billing address",
                "desc": "The billing address of the contact",
                "type": AutoHashOrNothingType,
            },
            "shipping_address": {
                "display_name": "Shipping Address",
                "short_desc": "Shipping address",
                "desc": "The shipping address of the contact",
                "type": AutoHashOrNothingType,
            },
            "custom_fields": {
                "display_name": "Custom Fields",
                "short_desc": "Custom field values",
                "desc": "Custom field values for the contact",
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
