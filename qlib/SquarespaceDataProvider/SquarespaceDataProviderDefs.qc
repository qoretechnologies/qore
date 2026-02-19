# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SquarespaceDataProviderDefs.qc defines constants and definitions for the SquarespaceDataProvider module

/*  SquarespaceDataProviderDefs.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SquarespaceDataProvider module
public namespace SquarespaceDataProvider {
    #! Application name constant
    public const AppName = "Squarespace";

    #! Default page size for list operations (Squarespace max is 50)
    public const DefaultPageSize = 50;

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

    # --- API version path constants ---

    #! API version for orders endpoints
    public const OrdersApiVersion = "1.0";

    #! API version for products endpoints
    public const ProductsApiVersion = "v2";

    #! API version for inventory endpoints
    public const InventoryApiVersion = "1.0";

    #! API version for transactions endpoints
    public const TransactionsApiVersion = "1.0";

    #! API version for webhooks endpoints
    public const WebhooksApiVersion = "1.0";

    # --- Event name constants (used in triggers) ---

    #! Event name for order updated
    public const EVENT_ORDER_UPDATED = "order-updated";

    #! Event name for product updated
    public const EVENT_PRODUCT_UPDATED = "product-updated";

    #! Event name for transaction updated
    public const EVENT_TRANSACTION_UPDATED = "transaction-updated";

    #! Squarespace logo in SVG format (loaded from file)
    public const SquarespaceLogo = File::readTextFile(get_script_dir() + "/squarespace-logo.svg");
}
