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

    #! Allowed values for LINK_SUBJECT_TYPE field (notes)
    public const LinkSubjectTypeAllowedValues = (
        <AllowedValueInfo>{"value": "Contact", "display_name": "Contact"},
        <AllowedValueInfo>{"value": "Lead", "display_name": "Lead"},
        <AllowedValueInfo>{"value": "Opportunity", "display_name": "Opportunity"},
        <AllowedValueInfo>{"value": "Organisation", "display_name": "Organisation"},
        <AllowedValueInfo>{"value": "Project", "display_name": "Project"},
    );

    #! Common DPAT_FIND action options shared across list-* actions
    /** Pagination, filter and brief/count_total options shared by every list action.
        Resource-specific id options are added per-action.
    */
    public const CommonListActionOptions = {
        "skip": <ActionOptionInfo>{
            "display_name": "Skip",
            "short_desc": "Number of records to skip",
            "type": AbstractDataProviderTypeMap."*int",
            "desc": "Number of records to skip for pagination",
            "preselected": True,
        },
        "top": <ActionOptionInfo>{
            "display_name": "Top",
            "short_desc": "Maximum number of records to return",
            "type": AbstractDataProviderTypeMap."*int",
            "desc": "Maximum number of records to return per response (max 500)",
            "preselected": True,
        },
        "brief": <ActionOptionInfo>{
            "display_name": "Brief",
            "short_desc": "Return brief results",
            "type": AbstractDataProviderTypeMap."*bool",
            "desc": "If `True`, returns only a subset of fields for each record",
        },
        "count_total": <ActionOptionInfo>{
            "display_name": "Count Total",
            "short_desc": "Include total count in response",
            "type": AbstractDataProviderTypeMap."*bool",
            "desc": "If `True`, returns the total number of matching records in an "
                "`X-Total-Count` response header",
        },
        "updated_after_utc": <ActionOptionInfo>{
            "display_name": "Updated After (UTC)",
            "short_desc": "Only return records updated after this UTC timestamp",
            "type": AbstractDataProviderTypeMap."*date",
            "desc": "Only return records that were updated after the given UTC timestamp. "
                "Routes the request through Insightly's `/Search` endpoint. "
                "Format: ISO 8601, e.g. `2026-04-09T16:58:14Z`",
            "example_value": "2026-04-09T16:58:14Z",
        },
        "field_name": <ActionOptionInfo>{
            "display_name": "Field Name",
            "short_desc": "Filter by a standard or custom field name",
            "type": AbstractDataProviderTypeMap."*string",
            "desc": "Standard field name (e.g. `LEAD_RATING`) or custom field name "
                "(e.g. `MyCustomField__c`) to filter on. Must be paired with `field_value`. "
                "Routes the request through Insightly's `/Search` endpoint.",
            "example_value": "LEAD_RATING",
        },
        "field_value": <ActionOptionInfo>{
            "display_name": "Field Value",
            "short_desc": "Value to match for the named field",
            "type": AbstractDataProviderTypeMap."*string",
            "desc": "Value to match against `field_name`. Must be paired with `field_name`. "
                "Routes the request through Insightly's `/Search` endpoint.",
            "example_value": "5",
        },
    };

    #! Common DPAT_FIND_SINGLE action options shared across get-* actions
    /** Resource-specific id options are added per-action; this hash is empty by design --
        kept for symmetry with #CommonListActionOptions.
    */
    public const CommonSingleActionOptions = {};

    #! Allowed values for OBJECT_NAME field (links)
    public const ObjectNameAllowedValues = (
        <AllowedValueInfo>{"value": "Contact", "display_name": "Contact"},
        <AllowedValueInfo>{"value": "Email", "display_name": "Email"},
        <AllowedValueInfo>{"value": "Event", "display_name": "Event"},
        <AllowedValueInfo>{"value": "Lead", "display_name": "Lead"},
        <AllowedValueInfo>{"value": "Note", "display_name": "Note"},
        <AllowedValueInfo>{"value": "Opportunity", "display_name": "Opportunity"},
        <AllowedValueInfo>{"value": "Organisation", "display_name": "Organisation"},
        <AllowedValueInfo>{"value": "Project", "display_name": "Project"},
        <AllowedValueInfo>{"value": "Task", "display_name": "Task"},
    );

    #! Insightly logo in SVG format (loaded from file)
    public const InsightlyLogo = File::readTextFile(get_script_dir() + "/insightly-logo.svg");
}
