# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KeapDataProviderDefs.qc defines constants and definitions for the KeapDataProvider module

/*  KeapDataProviderDefs.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the KeapDataProvider module
public namespace KeapDataProvider {
    #! Application name constant
    public const AppName = "Keap";

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

    #! Event name for new contact added
    public const EVENT_CONTACT_ADDED = "contact-added";

    #! Event name for contact updated
    public const EVENT_CONTACT_UPDATED = "contact-updated";

    #! Event name for tag applied to contact
    public const EVENT_TAG_APPLIED = "tag-applied";

    #! Event name for new opportunity added
    public const EVENT_OPPORTUNITY_ADDED = "opportunity-added";

    #! Event name for new order added
    public const EVENT_ORDER_ADDED = "order-added";

    #! Event name for new task created
    public const EVENT_TASK_CREATED = "task-created";

    #! Event name for new note added
    public const EVENT_NOTE_ADDED = "note-added";

    #! Keap logo in SVG format (loaded from file)
    public const KeapLogo = File::readTextFile(get_script_dir() + "/keap-logo.svg");
}
