# -*- mode: qore; indent-tabs-mode: nil -*-
/*  InsightlyTaskDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License - see COPYING for details
*/

public namespace InsightlyDataProvider {

public class InsightlyTaskDataType inherits HashDataType {
    public {
        const Fields = {
            "TASK_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Task ID",
                "short_desc": "The unique task identifier",
                "desc": "The unique identifier of the task",
            },
            "TITLE": {
                "type": SoftStringOrNothingType,
                "display_name": "Title",
                "short_desc": "Task title",
                "desc": "The title of the task",
            },
            "CATEGORY_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Category ID",
                "short_desc": "Task category identifier",
                "desc": "The ID of the task category",
            },
            "DUE_DATE": {
                "type": SoftDateOrNothingType,
                "display_name": "Due Date",
                "short_desc": "Task due date",
                "desc": "The date the task is due",
            },
            "COMPLETED_DATE_UTC": {
                "type": SoftDateOrNothingType,
                "display_name": "Completed Date",
                "short_desc": "Completion timestamp (UTC)",
                "desc": "The UTC timestamp when the task was completed. Read-only",
            },
            "COMPLETED": {
                "type": SoftBoolOrNothingType,
                "display_name": "Completed",
                "short_desc": "Whether the task is completed",
                "desc": "If `True`, the task has been completed",
            },
            "DETAILS": {
                "type": SoftStringOrNothingType,
                "display_name": "Details",
                "short_desc": "Task details",
                "desc": "Detailed description of the task",
            },
            "STATUS": {
                "type": SoftStringOrNothingType,
                "display_name": "Status",
                "short_desc": "Task status",
                "desc": "The current status of the task",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "Not Started", "display_name": "Not Started",
                        "desc": "Not yet started"},
                    <AllowedValueInfo>{"value": "In Progress", "display_name": "In Progress",
                        "desc": "Currently in progress"},
                    <AllowedValueInfo>{"value": "Completed", "display_name": "Completed",
                        "desc": "Successfully completed"},
                    <AllowedValueInfo>{"value": "Deferred", "display_name": "Deferred",
                        "desc": "Deferred to a later date"},
                    <AllowedValueInfo>{"value": "Waiting", "display_name": "Waiting",
                        "desc": "Waiting on something"},
                ),
            },
            "PERCENT_COMPLETE": {
                "type": SoftIntOrNothingType,
                "display_name": "Percent Complete",
                "short_desc": "Completion percentage",
                "desc": "The percentage of the task that has been completed (0-100)",
            },
            "START_DATE": {
                "type": SoftDateOrNothingType,
                "display_name": "Start Date",
                "short_desc": "Task start date",
                "desc": "The date the task was or will be started",
            },
            "PRIORITY": {
                "type": SoftIntOrNothingType,
                "display_name": "Priority",
                "short_desc": "Task priority (1-4)",
                "desc": "The priority level of the task",
                "allowed_values": (
                    <AllowedValueInfo>{"value": 1, "display_name": "Low",
                        "desc": "Low priority"},
                    <AllowedValueInfo>{"value": 2, "display_name": "Medium",
                        "desc": "Medium priority"},
                    <AllowedValueInfo>{"value": 3, "display_name": "High",
                        "desc": "High priority"},
                    <AllowedValueInfo>{"value": 4, "display_name": "Urgent",
                        "desc": "Urgent priority"},
                ),
            },
            "RESPONSIBLE_USER_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Responsible User ID",
                "short_desc": "Responsible user identifier",
                "desc": "The ID of the user responsible for this task",
            },
            "OWNER_USER_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Owner User ID",
                "short_desc": "The owner user identifier",
                "desc": "The ID of the user who owns this task",
            },
            "ASSIGNED_TEAM_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Assigned Team ID",
                "short_desc": "Assigned team identifier",
                "desc": "The ID of the team assigned to this task",
            },
            "PUBLICLY_VISIBLE": {
                "type": SoftBoolOrNothingType,
                "display_name": "Publicly Visible",
                "short_desc": "Whether the task is publicly visible",
                "desc": "If `True`, the task is visible to all users",
            },
            "REMINDER_DATE_UTC": {
                "type": SoftDateOrNothingType,
                "display_name": "Reminder Date",
                "short_desc": "Reminder timestamp (UTC)",
                "desc": "The UTC timestamp for the task reminder",
            },
            "REMINDER_SENT": {
                "type": SoftBoolOrNothingType,
                "display_name": "Reminder Sent",
                "short_desc": "Whether the reminder has been sent",
                "desc": "If `True`, the reminder has been sent. Read-only",
            },
            "RECURRENCE": {
                "type": SoftStringOrNothingType,
                "display_name": "Recurrence",
                "short_desc": "Recurrence pattern",
                "desc": "The recurrence pattern for the task",
            },
            "TASKLINKS": {
                "type": new ListDataType("InsightlyTaskTaskLinkList",
                    new InsightlyTaskLinkDataType(), True),
                "display_name": "Task Links",
                "short_desc": "Related object links",
                "desc": "A list of links to related objects",
            },
            "DATE_CREATED_UTC": {
                "type": SoftDateOrNothingType,
                "display_name": "Date Created",
                "short_desc": "Creation timestamp (UTC)",
                "desc": "The UTC timestamp when the task was created",
            },
            "DATE_UPDATED_UTC": {
                "type": SoftDateOrNothingType,
                "display_name": "Date Updated",
                "short_desc": "Last update timestamp (UTC)",
                "desc": "The UTC timestamp when the task was last updated",
            },
        };
    }

    constructor() { addQoreFields(Fields); }
}

