# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file CloseDataProviderDefs.qc defines constants and definitions for the CloseDataProvider module

/*  CloseDataProviderDefs.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the CloseDataProvider module
public namespace CloseDataProvider {
    #! Application name constant
    public const AppName = "Close";

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

    #! Event name for new lead created
    public const EVENT_LEAD_CREATED = "lead-created";

    #! Event name for lead updated
    public const EVENT_LEAD_UPDATED = "lead-updated";

    #! Event name for lead deleted
    public const EVENT_LEAD_DELETED = "lead-deleted";

    #! Event name for new contact created
    public const EVENT_CONTACT_CREATED = "contact-created";

    #! Event name for contact updated
    public const EVENT_CONTACT_UPDATED = "contact-updated";

    #! Event name for contact deleted
    public const EVENT_CONTACT_DELETED = "contact-deleted";

    #! Event name for new opportunity created
    public const EVENT_OPPORTUNITY_CREATED = "opportunity-created";

    #! Event name for opportunity updated
    public const EVENT_OPPORTUNITY_UPDATED = "opportunity-updated";

    #! Event name for opportunity deleted
    public const EVENT_OPPORTUNITY_DELETED = "opportunity-deleted";

    # --- Close event log object_type/action constants ---

    #! Close object type: lead
    public const CLOSE_OBJECT_LEAD = "lead";

    #! Close object type: contact
    public const CLOSE_OBJECT_CONTACT = "contact";

    #! Close object type: opportunity
    public const CLOSE_OBJECT_OPPORTUNITY = "opportunity";

    #! Close action: created
    public const CLOSE_ACTION_CREATED = "created";

    #! Close action: updated
    public const CLOSE_ACTION_UPDATED = "updated";

    #! Close action: deleted
    public const CLOSE_ACTION_DELETED = "deleted";

    #! Close CRM logo in SVG format (loaded from file)
    public const CloseLogo = File::readTextFile(get_script_dir() + "/close-logo.svg");
}
