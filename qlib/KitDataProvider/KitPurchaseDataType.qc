# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore KitDataProvider module definition

/*  KitPurchaseDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the KitDataProvider module
public namespace KitDataProvider {

#! Kit purchase record type
public class KitPurchaseDataType inherits HashDataType {
    public {
        #! Field definitions for Kit purchases
        const Fields = {
            "id": {
                "type": SoftIntType,
                "display_name": "ID",
                "short_desc": "The purchase ID",
                "desc": "The unique identifier of the purchase",
            },
            "transaction_id": {
                "type": StringOrNothingType,
                "display_name": "Transaction ID",
                "short_desc": "The transaction ID",
                "desc": "The external transaction identifier for the purchase",
            },
            "email_address": {
                "type": StringOrNothingType,
                "display_name": "Email Address",
                "short_desc": "The buyer's email address",
                "desc": "The email address of the buyer",
            },
            "currency": {
                "type": StringOrNothingType,
                "display_name": "Currency",
                "short_desc": "The currency code",
                "desc": "The ISO 4217 currency code for the purchase",
            },
            "subtotal": {
                "type": FloatOrNothingType,
                "display_name": "Subtotal",
                "short_desc": "The purchase subtotal",
                "desc": "The subtotal amount of the purchase before tax, shipping, and discount",
            },
            "tax": {
                "type": FloatOrNothingType,
                "display_name": "Tax",
                "short_desc": "The tax amount",
                "desc": "The tax amount applied to the purchase",
            },
            "shipping": {
                "type": FloatOrNothingType,
                "display_name": "Shipping",
                "short_desc": "The shipping cost",
                "desc": "The shipping cost for the purchase",
            },
            "discount": {
                "type": FloatOrNothingType,
                "display_name": "Discount",
                "short_desc": "The discount amount",
                "desc": "The discount amount applied to the purchase",
            },
            "total": {
                "type": FloatOrNothingType,
                "display_name": "Total",
                "short_desc": "The total amount",
                "desc": "The total amount of the purchase",
            },
            "status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "The purchase status",
                "desc": "The current status of the purchase",
            },
            "transaction_time": {
                "type": DateOrNothingType,
                "display_name": "Transaction Time",
                "short_desc": "When the transaction occurred",
                "desc": "The timestamp when the transaction occurred",
            },
            "subscriber_id": {
                "type": SoftIntOrNothingType,
                "display_name": "Subscriber ID",
                "short_desc": "The associated subscriber ID",
                "desc": "The unique identifier of the subscriber associated with the purchase",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace KitDataProvider
