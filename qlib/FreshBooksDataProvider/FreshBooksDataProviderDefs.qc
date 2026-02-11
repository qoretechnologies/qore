# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FreshBooksDataProviderDefs.qc defines constants and definitions for the FreshBooksDataProvider module

/*  FreshBooksDataProviderDefs.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the FreshBooksDataProvider module
public namespace FreshBooksDataProvider {
    #! Application name constant
    public const AppName = "FreshBooks";

    #! Event name for new invoice
    public const EVENT_INVOICE_CREATED = "invoice-created";

    #! Event name for new payment
    public const EVENT_PAYMENT_CREATED = "payment-created";

    #! Event name for new client
    public const EVENT_CLIENT_CREATED = "client-created";

    #! Event name for new expense
    public const EVENT_EXPENSE_CREATED = "expense-created";

    #! Event name for new estimate
    public const EVENT_ESTIMATE_CREATED = "estimate-created";

    #! FreshBooks webhook event name for invoice creation
    public const FB_EVENT_INVOICE_CREATE = "invoice.create";

    #! FreshBooks webhook event name for payment creation
    public const FB_EVENT_PAYMENT_CREATE = "payment.create";

    #! FreshBooks webhook event name for client creation
    public const FB_EVENT_CLIENT_CREATE = "client.create";

    #! FreshBooks webhook event name for expense creation
    public const FB_EVENT_EXPENSE_CREATE = "expense.create";

    #! FreshBooks webhook event name for estimate creation
    public const FB_EVENT_ESTIMATE_CREATE = "estimate.create";

    #! Mapping from internal event names to FreshBooks API event names
    public const EventNameMap = {
        EVENT_INVOICE_CREATED: FB_EVENT_INVOICE_CREATE,
        EVENT_PAYMENT_CREATED: FB_EVENT_PAYMENT_CREATE,
        EVENT_CLIENT_CREATED: FB_EVENT_CLIENT_CREATE,
        EVENT_EXPENSE_CREATED: FB_EVENT_EXPENSE_CREATE,
        EVENT_ESTIMATE_CREATED: FB_EVENT_ESTIMATE_CREATE,
    };

    #! Default page size for list operations
    public const DefaultPageSize = 100;

    #! Maximum page size for list operations
    public const MaxPageSize = 100;

    #! Default polling interval in seconds
    public const DefaultPollInterval = 60;

    #! Minimum polling interval in seconds
    public const MinPollInterval = 30;

    #! Maximum number of retries for rate-limited or transient error requests
    public const MaxRetries = 3;

    #! Set of retriable socket errors
    public const RetrySet = {
        "SOCKET-CONNECT-ERROR": True,
        "SOCKET-SSL-ERROR": True,
        "SOCKET-CLOSED": True,
        "SOCKET-TIMEOUT": True,
    };

    #! FreshBooks logo in SVG format (loaded from file)
    public const FreshBooksLogo = File::readTextFile(get_script_dir() + "/freshbooks-logo.svg");
}