public class InsightlyCreateTaskRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "TITLE": {
                "type": SoftStringType,
                "display_name": "Title",
                "required": True,
                "short_desc": "Task title",
                "desc": "The title of the task (required)",
                "example_value": "Follow up with client",
            },
            "CATEGORY_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Category ID",
                "short_desc": "Task category identifier",
                "desc": "The ID of the task category",
                "ref_data": "task_categories",
            },
            "DUE_DATE": {
                "type": SoftDateOrNothingType,
                "display_name": "Due Date",
                "short_desc": "Task due date",
                "desc": "The date the task is due",
                "preselected": True,
            },
            "DETAILS": {
                "type": SoftStringOrNothingType,
                "display_name": "Details",
                "short_desc": "Task details",
                "desc": "Detailed description of the task",
                "preselected": True,
            },
            "STATUS": {
                "type": SoftStringOrNothingType,
                "display_name": "Status",
                "short_desc": "Task status",
                "desc": "The current status of the task",
                "preselected": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "Not Started", "display_name": "Not Started",
                        "desc": "Not yet started"},
                    <AllowedValueInfo>{"value": "In Progress", "display_name": "In Progress",
                        "desc": "Currently in progress"},
                    <AllowedValueInfo>{"value": "Completed", "display_name": "Completed",
                        "desc": "Successfully completed"},
                    <AllowedValueInfo>{"value": "Deferred", "display_name": "Deferred",
                        "desc": "Deferred to a later date"},
                    <AllowedValueInfo>{"value": "Waiting", "display_name": "Waiting",
                        "desc": "Waiting on something"},
                ),
            },
            "PERCENT_COMPLETE": {
                "type": SoftIntOrNothingType,
                "display_name": "Percent Complete",
                "short_desc": "Completion percentage",
                "desc": "The percentage of the task that has been completed (0-100)",
            },
            "START_DATE": {
                "type": SoftDateOrNothingType,
                "display_name": "Start Date",
                "short_desc": "Task start date",
                "desc": "The date the task was or will be started",
            },
            "PRIORITY": {
                "type": SoftIntOrNothingType,
                "display_name": "Priority",
                "short_desc": "Task priority (1-4)",
                "desc": "The priority level of the task",
                "allowed_values": (
                    <AllowedValueInfo>{"value": 1, "display_name": "Low",
                        "desc": "Low priority"},
                    <AllowedValueInfo>{"value": 2, "display_name": "Medium",
                        "desc": "Medium priority"},
                    <AllowedValueInfo>{"value": 3, "display_name": "High",
                        "desc": "High priority"},
                    <AllowedValueInfo>{"value": 4, "display_name": "Urgent",
                        "desc": "Urgent priority"},
                ),
            },
            "COMPLETED": {
                "type": SoftBoolOrNothingType,
                "display_name": "Completed",
                "short_desc": "Whether the task is completed",
                "desc": "If `True`, the task has been completed",
            },
            "RESPONSIBLE_USER_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Responsible User ID",
                "short_desc": "Responsible user identifier",
                "desc": "The ID of the user responsible for this task",
                "ref_data": "users",
            },
            "OWNER_USER_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Owner User ID",
                "short_desc": "The owner user identifier",
                "desc": "The ID of the user who owns this task",
                "ref_data": "users",
            },
            "ASSIGNED_TEAM_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Assigned Team ID",
                "short_desc": "Assigned team identifier",
                "desc": "The ID of the team assigned to this task",
                "ref_data": "teams",
            },
            "PUBLICLY_VISIBLE": {
                "type": SoftBoolOrNothingType,
                "display_name": "Publicly Visible",
                "short_desc": "Whether the task is publicly visible",
                "desc": "If `True`, the task is visible to all users",
            },
            "REMINDER_DATE_UTC": {
                "type": SoftDateOrNothingType,
                "display_name": "Reminder Date",
                "short_desc": "Reminder timestamp (UTC)",
                "desc": "The UTC timestamp for the task reminder",
            },
            "RECURRENCE": {
                "type": SoftStringOrNothingType,
                "display_name": "Recurrence",
                "short_desc": "Recurrence pattern",
                "desc": "The recurrence pattern for the task",
            },
            "TASKLINKS": {
                "type": new ListDataType("InsightlyTaskLinkList",
                    new InsightlyTaskLinkDataType(), True),
                "display_name": "Task Links",
                "short_desc": "Related object links",
                "desc": "A list of links to related objects",
            },
        };
    }

    constructor() { addQoreFields(Fields); }
}

public class InsightlyUpdateTaskRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "TASK_ID": {
                "type": SoftIntType,
                "display_name": "Task ID",
                "required": True,
                "short_desc": "The task ID to update",
                "desc": "The unique identifier of the task to update",
                "ref_data": "tasks",
            },
        } + InsightlyDataProviderBase::makeFieldsOptional(
            InsightlyCreateTaskRequestDataType::Fields);
    }

    constructor() { addQoreFields(Fields); }
}

public class InsightlyDeleteTaskRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "TASK_ID": {
                "type": SoftIntType,
                "display_name": "Task ID",
                "required": True,
                "short_desc": "The task ID to delete",
                "desc": "The unique identifier of the task to delete",
                "ref_data": "tasks",
            },
        };
    }

    constructor() { addQoreFields(Fields); }
}

} # namespace InsightlyDataProvider
