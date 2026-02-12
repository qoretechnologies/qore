# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SquareDataProviderDefs.qc defines constants and definitions for the SquareDataProvider module

/*  SquareDataProviderDefs.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SquareDataProvider module
public namespace SquareDataProvider {
    #! Application name constant
    public const AppName = "Square";

    #! Default page size for list operations
    public const DefaultPageSize = 100;

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

    #! Event name for new customer
    public const EVENT_CUSTOMER_CREATED = "customer-created";

    #! Event name for new or updated order
    public const EVENT_ORDER_CREATED_OR_UPDATED = "order-created-or-updated";

    #! Event name for new or updated payment
    public const EVENT_PAYMENT_CREATED_OR_UPDATED = "payment-created-or-updated";

    #! Event name for new appointment (booking)
    public const EVENT_NEW_APPOINTMENT = "new-appointment";

    #! Event name for new catalog item
    public const EVENT_NEW_CATALOG_ITEM = "new-catalog-item";

    #! Event name for new invoice
    public const EVENT_NEW_INVOICE = "new-invoice";

    #! Event name for new events (generic event watcher)
    public const EVENT_NEW_EVENTS = "new-events";

    # --- Square webhook/event type constants ---

    #! Square event type: inventory count updated
    public const SQUARE_EVENT_INVENTORY_COUNT_UPDATED = "inventory.count.updated";

    #! Square event type: invoice created
    public const SQUARE_EVENT_INVOICE_CREATED = "invoice.created";

    #! Square event type: invoice updated
    public const SQUARE_EVENT_INVOICE_UPDATED = "invoice.updated";

    #! Square event type: payment created
    public const SQUARE_EVENT_PAYMENT_CREATED = "payment.created";

    #! Square event type: payment updated
    public const SQUARE_EVENT_PAYMENT_UPDATED = "payment.updated";

    #! Square event type: dispute created
    public const SQUARE_EVENT_DISPUTE_CREATED = "dispute.created";

    #! Square event type: dispute state changed
    public const SQUARE_EVENT_DISPUTE_STATE_CHANGED = "dispute.state.changed";

    #! Square event type: order created
    public const SQUARE_EVENT_ORDER_CREATED = "order.created";

    #! Square event type: order updated
    public const SQUARE_EVENT_ORDER_UPDATED = "order.updated";

    #! Square event type: labor shift created
    public const SQUARE_EVENT_LABOR_SHIFT_CREATED = "labor.shift.created";

    #! Square event type: labor shift updated
    public const SQUARE_EVENT_LABOR_SHIFT_UPDATED = "labor.shift.updated";

    #! Square event type: booking created
    public const SQUARE_EVENT_BOOKING_CREATED = "booking.created";

    #! Square event type: catalog version updated
    public const SQUARE_EVENT_CATALOG_VERSION_UPDATED = "catalog.version.updated";

    #! Square event type: customer created
    public const SQUARE_EVENT_CUSTOMER_CREATED = "customer.created";

    #! Square event type: customer updated
    public const SQUARE_EVENT_CUSTOMER_UPDATED = "customer.updated";

    #! Square event type: refund created
    public const SQUARE_EVENT_REFUND_CREATED = "refund.created";

    #! Square event type: refund updated
    public const SQUARE_EVENT_REFUND_UPDATED = "refund.updated";

    #! All supported Square event types for Watch New Events
    public const AllSquareEventTypes = (
        SQUARE_EVENT_INVENTORY_COUNT_UPDATED,
        SQUARE_EVENT_INVOICE_CREATED,
        SQUARE_EVENT_INVOICE_UPDATED,
        SQUARE_EVENT_PAYMENT_CREATED,
        SQUARE_EVENT_PAYMENT_UPDATED,
        SQUARE_EVENT_DISPUTE_CREATED,
        SQUARE_EVENT_DISPUTE_STATE_CHANGED,
        SQUARE_EVENT_ORDER_CREATED,
        SQUARE_EVENT_ORDER_UPDATED,
        SQUARE_EVENT_LABOR_SHIFT_CREATED,
        SQUARE_EVENT_LABOR_SHIFT_UPDATED,
        SQUARE_EVENT_BOOKING_CREATED,
        SQUARE_EVENT_CATALOG_VERSION_UPDATED,
        SQUARE_EVENT_CUSTOMER_CREATED,
        SQUARE_EVENT_CUSTOMER_UPDATED,
        SQUARE_EVENT_REFUND_CREATED,
        SQUARE_EVENT_REFUND_UPDATED,
    );

    #! Square logo in SVG format (loaded from file)
    public const SquareLogo = File::readTextFile(get_script_dir() + "/square-logo.svg");
}
