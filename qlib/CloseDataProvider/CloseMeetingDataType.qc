# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore CloseDataProvider module definition

/*  CloseMeetingDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Close meeting activity record type
public class CloseMeetingDataType inherits HashDataType {
    public {
        #! Field definitions for Close meetings
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "Meeting ID",
                "short_desc": "The meeting ID",
                "desc": "The unique identifier of the meeting activity",
            },
            "lead_id": {
                "type": StringOrNothingType,
                "display_name": "Lead ID",
                "short_desc": "The lead ID",
                "desc": "The unique identifier of the lead associated with this meeting",
            },
            "user_id": {
                "type": StringOrNothingType,
                "display_name": "User ID",
                "short_desc": "The user ID",
                "desc": "The unique identifier of the user who created the meeting",
            },
            "contact_id": {
                "type": StringOrNothingType,
                "display_name": "Contact ID",
                "short_desc": "The contact ID",
                "desc": "The unique identifier of the contact associated with this meeting",
            },
            "starts_at": {
                "type": StringOrNothingType,
                "display_name": "Starts At",
                "short_desc": "Meeting start time",
                "desc": "The start time of the meeting (ISO 8601 format)",
            },
            "ends_at": {
                "type": StringOrNothingType,
                "display_name": "Ends At",
                "short_desc": "Meeting end time",
                "desc": "The end time of the meeting (ISO 8601 format)",
            },
            "attendees": {
                "type": new SoftListDataType("Attendees", new CloseAttendeeDataType(), True),
                "display_name": "Attendees",
                "short_desc": "Meeting attendees",
                "desc": "A list of attendee objects for this meeting",
            },
            "location": {
                "type": StringOrNothingType,
                "display_name": "Location",
                "short_desc": "Meeting location",
                "desc": "The location of the meeting",
            },
            "status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "Meeting status",
                "desc": "The current status of the meeting",
            },
            "title": {
                "type": StringOrNothingType,
                "display_name": "Title",
                "short_desc": "Meeting title",
                "desc": "The title of the meeting",
            },
            "user_note_html": {
                "type": StringOrNothingType,
                "display_name": "Note HTML",
                "short_desc": "Meeting note in HTML",
                "desc": "The HTML note associated with the meeting",
            },
            "is_recurring": {
                "type": BoolOrNothingType,
                "display_name": "Is Recurring",
                "short_desc": "Whether the meeting is recurring",
                "desc": "Whether this meeting is part of a recurring series",
            },
            "date_created": {
                "type": StringOrNothingType,
                "display_name": "Date Created",
                "short_desc": "When the meeting was created",
                "desc": "The timestamp when the meeting was created (ISO 8601 format)",
            },
            "date_updated": {
                "type": StringOrNothingType,
                "display_name": "Date Updated",
                "short_desc": "When the meeting was last updated",
                "desc": "The timestamp when the meeting was last updated (ISO 8601 format)",
            },
            "organization_id": {
                "type": StringOrNothingType,
                "display_name": "Organization ID",
                "short_desc": "The organization ID",
                "desc": "The unique identifier of the organization this meeting belongs to",
            },
            "created_by": {
                "type": StringOrNothingType,
                "display_name": "Created By",
                "short_desc": "The user who created the meeting",
                "desc": "The unique identifier of the user who created this meeting",
            },
            "updated_by": {
                "type": StringOrNothingType,
                "display_name": "Updated By",
                "short_desc": "The user who last updated the meeting",
                "desc": "The unique identifier of the user who last updated this meeting",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace CloseDataProvider
