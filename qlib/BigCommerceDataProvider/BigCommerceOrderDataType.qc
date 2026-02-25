# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BigCommerceOrderDataType.qc defines the BigCommerceOrderDataType class

/*  BigCommerceOrderDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Data type for BigCommerce order records (V2 API format)
public class BigCommerceOrderDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "id": {
                "display_name": "ID",
                "type": IntType,
                "desc": "Unique numeric ID of the order",
            },
            "customer_id": {
                "display_name": "Customer ID",
                "type": SoftIntOrNothingType,
                "desc": "ID of the customer who placed the order",
            },
            "date_created": {
                "display_name": "Date Created",
                "type": SoftDateOrNothingType,
                "desc": "Date/time the order was created",
            },
            "date_modified": {
                "display_name": "Date Modified",
                "type": SoftDateOrNothingType,
                "desc": "Date/time the order was last modified",
            },
            "date_shipped": {
                "display_name": "Date Shipped",
                "type": SoftDateOrNothingType,
                "desc": "Date/time the order was shipped",
            },
            "status_id": {
                "display_name": "Status ID",
                "type": SoftIntOrNothingType,
                "desc": "Numeric ID of the order status",
            },
            "status": {
                "display_name": "Status",
                "type": SoftStringOrNothingType,
                "desc": "Order status name (e.g., Pending, Shipped, Completed)",
            },
            "subtotal_ex_tax": {
                "display_name": "Subtotal (excl. tax)",
                "type": SoftStringOrNothingType,
                "desc": "Subtotal excluding tax",
            },
            "subtotal_inc_tax": {
                "display_name": "Subtotal (incl. tax)",
                "type": SoftStringOrNothingType,
                "desc": "Subtotal including tax",
            },
            "total_ex_tax": {
                "display_name": "Total (excl. tax)",
                "type": SoftStringOrNothingType,
                "desc": "Total excluding tax",
            },
            "total_inc_tax": {
                "display_name": "Total (incl. tax)",
                "type": SoftStringOrNothingType,
                "desc": "Total including tax",
            },
            "items_total": {
                "display_name": "Items Total",
                "type": SoftIntOrNothingType,
                "desc": "Total number of items in the order",
            },
            "items_shipped": {
                "display_name": "Items Shipped",
                "type": SoftIntOrNothingType,
                "desc": "Number of items shipped",
            },
            "payment_method": {
                "display_name": "Payment Method",
                "type": SoftStringOrNothingType,
                "desc": "Payment method used",
            },
            "payment_status": {
                "display_name": "Payment Status",
                "type": SoftStringOrNothingType,
                "desc": "Payment status",
            },
            "currency_code": {
                "display_name": "Currency Code",
                "type": SoftStringOrNothingType,
                "desc": "Currency code for the order",
            },
            "billing_address": {
                "display_name": "Billing Address",
                "type": AutoHashOrNothingType,
                "desc": "Billing address object",
            },
            "ip_address": {
                "display_name": "IP Address",
                "type": SoftStringOrNothingType,
                "desc": "IP address of the customer",
            },
            "staff_notes": {
                "display_name": "Staff Notes",
                "type": SoftStringOrNothingType,
                "desc": "Internal staff notes",
            },
            "customer_message": {
                "display_name": "Customer Message",
                "type": SoftStringOrNothingType,
                "desc": "Message from the customer",
            },
            "products": {
                "display_name": "Products",
                "type": AutoHashOrNothingType,
                "desc": "Products resource URL object",
            },
            "shipping_addresses": {
                "display_name": "Shipping Addresses",
                "type": AutoHashOrNothingType,
                "desc": "Shipping addresses resource URL object",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("BigCommerceOrder") {
        addQoreFields(Fields);
    }
}

} # namespace BigCommerceDataProvider
