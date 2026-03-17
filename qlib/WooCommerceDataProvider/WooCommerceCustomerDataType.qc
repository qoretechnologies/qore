# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WooCommerceDataProvider module definition

/*  WooCommerceCustomerDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the WooCommerceDataProvider module
public namespace WooCommerceDataProvider {

#! WooCommerce customer data type
public class WooCommerceCustomerDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "id": {
                "display_name": "ID",
                "short_desc": "Unique identifier for the customer",
                "type": IntOrNothingType,
                "desc": "The unique identifier for the customer",
            },
            "email": {
                "display_name": "Email",
                "short_desc": "Customer email address",
                "type": StringOrNothingType,
                "desc": "The customer's email address",
            },
            "first_name": {
                "display_name": "First Name",
                "short_desc": "Customer first name",
                "type": StringOrNothingType,
                "desc": "The customer's first name",
            },
            "last_name": {
                "display_name": "Last Name",
                "short_desc": "Customer last name",
                "type": StringOrNothingType,
                "desc": "The customer's last name",
            },
            "username": {
                "display_name": "Username",
                "short_desc": "Customer username",
                "type": StringOrNothingType,
                "desc": "The customer's login username",
            },
            "role": {
                "display_name": "Role",
                "short_desc": "Customer role",
                "type": StringOrNothingType,
                "desc": "The customer's WordPress role (e.g., `customer`)",
            },
            "billing": {
                "display_name": "Billing",
                "short_desc": "Billing address data",
                "type": AutoHashOrNothingType,
                "desc": "The customer's billing address details",
            },
            "shipping": {
                "display_name": "Shipping",
                "short_desc": "Shipping address data",
                "type": AutoHashOrNothingType,
                "desc": "The customer's shipping address details",
            },
            "is_paying_customer": {
                "display_name": "Is Paying Customer",
                "short_desc": "Whether the customer has made a purchase",
                "type": SoftBoolOrNothingType,
                "desc": "If `True`, the customer has completed at least one paid order",
            },
            "orders_count": {
                "display_name": "Orders Count",
                "short_desc": "Total number of orders",
                "type": IntOrNothingType,
                "desc": "The total number of orders placed by this customer",
            },
            "total_spent": {
                "display_name": "Total Spent",
                "short_desc": "Total amount spent",
                "type": StringOrNothingType,
                "desc": "The total amount spent by this customer across all orders",
            },
            "avatar_url": {
                "display_name": "Avatar URL",
                "short_desc": "Customer avatar URL",
                "type": StringOrNothingType,
                "desc": "The URL to the customer's avatar image",
            },
            "date_created": {
                "display_name": "Date Created",
                "short_desc": "Date the customer was created",
                "type": DateOrNothingType,
                "desc": "The date and time the customer account was created",
            },
            "date_modified": {
                "display_name": "Date Modified",
                "short_desc": "Date the customer was last modified",
                "type": DateOrNothingType,
                "desc": "The date and time the customer account was last modified",
            },
            "meta_data": {
                "display_name": "Meta Data",
                "short_desc": "Customer metadata",
                "type": SoftListOrNothingType,
                "desc": "List of metadata key-value pairs associated with the customer",
            },
        };
    }

    #! Creates the object
    constructor() {
        map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type, $1.value.default_value)),
            Fields.pairIterator();
    }
}

} # namespace WooCommerceDataProvider
