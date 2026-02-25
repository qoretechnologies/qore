# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BigCommerceDataProviderDefs.qc defines constants and definitions for the BigCommerceDataProvider module

/*  BigCommerceDataProviderDefs.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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
    #! Application name constant
    public const AppName = "BigCommerce";

    #! Default page size for list operations
    public const DefaultPageSize = 50;

    #! Maximum page size for BigCommerce API
    public const MaxPageSize = 250;

    #! Default polling interval in seconds
    public const DefaultPollInterval = 60;

    #! Minimum polling interval in seconds
    public const MinPollInterval = 30;

    #! Maximum number of seen IDs to track for deduplication
    public const MaxSeenIds = 10000;

    #! Maximum number of retries for rate-limited or transient error requests
    public const MaxRetries = 3;

    #! Set of retriable socket errors
    public const RetrySet = {
        "SOCKET-CONNECT-ERROR": True,
        "SOCKET-SSL-ERROR": True,
        "SOCKET-CLOSED": True,
        "SOCKET-TIMEOUT": True,
    };

    # --- Event name constants (used in triggers) ---

    #! Event name for abandoned cart
    public const EVENT_CART_ABANDONED = "cart-abandoned";

    #! Event name for new cart
    public const EVENT_CART_CREATED = "cart-created";

    #! Event name for new customer
    public const EVENT_CUSTOMER_CREATED = "customer-created";

    #! Event name for new customer address
    public const EVENT_CUSTOMER_ADDRESS_CREATED = "customer-address-created";

    #! Event name for new order
    public const EVENT_ORDER_CREATED = "order-created";

    #! Event name for new product
    public const EVENT_PRODUCT_CREATED = "product-created";

    #! Event name for new shipment
    public const EVENT_SHIPMENT_CREATED = "shipment-created";

    #! Event name for updated customer
    public const EVENT_CUSTOMER_UPDATED = "customer-updated";

    #! Event name for updated customer address
    public const EVENT_CUSTOMER_ADDRESS_UPDATED = "customer-address-updated";

    #! Event name for updated order
    public const EVENT_ORDER_UPDATED = "order-updated";

    #! Event name for updated order status
    public const EVENT_ORDER_STATUS_UPDATED = "order-status-updated";

    #! BigCommerce logo in SVG format (loaded from file)
    public const BigCommerceLogo = File::readTextFile(get_script_dir() + "/bigcommerce-logo.svg");
}
