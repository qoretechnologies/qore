# -*- mode: qore; indent-tabs-mode: nil -*-
/*  InsightlyCalendarEventDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License - see COPYING for details
*/
public namespace InsightlyDataProvider {

public class InsightlyCalendarEventDataType inherits HashDataType {
    public {
        const Fields = {
            "EVENT_ID": {
                "type": SoftIntOrNothingType, "display_name": "Event ID",
                "short_desc": "The unique event identifier",
                "desc": "The unique identifier of the calendar event",
            },
            "TITLE": {
                "type": SoftStringOrNothingType, "display_name": "Title",
                "short_desc": "Event title", "desc": "The title of the calendar event",
            },
            "LOCATION": {
                "type": SoftStringOrNothingType, "display_name": "Location",
                "short_desc": "Event location", "desc": "The location of the event",
            },
            "DETAILS": {
                "type": SoftStringOrNothingType, "display_name": "Details",
                "short_desc": "Event details",
                "desc": "Detailed description of the calendar event",
            },
            "START_DATE_UTC": {
                "type": SoftDateOrNothingType, "display_name": "Start Date",
                "short_desc": "Event start date/time (UTC)",
                "desc": "The UTC start date and time of the event",
            },
            "END_DATE_UTC": {
                "type": SoftDateOrNothingType, "display_name": "End Date",
                "short_desc": "Event end date/time (UTC)",
                "desc": "The UTC end date and time of the event",
            },
            "ALL_DAY": {
                "type": SoftBoolOrNothingType, "display_name": "All Day",
                "short_desc": "Whether this is an all-day event",
                "desc": "If `True`, the event spans the entire day",
            },
            "PUBLICLY_VISIBLE": {
                "type": SoftBoolOrNothingType, "display_name": "Publicly Visible",
                "short_desc": "Whether the event is publicly visible",
                "desc": "If `True`, the event is visible to all users",
            },
            "REMINDER_DATE_UTC": {
                "type": SoftDateOrNothingType, "display_name": "Reminder Date",
                "short_desc": "Reminder timestamp (UTC)",
                "desc": "The UTC timestamp for the event reminder",
            },
            "REMINDER_SENT": {
                "type": SoftBoolOrNothingType, "display_name": "Reminder Sent",
                "short_desc": "Whether the reminder has been sent",
                "desc": "If `True`, the reminder has been sent. Read-only",
            },
            "OWNER_USER_ID": {
                "type": SoftIntOrNothingType, "display_name": "Owner User ID",
                "short_desc": "The owner user identifier",
                "desc": "The ID of the user who owns this event",
            },
            "EVENTLINKS": {
                "type": new ListDataType("InsightlyEventLinkList",
                    new InsightlyEventLinkDataType(), True),
                "display_name": "Event Links",
                "short_desc": "Related object links",
                "desc": "A list of links to related objects",
            },
            "DATE_CREATED_UTC": {
                "type": SoftDateOrNothingType, "display_name": "Date Created",
                "short_desc": "Creation timestamp (UTC)",
                "desc": "The UTC timestamp when the event was created",
            },
            "DATE_UPDATED_UTC": {
                "type": SoftDateOrNothingType, "display_name": "Date Updated",
                "short_desc": "Last update timestamp (UTC)",
                "desc": "The UTC timestamp when the event was last updated",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class InsightlyCreateCalendarEventRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "TITLE": {
                "type": SoftStringOrNothingType, "display_name": "Title",
                "short_desc": "Event title",
                "desc": "The title of the calendar event",
                "preselected": True,
                "example_value": "Team Meeting",
            },
            "LOCATION": {
                "type": SoftStringOrNothingType, "display_name": "Location",
                "short_desc": "Event location", "desc": "The location of the event",
                "preselected": True,
            },
            "DETAILS": {
                "type": SoftStringOrNothingType, "display_name": "Details",
                "short_desc": "Event details",
                "desc": "Detailed description of the calendar event",
                "preselected": True,
            },
            "START_DATE_UTC": {
                "type": SoftDateType, "display_name": "Start Date",
                "required": True,
                "short_desc": "Event start date/time (UTC)",
                "desc": "The UTC start date and time of the event (required)",
            },
            "END_DATE_UTC": {
                "type": SoftDateType, "display_name": "End Date",
                "required": True,
                "short_desc": "Event end date/time (UTC)",
                "desc": "The UTC end date and time of the event (required)",
            },
            "ALL_DAY": {
                "type": SoftBoolOrNothingType, "display_name": "All Day",
                "short_desc": "Whether this is an all-day event",
                "desc": "If `True`, the event spans the entire day",
            },
            "PUBLICLY_VISIBLE": {
                "type": SoftBoolOrNothingType, "display_name": "Publicly Visible",
                "short_desc": "Whether the event is publicly visible",
                "desc": "If `True`, the event is visible to all users",
            },
            "REMINDER_DATE_UTC": {
                "type": SoftDateOrNothingType, "display_name": "Reminder Date",
                "short_desc": "Reminder timestamp (UTC)",
                "desc": "The UTC timestamp for the event reminder",
            },
            "OWNER_USER_ID": {
                "type": SoftIntOrNothingType, "display_name": "Owner User ID",
                "short_desc": "The owner user identifier",
                "desc": "The ID of the user who owns this event",
                "ref_data": "users",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class InsightlyUpdateCalendarEventRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "EVENT_ID": {
                "type": SoftIntType, "display_name": "Event ID",
                "required": True, "short_desc": "The event ID to update",
                "desc": "The unique identifier of the calendar event to update",
                "ref_data": "calendar_events",
            },
        } + InsightlyDataProviderBase::makeFieldsOptional(
            InsightlyCreateCalendarEventRequestDataType::Fields);
    }
    constructor() { addQoreFields(Fields); }
}

public class InsightlyDeleteCalendarEventRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "EVENT_ID": {
                "type": SoftIntType, "display_name": "Event ID",
                "required": True, "short_desc": "The event ID to delete",
                "desc": "The unique identifier of the calendar event to delete",
                "ref_data": "calendar_events",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

} # namespace InsightlyDataProvider
