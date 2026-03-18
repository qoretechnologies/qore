# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WooCommerceDataProvider module definition

/*  WooCommerceRefundDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! WooCommerce refund data type
public class WooCommerceRefundDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "id": {
                "display_name": "ID",
                "short_desc": "Unique identifier for the refund",
                "type": IntOrNothingType,
                "desc": "The unique identifier for the refund",
            },
            "date_created": {
                "display_name": "Date Created",
                "short_desc": "Date the refund was created",
                "type": DateOrNothingType,
                "desc": "The date and time the refund was created",
            },
            "amount": {
                "display_name": "Amount",
                "short_desc": "Refund amount",
                "type": StringOrNothingType,
                "desc": "The total refund amount",
            },
            "reason": {
                "display_name": "Reason",
                "short_desc": "Reason for the refund",
                "type": StringOrNothingType,
                "desc": "The reason provided for the refund",
            },
            "refunded_by": {
                "display_name": "Refunded By",
                "short_desc": "User ID who processed the refund",
                "type": IntOrNothingType,
                "desc": "The user ID of the person who processed the refund",
            },
            "refunded_payment": {
                "display_name": "Refunded Payment",
                "short_desc": "Whether the payment was refunded via the gateway",
                "type": SoftBoolOrNothingType,
                "desc": "If `True`, the payment was refunded through the payment gateway",
            },
            "line_items": {
                "display_name": "Line Items",
                "short_desc": "Refunded line items",
                "type": SoftListOrNothingType,
                "desc": "List of line items that were refunded",
            },
            "meta_data": {
                "display_name": "Meta Data",
                "short_desc": "Refund metadata",
                "type": SoftListOrNothingType,
                "desc": "List of metadata key-value pairs associated with the refund",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace WooCommerceDataProvider
