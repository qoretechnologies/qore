# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WooCommerceDataProvider module definition

/*  WooCommerceCouponDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! WooCommerce coupon data type
public class WooCommerceCouponDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "id": {
                "display_name": "ID",
                "short_desc": "Unique identifier for the coupon",
                "type": IntOrNothingType,
                "desc": "The unique identifier for the coupon",
            },
            "code": {
                "display_name": "Code",
                "short_desc": "Coupon code",
                "type": StringOrNothingType,
                "desc": "The coupon code that customers enter at checkout",
            },
            "amount": {
                "display_name": "Amount",
                "short_desc": "Discount amount",
                "type": StringOrNothingType,
                "desc": "The discount amount for the coupon",
            },
            "discount_type": {
                "display_name": "Discount Type",
                "short_desc": "Type of discount",
                "type": StringOrNothingType,
                "desc": "Discount type: `percent`, `fixed_cart`, `fixed_product`",
            },
            "description": {
                "display_name": "Description",
                "short_desc": "Coupon description",
                "type": StringOrNothingType,
                "desc": "The description of the coupon",
            },
            "date_expires": {
                "display_name": "Date Expires",
                "short_desc": "Coupon expiration date",
                "type": StringOrNothingType,
                "desc": "The date the coupon expires, in ISO 8601 format",
            },
            "usage_count": {
                "display_name": "Usage Count",
                "short_desc": "Number of times the coupon has been used",
                "type": IntOrNothingType,
                "desc": "The number of times the coupon has been used",
            },
            "individual_use": {
                "display_name": "Individual Use",
                "short_desc": "Whether the coupon can be used alone only",
                "type": SoftBoolOrNothingType,
                "desc": "If `True`, the coupon cannot be combined with other coupons",
            },
            "product_ids": {
                "display_name": "Product IDs",
                "short_desc": "Products the coupon applies to",
                "type": SoftListOrNothingType,
                "desc": "List of product IDs the coupon can be applied to",
            },
            "excluded_product_ids": {
                "display_name": "Excluded Product IDs",
                "short_desc": "Products excluded from the coupon",
                "type": SoftListOrNothingType,
                "desc": "List of product IDs excluded from the coupon",
            },
            "usage_limit": {
                "display_name": "Usage Limit",
                "short_desc": "Maximum number of times the coupon can be used",
                "type": IntOrNothingType,
                "desc": "The maximum number of times the coupon can be used in total",
            },
            "usage_limit_per_user": {
                "display_name": "Usage Limit Per User",
                "short_desc": "Maximum uses per customer",
                "type": IntOrNothingType,
                "desc": "The maximum number of times the coupon can be used per customer",
            },
            "free_shipping": {
                "display_name": "Free Shipping",
                "short_desc": "Whether the coupon grants free shipping",
                "type": SoftBoolOrNothingType,
                "desc": "If `True`, the coupon grants free shipping",
            },
            "minimum_amount": {
                "display_name": "Minimum Amount",
                "short_desc": "Minimum order amount required",
                "type": StringOrNothingType,
                "desc": "The minimum order subtotal required to use the coupon",
            },
            "maximum_amount": {
                "display_name": "Maximum Amount",
                "short_desc": "Maximum order amount allowed",
                "type": StringOrNothingType,
                "desc": "The maximum order subtotal allowed to use the coupon",
            },
            "date_created": {
                "display_name": "Date Created",
                "short_desc": "Date the coupon was created",
                "type": DateOrNothingType,
                "desc": "The date and time the coupon was created",
            },
            "date_modified": {
                "display_name": "Date Modified",
                "short_desc": "Date the coupon was last modified",
                "type": DateOrNothingType,
                "desc": "The date and time the coupon was last modified",
            },
            "meta_data": {
                "display_name": "Meta Data",
                "short_desc": "Coupon metadata",
                "type": SoftListOrNothingType,
                "desc": "List of metadata key-value pairs associated with the coupon",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace WooCommerceDataProvider
