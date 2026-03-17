# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WooCommerceDataProvider module definition

/*  WooCommerceEventDataTypes.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Order event data type for WooCommerce webhook events
public class WooCommerceOrderEventDataType inherits HashDataType {
    public {
        #! Field definitions for order events
        const Fields = {
            "id": {
                "display_name": "Order ID",
                "type": IntOrNothingType,
                "desc": "The order ID",
            },
            "parent_id": {
                "display_name": "Parent ID",
                "type": IntOrNothingType,
                "desc": "Parent order ID",
            },
            "status": {
                "display_name": "Status",
                "type": StringOrNothingType,
                "desc": "Order status",
            },
            "currency": {
                "display_name": "Currency",
                "type": StringOrNothingType,
                "desc": "Currency code",
            },
            "total": {
                "display_name": "Total",
                "type": StringOrNothingType,
                "desc": "Order total",
            },
            "customer_id": {
                "display_name": "Customer ID",
                "type": IntOrNothingType,
                "desc": "Customer ID",
            },
            "billing": {
                "display_name": "Billing",
                "type": AutoHashOrNothingType,
                "desc": "Billing address",
            },
            "shipping": {
                "display_name": "Shipping",
                "type": AutoHashOrNothingType,
                "desc": "Shipping address",
            },
            "line_items": {
                "display_name": "Line Items",
                "type": SoftListOrNothingType,
                "desc": "Order line items",
            },
            "payment_method": {
                "display_name": "Payment Method",
                "type": StringOrNothingType,
                "desc": "Payment method",
            },
            "date_created": {
                "display_name": "Date Created",
                "type": DateOrNothingType,
                "desc": "Date order was created",
            },
        };
    }

    #! Creates the type
    constructor() {
        map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type)), Fields.pairIterator();
    }
}

#! Product event data type for WooCommerce webhook events
public class WooCommerceProductEventDataType inherits HashDataType {
    public {
        #! Field definitions for product events
        const Fields = {
            "id": {
                "display_name": "Product ID",
                "type": IntOrNothingType,
                "desc": "The product ID",
            },
            "name": {
                "display_name": "Name",
                "type": StringOrNothingType,
                "desc": "Product name",
            },
            "slug": {
                "display_name": "Slug",
                "type": StringOrNothingType,
                "desc": "Product slug",
            },
            "type": {
                "display_name": "Type",
                "type": StringOrNothingType,
                "desc": "Product type (simple, grouped, external, variable)",
            },
            "status": {
                "display_name": "Status",
                "type": StringOrNothingType,
                "desc": "Product status (draft, pending, private, publish)",
            },
            "sku": {
                "display_name": "SKU",
                "type": StringOrNothingType,
                "desc": "Stock keeping unit",
            },
            "regular_price": {
                "display_name": "Regular Price",
                "type": StringOrNothingType,
                "desc": "Product regular price",
            },
            "sale_price": {
                "display_name": "Sale Price",
                "type": StringOrNothingType,
                "desc": "Product sale price",
            },
            "stock_quantity": {
                "display_name": "Stock Quantity",
                "type": IntOrNothingType,
                "desc": "Stock quantity",
            },
            "stock_status": {
                "display_name": "Stock Status",
                "type": StringOrNothingType,
                "desc": "Stock status (instock, outofstock, onbackorder)",
            },
            "date_created": {
                "display_name": "Date Created",
                "type": DateOrNothingType,
                "desc": "Date product was created",
            },
        };
    }

    #! Creates the type
    constructor() {
        map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type)), Fields.pairIterator();
    }
}

#! Customer event data type for WooCommerce webhook events
public class WooCommerceCustomerEventDataType inherits HashDataType {
    public {
        #! Field definitions for customer events
        const Fields = {
            "id": {
                "display_name": "Customer ID",
                "type": IntOrNothingType,
                "desc": "The customer ID",
            },
            "email": {
                "display_name": "Email",
                "type": StringOrNothingType,
                "desc": "Customer email address",
            },
            "first_name": {
                "display_name": "First Name",
                "type": StringOrNothingType,
                "desc": "Customer first name",
            },
            "last_name": {
                "display_name": "Last Name",
                "type": StringOrNothingType,
                "desc": "Customer last name",
            },
            "username": {
                "display_name": "Username",
                "type": StringOrNothingType,
                "desc": "Customer username",
            },
            "role": {
                "display_name": "Role",
                "type": StringOrNothingType,
                "desc": "Customer role",
            },
            "date_created": {
                "display_name": "Date Created",
                "type": DateOrNothingType,
                "desc": "Date customer was created",
            },
        };
    }

    #! Creates the type
    constructor() {
        map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type)), Fields.pairIterator();
    }
}

#! Coupon event data type for WooCommerce webhook events
public class WooCommerceCouponEventDataType inherits HashDataType {
    public {
        #! Field definitions for coupon events
        const Fields = {
            "id": {
                "display_name": "Coupon ID",
                "type": IntOrNothingType,
                "desc": "The coupon ID",
            },
            "code": {
                "display_name": "Code",
                "type": StringOrNothingType,
                "desc": "Coupon code",
            },
            "amount": {
                "display_name": "Amount",
                "type": StringOrNothingType,
                "desc": "Coupon amount",
            },
            "discount_type": {
                "display_name": "Discount Type",
                "type": StringOrNothingType,
                "desc": "Discount type (percent, fixed_cart, fixed_product)",
            },
            "usage_count": {
                "display_name": "Usage Count",
                "type": IntOrNothingType,
                "desc": "Number of times the coupon has been used",
            },
            "date_created": {
                "display_name": "Date Created",
                "type": DateOrNothingType,
                "desc": "Date coupon was created",
            },
        };
    }

    #! Creates the type
    constructor() {
        map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type)), Fields.pairIterator();
    }
}

#! Add-to-cart event data type for WooCommerce custom action webhooks
public class WooCommerceAddToCartEventDataType inherits HashDataType {
    public {
        #! Field definitions for add-to-cart events
        const Fields = {
            "action": {
                "display_name": "Action",
                "type": StringOrNothingType,
                "desc": "The action that triggered the webhook",
            },
            "arg": {
                "display_name": "Arg",
                "type": StringOrNothingType,
                "desc": "The cart item key (not a product ID)",
            },
        };
    }

    #! Creates the type
    constructor() {
        map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type)), Fields.pairIterator();
    }
}

} # namespace WooCommerceDataProvider
