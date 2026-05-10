# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WooCommerceDataProvider module definition

/*  WooCommerceCouponLineDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! WooCommerce order coupon-line data type
public class WooCommerceCouponLineDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "id": {
                "display_name": "ID",
                "short_desc": "Coupon line ID",
                "type": IntOrNothingType,
                "desc": "The coupon line ID (omit when creating)",
            },
            "code": {
                "display_name": "Code",
                "short_desc": "Coupon code",
                "type": StringOrNothingType,
                "desc": "The coupon code applied to the order",
                "example_value": "SUMMER2026",
            },
            "discount": {
                "display_name": "Discount",
                "short_desc": "Discount applied",
                "type": StringOrNothingType,
                "desc": "The discount amount applied",
                "example_value": "10.00",
            },
            "discount_tax": {
                "display_name": "Discount Tax",
                "short_desc": "Tax on discount",
                "type": StringOrNothingType,
                "desc": "The tax applied to the discount",
            },
            "meta_data": {
                "display_name": "Meta Data",
                "short_desc": "Coupon line meta data",
                "type": new ListDataType("CouponLineMetaData", new WooCommerceMetaDataType(), True),
                "desc": "Per-line metadata key-value pairs",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }

    #! Creates the object with a specific base type
    constructor(Type base_type) : HashDataType(base_type, "WooCommerceCouponLine") {
        addQoreFields(Fields);
    }
}

} # namespace WooCommerceDataProvider
