# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore CloseDataProvider module definition

/*  CloseTaskDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Close task record type
public class CloseTaskDataType inherits HashDataType {
    public {
        #! Field definitions for Close tasks
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The task ID",
                "desc": "The unique identifier of the task",
            },
            "_type": {
                "type": StringOrNothingType,
                "display_name": "Task Type",
                "short_desc": "The task type",
                "desc": "The type of the task (e.g., `lead`, `incoming_email`, `outgoing_call`)",
            },
            "lead_id": {
                "type": StringOrNothingType,
                "display_name": "Lead ID",
                "short_desc": "The associated lead ID",
                "desc": "The unique identifier of the lead this task is associated with",
            },
            "lead_name": {
                "type": StringOrNothingType,
                "display_name": "Lead Name",
                "short_desc": "The associated lead name",
                "desc": "The display name of the lead this task is associated with",
            },
            "text": {
                "type": StringOrNothingType,
                "display_name": "Text",
                "short_desc": "The task text",
                "desc": "The text description or body of the task",
            },
            "date": {
                "type": StringOrNothingType,
                "display_name": "Date",
                "short_desc": "The task due date",
                "desc": "The due date of the task (YYYY-MM-DD format)",
            },
            "is_complete": {
                "type": BoolOrNothingType,
                "display_name": "Is Complete",
                "short_desc": "Whether the task is complete",
                "desc": "Indicates whether the task has been marked as complete (`True` or `False`)",
            },
            "assigned_to": {
                "type": StringOrNothingType,
                "display_name": "Assigned To",
                "short_desc": "The assigned user ID",
                "desc": "The unique identifier of the user this task is assigned to",
            },
            "assigned_to_name": {
                "type": StringOrNothingType,
                "display_name": "Assigned To Name",
                "short_desc": "The assigned user name",
                "desc": "The display name of the user this task is assigned to",
            },
            "object_id": {
                "type": StringOrNothingType,
                "display_name": "Object ID",
                "short_desc": "The related object ID",
                "desc": "The unique identifier of the related object (e.g., lead, contact)",
            },
            "object_type": {
                "type": StringOrNothingType,
                "display_name": "Object Type",
                "short_desc": "The related object type",
                "desc": "The type of the related object (e.g., lead, contact)",
            },
            "organization_id": {
                "type": StringOrNothingType,
                "display_name": "Organization ID",
                "short_desc": "The organization ID",
                "desc": "The unique identifier of the organization this task belongs to",
            },
            "date_created": {
                "type": StringOrNothingType,
                "display_name": "Date Created",
                "short_desc": "When the task was created",
                "desc": "The timestamp when the task was created (ISO 8601 format)",
            },
            "date_updated": {
                "type": StringOrNothingType,
                "display_name": "Date Updated",
                "short_desc": "When the task was last updated",
                "desc": "The timestamp when the task was last updated (ISO 8601 format)",
            },
            "created_by": {
                "type": StringOrNothingType,
                "display_name": "Created By",
                "short_desc": "The user ID who created the task",
                "desc": "The unique identifier of the user who created this task",
            },
            "updated_by": {
                "type": StringOrNothingType,
                "display_name": "Updated By",
                "short_desc": "The user ID who last updated the task",
                "desc": "The unique identifier of the user who last updated this task",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace CloseDataProvider
