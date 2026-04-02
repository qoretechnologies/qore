# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore FreshsalesDataProvider module definition

/*  FreshsalesTaskDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the FreshsalesDataProvider module
public namespace FreshsalesDataProvider {

#! Freshsales task record type
public class FreshsalesTaskDataType inherits HashDataType {
    public {
        #! Field definitions for Freshsales tasks
        const Fields = {
            "id": {
                "type": IntType,
                "display_name": "ID",
                "short_desc": "The task ID",
                "desc": "The unique identifier of the task",
            },
            "title": {
                "type": StringOrNothingType,
                "display_name": "Title",
                "short_desc": "Task title",
                "desc": "The title of the task",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "Task description",
                "desc": "A detailed description of the task",
            },
            "due_date": {
                "type": StringOrNothingType,
                "display_name": "Due Date",
                "short_desc": "Completion deadline",
                "desc": "The due date for the task",
            },
            "status": {
                "type": SoftIntOrNothingType,
                "display_name": "Status",
                "short_desc": "Task status",
                "desc": "The status of the task: `0` for open, `1` for completed",
                "allowed_values": (
                    <AllowedValueInfo>{"value": 0, "display_name": "Open"},
                    <AllowedValueInfo>{"value": 1, "display_name": "Completed"},
                ),
            },
            "owner_id": {
                "type": IntOrNothingType,
                "display_name": "Owner ID",
                "short_desc": "Assigned user ID",
                "desc": "The ID of the user assigned to this task",
            },
            "targetable_id": {
                "type": IntOrNothingType,
                "display_name": "Target ID",
                "short_desc": "Associated record ID",
                "desc": "The ID of the contact, account, or deal associated with this task",
            },
            "targetable_type": {
                "type": StringOrNothingType,
                "display_name": "Target Type",
                "short_desc": "Associated record type",
                "desc": "The type of record associated with this task (e.g., `Contact`, "
                    "`SalesAccount`, `Deal`)",
            },
            "outcome_id": {
                "type": IntOrNothingType,
                "display_name": "Outcome ID",
                "short_desc": "Task outcome",
                "desc": "The ID of the task outcome",
            },
            "task_type_id": {
                "type": IntOrNothingType,
                "display_name": "Task Type ID",
                "short_desc": "Task type",
                "desc": "The ID of the task type",
            },
            "creater_id": {
                "type": IntOrNothingType,
                "display_name": "Creator ID",
                "short_desc": "Creator user ID",
                "desc": "The ID of the user who created the task",
            },
            "created_at": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "desc": "The timestamp when the task was created",
            },
            "updated_at": {
                "type": StringOrNothingType,
                "display_name": "Updated At",
                "short_desc": "Last update timestamp",
                "desc": "The timestamp when the task was last updated",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Freshsales task request data type for create operations
public class FreshsalesCreateTaskRequestDataType inherits HashDataType {
    public {
        #! Field definitions for creating a task
        const Fields = {
            "title": {
                "type": StringType,
                "display_name": "Title",
                "required": True,
                "short_desc": "Task title",
                "desc": "The title of the task",
                "example_value": "Follow up with client",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "Task description",
                "desc": "A detailed description of the task",
                "preselected": True,
            },
            "due_date": {
                "type": StringOrNothingType,
                "display_name": "Due Date",
                "short_desc": "Completion deadline",
                "desc": "The due date for the task (format: `YYYY-MM-DD`)",
                "preselected": True,
            },
            "status": {
                "type": SoftIntOrNothingType,
                "display_name": "Status",
                "short_desc": "Task status",
                "desc": "The status of the task: `0` for open, `1` for completed",
                "allowed_values": (
                    <AllowedValueInfo>{"value": 0, "display_name": "Open"},
                    <AllowedValueInfo>{"value": 1, "display_name": "Completed"},
                ),
            },
            "owner_id": {
                "type": SoftIntOrNothingType,
                "display_name": "Owner ID",
                "short_desc": "Assigned user ID",
                "desc": "The ID of the user to assign to this task",
                "ref_data": "owners",
            },
            "targetable_id": {
                "type": SoftIntOrNothingType,
                "display_name": "Target ID",
                "short_desc": "Associated record ID",
                "desc": "The ID of the contact, account, or deal to associate",
                "preselected": True,
            },
            "targetable_type": {
                "type": StringOrNothingType,
                "display_name": "Target Type",
                "short_desc": "Associated record type",
                "desc": "The type of record to associate: `Contact`, `SalesAccount`, or `Deal`",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "Contact", "display_name": "Contact"},
                    <AllowedValueInfo>{"value": "SalesAccount", "display_name": "Account"},
                    <AllowedValueInfo>{"value": "Deal", "display_name": "Deal"},
                ),
            },
            "outcome_id": {
                "type": SoftIntOrNothingType,
                "display_name": "Outcome ID",
                "short_desc": "Task outcome",
                "desc": "The ID of the task outcome",
                "ref_data": "sales_activity_outcomes",
            },
            "task_type_id": {
                "type": SoftIntOrNothingType,
                "display_name": "Task Type ID",
                "short_desc": "Task type",
                "desc": "The ID of the task type",
                "ref_data": "sales_activity_types",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Freshsales task request data type for update operations
public class FreshsalesUpdateTaskRequestDataType inherits HashDataType {
    public {
        #! Field definitions for updating a task
        const Fields = {
            "id": {
                "type": SoftIntType,
                "display_name": "Task ID",
                "required": True,
                "short_desc": "The task ID to update",
                "desc": "The unique identifier of the task to update",
            },
        } + (map {$1.key: $1.value + {"required": False}},
            FreshsalesCreateTaskRequestDataType::Fields.pairIterator());
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Freshsales task request data type for delete operations
public class FreshsalesDeleteTaskRequestDataType inherits HashDataType {
    public {
        #! Field definitions for deleting a task
        const Fields = {
            "id": {
                "type": SoftIntType,
                "display_name": "Task ID",
                "required": True,
                "short_desc": "The task ID to delete",
                "desc": "The unique identifier of the task to delete",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace FreshsalesDataProvider
