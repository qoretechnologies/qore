# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file InsightlyDataProviderDefs.qc defines constants and definitions for the InsightlyDataProvider module

/*  InsightlyDataProviderDefs.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the InsightlyDataProvider module
public namespace InsightlyDataProvider {
    #! Application name constant
    public const AppName = "Insightly";

    #! Default page size for list operations
    public const DefaultPageSize = 100;

    #! Maximum page size for list operations
    public const MaxPageSize = 500;

    #! Default polling interval in seconds
    public const DefaultPollInterval = 120;

    #! Minimum polling interval in seconds
    public const MinPollInterval = 60;

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

    #! Event name for new contact
    public const EVENT_NEW_CONTACT = "new-contact";

    #! Event name for new organisation
    public const EVENT_NEW_ORGANISATION = "new-organisation";

    #! Event name for new lead
    public const EVENT_NEW_LEAD = "new-lead";

    #! Event name for new opportunity
    public const EVENT_NEW_OPPORTUNITY = "new-opportunity";

    #! Event name for new task
    public const EVENT_NEW_TASK = "new-task";

    #! Allowed values for VISIBLE_TO field
    public const VisibleToAllowedValues = (
        <AllowedValueInfo>{"value": "EVERYONE", "display_name": "Everyone",
            "desc": "Visible to everyone"},
        <AllowedValueInfo>{"value": "OWNER", "display_name": "Owner Only",
            "desc": "Visible only to the owner"},
        <AllowedValueInfo>{"value": "TEAM", "display_name": "Team",
            "desc": "Visible to the owner's team"},
    );

    #! Insightly logo in SVG format (loaded from file)
    public const InsightlyLogo = File::readTextFile(get_script_dir() + "/insightly-logo.svg");
}
