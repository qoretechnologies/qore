# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ZohoInvoiceDataProviderDefs.qc defines constants and definitions for the ZohoInvoiceDataProvider module

/*  ZohoInvoiceDataProviderDefs.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ZohoInvoiceDataProvider module
public namespace ZohoInvoiceDataProvider {
    #! Application name for Zoho Invoice
    public const AppName = "ZohoInvoice";

    #! Event name for new invoice
    public const EVENT_NEW_INVOICE = "new-invoice";

    #! Event name for new contact
    public const EVENT_NEW_CONTACT = "new-contact";

    #! Event name for new estimate
    public const EVENT_NEW_ESTIMATE = "new-estimate";

    #! Event name for new expense
    public const EVENT_NEW_EXPENSE = "new-expense";

    #! Event name for new item
    public const EVENT_NEW_ITEM = "new-item";

    #! Event name for new project
    public const EVENT_NEW_PROJECT = "new-project";

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

    #! Zoho Invoice logo in SVG format (loaded from file)
    public const ZohoInvoiceLogo = File::readTextFile(get_script_dir() + "/zohoinvoice-logo.svg");
}
