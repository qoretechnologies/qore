# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore KeapDataProvider module definition

/*  KeapTaskDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Keap task record type
public class KeapTaskDataType inherits HashDataType {
    private {
        #! Field definitions for Keap tasks
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The task ID",
                "desc": "The unique identifier of the task",
            },
            "title": {
                "type": StringOrNothingType,
                "display_name": "Title",
                "short_desc": "The task title",
                "desc": "The title of the task",
                "example_value": "Follow up with lead",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "The task description",
                "desc": "A detailed description of the task",
            },
            "contact": {
                "type": AutoHashOrNothingType,
                "display_name": "Contact",
                "short_desc": "The associated contact",
                "desc": "The contact associated with this task (hash with id, first_name, "
                    "last_name, email, etc.)",
            },
            "due_date": {
                "type": DateOrNothingType,
                "display_name": "Due Date",
                "short_desc": "The task due date",
                "desc": "The date when the task is due",
            },
            "priority": {
                "type": IntOrNothingType,
                "display_name": "Priority",
                "short_desc": "The task priority",
                "desc": "The priority level of the task (integer value)",
            },
            "type": {
                "type": StringOrNothingType,
                "display_name": "Type",
                "short_desc": "The task type",
                "desc": "The type of the task",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "Appointment"},
                    <AllowedValueInfo>{"value": "Call"},
                    <AllowedValueInfo>{"value": "Email"},
                    <AllowedValueInfo>{"value": "Fax"},
                    <AllowedValueInfo>{"value": "Letter"},
                    <AllowedValueInfo>{"value": "Other"},
                ),
            },
            "completed": {
                "type": BoolOrNothingType,
                "display_name": "Completed",
                "short_desc": "Whether the task is completed",
                "desc": "Indicates whether the task has been completed (`True` or `False`)",
            },
            "completion_date": {
                "type": DateOrNothingType,
                "display_name": "Completion Date",
                "short_desc": "When the task was completed",
                "desc": "The timestamp when the task was completed",
            },
            "creation_date": {
                "type": DateOrNothingType,
                "display_name": "Creation Date",
                "short_desc": "When the task was created",
                "desc": "The timestamp when the task was created",
            },
            "modification_date": {
                "type": DateOrNothingType,
                "display_name": "Modification Date",
                "short_desc": "When the task was last modified",
                "desc": "The timestamp when the task was last modified",
            },
            "user_id": {
                "type": IntOrNothingType,
                "display_name": "User ID",
                "short_desc": "The assigned user ID",
                "desc": "The ID of the user assigned to this task",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace KeapDataProvider
