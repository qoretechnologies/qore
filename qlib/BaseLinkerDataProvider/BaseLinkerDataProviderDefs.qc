# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerDataProviderDefs.qc defines constants and definitions for the BaseLinkerDataProvider module

/*  BaseLinkerDataProviderDefs.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the BaseLinkerDataProvider module
public namespace BaseLinkerDataProvider {
    #! Application name
    public const AppName = "BaseLinker";

    #! Application display name (BaseLinker rebranded to Base.com in 2025; keep both
    #! names so users find it under the familiar "BaseLinker" brand)
    public const AppDisplayName = "BaseLinker (Base.com)";

    #! BaseLinker returns up to 100 orders per getOrders call
    public const OrdersPageSize = 100;

    #! BaseLinker returns up to 100 order returns per getOrderReturns call
    public const OrderReturnsPageSize = 100;

    #! BaseLinker returns up to 1000 inventory products per getInventoryProductsList call
    public const InventoryProductsPageSize = 1000;

    #! BaseLinker returns up to 100 journal log entries per getJournalList call
    public const JournalPageSize = 100;

    #! Default polling interval for watch data providers (seconds)
    public const DefaultPollInterval = 120;

    #! Minimum polling interval (seconds)
    public const MinPollInterval = 30;

    #! Maximum retries for rate-limited / transient errors
    public const MaxRetries = 3;

    #! Retriable socket-level errors
    public const RetrySet = {
        "SOCKET-CONNECT-ERROR": True,
        "SOCKET-SSL-ERROR": True,
        "SOCKET-CLOSED": True,
        "SOCKET-TIMEOUT": True,
    };

    # -------------------------------------------------------------------
    # Event name constants (kebab-case action names)
    # -------------------------------------------------------------------

    #! Event: order created (log_type 1)
    public const EVENT_ORDER_CREATED = "order-created";

    #! Event: order downloaded from shop (log_type 2)
    public const EVENT_ORDER_DOWNLOADED = "order-downloaded";

    #! Event: order paid (log_type 3)
    public const EVENT_ORDER_PAID = "order-paid";

    #! Event: order removed (log_type 4)
    public const EVENT_ORDER_REMOVED = "order-removed";

    #! Event: orders merged (log_type 5)
    public const EVENT_ORDERS_MERGED = "orders-merged";

    #! Event: order split (log_type 6)
    public const EVENT_ORDER_SPLIT = "order-split";

    #! Event: invoice issued (log_type 7)
    public const EVENT_INVOICE_ISSUED = "invoice-issued";

    #! Event: receipt issued (log_type 8)
    public const EVENT_RECEIPT_ISSUED = "receipt-issued";

    #! Event: courier package created (log_type 9)
    public const EVENT_PACKAGE_CREATED = "package-created";

    #! Event: courier package deleted (log_type 10)
    public const EVENT_PACKAGE_DELETED = "package-deleted";

    #! Event: delivery data edited (log_type 11)
    public const EVENT_DELIVERY_DATA_EDITED = "delivery-data-edited";

    #! Event: order product added (log_type 12)
    public const EVENT_ORDER_PRODUCT_ADDED = "order-product-added";

    #! Event: order product edited (log_type 13)
    public const EVENT_ORDER_PRODUCT_EDITED = "order-product-edited";

    #! Event: order product removed (log_type 14)
    public const EVENT_ORDER_PRODUCT_REMOVED = "order-product-removed";

    #! Event: blacklist entry added (log_type 15)
    public const EVENT_BLACKLIST_ADDED = "blacklist-added";

    #! Event: order data edited (log_type 16)
    public const EVENT_ORDER_DATA_EDITED = "order-data-edited";

    #! Event: order copied (log_type 17)
    public const EVENT_ORDER_COPIED = "order-copied";

    #! Event: order status changed (log_type 18)
    public const EVENT_ORDER_STATUS_CHANGED = "order-status-changed";

    #! Event: invoice deleted (log_type 19)
    public const EVENT_INVOICE_DELETED = "invoice-deleted";

    #! Event: receipt deleted (log_type 20)
    public const EVENT_RECEIPT_DELETED = "receipt-deleted";

    #! Event: invoice edited (log_type 21)
    public const EVENT_INVOICE_EDITED = "invoice-edited";

    #! Event: courier package status changed (log_type 22)
    public const EVENT_PACKAGE_STATUS_CHANGED = "package-status-changed";

    #! Mapping from BaseLinker numeric log_type codes to event name constants
    public const JournalLogTypeMap = {
        1: EVENT_ORDER_CREATED,
        2: EVENT_ORDER_DOWNLOADED,
        3: EVENT_ORDER_PAID,
        4: EVENT_ORDER_REMOVED,
        5: EVENT_ORDERS_MERGED,
        6: EVENT_ORDER_SPLIT,
        7: EVENT_INVOICE_ISSUED,
        8: EVENT_RECEIPT_ISSUED,
        9: EVENT_PACKAGE_CREATED,
        10: EVENT_PACKAGE_DELETED,
        11: EVENT_DELIVERY_DATA_EDITED,
        12: EVENT_ORDER_PRODUCT_ADDED,
        13: EVENT_ORDER_PRODUCT_EDITED,
        14: EVENT_ORDER_PRODUCT_REMOVED,
        15: EVENT_BLACKLIST_ADDED,
        16: EVENT_ORDER_DATA_EDITED,
        17: EVENT_ORDER_COPIED,
        18: EVENT_ORDER_STATUS_CHANGED,
        19: EVENT_INVOICE_DELETED,
        20: EVENT_RECEIPT_DELETED,
        21: EVENT_INVOICE_EDITED,
        22: EVENT_PACKAGE_STATUS_CHANGED,
    };

    #! Reverse mapping: event name to numeric log_type
    public const JournalEventToLogType = {
        EVENT_ORDER_CREATED: 1,
        EVENT_ORDER_DOWNLOADED: 2,
        EVENT_ORDER_PAID: 3,
        EVENT_ORDER_REMOVED: 4,
        EVENT_ORDERS_MERGED: 5,
        EVENT_ORDER_SPLIT: 6,
        EVENT_INVOICE_ISSUED: 7,
        EVENT_RECEIPT_ISSUED: 8,
        EVENT_PACKAGE_CREATED: 9,
        EVENT_PACKAGE_DELETED: 10,
        EVENT_DELIVERY_DATA_EDITED: 11,
        EVENT_ORDER_PRODUCT_ADDED: 12,
        EVENT_ORDER_PRODUCT_EDITED: 13,
        EVENT_ORDER_PRODUCT_REMOVED: 14,
        EVENT_BLACKLIST_ADDED: 15,
        EVENT_ORDER_DATA_EDITED: 16,
        EVENT_ORDER_COPIED: 17,
        EVENT_ORDER_STATUS_CHANGED: 18,
        EVENT_INVOICE_DELETED: 19,
        EVENT_RECEIPT_DELETED: 20,
        EVENT_INVOICE_EDITED: 21,
        EVENT_PACKAGE_STATUS_CHANGED: 22,
    };

    #! Known BaseLinker rate-limit error codes
    public const RateLimitErrorCodes = {
        "ERROR_RATE_LIMIT": True,
        "ERROR_API_RATE_LIMIT": True,
        "ERROR_TOO_MANY_REQUESTS": True,
    };

    #! BaseLinker logo in SVG format (loaded from file)
    public const BaseLinkerLogo = File::readTextFile(get_script_dir() + "/baselinker-logo.svg");
}
