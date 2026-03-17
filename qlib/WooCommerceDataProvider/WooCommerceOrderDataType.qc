# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WooCommerceDataProvider module definition

/*  WooCommerceOrderDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! WooCommerce order data type
public class WooCommerceOrderDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "id": {
                "display_name": "ID",
                "short_desc": "Unique identifier for the order",
                "type": IntOrNothingType,
                "desc": "The unique identifier for the order",
            },
            "parent_id": {
                "display_name": "Parent ID",
                "short_desc": "Parent order ID",
                "type": IntOrNothingType,
                "desc": "The parent order ID, if applicable",
            },
            "number": {
                "display_name": "Number",
                "short_desc": "Order number",
                "type": StringOrNothingType,
                "desc": "The order number",
            },
            "order_key": {
                "display_name": "Order Key",
                "short_desc": "Unique order key",
                "type": StringOrNothingType,
                "desc": "The unique order key used for identification",
            },
            "created_via": {
                "display_name": "Created Via",
                "short_desc": "Source that created the order",
                "type": StringOrNothingType,
                "desc": "The source that created the order (e.g., `checkout`, `rest-api`)",
            },
            "status": {
                "display_name": "Status",
                "short_desc": "Order status",
                "type": StringOrNothingType,
                "desc": "Order status: `pending`, `processing`, `on-hold`, `completed`, `cancelled`, "
                    "`refunded`, `failed`, `trash`",
            },
            "currency": {
                "display_name": "Currency",
                "short_desc": "Currency code",
                "type": StringOrNothingType,
                "desc": "The three-letter ISO currency code for the order (e.g., `USD`, `EUR`)",
            },
            "date_created": {
                "display_name": "Date Created",
                "short_desc": "Date the order was created",
                "type": DateOrNothingType,
                "desc": "The date and time the order was created",
            },
            "date_modified": {
                "display_name": "Date Modified",
                "short_desc": "Date the order was last modified",
                "type": DateOrNothingType,
                "desc": "The date and time the order was last modified",
            },
            "discount_total": {
                "display_name": "Discount Total",
                "short_desc": "Total discount amount",
                "type": StringOrNothingType,
                "desc": "The total discount amount for the order",
            },
            "shipping_total": {
                "display_name": "Shipping Total",
                "short_desc": "Total shipping amount",
                "type": StringOrNothingType,
                "desc": "The total shipping amount for the order",
            },
            "total": {
                "display_name": "Total",
                "short_desc": "Order total",
                "type": StringOrNothingType,
                "desc": "The grand total for the order",
            },
            "total_tax": {
                "display_name": "Total Tax",
                "short_desc": "Total tax amount",
                "type": StringOrNothingType,
                "desc": "The total tax amount for the order",
            },
            "customer_id": {
                "display_name": "Customer ID",
                "short_desc": "Customer identifier",
                "type": IntOrNothingType,
                "desc": "The unique identifier for the customer associated with this order; `0` for "
                    "guest orders",
            },
            "customer_note": {
                "display_name": "Customer Note",
                "short_desc": "Note left by the customer",
                "type": StringOrNothingType,
                "desc": "A note left by the customer during checkout",
            },
            "billing": {
                "display_name": "Billing",
                "short_desc": "Billing address data",
                "type": AutoHashOrNothingType,
                "desc": "Billing address details for the order",
            },
            "shipping": {
                "display_name": "Shipping",
                "short_desc": "Shipping address data",
                "type": AutoHashOrNothingType,
                "desc": "Shipping address details for the order",
            },
            "payment_method": {
                "display_name": "Payment Method",
                "short_desc": "Payment method identifier",
                "type": StringOrNothingType,
                "desc": "The payment method identifier (e.g., `bacs`, `paypal`)",
            },
            "payment_method_title": {
                "display_name": "Payment Method Title",
                "short_desc": "Payment method display name",
                "type": StringOrNothingType,
                "desc": "The human-readable payment method title",
            },
            "set_paid": {
                "display_name": "Set Paid",
                "short_desc": "Whether the order is paid",
                "type": SoftBoolOrNothingType,
                "desc": "If `True`, the order is marked as paid; used during order creation",
            },
            "transaction_id": {
                "display_name": "Transaction ID",
                "short_desc": "Payment transaction identifier",
                "type": StringOrNothingType,
                "desc": "The unique transaction ID from the payment gateway",
            },
            "line_items": {
                "display_name": "Line Items",
                "short_desc": "Order line items",
                "type": SoftListOrNothingType,
                "desc": "List of line items (products) in the order",
            },
            "shipping_lines": {
                "display_name": "Shipping Lines",
                "short_desc": "Shipping line items",
                "type": SoftListOrNothingType,
                "desc": "List of shipping lines applied to the order",
            },
            "fee_lines": {
                "display_name": "Fee Lines",
                "short_desc": "Fee line items",
                "type": SoftListOrNothingType,
                "desc": "List of fee lines applied to the order",
            },
            "coupon_lines": {
                "display_name": "Coupon Lines",
                "short_desc": "Coupon line items",
                "type": SoftListOrNothingType,
                "desc": "List of coupon lines applied to the order",
            },
            "refunds": {
                "display_name": "Refunds",
                "short_desc": "Order refunds",
                "type": SoftListOrNothingType,
                "desc": "List of refunds applied to the order",
            },
            "meta_data": {
                "display_name": "Meta Data",
                "short_desc": "Order metadata",
                "type": SoftListOrNothingType,
                "desc": "List of metadata key-value pairs associated with the order",
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
