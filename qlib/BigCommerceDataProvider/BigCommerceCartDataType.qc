# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BigCommerceCartDataType.qc defines the BigCommerceCartDataType class

/*  BigCommerceCartDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Data type for BigCommerce cart records
public class BigCommerceCartDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "id": {
                "display_name": "ID",
                "type": StringType,
                "desc": "Unique string ID of the cart",
            },
            "customer_id": {
                "display_name": "Customer ID",
                "type": SoftIntOrNothingType,
                "desc": "ID of the customer who owns the cart",
            },
            "email": {
                "display_name": "Email",
                "type": SoftStringOrNothingType,
                "desc": "Email address associated with the cart",
            },
            "currency": {
                "display_name": "Currency",
                "type": AutoHashOrNothingType,
                "desc": "Currency object with code and name",
            },
            "created_time": {
                "display_name": "Created Time",
                "type": SoftDateOrNothingType,
                "desc": "Date/time the cart was created",
            },
            "updated_time": {
                "display_name": "Updated Time",
                "type": SoftDateOrNothingType,
                "desc": "Date/time the cart was last updated",
            },
            "base_amount": {
                "display_name": "Base Amount",
                "type": SoftFloatOrNothingType,
                "desc": "Base amount of the cart before discounts",
            },
            "discount_amount": {
                "display_name": "Discount Amount",
                "type": SoftFloatOrNothingType,
                "desc": "Total discount amount applied",
            },
            "cart_amount": {
                "display_name": "Cart Amount",
                "type": SoftFloatOrNothingType,
                "desc": "Total cart amount after discounts",
            },
            "line_items": {
                "display_name": "Line Items",
                "type": AutoHashOrNothingType,
                "desc": "Line items in the cart, grouped by type",
            },
            "channel_id": {
                "display_name": "Channel ID",
                "type": SoftIntOrNothingType,
                "desc": "ID of the channel this cart belongs to",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("BigCommerceCart") {
        addQoreFields(Fields);
    }
}

} # namespace BigCommerceDataProvider
