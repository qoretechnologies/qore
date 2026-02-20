# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file TableauDataProviderDefs.qc defines constants and definitions for the TableauDataProvider module

/*  TableauDataProviderDefs.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the TableauDataProvider module
public namespace TableauDataProvider {
    #! Application name constant
    public const AppName = "Tableau";

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

    #! Event name for datasource created (webhook)
    public const EVENT_DATASOURCE_CREATED = "datasource-created";

    #! Event name for datasource deleted (webhook)
    public const EVENT_DATASOURCE_DELETED = "datasource-deleted";

    #! Event name for datasource updated (webhook)
    public const EVENT_DATASOURCE_UPDATED = "datasource-updated";

    #! Event name for datasource refresh started (webhook)
    public const EVENT_DATASOURCE_REFRESH_STARTED = "datasource-refresh-started";

    #! Event name for datasource refresh succeeded (webhook)
    public const EVENT_DATASOURCE_REFRESH_SUCCEEDED = "datasource-refresh-succeeded";

    #! Event name for datasource refresh failed (webhook)
    public const EVENT_DATASOURCE_REFRESH_FAILED = "datasource-refresh-failed";

    #! Event name for watching new datasources (polling)
    public const EVENT_WATCH_DATASOURCES = "watch-datasources";

    #! Event name for watching new projects (polling)
    public const EVENT_WATCH_PROJECTS = "watch-projects";

    # --- Tableau webhook event type strings ---

    #! Tableau webhook event: datasource created
    public const TABLEAU_EVENT_DATASOURCE_CREATED = "DatasourceCreated";

    #! Tableau webhook event: datasource deleted
    public const TABLEAU_EVENT_DATASOURCE_DELETED = "DatasourceDeleted";

    #! Tableau webhook event: datasource updated
    public const TABLEAU_EVENT_DATASOURCE_UPDATED = "DatasourceUpdated";

    #! Tableau webhook event: datasource refresh started
    public const TABLEAU_EVENT_DATASOURCE_REFRESH_STARTED = "DatasourceRefreshStarted";

    #! Tableau webhook event: datasource refresh succeeded
    public const TABLEAU_EVENT_DATASOURCE_REFRESH_SUCCEEDED = "DatasourceRefreshSucceeded";

    #! Tableau webhook event: datasource refresh failed
    public const TABLEAU_EVENT_DATASOURCE_REFRESH_FAILED = "DatasourceRefreshFailed";

    #! Map of internal event names to Tableau webhook event type strings
    public const EventNameToTableauType = {
        EVENT_DATASOURCE_CREATED: TABLEAU_EVENT_DATASOURCE_CREATED,
        EVENT_DATASOURCE_DELETED: TABLEAU_EVENT_DATASOURCE_DELETED,
        EVENT_DATASOURCE_UPDATED: TABLEAU_EVENT_DATASOURCE_UPDATED,
        EVENT_DATASOURCE_REFRESH_STARTED: TABLEAU_EVENT_DATASOURCE_REFRESH_STARTED,
        EVENT_DATASOURCE_REFRESH_SUCCEEDED: TABLEAU_EVENT_DATASOURCE_REFRESH_SUCCEEDED,
        EVENT_DATASOURCE_REFRESH_FAILED: TABLEAU_EVENT_DATASOURCE_REFRESH_FAILED,
    };

    #! Tableau logo in SVG format (loaded from file)
    public const TableauLogo = File::readTextFile(get_script_dir() + "/tableau-logo.svg");
}
