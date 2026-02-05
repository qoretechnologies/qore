# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore UnleashedDataProvider module definition

/*  UnleashedDataProviderDefs.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the UnleashedDataProvider module
public namespace UnleashedDataProvider {
    #! Application name constant
    public const AppName = "Unleashed";

    #! Unleashed logo
    public const UnleashedLogo = File::readTextFile(get_script_dir() + "/unleashed-logo.svg");

    #! Default pagination page size
    public const DefaultPageSize = 200;

    #! Maximum pagination page size
    public const MaxPageSize = 1000;

    #! Default polling interval in seconds
    public const DefaultPollInterval = 120;

    #! Minimum polling interval in seconds
    public const MinPollInterval = 30;

    # ========== EVENT TYPES ==========

    #! Event type for new customer detection
    public const EVENT_NEW_CUSTOMER = "new-customer";

    #! Event type for new sales order detection
    public const EVENT_NEW_SALES_ORDER = "new-sales-order";

    # ========== ORDER STATUS VALUES ==========

    #! Sales order status: Open
    public const ORDER_STATUS_OPEN = "Open";

    #! Sales order status: Parked
    public const ORDER_STATUS_PARKED = "Parked";

    #! Sales order status: Backordered
    public const ORDER_STATUS_BACKORDERED = "Backordered";

    #! Sales order status: Placed
    public const ORDER_STATUS_PLACED = "Placed";

    #! Sales order status: Completed
    public const ORDER_STATUS_COMPLETED = "Completed";

    #! Sales order status: Deleted
    public const ORDER_STATUS_DELETED = "Deleted";

    # ========== CREDIT NOTE STATUS VALUES ==========

    #! Credit note status: Parked
    public const CREDIT_STATUS_PARKED = "Parked";

    #! Credit note status: Completed
    public const CREDIT_STATUS_COMPLETED = "Completed";

    # ========== STATUS LISTS ==========

    #! List of valid sales order statuses
    public const OrderStatuses = (
        ORDER_STATUS_OPEN,
        ORDER_STATUS_PARKED,
        ORDER_STATUS_BACKORDERED,
        ORDER_STATUS_PLACED,
        ORDER_STATUS_COMPLETED,
        ORDER_STATUS_DELETED,
    );

    #! List of valid credit note statuses
    public const CreditStatuses = (
        CREDIT_STATUS_PARKED,
        CREDIT_STATUS_COMPLETED,
    );
}
