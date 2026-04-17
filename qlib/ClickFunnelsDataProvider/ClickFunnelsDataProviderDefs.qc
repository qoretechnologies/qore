# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ClickFunnelsDataProviderDefs.qc defines constants for the ClickFunnelsDataProvider module

/*  ClickFunnelsDataProviderDefs.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ClickFunnelsDataProvider module
public namespace ClickFunnelsDataProvider {
    #! Application name constant
    public const AppName = "ClickFunnels";

    #! Default page size for list operations
    public const DefaultPageSize = 20;

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

    # --- Event name constants ---

    #! Event name for new contact created
    public const EVENT_CONTACT_CREATED = "contact.created";

    #! Event name for contact updated
    public const EVENT_CONTACT_UPDATED = "contact.updated";

    #! Event name for new order created
    public const EVENT_ORDER_CREATED = "order.created";

    #! Event name for order updated
    public const EVENT_ORDER_UPDATED = "order.updated";

    #! Event name for order completed
    public const EVENT_ORDER_COMPLETED = "order.completed";

    #! Event name for new form submission created
    public const EVENT_FORM_SUBMISSION_CREATED = "form_submission.created";

    #! Event name for new enrollment created
    public const EVENT_ENROLLMENT_CREATED = "courses/enrollment.created";

    #! Event name for new fulfillment created
    public const EVENT_FULFILLMENT_CREATED = "fulfillment.created";

    #! ClickFunnels logo in SVG format (loaded from file)
    public const ClickFunnelsLogo = File::readTextFile(get_script_dir() + "/clickfunnels-logo.svg");
}
