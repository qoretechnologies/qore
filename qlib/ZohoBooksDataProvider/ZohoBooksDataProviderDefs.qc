# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ZohoBooksDataProviderDefs.qc defines constants and definitions for the ZohoBooksDataProvider module

/*  ZohoBooksDataProviderDefs.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ZohoBooksDataProvider module
public namespace ZohoBooksDataProvider {
    #! Application name for Zoho Books
    public const AppName = "ZohoBooks";

    #! Event name for new invoice
    public const EVENT_NEW_INVOICE = "new-invoice";

    #! Event name for new bill
    public const EVENT_NEW_BILL = "new-bill";

    #! Event name for new payment
    public const EVENT_NEW_PAYMENT = "new-payment";

    #! Event name for new customer
    public const EVENT_NEW_CUSTOMER = "new-customer";

    #! Event name for new expense
    public const EVENT_NEW_EXPENSE = "new-expense";

    #! Event name for new sales order
    public const EVENT_NEW_SALESORDER = "new-salesorder";

    #! Event name for new estimate
    public const EVENT_NEW_ESTIMATE = "new-estimate";

    #! Event name for new item
    public const EVENT_NEW_ITEM = "new-item";

    #! Event name for new purchase order
    public const EVENT_NEW_PURCHASEORDER = "new-purchaseorder";

    #! Event name for new vendor
    public const EVENT_NEW_VENDOR = "new-vendor";

    #! Default page size for list operations
    public const DefaultPageSize = 200;

    #! Maximum page size for list operations
    public const MaxPageSize = 200;

    #! Default polling interval in seconds
    public const DefaultPollInterval = 120;

    #! Minimum polling interval in seconds
    public const MinPollInterval = 30;

    #! Maximum number of retries for rate-limited requests
    public const MaxRetries = 5;

    #! Set of retriable socket errors
    public const RetrySet = {
        "SOCKET-SSL-ERROR": True,
        "SOCKET-CLOSED": True,
    };

    #! Zoho Books logo in SVG format (loaded from file)
    public const ZohoBooksLogo = File::readTextFile(get_script_dir() + "/zohobooks-logo.svg");
}
