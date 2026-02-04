# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore Cin7CoreDataProvider module definition

/*  Cin7CoreDataProviderDefs.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the Cin7CoreDataProvider module
public namespace Cin7CoreDataProvider {
    #! Application name constant
    public const AppName = "Cin7Core";

    #! Cin7Core logo
    public const Cin7CoreLogo = File::readTextFile(get_script_dir() + "/cin7core-logo.svg");

    #! Default pagination page size
    public const DefaultPageSize = 100;

    #! Maximum pagination page size
    public const MaxPageSize = 1000;

    #! Default polling interval in seconds
    public const DefaultPollInterval = 120;

    #! Minimum polling interval in seconds
    public const MinPollInterval = 30;

    # ========== EVENT TYPES ==========

    #! Event type for new payment detection
    public const EVENT_NEW_PAYMENT = "new-payment";

    #! Event type for new sale credit note authorized
    public const EVENT_NEW_SALE_CREDIT_NOTE_AUTHORIZED = "new-sale-credit-note-authorized";

    #! Event type for new sale invoice authorized
    public const EVENT_NEW_SALE_INVOICE_AUTHORIZED = "new-sale-invoice-authorized";

    #! Event type for sale shipment tracking number changed
    public const EVENT_SALE_TRACKING_CHANGED = "sale-tracking-changed";

    #! Event type for sale voided
    public const EVENT_SALE_VOIDED = "sale-voided";

    #! Event type for stock level update
    public const EVENT_STOCK_LEVEL_UPDATE = "stock-level-update";

    #! Event type for customer updated
    public const EVENT_CUSTOMER_UPDATED = "customer-updated";

    #! Event type for supplier updated
    public const EVENT_SUPPLIER_UPDATED = "supplier-updated";

    # ========== STATUS VALUES ==========

    #! Customer status: Active
    public const CUSTOMER_STATUS_ACTIVE = "Active";

    #! Customer status: Deprecated
    public const CUSTOMER_STATUS_DEPRECATED = "Deprecated";

    #! Sale status: Draft
    public const SALE_STATUS_DRAFT = "Draft";

    #! Sale status: Authorised
    public const SALE_STATUS_AUTHORISED = "Authorised";

    #! Sale status: Picking
    public const SALE_STATUS_PICKING = "Picking";

    #! Sale status: Packed
    public const SALE_STATUS_PACKED = "Packed";

    #! Sale status: Shipped
    public const SALE_STATUS_SHIPPED = "Shipped";

    #! Sale status: Voided
    public const SALE_STATUS_VOIDED = "Voided";

    #! Sale status: Completed
    public const SALE_STATUS_COMPLETED = "Completed";

    #! Invoice status: Draft
    public const INVOICE_STATUS_DRAFT = "DRAFT";

    #! Invoice status: Authorised
    public const INVOICE_STATUS_AUTHORISED = "AUTHORISED";

    #! Invoice status: Void
    public const INVOICE_STATUS_VOID = "VOID";

    #! Stock adjustment status: Draft
    public const STOCK_ADJUSTMENT_STATUS_DRAFT = "DRAFT";

    #! Stock adjustment status: Authorised
    public const STOCK_ADJUSTMENT_STATUS_AUTHORISED = "AUTHORISED";

    #! Stock adjustment status: Void
    public const STOCK_ADJUSTMENT_STATUS_VOID = "VOID";

    #! Fulfilment status: NOT AVAILABLE
    public const FULFILMENT_STATUS_NOT_AVAILABLE = "NOT AVAILABLE";

    #! Fulfilment status: PICK
    public const FULFILMENT_STATUS_PICK = "PICK";

    #! Fulfilment status: PACK
    public const FULFILMENT_STATUS_PACK = "PACK";

    #! Fulfilment status: SHIP
    public const FULFILMENT_STATUS_SHIP = "SHIP";

    #! Fulfilment status: FULFILLED
    public const FULFILMENT_STATUS_FULFILLED = "FULFILLED";

    # ========== STATUS LISTS ==========

    #! List of valid customer statuses
    public const CustomerStatuses = (
        CUSTOMER_STATUS_ACTIVE,
        CUSTOMER_STATUS_DEPRECATED,
    );

    #! List of valid sale statuses
    public const SaleStatuses = (
        SALE_STATUS_DRAFT,
        SALE_STATUS_AUTHORISED,
        SALE_STATUS_PICKING,
        SALE_STATUS_PACKED,
        SALE_STATUS_SHIPPED,
        SALE_STATUS_VOIDED,
        SALE_STATUS_COMPLETED,
    );

    #! List of valid invoice statuses
    public const InvoiceStatuses = (
        INVOICE_STATUS_DRAFT,
        INVOICE_STATUS_AUTHORISED,
        INVOICE_STATUS_VOID,
    );

    #! List of valid stock adjustment statuses
    public const StockAdjustmentStatuses = (
        STOCK_ADJUSTMENT_STATUS_DRAFT,
        STOCK_ADJUSTMENT_STATUS_AUTHORISED,
        STOCK_ADJUSTMENT_STATUS_VOID,
    );

    #! List of valid fulfilment statuses
    public const FulfilmentStatuses = (
        FULFILMENT_STATUS_NOT_AVAILABLE,
        FULFILMENT_STATUS_PICK,
        FULFILMENT_STATUS_PACK,
        FULFILMENT_STATUS_SHIP,
        FULFILMENT_STATUS_FULFILLED,
    );
}
