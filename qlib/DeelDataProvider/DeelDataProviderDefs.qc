# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DeelDataProviderDefs.qc defines constants and definitions for the DeelDataProvider module

/*  DeelDataProviderDefs.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the DeelDataProvider module
public namespace DeelDataProvider {
    #! Application name constant
    public const AppName = "Deel";

    #! Default page size for list operations
    public const DefaultPageSize = 100;

    #! Default polling interval in seconds
    public const DefaultPollInterval = 60;

    #! Minimum polling interval in seconds
    public const MinPollInterval = 30;

    #! Maximum number of seen IDs to track for deduplication
    public const MaxSeenIds = 10000;

    #! Maximum number of retries for transient errors
    public const MaxRetries = 3;

    #! Set of retriable socket errors
    public const RetrySet = {
        "SOCKET-CONNECT-ERROR": True,
        "SOCKET-SSL-ERROR": True,
        "SOCKET-CLOSED": True,
        "SOCKET-TIMEOUT": True,
    };

    # --- Event name constants (used in triggers) ---

    #! Event name for contract created
    public const EVENT_CONTRACT_CREATED = "contract-created";

    #! Event name for contract status updated
    public const EVENT_CONTRACT_STATUS_UPDATED = "contract-status-updated";

    #! Event name for contract terminated
    public const EVENT_CONTRACT_TERMINATED = "contract-terminated";

    #! Event name for invoice adjustment created
    public const EVENT_ADJUSTMENT_CREATED = "invoice-adjustment-created";

    #! Event name for invoice adjustment reviewed
    public const EVENT_ADJUSTMENT_REVIEWED = "invoice-adjustment-reviewed";

    #! Event name for timesheet created
    public const EVENT_TIMESHEET_CREATED = "timesheet-created";

    #! Event name for timesheet reviewed
    public const EVENT_TIMESHEET_REVIEWED = "timesheet-reviewed";

    #! Event name for time-off request created
    public const EVENT_TIME_OFF_CREATED = "time-off-created";

    #! Event name for time-off request updated
    public const EVENT_TIME_OFF_UPDATED = "time-off-updated";

    #! Event name for time-off request reviewed
    public const EVENT_TIME_OFF_REVIEWED = "time-off-reviewed";

    # --- Deel API event type constants ---

    #! Deel webhook event type: contract created
    public const DEEL_EVENT_CONTRACT_CREATED = "contract.created";

    #! Deel webhook event type: contract status updated
    public const DEEL_EVENT_CONTRACT_STATUS_UPDATED = "contract.status_updated";

    #! Deel webhook event type: contract terminated
    public const DEEL_EVENT_CONTRACT_TERMINATED = "contract.terminated";

    #! Deel webhook event type: adjustment created
    public const DEEL_EVENT_ADJUSTMENT_CREATED = "adjustment.created";

    #! Deel webhook event type: adjustment reviewed
    public const DEEL_EVENT_ADJUSTMENT_REVIEWED = "adjustment.reviewed";

    #! Deel webhook event type: timesheet submitted
    public const DEEL_EVENT_TIMESHEET_CREATED = "timesheet.submitted";

    #! Deel webhook event type: timesheet reviewed
    public const DEEL_EVENT_TIMESHEET_REVIEWED = "timesheet.reviewed";

    #! Deel webhook event type: time-off request created
    public const DEEL_EVENT_TIME_OFF_CREATED = "time_off.created";

    #! Deel webhook event type: time-off request updated
    public const DEEL_EVENT_TIME_OFF_UPDATED = "time_off.updated";

    #! Deel webhook event type: time-off request reviewed
    public const DEEL_EVENT_TIME_OFF_REVIEWED = "time_off.reviewed";

    #! Deel logo in SVG format (loaded from file)
    public const DeelLogo = File::readTextFile(get_script_dir() + "/deel-logo.svg");
}
