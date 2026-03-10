# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ApolloDataProvider module definition

/*  ApolloTaskDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ApolloDataProvider module
public namespace ApolloDataProvider {

#! Apollo task record type
public class ApolloTaskDataType inherits HashDataType {
    public {
        #! Field definitions for Apollo tasks
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "Task ID",
                "desc": "The unique identifier of the task",
            },
            "type": {
                "type": StringOrNothingType,
                "display_name": "Type",
                "short_desc": "Task type",
                "desc": "The type of the task",
            },
            "priority": {
                "type": StringOrNothingType,
                "display_name": "Priority",
                "short_desc": "Priority level",
                "desc": "The priority level of the task",
            },
            "status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "Task status",
                "desc": "The current status of the task",
            },
            "note": {
                "type": StringOrNothingType,
                "display_name": "Note",
                "short_desc": "Task description/note",
                "desc": "The description or note for the task",
            },
            "due_date": {
                "type": DateOrNothingType,
                "display_name": "Due Date",
                "short_desc": "Due date",
                "desc": "The due date of the task",
            },
            "user_id": {
                "type": StringOrNothingType,
                "display_name": "User ID",
                "short_desc": "Assigned user ID",
                "desc": "The unique identifier of the user assigned to this task",
            },
            "contact_ids": {
                "type": AutoListOrNothingType,
                "display_name": "Contact IDs",
                "short_desc": "Related contact IDs",
                "desc": "The list of contact IDs related to this task",
            },
            "account_ids": {
                "type": AutoListOrNothingType,
                "display_name": "Account IDs",
                "short_desc": "Related account IDs",
                "desc": "The list of account IDs related to this task",
            },
            "created_at": {
                "type": DateOrNothingType,
                "display_name": "Created At",
                "short_desc": "Created timestamp",
                "desc": "The timestamp when the task was created",
            },
            "completed_at": {
                "type": DateOrNothingType,
                "display_name": "Completed At",
                "short_desc": "Completed timestamp",
                "desc": "The timestamp when the task was completed",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace ApolloDataProvider
