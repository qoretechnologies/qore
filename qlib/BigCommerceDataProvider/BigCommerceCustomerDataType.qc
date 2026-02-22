# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BigCommerceCustomerDataType.qc defines the BigCommerceCustomerDataType class

/*  BigCommerceCustomerDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the BigCommerceDataProvider module
public namespace BigCommerceDataProvider {

#! Data type for BigCommerce customer records
public class BigCommerceCustomerDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "id": {
                "display_name": "ID",
                "type": IntType,
                "desc": "Unique numeric ID of the customer",
            },
            "email": {
                "display_name": "Email",
                "type": StringType,
                "desc": "Email address of the customer",
            },
            "first_name": {
                "display_name": "First Name",
                "type": StringType,
                "desc": "First name of the customer",
            },
            "last_name": {
                "display_name": "Last Name",
                "type": StringType,
                "desc": "Last name of the customer",
            },
            "company": {
                "display_name": "Company",
                "type": SoftStringOrNothingType,
                "desc": "Company name of the customer",
            },
            "phone": {
                "display_name": "Phone",
                "type": SoftStringOrNothingType,
                "desc": "Phone number of the customer",
            },
            "registration_ip_address": {
                "display_name": "Registration IP",
                "type": SoftStringOrNothingType,
                "desc": "IP address from which the customer was registered",
            },
            "notes": {
                "display_name": "Notes",
                "type": SoftStringOrNothingType,
                "desc": "Admin notes about the customer",
            },
            "tax_exempt_category": {
                "display_name": "Tax Exempt Category",
                "type": SoftStringOrNothingType,
                "desc": "Tax exempt category code",
            },
            "customer_group_id": {
                "display_name": "Customer Group ID",
                "type": SoftIntOrNothingType,
                "desc": "ID of the customer group this customer belongs to",
            },
            "date_created": {
                "display_name": "Date Created",
                "type": SoftDateOrNothingType,
                "desc": "Date/time the customer was created",
            },
            "date_modified": {
                "display_name": "Date Modified",
                "type": SoftDateOrNothingType,
                "desc": "Date/time the customer was last modified",
            },
            "accepts_product_review_abandoned_cart_emails": {
                "display_name": "Accepts Marketing Emails",
                "type": SoftBoolOrNothingType,
                "desc": "Whether the customer accepts product review and abandoned cart emails",
            },
            "store_credit_amounts": {
                "display_name": "Store Credit",
                "type": AutoListOrNothingType,
                "desc": "Store credit amounts by currency",
            },
            "channel_ids": {
                "display_name": "Channel IDs",
                "type": AutoListOrNothingType,
                "desc": "Array of channel IDs the customer can access",
            },
            "addresses": {
                "display_name": "Addresses",
                "type": AutoListOrNothingType,
                "desc": "Customer address records (when included via `include=addresses`)",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("BigCommerceCustomer") {
        addQoreFields(Fields);
    }
}

} # namespace BigCommerceDataProvider
