# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file CloseEventDataType.qc defines the data type for Close CRM event log entries

/*  CloseEventDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Data type for Close CRM event log entries
public class CloseEventDataType inherits HashDataType {
    public {
        #! Field definitions for Close event log entries
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "Event ID",
                "short_desc": "The event ID",
                "desc": "The unique identifier of the event log entry",
            },
            "object_type": {
                "type": StringOrNothingType,
                "display_name": "Object Type",
                "short_desc": "The type of object",
                "desc": "The type of object (lead, contact, opportunity)",
            },
            "object_id": {
                "type": StringOrNothingType,
                "display_name": "Object ID",
                "short_desc": "The ID of the affected object",
                "desc": "The unique identifier of the affected object",
            },
            "action": {
                "type": StringOrNothingType,
                "display_name": "Action",
                "short_desc": "The action performed",
                "desc": "The action performed (created, updated, deleted)",
            },
            "lead_id": {
                "type": StringOrNothingType,
                "display_name": "Lead ID",
                "short_desc": "The associated lead ID",
                "desc": "The unique identifier of the associated lead",
            },
            "user_id": {
                "type": StringOrNothingType,
                "display_name": "User ID",
                "short_desc": "The user who triggered the event",
                "desc": "The unique identifier of the user who triggered this event",
            },
            "date_created": {
                "type": StringOrNothingType,
                "display_name": "Date Created",
                "short_desc": "When the event was created",
                "desc": "The timestamp when the event was created (ISO 8601 format)",
            },
            "date_updated": {
                "type": StringOrNothingType,
                "display_name": "Date Updated",
                "short_desc": "When the event was last updated",
                "desc": "The timestamp when the event was last updated (ISO 8601 format)",
            },
            "data": {
                "type": AutoHashOrNothingType,
                "display_name": "Data",
                "short_desc": "The current state of the object",
                "desc": "The current state of the object after the event",
            },
            "previous_data": {
                "type": AutoHashOrNothingType,
                "display_name": "Previous Data",
                "short_desc": "The previous state (for updates)",
                "desc": "The previous state of the object before the event (populated for update "
                    "actions)",
            },
            "changed_fields": {
                "type": AutoListOrNothingType,
                "display_name": "Changed Fields",
                "short_desc": "List of fields that changed",
                "desc": "A list of field names that were changed by this event",
            },
            "meta": {
                "type": AutoHashOrNothingType,
                "display_name": "Meta",
                "short_desc": "Request metadata",
                "desc": "Additional metadata about the request that triggered this event",
            },
            "organization_id": {
                "type": StringOrNothingType,
                "display_name": "Organization ID",
                "short_desc": "The organization ID",
                "desc": "The unique identifier of the organization this event belongs to",
            },
            "request_id": {
                "type": StringOrNothingType,
                "display_name": "Request ID",
                "short_desc": "The request ID",
                "desc": "The unique identifier of the API request that triggered this event",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace CloseDataProvider
