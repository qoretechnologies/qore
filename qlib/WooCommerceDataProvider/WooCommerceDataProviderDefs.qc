# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WooCommerceDataProvider module definition

/*  WooCommerceDataProviderDefs.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! WooCommerce app name constant
public const AppName = "WooCommerce";

#! Default page size for pagination
public const DefaultPageSize = 100;

#! Maximum page size for WooCommerce API
public const MaxPageSize = 100;

# ---- Order Statuses ----

#! Order status: pending payment
public const ORDER_STATUS_PENDING = "pending";

#! Order status: processing
public const ORDER_STATUS_PROCESSING = "processing";

#! Order status: on hold
public const ORDER_STATUS_ON_HOLD = "on-hold";

#! Order status: completed
public const ORDER_STATUS_COMPLETED = "completed";

#! Order status: cancelled
public const ORDER_STATUS_CANCELLED = "cancelled";

#! Order status: refunded
public const ORDER_STATUS_REFUNDED = "refunded";

#! Order status: failed
public const ORDER_STATUS_FAILED = "failed";

#! Order status: trash
public const ORDER_STATUS_TRASH = "trash";

#! All supported order statuses
public const AllOrderStatuses = (
    ORDER_STATUS_PENDING,
    ORDER_STATUS_PROCESSING,
    ORDER_STATUS_ON_HOLD,
    ORDER_STATUS_COMPLETED,
    ORDER_STATUS_CANCELLED,
    ORDER_STATUS_REFUNDED,
    ORDER_STATUS_FAILED,
    ORDER_STATUS_TRASH,
);

#! Allowed values for order statuses
public const OrderStatusAllowedValues = map <AllowedValueInfo>{
    "display_name": $1,
    "short_desc": sprintf("Order status: %s", $1),
    "value": $1,
}, AllOrderStatuses;

# ---- Product Types ----

#! Product type: simple
public const PRODUCT_TYPE_SIMPLE = "simple";

#! Product type: grouped
public const PRODUCT_TYPE_GROUPED = "grouped";

#! Product type: external
public const PRODUCT_TYPE_EXTERNAL = "external";

#! Product type: variable
public const PRODUCT_TYPE_VARIABLE = "variable";

#! All supported product types
public const AllProductTypes = (
    PRODUCT_TYPE_SIMPLE,
    PRODUCT_TYPE_GROUPED,
    PRODUCT_TYPE_EXTERNAL,
    PRODUCT_TYPE_VARIABLE,
);

#! Allowed values for product types
public const ProductTypeAllowedValues = map <AllowedValueInfo>{
    "display_name": $1,
    "short_desc": sprintf("Product type: %s", $1),
    "value": $1,
}, AllProductTypes;

# ---- Product Statuses ----

#! All supported product statuses
public const AllProductStatuses = ("draft", "pending", "private", "publish");

#! Allowed values for product statuses
public const ProductStatusAllowedValues = map <AllowedValueInfo>{
    "display_name": $1,
    "short_desc": sprintf("Product status: %s", $1),
    "value": $1,
}, AllProductStatuses;

# ---- Stock Statuses ----

#! All supported stock statuses
public const AllStockStatuses = ("instock", "outofstock", "onbackorder");

#! Allowed values for stock statuses
public const StockStatusAllowedValues = map <AllowedValueInfo>{
    "display_name": $1,
    "short_desc": sprintf("Stock status: %s", $1),
    "value": $1,
}, AllStockStatuses;

# ---- Discount Types ----

#! All supported coupon discount types
public const AllDiscountTypes = ("percent", "fixed_cart", "fixed_product");

#! Allowed values for discount types
public const DiscountTypeAllowedValues = map <AllowedValueInfo>{
    "display_name": $1,
    "short_desc": sprintf("Discount type: %s", $1),
    "value": $1,
}, AllDiscountTypes;

# ---- Webhook Statuses ----

#! All supported webhook statuses
public const AllWebhookStatuses = ("active", "paused", "disabled");

#! Allowed values for webhook statuses
public const WebhookStatusAllowedValues = map <AllowedValueInfo>{
    "display_name": $1,
    "short_desc": sprintf("Webhook status: %s", $1),
    "value": $1,
}, AllWebhookStatuses;

# ---- Event Types (Built-in Webhook Topics) ----

public const EVENT_ORDER_CREATED = "order-created";
public const EVENT_ORDER_UPDATED = "order-updated";
public const EVENT_ORDER_DELETED = "order-deleted";
public const EVENT_ORDER_RESTORED = "order-restored";

public const EVENT_PRODUCT_CREATED = "product-created";
public const EVENT_PRODUCT_UPDATED = "product-updated";
public const EVENT_PRODUCT_DELETED = "product-deleted";
public const EVENT_PRODUCT_RESTORED = "product-restored";

public const EVENT_CUSTOMER_CREATED = "customer-created";
public const EVENT_CUSTOMER_UPDATED = "customer-updated";
public const EVENT_CUSTOMER_DELETED = "customer-deleted";

public const EVENT_COUPON_CREATED = "coupon-created";
public const EVENT_COUPON_UPDATED = "coupon-updated";
public const EVENT_COUPON_DELETED = "coupon-deleted";
public const EVENT_COUPON_RESTORED = "coupon-restored";

# ---- Event Types (Custom Action Triggers) ----

public const EVENT_PAYMENT_COMPLETE = "payment-complete";
public const EVENT_ORDER_STATUS_PENDING = "order-status-pending";
public const EVENT_ORDER_STATUS_PROCESSING = "order-status-processing";
public const EVENT_ORDER_STATUS_COMPLETED = "order-status-completed";
public const EVENT_ORDER_STATUS_ON_HOLD = "order-status-on-hold";
public const EVENT_ORDER_STATUS_REFUNDED = "order-status-refunded";
public const EVENT_ORDER_STATUS_FAILED = "order-status-failed";
public const EVENT_ORDER_STATUS_CANCELLED = "order-status-cancelled";
public const EVENT_ADD_TO_CART = "add-to-cart";
public const EVENT_LOW_STOCK = "low-stock";
public const EVENT_NO_STOCK = "no-stock";

# ---- Webhook Topic Mapping ----

#! Map of event types to WooCommerce webhook topics
public const EventToTopicMap = {
    EVENT_ORDER_CREATED: "order.created",
    EVENT_ORDER_UPDATED: "order.updated",
    EVENT_ORDER_DELETED: "order.deleted",
    EVENT_ORDER_RESTORED: "order.restored",
    EVENT_PRODUCT_CREATED: "product.created",
    EVENT_PRODUCT_UPDATED: "product.updated",
    EVENT_PRODUCT_DELETED: "product.deleted",
    EVENT_PRODUCT_RESTORED: "product.restored",
    EVENT_CUSTOMER_CREATED: "customer.created",
    EVENT_CUSTOMER_UPDATED: "customer.updated",
    EVENT_CUSTOMER_DELETED: "customer.deleted",
    EVENT_COUPON_CREATED: "coupon.created",
    EVENT_COUPON_UPDATED: "coupon.updated",
    EVENT_COUPON_DELETED: "coupon.deleted",
    EVENT_COUPON_RESTORED: "coupon.restored",
    EVENT_PAYMENT_COMPLETE: "action.woocommerce_payment_complete",
    EVENT_ORDER_STATUS_PENDING: "action.woocommerce_order_status_pending",
    EVENT_ORDER_STATUS_PROCESSING: "action.woocommerce_order_status_processing",
    EVENT_ORDER_STATUS_COMPLETED: "action.woocommerce_order_status_completed",
    EVENT_ORDER_STATUS_ON_HOLD: "action.woocommerce_order_status_on-hold",
    EVENT_ORDER_STATUS_REFUNDED: "action.woocommerce_order_status_refunded",
    EVENT_ORDER_STATUS_FAILED: "action.woocommerce_order_status_failed",
    EVENT_ORDER_STATUS_CANCELLED: "action.woocommerce_order_status_cancelled",
    EVENT_ADD_TO_CART: "action.woocommerce_add_to_cart",
    EVENT_LOW_STOCK: "action.woocommerce_low_stock",
    EVENT_NO_STOCK: "action.woocommerce_no_stock",
};

#! Map of custom action event types to their enrichment API paths
public const CustomActionEnrichmentMap = {
    EVENT_PAYMENT_COMPLETE: "orders",
    EVENT_ORDER_STATUS_PENDING: "orders",
    EVENT_ORDER_STATUS_PROCESSING: "orders",
    EVENT_ORDER_STATUS_COMPLETED: "orders",
    EVENT_ORDER_STATUS_ON_HOLD: "orders",
    EVENT_ORDER_STATUS_REFUNDED: "orders",
    EVENT_ORDER_STATUS_FAILED: "orders",
    EVENT_ORDER_STATUS_CANCELLED: "orders",
    EVENT_LOW_STOCK: "products",
    EVENT_NO_STOCK: "products",
    # EVENT_ADD_TO_CART has no enrichment (arg is cart_item_key, not resource ID)
};

#! All built-in event types (full payload, no enrichment needed)
public const AllBuiltInEventTypes = (
    EVENT_ORDER_CREATED,
    EVENT_ORDER_UPDATED,
    EVENT_ORDER_DELETED,
    EVENT_ORDER_RESTORED,
    EVENT_PRODUCT_CREATED,
    EVENT_PRODUCT_UPDATED,
    EVENT_PRODUCT_DELETED,
    EVENT_PRODUCT_RESTORED,
    EVENT_CUSTOMER_CREATED,
    EVENT_CUSTOMER_UPDATED,
    EVENT_CUSTOMER_DELETED,
    EVENT_COUPON_CREATED,
    EVENT_COUPON_UPDATED,
    EVENT_COUPON_DELETED,
    EVENT_COUPON_RESTORED,
);

#! All custom action event types (enrichment needed)
public const AllCustomActionEventTypes = (
    EVENT_PAYMENT_COMPLETE,
    EVENT_ORDER_STATUS_PENDING,
    EVENT_ORDER_STATUS_PROCESSING,
    EVENT_ORDER_STATUS_COMPLETED,
    EVENT_ORDER_STATUS_ON_HOLD,
    EVENT_ORDER_STATUS_REFUNDED,
    EVENT_ORDER_STATUS_FAILED,
    EVENT_ORDER_STATUS_CANCELLED,
    EVENT_ADD_TO_CART,
    EVENT_LOW_STOCK,
    EVENT_NO_STOCK,
);

#! All event types
public const AllEventTypes = AllBuiltInEventTypes + AllCustomActionEventTypes;

#! WooCommerce logo in SVG format (loaded from file)
public const WooCommerceLogo = File::readTextFile(get_script_dir() + "/woocommerce-logo.svg");

} # namespace WooCommerceDataProvider
